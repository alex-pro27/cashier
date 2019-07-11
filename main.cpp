#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <string>
#include <time.h>
#include "websocket.h"
#include "Cashier.hpp"
#include "json.hpp"
#include "helpers.hpp"
#include "PiritLib.h"
#include "Arcus.hpp"

using namespace std;
using namespace Helpers;

constexpr auto PRINTER_PORT = "COM9";
constexpr auto PRINTER_COM_SPEED = 57600;

webSocket server;
CashierHandlers* cashier;

void openHandler(int clientID) {
	ostringstream os;
	os << "Client " << clientID << " has joined.";
	cout << os.str() << endl;
	vector<int> clientIDs = server.getClientIDs();
	RSJresource r("{}");
	r["data"] = RSJresource("{}");
	r["event"] = "onconnected";
	r["last_answer"] = cashier->answer;
	server.wsSend(clientID, r.to_json());
}

void closeHandler(int clientID) {
	ostringstream os;
	os << "Client " << clientID << " has leaved.";
	cout << os.str() << endl;
}

void messageHandler(int clientID, std::string message) {
	ostringstream os;
	os << "send " << clientID;
	cout << os.str() << endl;
	RSJresource res(message);
	string ev = res["event"].as<string>();
	RSJresource data = res["data"].as<string>();

	int err_code = openPort(PRINTER_PORT, PRINTER_COM_SPEED);
	defer(closePort());

	if (err_code) {
		RSJresource r("{}");
		r["event"] = "on" + ev;
		r["data"] = RSJresource("{}");
		r["data"]["error"] = "true";
		r["data"]["error_code"] = err_code;
		r["data"]["text"] = ws2s(L"Нет связи с ккт");
		server.wsSend(clientID, r.to_json());
		return;
	}
	//commandStart();

	if (ev == "open_shift") {
		server.wsSend(clientID, cashier->openShift(ev, data).to_json());
	}
	else if (ev == "close_shift") {
		server.wsSend(clientID, cashier->closeShift(ev, data).to_json());
	}
	else if (ev == "force_close_shift") {
		server.wsSend(clientID, cashier->forceCloseShift(ev, data).to_json());
	}
	else if (ev == "new_transaction") {
		server.wsSend(clientID, cashier->transactionHandler(ev, data).to_json());
	}
	else if (ev == "cancel_pyment") {
		server.wsSend(clientID, cashier->cancelPymentByLink(ev, data).to_json());
	}
	else if (ev == "set_zero_cash") {
		server.wsSend(clientID, cashier->setZeroCashDrawer(ev, data).to_json());
	}
	else if (ev == "cash_drawer_handler") {
		server.wsSend(clientID, cashier->cashDrawerHandler(ev, data).to_json());
	}
	else if (ev == "get_kkt_info") {
		server.wsSend(clientID, cashier->KKTInfo(ev, data).to_json());
	}
	else {
		char text[80];
		RSJresource r("{}");
		r["data"] = RSJresource("{}");
		r["event"] = "on" + ev;
		sprintf(text, "event %s not supported", ev.c_str());
		r["data"]["error"] = "true";
		r["data"]["error_code"] = 403;
		r["data"]["text"] = (const char*)text;
		server.wsSend(clientID, r.to_json());
	}
}

int main(int argc, char* argv[]) {
	int port = 8000;
	//cout << "Please set server port: ";
	//cin >> port;
	/* set event handler */
	cashier = new CashierHandlers();
	server.setOpenHandler(openHandler);
	server.setCloseHandler(closeHandler);
	server.setMessageHandler(messageHandler);
	server.startServer(port);
	delete cashier;
	return 0;
}