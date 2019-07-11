#pragma once
#ifndef CASHIER_H
#define CASHIER_H

#include <iostream>
#include <string>
#include <regex>
#include <fstream>
#include "websocket.h"
#include "Arcus.hpp"
#include "json.hpp"
#include "helpers.hpp"
#include "PiritLib.h"

using namespace std;
using namespace Helpers;

class CashierHandlers {
public:
	RSJresource answer;
	ArcusHandlers* arcus;
	CashierHandlers();
	~CashierHandlers();
	RSJresource openShift(string ev, RSJresource data);
	RSJresource closeShift(string ev, RSJresource data);
	RSJresource forceCloseShift(string ev, RSJresource data);
	RSJresource transactionHandler(string ev, RSJresource data);
	RSJresource cashDrawerHandler(string ev, RSJresource data);
	RSJresource cancelPymentByLink(string ev, RSJresource data);
	RSJresource setZeroCashDrawer(string ev, RSJresource data);
	RSJresource KKTInfo(string ev, RSJresource data);
	string getKKTInfo(int num);
	string getFnNumber();
	string getCacheBalance();
	string getINN();
	string getShiftNumber();

private:
	int doc_counter = 0;
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
	
	this->answer["data"]["error"] = error;
	this->answer["data"]["error_code"] = err_code;
	this->answer["data"]["text"] = text;
	this->answer["data"]["cacher"] = cashier;
	this->answer["data"]["inn"] = getINN();
	this->answer["data"]["cache_balance"] = this->getCacheBalance();
	this->answer["data"]["shift_number"] = this->getShiftNumber();
	this->answer["data"]["fn_number"] = this->getFnNumber();
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

	this->arcus->clearAuths();
	this->doc_counter = 0;
	this->answer["data"]["error"] = error;
	this->answer["data"]["error_code"] = err_code;
	this->answer["data"]["text"] = text;
	this->answer["data"]["cacher"] = cashier;
	this->answer["data"]["inn"] = getINN();
	this->answer["data"]["cache_balance"] = this->getCacheBalance();
	this->answer["data"]["shift_number"] = this->getShiftNumber();
	this->answer["data"]["fn_number"] = this->getFnNumber();
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
		this->arcus->clearAuths();
		this->doc_counter = 0;
		this->answer["data"]["error"] = "false";
		this->answer["data"]["inn"] = this->getINN();
		this->answer["data"]["cache_balance"] = this->getCacheBalance();
		this->answer["data"]["shift_number"] = this->getShiftNumber();
		this->answer["data"]["fn_number"] = this->getFnNumber();
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
	int num_depart = 1;	
	// Номер отдела (1..99)
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
		authID = this->arcus->auth();

		if (doc_type == 2) {
			this->arcus->purchase(authID, (char*)to_string(sum).c_str());
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
			this->arcus->cancelByLink(authID, (char*)_rrn.c_str(), (char*)to_string(sum).c_str());
		}

		text = cp2utf((char*)arcus->auths[authID].text_message);
		err_code = atoi(this->arcus->auths[authID].responseCode);

		if (err_code) {
			this->answer["data"]["auth_id"] = authID + 1;
			goto SEND;
		}
		flag_cashless = true;
		this->answer["data"]["rrn"] = "LINK_" + string(this->arcus->auths[authID].rrn);
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
		libCancelDocument();
		goto SEND;
	}
	flag_is_opened_doc = true;

	if (flag_cashless) {
		// Если безнали, то печатаем чек оплаты и копию
		this->printCheque();
		this->printCheque();
	}

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
		else {
			MData ans = libGetReceiptData(2);
			if (ans.errCode == 0) {
				string _data(ans.data);
				auto s = split(_data.substr(8, ans.dataLength - 12), "\x1c");
				this->answer["data"]["check_number"] = s[1];
				this->answer["data"]["doc_number"] = s[3];
				this->answer["data"]["discount_sum"] = s[5];
			}
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
	else {
		this->answer["data"]["amount"] = sum;
		this->answer["data"]["inn"] = getINN();
		this->answer["data"]["cache_balance"] = this->getCacheBalance();
		this->answer["data"]["shift_number"] = this->getShiftNumber();
		this->answer["data"]["fn_number"] = this->getFnNumber();
	}
	this->answer["data"]["error"] = error;
	this->answer["data"]["cashier"] = cashier;
	this->answer["data"]["error_code"] = err_code;
	this->answer["data"]["text"] = text;
	return this->answer;
};

RSJresource CashierHandlers::cashDrawerHandler(string ev, RSJresource data) {
	this->answer["event"] = "on" + ev;
	this->answer["data"] = RSJresource("{}");
	int err_code = 0;
	bool flag_is_opened_doc = false;
	string error;
	string text;
	string cashier = data["cashier"].as<string>();
	// Сумма в копейках
	string amount = data["amount"].as<string>();
	// Номер отдела (1..99)
	int num_depart = 1;
	// Номер документа
	int doc_num = ++this->doc_counter;
	// Режим и тип документа (4 - внесение, 5 - изъятие)
	int doc_type = data["doc_type"].as<int>();

	if (!(doc_type == 4 || doc_type == 5)) {
		error = "true";
		text = ws2s(L"Неверный тип документа (4-внесение, 5-изъятие)");
		err_code = 1;
		goto SEND;
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
		libCancelDocument();
		goto SEND;
	}
	flag_is_opened_doc = true;
	libOpenCashDrawer(0);
	err_code = libCashInOut("", atoi(amount.c_str()));
	if (err_code > 0) {
		text = ws2s(L"Не удалось внести/изъять деньги");
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
	}
	else {
		this->answer["data"]["inn"] = getINN();
		this->answer["data"]["cache_balance"] = this->getCacheBalance();
		this->answer["data"]["shift_number"] = this->getShiftNumber();
		this->answer["data"]["fn_number"] = this->getFnNumber();
		this->answer["data"]["amount"] = amount;
	}
	this->answer["data"]["error"] = error;
	this->answer["data"]["cashier"] = cashier;
	this->answer["data"]["error_code"] = err_code;
	this->answer["data"]["text"] = text;
	return this->answer;
};

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
	this->answer["data"]["inn"] = getINN();
	this->answer["data"]["cache_balance"] = this->getCacheBalance();
	this->answer["data"]["shift_number"] = this->getShiftNumber();
	this->answer["data"]["fn_number"] = this->getFnNumber();
	return this->answer;
};

RSJresource CashierHandlers::setZeroCashDrawer(string ev, RSJresource data) {
	this->answer["event"] = "on" + ev;
	this->answer["data"] = RSJresource("{}");
	string error = "false";
	string text;
	libOpenCashDrawer(0);
	int err_code = libSetToZeroCashInCashDrawer();
	if (err_code) {
		error = "true";
		text = ws2s(L"Не удалось обнулить денежный ящик");
	}
	this->answer["data"]["error"] = error;
	this->answer["data"]["error_code"] = err_code;
	this->answer["data"]["text"] = text;
	this->answer["data"]["inn"] = getINN();
	this->answer["data"]["cache_balance"] = this->getCacheBalance();
	this->answer["data"]["shift_number"] = this->getShiftNumber();
	this->answer["data"]["fn_number"] = this->getFnNumber();
	return this->answer;
};

RSJresource CashierHandlers::KKTInfo(string ev, RSJresource data) {
	this->answer["event"] = "on" + ev;
	this->answer["data"] = RSJresource("{}");
	this->answer["data"]["inn"] = getINN();
	this->answer["data"]["cache_balance"] = this->getCacheBalance();
	this->answer["data"]["shift_number"] = this->getShiftNumber();
	this->answer["data"]["fn_number"] = this->getFnNumber();
	return this->answer;
};

string CashierHandlers::getKKTInfo(int num) {
	MData inf = libGetKKTInfo(num);
	if (inf.errCode == 0) {
		string data(inf.data);
		return data.substr(8, inf.dataLength - 12);
	}
	else {
		return NULL;
	}
};

string CashierHandlers::getFnNumber() {
	return this->getKKTInfo(1);
};

inline string CashierHandlers::getCacheBalance() {
	return this->getKKTInfo(7);
};

inline string CashierHandlers::getINN() {
	return this->getKKTInfo(3);
};

inline string CashierHandlers::getShiftNumber() {
	MData inf = libGetCountersAndRegisters(1);
	if (inf.errCode == 0) {
		string data(inf.data);
		return data.substr(8, inf.dataLength - 12);
	}
	else {
		return NULL;
	}
};

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
