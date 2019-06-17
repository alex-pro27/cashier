#pragma once
#ifndef ARCUS_H
#define ARCUS_H

#ifdef _MSC_VER
#pragma pack(pop)
#endif

#include <windows.h>
#include <vector>

using namespace std;

namespace Arcus {

	enum {
		lnProcCode = 7,
		lnRFU = 60,
		lnPan = 20,
		lnExpiry = 5,
		lnAmount = 13,
		lnCurrency = 4,
		lnTerminalID = 9,
		lnSpdhTerminalID = 17,
		lnRetrievalReference = 13,
		lnAuthIdentResponse = 9,
		lnResponseCode = 4,
		lnCardType = 80,
		lnDate = 7,
		lnTime = 7,
		lnBatchNum = 8,
		lnRFUCredit = 50,
		lnPinblock = 17,
		lnPayData = 50,
		lnPayId = 3,
		lnMtid = 5,
		lnReceivedTextMsg = 80,
		lnAID = 80,
		lnApplicationLabel = 80,
		lnTVR = 80,
		lnCardholderName = 27,
		lnTraceID = 61,
		lnDateTime = 13
	};

	struct UserAuthIntFull {
		int handle;
		int abg_id;
		int operType;							//[in] ��� �������� (��������)
		char rfu[lnRFU];
		char pan[lnPan];
		char expiry[lnExpiry];
		char pay_acc[lnPan];
		char additional_payment_data[80];
		char amount[lnAmount];					//[in] ����� � ��������
		char original_amount[lnAmount];			//[in] ������������ ����� � ��������
		char currency[lnCurrency];				//[in] ��� ������
		char terminalID[lnTerminalID];
		char rrn[lnRetrievalReference];			//[in][out] ������ (������ ��� �������� ��� �������
												//�����, � ��������� ������� - �����)
		char authCode[lnAuthIdentResponse];		//[out][in] ��� �����������
		char responseCode[lnResponseCode];		//[out] ��� ������
		char cardType[lnCardType];				//[out] �������� ���� �����
		char date[lnDate];						//[out] ���� ����������
		char time[lnTime];						//[out] ����� ����������
		char payment_data[lnPayData];					// ������ ��� �������� �� ����
		char data_to_print[lnPayData];					// ������ ��� ������ �� ����
		char home_operator[lnPayData];					// ������ ��� ������ �� ����
		char received_text_message[lnReceivedTextMsg];
		char text_message[lnReceivedTextMsg];	//[out] �����������
		char AID[lnAID];
		char ApplicationLabel[lnApplicationLabel];
		char TVR[lnTVR];
		int system_res;
		unsigned short receiptNumber;			//����� ����
		unsigned short Invoice;					//����� �������
		int operatopn_type;						//��� ��������
		char card_number[20];					//����� �����
		char merchant[80];						//������������� �����������
		char date_buffer[100];
		unsigned short debit_number;
		char debit_total[7];
		char debit_clear_total[7];
		unsigned short return_number;
		char return_total[7];
		char return_clear_total[7];
		unsigned long butch_num;				//����� ����� / ������
		char TrAmount[13];
		char enc_data[64];
		char cardholder_name[lnCardholderName];	//��� ��������� ����
		char max_discount[lnAmount];
		char min_discount[lnAmount];
		char commission_amount[lnAmount + 1];	//����� �������� (� ����������� �������� ������)
		char PaymentsData[102]; //� unipay (������ ���� ������ ���� ����������������)
		//PaymentsData[0:2] � ������������� �������;
		//PaymentsData[3:35] � ���� �������, �������� 1;
		//PaymentsData[36:68] - ���� �������, �������� 2;
		//PaymentsData[69:101] - ���� �������, �������� 3;
		char BIN[7];
		char Hash[41];
		char Last4Digits[7];
		char isDiscoutedGoods;
		char Balance[lnAmount + 1];
		char TraceID[lnTraceID];
		char OriginalDateTime[lnDateTime];
		unsigned short CashRecieptNumber;
		char OutPutTransactionData[256];
		char TransactionID[19];
		char ComissionOffline[lnAmount];
		char ComissionOfflineCurrency[lnCurrency];
		char ComissionAcquirer[lnAmount];
		char ComissionAcquirerCurrency[lnCurrency];
		char ComissionIssuer[lnAmount];
		char ComissionIssuerCurrency[lnCurrency];
		char AuthorizedAmount[lnAmount];
		char AuthorizedAmountCurrency[lnCurrency];
		char HostResponseCode[lnResponseCode];
		char HostDialectName[32];
		unsigned char CardTypeLimitation; //������� �����������, ��. CARD_TYPE_*
		char RequestedDiscount[lnAmount]; //����� � ����������� �������� ������, ������ ������������ ����� �������� �������
		unsigned char PosEntryMode; //����� ����� �����
		char NoICCremoving;
		//char Envelope9F70[LLC_ENVELOPELEN]; //���������� ���� 9F70 �������� (������ LLC)
		//char EqualDeal[LLC_EQUALDEALLEN]; //���������� ���� EqualDeal (������ LLC)
		//char HostDesc[LLC_HOSTDESCLEN]; //���������� ���� HostDesc (������ LLC)
		int EqualDealLen; //����� ���� EqualDeal (������ LLC)
		int HostDescLen; //����� ���� HostDesc (������ LLC)
		//union {
			//struct TLV_data_t as_tlv;
			//unsigned char storage[34];
		//} Tag9F6E;
		int OriginalOperType; //[in] ��� ������������ �������� ��� ������ ��� �����(������ ��� �������� OW / Fuel + )
			/* add boundary padding here if needed */
	};

	typedef int(__cdecl* _PCPOSFUNCFULL)(UserAuthIntFull* auth_st_full, int size);

	class ArcusHandlers {

	private:
		_PCPOSFUNCFULL ProcessOwFull;
		HMODULE hLib;
	public:

		vector<UserAuthIntFull> auths;

		ArcusHandlers();
		~ArcusHandlers();
		int auth(void);
		void clearAuths(void);
		void purchase(int, char*);
		void cancel(int);
		void force_cancel(int, char*, char*);
		void apply(int);
		int getNextAuthID(void);
		int addAuth(UserAuthIntFull auth);
		string getCheque(void);
	};
}
#endif