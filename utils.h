#pragma once

#include "defines.h"
#include <string>
#include <variant>

namespace lxd {
	DLL_PUBLIC std::wstring GetPathOfExe();
	DLL_PUBLIC std::wstring GetDirOfExe();
	DLL_PUBLIC std::wstring GetExeName();
	DLL_PUBLIC std::wstring GetPathOfAppData();
	DLL_PUBLIC void SetEnv(std::wstring_view name, int value);
	DLL_PUBLIC void SetEnv(std::wstring_view name, std::wstring_view value);
	DLL_PUBLIC std::variant<int, std::wstring> GetEnv(std::wstring_view name);
}