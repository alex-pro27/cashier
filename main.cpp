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

using namespace std;
using namespace Arcus;
using namespace Helpers;

webSocket server;
ArcusHandlers* arcus;
bool isOpenShift = false;

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
	RSJresource r("{}");
	r["data"] = RSJresource("{}");
	string amount;
	int authID = 0;

	// FIXME <!--
	if (ev == "test") {
		vector<RSJresource> wares = res["wares"].as_vector<RSJresource>();
		for (vector<RSJresource>::iterator it = wares.begin(); it != wares.end(); ++it) {
			RSJresource ware = *it;
			string name = ware["name"].as<string>();
			cout << utf2oem((char*)name.c_str()) << endl;
		}
	}
	// FIXME -->

	if (ev == "open_shift") {
		string cashier = res["cashier"].as<string>();
		int err = libOpenShift((char*)utf2oem((char*)cashier.c_str()).c_str());
		r["event"] = "onopen_shift";
		if (err > 0) {
			r["data"]["error"] = "true";
			server.wsSend(clientID, r.to_json());
			return;
		}
		isOpenShift = true;
		server.wsSend(clientID, r.to_json());
	}
	else if (ev == "close_shift") {
		arcus->clearAuths();
		r["event"] = "onclose_shift";
		r["data"]["error"] = "false";
		string cashier = res["cashier"].as<string>();
		int err = libPrintZReport((char*)utf2oem((char*)cashier.c_str()).c_str(), 1);
		if (err) {
			r["data"]["error"] = "true";
			r["data"]["text"] = ws2s(L"Ошибка закрытия смены");
			r["data"]["error_code"] = err;
		}
		server.wsSend(clientID, r.to_json());
	}
	else if (ev == "force_close_shift") {
		arcus->clearAuths();
		int err = libEmergencyCloseShift();
		r["event"] = "onforce_close_shift";
		r["data"]["error"] = "false";
		server.wsSend(clientID, r.to_json());
	}
	else if (ev == "purchase") {
		//MData d = libGetStatusFlags();
		amount = res["amount"].as<string>();
		r["event"] = "onpurchase";
		if (!atoi(amount.c_str())) {
			r["data"]["error"] = "true";
			r["data"]["text"] = ws2s(L"Некорректный ввод суммы для оплаты");
			server.wsSend(clientID, r.to_json());
			return;
		}
		string cashier = res["cashier"].as<string>();
		string error = "false";
		string text;
		int response_code = 0;
		int err = 0;
		int err_connect_print = openPort(PRINTER_PORT, PRINTER_COM_SPEED);
		if (err_connect_print) {
			error = "true";
			text = ws2s(L"Нет связи с ккт");
			goto SENDDATA;
		}
		else {
			int doc_type = 2;	// Режим и тип документа (2-продажа, 3-возврат)
			int num_depart = 1;	// Номер отдела (1..99)
			int doc_num = std::rand(); // FIXME
			err = libOpenDocument(
				doc_type, num_depart, (char*)cashier.c_str(), doc_num
			); // Открыть документ
			if (err) {
				libCancelDocument();
				error = "true";
				text = ws2s(L"Не удалось создать документ");
				goto SENDDATA;
			}

			unsigned char taxNumber = 1; // Номер ставки налога (0..5)
			unsigned char numDepart = 1; // Номер секции (1..16)
			long long sum = 0;
			vector<RSJresource> wares = res["wares"].as_vector<RSJresource>();
			for (vector<RSJresource>::iterator it = wares.begin(); it != wares.end(); ++it) {
				RSJresource ware = *it;
				string name = utf2oem((char*)ware["name"].as<string>().c_str());
				string barcode = utf2oem((char*)ware["barcode"].as<string>().c_str());
				long quantity = ware["quantity"].as<long>();
				double price = ware["price"].as<double>();
				sum += price * 1000;
				err = libAddPosition(name.c_str(), barcode.c_str(), quantity, price, taxNumber, 0, numDepart, 0, 0, 0);
				// FIXME
				if (err) {
					error = "true";
					text = ws2s(L"Ошибка добавлнения позиции");
					libCancelDocument();
					goto SENDDATA;
				}
				// FIXME
			}
			
			//err = libAddPosition("TOBAP N:1 KPEM 'ABCDEFGH'", "9785845913784", quantity, price, taxNumber, 0, numDepart, 0, 0, 0);
			
			err = libSubTotal();
			if (err) {
				error = "true";
				text = ws2s(L"Ошибка подъитога");
				response_code = err;
				libCancelDocument();
				goto SENDDATA;
			}
			err = libAddPayment(1, sum, "");
			if (err) {
				error = "true";
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

			//libPrintBarCode(2, 8, 8, 8, cashier.c_str());
			//libCutDocument();
		}

		if (!err) {
			authID = arcus->auth();
			arcus->purchase(authID, (char*)amount.c_str());
			r["data"]["auth_id"] = authID + 1;
			r["data"]["rrn"] = (const char*)arcus->auths[authID].rrn;

			string cheque = arcus->getCheque();
			//r["data"]["cheque"] = cp2utf((char*)cheque.c_str());
			//if (cheque.size()) {
			//	int err = libPrintString((char*)cheque.c_str(), get_mask(1, 0));
			//	if (err > 0) {
			//		int ee = 1;
			//	}
			//}
			if (atoi(arcus->auths[authID].responseCode) > 0) {
				error = "true";
				response_code = atoi(arcus->auths[authID].responseCode);
				libCancelDocument();
			}
			else {
				MData ans = libCloseDocument(0);
				if (ans.errCode > 0) {
					text = ws2s(L"Ошибка закрытия документа");
					response_code = ans.errCode;
					error = "true";
					libCancelDocument();
				}
			}
		}
	SENDDATA:
		r["data"]["error"] = error;
		r["data"]["cashier"] = cashier;
		r["data"]["response_code"] = response_code;
		r["data"]["text"] = text;
		server.wsSend(clientID, r.to_json());
		closePort();
	}
	else if (ev == "cancel") {
		string auth_id = res["auth_id"].as<string>();
		int oldAuthID = atoi(auth_id.c_str());
		r["event"] = "oncancel";
		if (!oldAuthID || arcus->auths.size() < oldAuthID) {
			r["data"]["error"] = "true";
			r["data"]["text"] = ws2s(L"Авторизация не найдена");
			server.wsSend(clientID, r.to_json());
			return;
		}
		//authID = arcus->auth();
		arcus->cancel(oldAuthID -1);
		string error = "false";
		if (atoi(arcus->auths[oldAuthID - 1].responseCode) > 0) {
			error = "true";
		}
		r["data"]["error"] = error;
		r["data"]["response_code"] = (const char*)arcus->auths[oldAuthID - 1].responseCode;
		r["data"]["text"] = cp2utf(arcus->auths[oldAuthID -1].text_message);
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
	openPort(PRINTER_PORT, PRINTER_COM_SPEED);
	commandStart();
	libBeep(200);
	//cout << "Please set server port: ";
	//cin >> port;
	/* set event handler */
	server.setOpenHandler(openHandler);
	server.setCloseHandler(closeHandler);
	server.setMessageHandler(messageHandler);
	//server.setPeriodicHandler(periodicHandler);
	server.startServer(port);
	closePort();
	return 0;
}