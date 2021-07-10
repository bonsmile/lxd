#pragma once

#include "defines.h"
#include <vector>
#include <string_view>

namespace lxd {
	DLL_PUBLIC std::vector<std::string_view> Split(std::string_view src, std::string_view separate_character);
}