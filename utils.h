#pragma once

#include "defines.h"
#include <string>

namespace lxd {
	DLL_PUBLIC std::wstring GetPathOfExe();
	DLL_PUBLIC std::wstring GetDirOfExe();
	DLL_PUBLIC std::wstring GetExeName();
}