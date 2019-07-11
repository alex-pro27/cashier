#pragma once
#ifndef HELPERS_H
#define HELPERS_H
#define DEFER_1(x, y) x##y
#define DEFER_2(x, y) DEFER_1(x, y)
#define DEFER_3(x)    DEFER_2(x, __COUNTER__)
#define defer(code)   auto DEFER_3(_defer_) = defer_func([&](){code;})

#include <locale> 
#include <codecvt>
#include <string>
#include <vector>

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

namespace Helpers {

	vector<string> split(const string& str, const string& delim) {
		vector<string> tokens;
		size_t prev = 0, pos = 0;
		do {
			pos = str.find(delim, prev);
			if (pos == string::npos) pos = str.length();
			string token = str.substr(prev, pos - prev);
			if (!token.empty()) tokens.push_back(token);
			prev = pos + delim.length();
		} while (pos < str.length() && prev < str.length());
		return tokens;
	}

	std::wstring s2ws(const std::string& str) {
		using convert_typeX = std::codecvt_utf8<wchar_t>;
		std::wstring_convert<convert_typeX, wchar_t> converterX;
		return converterX.from_bytes(str);
	};

	std::string ws2s(const std::wstring& wstr) {
		using convert_typeX = std::codecvt_utf8<wchar_t>;
		std::wstring_convert<convert_typeX, wchar_t> converterX;
		return converterX.to_bytes(wstr);
	};

	std::string transcode(char* pszCode, int from, int to) {
		BSTR bstrWide;
		char psz[4096];
		int nLength;
		char* text;
		nLength = MultiByteToWideChar(from, 0, pszCode, strlen(pszCode) + 1, NULL, NULL);
		bstrWide = SysAllocStringLen(NULL, nLength);
		MultiByteToWideChar(from, 0, pszCode, strlen(pszCode) + 1, bstrWide, nLength);
		nLength = WideCharToMultiByte(to, 0, bstrWide, -1, NULL, 0, NULL, NULL);
		WideCharToMultiByte(to, 0, bstrWide, -1, psz, nLength, NULL, NULL);
		SysFreeString(bstrWide);
		return string(psz);
	};

	std::string cp2utf(char* pszCode) {
		return transcode(pszCode, CP_ACP, CP_UTF8);
	}

	std::string utf2oem(char* pszCode) {
		return transcode(pszCode, CP_UTF8, CP_OEMCP);
	};

	std::string cp2oem(char* pszCode) {
		return transcode(pszCode, CP_ACP, CP_OEMCP);
	};

	unsigned int get_mask(unsigned int pos, unsigned int n) {
		return ~(~0 << n) << pos;
	};
}

#endif