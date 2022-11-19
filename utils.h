#pragma once

#include "defines.h"
#ifdef _WIN32
#include <variant>
#endif // _WIN32

namespace lxd {
	DLL_PUBLIC String GetDirOfExe();
#ifdef _WIN32
	DLL_PUBLIC std::wstring GetPathOfExe();
	DLL_PUBLIC std::wstring GetExeName();
	DLL_PUBLIC std::wstring GetPathOfAppData();
	DLL_PUBLIC void SetEnv(std::wstring_view name, int value);
	DLL_PUBLIC void SetEnv(std::wstring_view name, std::wstring_view value);
	DLL_PUBLIC std::variant<int, std::wstring> GetEnv(std::wstring_view name);
#endif
}