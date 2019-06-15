#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <windows.h>
#include "Arcus.h"

using namespace std;
using namespace Arcus;

ArcusHandlers::ArcusHandlers() {
	this->hLib = LoadLibrary(L"C:\\Arcus2\\DLL\\ArcCom.dll");
	this->ProcessOwFull = (_PCPOSFUNCFULL)GetProcAddress(this->hLib, "ProcessOwFull");
}

ArcusHandlers::~ArcusHandlers() {
	FreeLibrary(this->hLib);
}

void ArcusHandlers::apply(int authID) {
	ProcessOwFull(&this->auths[authID], sizeof(UserAuthIntFull));
}

int ArcusHandlers::auth() {
	UserAuthIntFull __auth;
	memset(&__auth, 0, sizeof(UserAuthIntFull));
	return this->addAuth(__auth);
}

int ArcusHandlers::getNextAuthID() {
	int i;
	for (i = 0; i < this->auths.size(); i++) {
		if (sizeof(this->auths[i]) == 0)
			break;
	}
	return i;
}

int ArcusHandlers::addAuth(UserAuthIntFull _auth) {
	
	int authID = getNextAuthID();
	if (authID >= this->auths.size()) {
		this->auths.push_back(_auth);
	}
	else {
		this->auths[authID] = _auth;
	}
	return authID;
}

void ArcusHandlers::purchase(int authID, char* amount) {
	this->auths[authID].operType = 1;
	strncpy_s(this->auths[authID].amount, sizeof(this->auths[authID].amount), amount, sizeof(amount)*4);
	cout << this->auths[authID].amount << endl;
	this->apply(authID);
}

void ArcusHandlers::cancel(int authID) {
	this->auths[authID].operType = 3;
	this->apply(authID);
}

void ArcusHandlers::clearAuths() {
	this->auths.clear();
}

string ArcusHandlers::getCheque() {
	ifstream cheque(L"C:\\Arcus2\\cheq.out");
	ostringstream data;
	string line;
	if (cheque.is_open()) {
		while (getline(cheque, line)) {
			data << line;
		}
		cheque.close();
	}
	return data.str();
}
