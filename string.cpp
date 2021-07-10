#include "string.h"
#include <algorithm>

namespace lxd {
	std::vector<std::string_view> Split(std::string_view str, std::string_view delims) {
		std::vector<std::string_view> output;
		//output.reserve(str.size() / 2);

		for(auto first = str.data(), second = str.data(), last = first + str.size(); second != last && first != last; first = second + 1) {
			second = std::find_first_of(first, last, std::cbegin(delims), std::cend(delims));

			if(first != second)
				output.emplace_back(first, second - first);
		}

		return output;
	}
}