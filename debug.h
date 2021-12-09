#pragma once

//#include <fmt/xchar.h>
#include <fmt/ostream.h>
#include <Windows.h>
#include <debugapi.h>
//#if defined(_DEBUG)
#include "AsyncFile.h"
#include "encoding.h"
#include <fmt/xchar.h>
#include <fmt/format.h>

//#endif
namespace lxd {
	template <typename... Args>
	DLL_PUBLIC void print(const fmt::format_string<Args...>& format_str, Args&&... args) {
		std::string output = fmt::format(format_str, std::forward<Args>(args)...);
		OutputDebugStringA(output.c_str());
		//DWORD offsetHigh;
		//AsyncFile::GetInstance()->Write(output.c_str(), output.size(), AsyncFile::GetInstance()->GetFileLength(&offsetHigh), 0);
	}

	template <typename... Args>
	DLL_PUBLIC void print(const wchar_t* format_str, Args&&... args) {
		std::wstring output = fmt::format(format_str, std::forward<Args>(args)...);
		OutputDebugStringW(output.c_str());
		//auto utf8 = utf8_encode(output);
		//DWORD offsetHigh;
		//AsyncFile::GetInstance()->Write(utf8.c_str(), utf8.size(), AsyncFile::GetInstance()->GetFileLength(&offsetHigh), 0);
	}

	#define verify(x) {if(!(x)) _asm{int 0x03}}
}