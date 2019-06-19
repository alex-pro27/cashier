#pragma once
#ifndef CASHIER_H
#define CASHIER_H

#include <iostream>
#include <string>
#include "websocket.h"
#include "Arcus.hpp"
#include "json.hpp"
#include "helpers.hpp"
#include <regex>
#include <fstream>
#include <windows.h>
#include "PiritLib.h"

using namespace std;
using namespace Helpers;

class CashierHandlers {
public:
	RSJresource answer;
	CashierHandlers();
	~CashierHandlers();
	RSJresource openShift(string ev, RSJresource data);
	RSJresource closeShift(string ev, RSJresource data);
	RSJresource forceCloseShift(string ev, RSJresource data);
	RSJresource transactionHandler(string ev, RSJresource data);
	RSJresource cashDrawerHandler(string ev, RSJresource data);
	RSJresource cancelPymentByLink(string ev, RSJresource data);
	RSJresource setZeroCashDrawer(string ev, RSJresource data);

private:
	bool is_open_shift = false;
	int doc_counter = 0;
	ArcusHandlers* arcus;
	void printCheque();
};

CashierHandlers::CashierHandlers() {
	this->arcus = new ArcusHandlers();
	this->answer = RSJresource("{}");
}

CashierHandlers::~CashierHandlers() {
	delete this->arcus;
}

RSJresource CashierHandlers::openShift(string ev, RSJresource data) {
	int err_code = 0;
	string error;
	string text;
	string cashier = data["cashier"].as<string>();
	this->answer["event"] = "on" + ev;
	this->answer["data"] = RSJresource("{}");
	this->answer["data"]["error"] = "false";
	err_code = libOpenShift((char*)utf2oem((char*)cashier.c_str()).c_str());
	if (err_code) {
		error = "true";
		text = ws2s(L"Ошибка открытия смены");
	}
	
	this->is_open_shift = true;
	this->answer["data"]["error"] = error;
	this->answer["data"]["error_code"] = err_code;
	this->answer["data"]["text"] = text;
	return this->answer;
}

RSJresource CashierHandlers::closeShift(string ev, RSJresource data) {
	int err_code = 0;
	string error = "false";
	string text;
	string cashier = data["cashier"].as<string>();
	this->answer["event"] = "on" + ev;
	this->answer["data"] = RSJresource("{}");
	err_code = libPrintZReport((char*)utf2oem((char*)cashier.c_str()).c_str(), 1);
	if (err_code) {
		error = "true";
		text = ws2s(L"Ошибка закрытия смены");
	}

	this->is_open_shift = false;
	this->arcus->clearAuths();
	this->doc_counter = 0;
	this->answer["data"]["error"] = error;
	this->answer["data"]["error_code"] = err_code;
	this->answer["data"]["text"] = text;
	return this->answer;
}

RSJresource CashierHandlers::forceCloseShift(string ev, RSJresource data) {
	int err_code = libEmergencyCloseShift();
	this->answer["event"] = "on" + ev;
	this->answer["data"] = RSJresource("{}");
	if (err_code) {
		this->answer["data"]["error"] = "true";
		this->answer["data"]["error_code"] = err_code;
	}
	else {
		this->is_open_shift = false;
		this->arcus->clearAuths();
		this->doc_counter = 0;
		this->answer["data"]["error"] = "false";
	}
	return this->answer;
}

RSJresource CashierHandlers::transactionHandler(string ev, RSJresource data) {
	this->answer["event"] = "on" + ev;
	this->answer["data"] = RSJresource("{}");
	int authID;
	int err_code = 0;
	string error;
	string text;
	string cashier = data["cashier"].as<string>();
	int num_depart = 1;	// Номер отдела (1..99)
	int doc_num = ++this->doc_counter;
	long long sum = 0;
	double _sum = 0.0;
	// 0 - наличка, 1 - безнал(настраивается в Fito)
	int pyment_type = data["pyment_type"].as<int>();
	ifstream cheque;
	string line;
	// Режим и тип документа (2-продажа, 3-возврат)
	int doc_type = data["doc_type"].as<int>();
	vector<RSJresource> wares = data["wares"].as_vector<RSJresource>();
	bool flag_cashless = false;
	bool flag_is_opened_doc = false;

	if (!this->is_open_shift) {
		text = ws2s(L"Сначала откройте смену");
		err_code = 1;
		goto SEND;
	}

	if (!(doc_type == 2 || doc_type == 3)) {
		error = "true";
		text = ws2s(L"Неверный тип документа (2-продажа, 3-возврат)");
		err_code = 1;
		goto SEND;
	}

	for (vector<RSJresource>::iterator it = wares.begin(); it != wares.end(); ++it) {
		RSJresource ware = *it;
		double price = ware["price"].as<double>();
		double quantity = ware["quantity"].as<double>();
		_sum += price * quantity;
		sum = (((int)(_sum * 100 + 0.5)) / 100.0) * 100;
	}

	if (pyment_type == 1) {
		// Оплата безналичным
		authID = arcus->auth();

		if (doc_type == 2) {
			arcus->purchase(authID, (char*)sum);
		}
		else if (doc_type == 3) {
			string rrn = data["rrn"].as<string>();
			if (rrn == "") {
				text = ws2s(L"Отсутсвует параметр rrn(ссылка платежа)");
				err_code = 405;
				goto SEND;
			}
			const regex pattern("[^0-9]");
			string _rrn = regex_replace(rrn, pattern, "");
			arcus->cancelByLink(authID, (char*)_rrn.c_str(), (char*)sum);
		}

		text = cp2utf((char*)arcus->auths[authID].text_message);
		err_code = atoi(arcus->auths[authID].responseCode);

		if (err_code) {
			this->answer["data"]["auth_id"] = authID + 1;
			goto SEND;
		}
		flag_cashless = true;
		this->answer["data"]["rrn"] = "LINK_" + string(arcus->auths[authID].rrn);
		this->answer["data"]["auth_id"] = authID + 1;
	}
	else if (pyment_type == 0) {
		// Наличка
		libOpenCashDrawer(0); // Открыть денежный ящик
	}
	// Открыть документ
	err_code = libOpenDocument(
		doc_type,
		num_depart,
		(char*)utf2oem((char*)cashier.c_str()).c_str(),
		doc_num
	);

	if (err_code) {
		text = ws2s(L"Не удалось создать документ");
		goto SEND;
	}
	flag_is_opened_doc = true;
	this->printCheque();

	for (vector<RSJresource>::iterator it = wares.begin(); it != wares.end(); ++it) {
		RSJresource ware = *it;
		string name = utf2oem((char*)ware["name"].as<string>().c_str());
		string barcode = utf2oem((char*)ware["barcode"].as<string>().c_str());
		double quantity = ware["quantity"].as<double>();
		double price = ware["price"].as<double>();
		int tax_number = ware["tax_number"].as<int>(); // НДС 0 - 20%, 1 - 10%
		err_code = libAddPosition(name.c_str(), barcode.c_str(), quantity, price, tax_number, 0, num_depart, 0, 0, 0);
		if (err_code) {
			text = ws2s(L"Ошибка добавлнения позиции: " + s2ws(ware["name"].as<string>()));
			goto SEND;
		}
	}

	// подъитог
	err_code = libSubTotal();

	if (err_code) {
		text = ws2s(L"Ошибка подъитога");
		libCancelDocument();
		goto SEND;
	}

	// добавление типа оплаты
	err_code = libAddPayment(pyment_type, sum, "");

	if (err_code) {
		text = ws2s(L"Ошибка добавления типа оплаты");
		goto SEND;
	}

	if (!err_code) {
		// Закрыть документ
		MData ans = libCloseDocument(0);
		if (ans.errCode > 0) {
			text = ws2s(L"Ошибка закрытия документа");
			err_code = ans.errCode;
		}
	}

SEND:
	if (err_code) {
		error = "true";
		if (flag_is_opened_doc)
			libCancelDocument();
		if (flag_cashless)
			this->arcus->cancelLast(authID);
	}
	this->answer["data"]["error"] = error;
	this->answer["data"]["cashier"] = cashier;
	this->answer["data"]["error_code"] = err_code;
	this->answer["data"]["text"] = text;
	this->answer["data"]["amount"] = sum;
	return this->answer;
}

RSJresource CashierHandlers::cashDrawerHandler(string ev, RSJresource data) {
	// TODO внесение/возврат средсв в денежный ящик
	return RSJresource();
}

RSJresource CashierHandlers::cancelPymentByLink(string ev, RSJresource data) {
	this->answer["event"] = "on" + ev;
	this->answer["data"] = RSJresource("{}");
	string rrn = data["rrn"].as<string>();
	string amount = data["amount"].as<string>();
	string error = "false";
	string text;
	int authID;
	int err_code;

	if (rrn == "") {
		text = ws2s(L"Отсутсвует параметр rrn(ссылка платежа)");
		err_code = 405;
	}
	else {
		authID = arcus->auth();
		const regex pattern("[^0-9]");
		string _rrn = regex_replace(rrn, pattern, "");
		arcus->cancelByLink(authID, (char*)_rrn.c_str(), (char*)amount.c_str());
		text = cp2utf(arcus->auths[authID].text_message);
		err_code = atoi((const char*)arcus->auths[authID].responseCode);
	}
	if (err_code) {
		error = "true";
	}
	this->answer["data"]["error"] = error;
	this->answer["data"]["error_code"] = err_code;
	this->answer["data"]["text"] = text;
	return this->answer;
}

RSJresource CashierHandlers::setZeroCashDrawer(string ev, RSJresource data) {
	this->answer["event"] = "on" + ev;
	this->answer["data"] = RSJresource("{}");
	string error = "false";
	string text;
	int err_code = libSetToZeroCashInCashDrawer();
	if (err_code) {
		error = "true";
		text = ws2s(L"Не удалось обнулить денежный ящик");
	}
	this->answer["data"]["error"] = error;
	this->answer["data"]["error_code"] = err_code;
	this->answer["data"]["text"] = text;
	return this->answer;
}

inline void CashierHandlers::printCheque() {
	// Напечатать чек оплаты по безналу (только если открыт документ)
	ifstream cheque(L"C:\\Arcus2\\cheq.out");
	string line;
	if (cheque.is_open()) {
		while (getline(cheque, line)) {
			libPrintRequsit(0, 1, cp2oem((char*)line.c_str()).c_str(), "", "", "");
		}
		cheque.close();
	}
	libCutDocument();
};

#endif // !CASHIER_H
