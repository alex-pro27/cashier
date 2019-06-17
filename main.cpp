#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <stdlib.h>
#include <string>
#include <sstream>
#include <time.h>
#include "websocket.h"
#include "Arcus.h"
#include "json.hpp"
#include "PiritLib.h"
#include "helpers.hpp"
#include <regex>

template <typename F>
struct privDefer {
	F f;
	privDefer(F f) : f(f) {}
	~privDefer() { f(); }
};

template <typename F>
privDefer<F> defer_func(F f) {
	return privDefer<F>(f);
}

#define DEFER_1(x, y) x##y
#define DEFER_2(x, y) DEFER_1(x, y)
#define DEFER_3(x)    DEFER_2(x, __COUNTER__)
#define defer(code)   auto DEFER_3(_defer_) = defer_func([&](){code;})

using namespace std;
using namespace Arcus;
using namespace Helpers;

webSocket server;
ArcusHandlers* arcus;
bool isOpenShift = false;
int docCounter = 0;

constexpr auto PRINTER_PORT = "COM9";
constexpr auto PRINTER_COM_SPEED = 57600;

/* called when a client connects */
void openHandler(int clientID) {
	ostringstream os;
	os << "Client " << clientID << " has joined.";
	cout << os.str() << endl;
	vector<int> clientIDs = server.getClientIDs();
	RSJresource r("{}");
	r["data"] = RSJresource("{}");
	r["event"] = "onopen";
	r["data"]["text"] = "Welcome!";
	server.wsSend(clientID, r.to_json());
}

/* called when a client disconnects */
void closeHandler(int clientID) {
	ostringstream os;
	os << "Client " << clientID << " has leaved.";
	cout << os.str() << endl;
	//vector<int> clientIDs = server.getClientIDs();
	//for (int i = 0; i < clientIDs.size(); i++) {
		//if (clientIDs[i] != clientID)
			//server.wsSend(clientIDs[i], os.str());
	//}
}

/* called when a client sends a message to the server */
void messageHandler(int clientID, std::string message) {
	ostringstream os;
	os << "send " << clientID;
	cout << os.str() << endl;
	RSJresource res(message);
	string ev = res["event"].as<string>();
	RSJresource data = res["data"].as<string>();
	RSJresource r("{}");
	r["data"] = RSJresource("{}");
	int authID = 0;
	int err;

	err = openPort(PRINTER_PORT, PRINTER_COM_SPEED);
	defer(closePort());

	if (err) {
		r["data"]["error"] = "true";
		r["data"]["error_code"] = err;
		r["data"]["message"] = ws2s(L"Нет связи с ккт");
		server.wsSend(clientID, r.to_json());
		return;
	}
	//commandStart();

	if (ev == "open_shift") {
		isOpenShift = true;
		string cashier = data["cashier"].as<string>();
		err = libOpenShift((char*)utf2oem((char*)cashier.c_str()).c_str());
		r["event"] = "onopen_shift";
		r["data"]["error"] = "false";
		if (err) {
			r["data"]["error"] = "true";
			server.wsSend(clientID, r.to_json());
			return;
		}
		isOpenShift = true;
		server.wsSend(clientID, r.to_json());
	}
	else if (ev == "close_shift") {
		r["event"] = "on_close_shift";
		r["data"]["error"] = "false";
		string cashier = data["cashier"].as<string>();
		int err = libPrintZReport((char*)utf2oem((char*)cashier.c_str()).c_str(), 1);
		if (err) {
			r["data"]["error"] = "true";
			r["data"]["text"] = ws2s(L"Ошибка закрытия смены");
			r["data"]["error_code"] = err;
		}
		else {
			arcus->clearAuths();
			docCounter = 0;
		}
		server.wsSend(clientID, r.to_json());
	}
	else if (ev == "force_close_shift") {
		int err = libEmergencyCloseShift();
		r["event"] = "on_force_close_shift";
		if (err) {
			r["data"]["error"] = "true";
			r["data"]["error_code"] = err;
		}
		else {
			docCounter = 0;
			arcus->clearAuths();
			r["data"]["error"] = "false";
		}
		server.wsSend(clientID, r.to_json());
	}
	else if (ev == "create_doc") {
		r["event"] = "on_create_doc";
		string cashier = data["cashier"].as<string>();
		string error = "false";
		string text;
		int err = 0;
		int num_depart = 1;	// Номер отдела (1..99)
		int doc_num = ++docCounter;
		//int doc_num = rand();
		long long sum = 0;
		double _sum = 0.0;
		vector<RSJresource> wares;

		int doc_type = data["doc_type"].as<int>();	// Режим и тип документа (2-продажа, 3-возврат)
		
		if (!isOpenShift) {
			text = ws2s(L"Сначала откройте смену");
			err = 1;
			goto SENDDATA;
		}

		if (!(doc_type == 2 || doc_type == 3)) {
			error = "true";
			text = ws2s(L"неверный тип документа (2-продажа, 3-возврат)");
			err = 1;
			goto SENDDATA;
		}
		// Открыть документ
		//commandStart();
		err = libOpenDocument(
			doc_type, 
			num_depart, 
			(char*)utf2oem((char*)cashier.c_str()).c_str(), 
			doc_num
		);
		if (err) {
			libCancelDocument();
			text = ws2s(L"Не удалось создать документ");
			goto SENDDATA;
		}
		
		wares = data["wares"].as_vector<RSJresource>();
		
		for (vector<RSJresource>::iterator it = wares.begin(); it != wares.end(); ++it) {
			RSJresource ware = *it;
			string name = utf2oem((char*)ware["name"].as<string>().c_str());
			string barcode = utf2oem((char*)ware["barcode"].as<string>().c_str());
			double quantity = ware["quantity"].as<double>();
			double price = ware["price"].as<double>();
			int tax_number = ware["tax_number"].as<int>(); // НДС 0 - 20%, 1 - 10%
			_sum += price * quantity;
			err = libAddPosition(name.c_str(), barcode.c_str(), quantity, price, tax_number, 0, num_depart, 0, 0, 0);
			if (err) {
				text = ws2s(L"Ошибка добавлнения позиции: " + s2ws(ware["name"].as<string>()));
				libCancelDocument();
				goto SENDDATA;
			}
			sum = (((int)(_sum * 100 + 0.5)) / 100.0) * 100;
		}	
		err = libSubTotal();
		if (err) {
			text = ws2s(L"Ошибка подъитога");
			libCancelDocument();
			goto SENDDATA;
		}
		err = libAddPayment(1, sum, "");
		if (err) {
			text = ws2s(L"Ошибка добавления типа оплаты");
			libCancelDocument();
			goto SENDDATA;
		}
		// err = libCompareSum(std::stol(amount));
		//if (err > 0) {
		//	r["data"]["error"] = "true";
		//	r["data"]["message"] = ws2s(L"Ошибка сравнения цен");
		//	libCancelDocument();
		//	goto PURCHARE;
		//}
		
		if (!err) {
			MData ans = libCloseDocument(0);
			if (ans.errCode > 0) {
				text = ws2s(L"Ошибка закрытия документа");
				err = ans.errCode;
				error = "true";
				libCancelDocument();
			}
		}
		else {
			error = "true";
		}
	SENDDATA:
		r["data"]["error"] = error;
		r["data"]["cashier"] = cashier;
		r["data"]["error_code"] = err;
		r["data"]["text"] = text;
		r["data"]["amount"] = sum;
		server.wsSend(clientID, r.to_json());
	}
	else if (ev == "pay") {
		r["event"] = "on_pay";
		string error = "false";
		string text;
		string amount = data["amount"].as<string>();
		if (!stoi(amount)) {
			error = "true";
			text = ws2s(L"Ошибка ввода суммы");
		}
		else {
			authID = arcus->auth();
			arcus->purchase(authID, (char*)amount.c_str());
			r["data"]["auth_id"] = authID + 1;
			r["data"]["rrn"] = "LINK_" + string(arcus->auths[authID].rrn);
			text = cp2utf((char*)arcus->auths[authID].text_message);
			if (atoi(arcus->auths[authID].responseCode) > 0) {
				error = "true";
				err = atoi(arcus->auths[authID].responseCode);
			}
		}
		r["data"]["error"] = error;
		r["data"]["error_code"] = err;
		r["data"]["text"] = text;
		server.wsSend(clientID, r.to_json());
	}
	else if (ev == "cancel_payment") {
		r["event"] = "on_cancel_payment";
		string auth_id = data["auth_id"].as<string>();
		string error = "false";
		string text;
		int oldAuthID = atoi(auth_id.c_str());
		if (!oldAuthID || arcus->auths.size() < oldAuthID) {
			r["data"]["error"] = "true";
			r["data"]["text"] = ws2s(L"Авторизация не найдена");
			server.wsSend(clientID, r.to_json());
			return;
		}
		
		//authID = arcus->auth();
		arcus->cancel(oldAuthID - 1);
		text = cp2utf(arcus->auths[oldAuthID - 1].text_message);
		if (atoi(arcus->auths[oldAuthID - 1].responseCode) > 0) {
			error = "true";
		}
		
		r["data"]["error"] = error;
		r["data"]["error_code"] = atoi((const char*)arcus->auths[oldAuthID - 1].responseCode);
		r["data"]["text"] = text;
		server.wsSend(clientID, r.to_json());
	}
	else if (ev == "cancel_payment_by_link") {
		r["event"] = "on_cancel_payment_by_linkt";
		string rrn = data["rrn"].as<string>();
		string amount = data["amount"].as<string>();
		string error = "false";
		string text;
		if (rrn == "") {
			error = "true";
			text = ws2s(L"Отсутсвует параметр rrn(ссылка платежа)");
		}
		else {
			authID = arcus->auth();
			const std::regex pattern("[^0-9]");
			string _rrn = regex_replace(rrn, pattern, "");
			arcus->force_cancel(authID, (char*)_rrn.c_str(), (char*)amount.c_str());
			text = cp2utf(arcus->auths[authID].text_message);
			if (atoi(arcus->auths[authID].responseCode) > 0) {
				error = "true";
			}
		}
		r["data"]["error"] = error;
		r["data"]["error_code"] = atoi((const char*)arcus->auths[authID].responseCode);
		r["data"]["text"] = text;
		server.wsSend(clientID, r.to_json());
	}
}

/* called once per select() loop */
void periodicHandler() {
	static time_t next = time(NULL) + 10;
	time_t current = time(NULL);
	if (current >= next) {
		ostringstream os;
		std::string timestring = ctime(&current);
		timestring = timestring.substr(0, timestring.size() - 1);
		os << timestring;

		vector<int> clientIDs = server.getClientIDs();
		for (int i = 0; i < clientIDs.size(); i++)
			server.wsSend(clientIDs[i], os.str());

		next = time(NULL) + 10;
	}
}

int main(int argc, char* argv[]) {
	arcus = new ArcusHandlers();
	int port = 8000;
	//libBeep(200);
	//cout << "Please set server port: ";
	//cin >> port;
	/* set event handler */
	server.setOpenHandler(openHandler);
	server.setCloseHandler(closeHandler);
	server.setMessageHandler(messageHandler);
	//server.setPeriodicHandler(periodicHandler);
	server.startServer(port);
	return 0;
}