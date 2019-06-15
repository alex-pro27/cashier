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

	if (ev == "open_shift") {
		openPort(PRINTER_PORT, PRINTER_COM_SPEED);
		string cashier = res["cashier"].as<string>();
		int err = libOpenShift(cashier.c_str());
		if (err > 0) {
			r["event"] = "onclose_shift";
			r["data"]["error"] = "false";
			return;
		}
		libPrintBarCode(2, 8, 8, 8, cashier.c_str());
		libCutDocument();
		closePort();
	}
	else if (ev == "close_shift") {
		arcus->clearAuths();
		r["event"] = "onclose_shift";
		r["data"]["error"] = "false";
		// TODO added handlers for KKT
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
		int err_connect_print = openPort(PRINTER_PORT, PRINTER_COM_SPEED);
		//if (err_connect_print) {
			//r["data"]["error"] = "true";
			//r["data"]["text"] = ws2s(L"Нет связи с ккт");
			//server.wsSend(clientID, r.to_json());
		//	return;
		//}
		authID = arcus->auth();
		arcus->purchase(authID, (char*)amount.c_str());
		
		string cashier = res["cashier"].as<string>();
		string error = "false";
		if (atoi(arcus->auths[authID].responseCode) > 0) {
			error = "true";
		}
		else {
			string cheque = arcus->getCheque();
			if (cheque.size()) {
				libPrintString((char*)cheque.c_str(), get_mask(1, 0));
			}
			int doc_type = 2;	// Режим и тип документа (2-продажа, 3-возврат)
			int num_depart = 1;	// Номер отдела (1..99)
			int errorCode = libOpenDocument(
				doc_type, num_depart, (char*)cashier.c_str(), atoi(arcus->auths[authID].rrn)
			); // Открыть документ
			//libAddPosition()
		}

		r["data"]["error"] = error;
		r["data"]["auth_id"] = authID + 1;
		r["data"]["amount"] = (const char*)arcus->auths[authID].amount;
		r["data"]["cashier"] = cashier;
		r["data"]["response_code"] = (const char*)arcus->auths[authID].responseCode;
		r["data"]["rrn"] = (const char*)arcus->auths[authID].rrn;
		r["data"]["text"] = cp2utf(arcus->auths[authID].text_message);
		server.wsSend(clientID, r.to_json());
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