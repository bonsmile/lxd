#pragma once

#include "defines.h"
#if defined(_WIN32)
#include <fmt/ostream.h>
#include <fmt/xchar.h>
#include <Windows.h>
#include <debugapi.h>
#else
#include <fmt/core.h>
#endif

//#endif
namespace lxd {
	template <typename... Args>
	DLL_PUBLIC void print(fmt::string_view fmt, Args&&... args) {
#ifdef _WIN32
		std::string output = fmt::vformat(fmt, fmt::make_format_args(std::forward<Args>(args)...));
		OutputDebugStringA(output.c_str());
#else
		fmt::print(fmt, fmt::make_format_args(std::forward<Args>(args)...));
#endif
	}
#ifdef _WIN32
	template <typename... Args>
	DLL_PUBLIC void print(fmt::wstring_view fmt, Args&&... args) {
		std::wstring output = fmt::vformat(fmt, fmt::make_wformat_args(std::forward<Args>(args)...));
		OutputDebugStringW(output.c_str());
	}
#endif
}
