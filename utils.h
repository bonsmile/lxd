#pragma once

#include "defines.h"
#ifdef _WIN32
#include <variant>
#endif // _WIN32
#include <functional>

namespace lxd {
	DLL_PUBLIC String GetDirOfExe();
	enum MsgType {
		Info,
		Warn,
		Error
	};
	DLL_PUBLIC void MsgBox(MsgType msgType, const Char* title, const Char* message);
#ifdef _WIN32
	DLL_PUBLIC std::wstring GetPathOfExe();
	DLL_PUBLIC std::wstring GetExeName();
	DLL_PUBLIC std::wstring GetPathOfAppData();
	DLL_PUBLIC void SetEnv(std::wstring_view name, int value);
	DLL_PUBLIC void SetEnv(std::wstring_view name, std::wstring_view value);
	DLL_PUBLIC std::variant<int, std::wstring> GetEnv(std::wstring_view name);
#endif
    DLL_PUBLIC void RunParallel(uint32_t times, std::function<void(uint32_t)> func) noexcept;
}
