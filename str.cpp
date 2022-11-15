#include "str.h"
#include <string.h>
#include <assert.h>
#include <algorithm>
#include <cctype>
#include <regex>
#include <charconv>
#include <cwctype>

namespace lxd {
	void Upper(std::string& str) {
		std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) { return (char)std::toupper(c); });
	}
	void Upper(std::wstring& str) {
		std::transform(str.begin(), str.end(), str.begin(), [](wchar_t c) { return (wchar_t)std::towupper(c); });
	}
	void Lower(std::string& str) {
		std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) { return (char)std::tolower(c); });
	}
	void Lower(std::wstring& str) {
		std::transform(str.begin(), str.end(), str.begin(), [](wchar_t c) { return (wchar_t)std::towlower(c); });
	}

	std::string Lower(std::string_view str) {
		std::string result(str);
		std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) { return (char)std::tolower(c); });
		return result;
	}

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

	std::vector<float> ExtractNumbersFromString(std::string_view sv) {
		std::regex reg(R"((-?(([1-9]\d*\.\d*)|(0\.\d*[1-9]\d*)))|(-?[1-9]\d*))");
		const std::cregex_iterator end;
		std::vector<float> result;
		for(std::cregex_iterator iter(sv.data(), sv.data() + sv.size(), reg); iter != end; ++iter) {
			float value;
			std::string match = iter->str();
			if(auto [p, ec] = std::from_chars(match.data(), match.data() + match.size(), value); ec == std::errc{}) {
				result.push_back(value);
			}
		}
		return result;
	}
}