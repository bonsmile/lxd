#pragma once

#include "defines.h"
#if defined(_WIN32)
#include <fmt/ostream.h>
#include <fmt/xchar.h>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <debugapi.h>
#else
#include <fmt/core.h>
#endif

//#endif
namespace lxd {
	/// <summary>
	/// https://stackoverflow.com/a/68675384/853569
	/// fmtlib 要求第一个参数必须是编译期确定的，有两种解决方法：
	/// 
	/// fmt::format(fmt::runtime(fmt), std::forward<Args>(args)...)
	/// 
	/// std::vformat(fmt, fmt::make_format_args(std::forward<Args>(args)...))
	/// 
	/// 这里选择第二种，因为编译期的格式合法性检查很重要
	/// </summary>
	template <typename... Args>
	DLL_PUBLIC void print(fmt::string_view fmt, Args&&... args) {
#ifdef _WIN32
	    std::string output = fmt::vformat(fmt, fmt::make_format_args(args...));
		OutputDebugStringA(output.c_str());
#else
		fmt::print(fmt::runtime(fmt), std::forward<Args>(args)...);
#endif
	}
#ifdef _WIN32
	template <typename... Args>
	DLL_PUBLIC void print(fmt::wstring_view fmt, Args&&... args) {
		std::wstring output = fmt::vformat(fmt, fmt::make_wformat_args(args...));
		OutputDebugStringW(output.c_str());
	}
#endif
}
