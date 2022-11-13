#pragma once

#include "defines.h"
#include <fmt/ostream.h>
#include <fmt/xchar.h>
#if defined(_WIN32)
#include <Windows.h>
#include <debugapi.h>
#else
#endif

//#endif
namespace lxd {
	template <typename... Args>
	DLL_PUBLIC void print(fmt::string_view fmt, Args&&... args) {
		std::string output = fmt::vformat(fmt, fmt::make_format_args(std::forward<Args>(args)...));
		OutputDebugStringA(output.c_str());
	}

	template <typename... Args>
	DLL_PUBLIC void print(fmt::wstring_view fmt, Args&&... args) {
		std::wstring output = fmt::vformat(fmt, fmt::make_wformat_args(std::forward<Args>(args)...));
		OutputDebugStringW(output.c_str());
	}
}
