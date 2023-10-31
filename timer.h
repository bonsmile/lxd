#pragma once

#include "defines.h"

namespace lxd {
	DLL_PUBLIC void sleep(int milliseconds);
	DLL_PUBLIC uint64_t nanosecond();
	DLL_PUBLIC float millisecond();
	DLL_PUBLIC double second();
	enum DateFormat {
		Default,
		Human
	};
	DLL_PUBLIC const std::string date(const DateFormat fmt = Default);
#ifdef _WIN32
	/// <summary>
	/// 将时间字符串转换成 struct tm, 是 strftime 的逆操作
	/// https://man7.org/linux/man-pages/man3/strptime.3.html
	/// </summary>
	/// <param name="s"></param>
	/// <param name="f"></param>
	/// <param name="tm"></param>
	/// <returns></returns>
	DLL_PUBLIC char* strptime(const char* s, const char* f, struct tm* tm);
#endif

	/// <summary>
	/// 从 1970.1.1/00:00:00 开始的毫秒数
	/// </summary>
	/// <param name="year"></param>
	/// <param name="month"></param>
	/// <param name="day"></param>
	/// <param name="hour"></param>
	/// <param name="minute"></param>
	/// <param name="second"></param>
	/// <returns>毫秒数</returns>
	DLL_PUBLIC int64_t MsSince1970(int year,
		int month,
		int day,
		int hour,
		int minute,
		int second);
}