#pragma once

#include "defines.h"
#include <vector>
#include <string_view>

#ifndef _T
#ifdef UNICODE
#define _T(x)      L ## x
#else
#define _T(x)      x
#endif
#endif

namespace lxd {
	DLL_PUBLIC void Upper(std::string& str);
	DLL_PUBLIC void Upper(std::wstring& str);
	DLL_PUBLIC void Lower(std::string& str);
	DLL_PUBLIC void Lower(std::wstring& str);
	DLL_PUBLIC std::string Lower(std::string_view str);
	DLL_PUBLIC std::vector<std::string_view> Split(std::string_view src, std::string_view separate_character);
	DLL_PUBLIC std::vector<float> ExtractNumbersFromString(std::string_view str);
}
