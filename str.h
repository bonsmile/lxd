#pragma once

#include "defines.h"
#include <vector>
#include <string_view>

#ifndef _T
#ifdef UNICODE
#define _T(x)      L ## x
#else
#define _T(x)      x
#endif
#endif

namespace lxd {

namespace impl {
	template<typename CHAR_T>
	inline std::basic_string<CHAR_T> _Concat(
	    const std::basic_string_view<CHAR_T>& s1,
	    const std::basic_string_view<CHAR_T>& s2) noexcept {
		std::basic_string<CHAR_T> result;
		result.reserve(s1.size() + s2.size());
		result.append(s1).append(s2);
		return result;
	}

	template<typename CHAR_T>
	inline std::basic_string<CHAR_T> _Concat(
	    const std::basic_string_view<CHAR_T>& s1,
	    const std::basic_string_view<CHAR_T>& s2,
	    const std::basic_string_view<CHAR_T>& s3) noexcept {
		std::basic_string<CHAR_T> result;
		result.reserve(s1.size() + s2.size() + s3.size());
		result.append(s1).append(s2).append(s3);
		return result;
	}

	template<typename CHAR_T>
	inline std::basic_string<CHAR_T> _Concat(
	    const std::basic_string_view<CHAR_T>& s1,
	    const std::basic_string_view<CHAR_T>& s2,
	    const std::basic_string_view<CHAR_T>& s3,
	    const std::basic_string_view<CHAR_T>& s4) noexcept {
		std::basic_string<CHAR_T> result;
		result.reserve(s1.size() + s2.size() + s3.size() + s4.size());
		result.append(s1).append(s2).append(s3).append(s4);
		return result;
	}

	template<typename CHAR_T>
	inline std::basic_string<CHAR_T> _Concat(
	    const std::basic_string_view<CHAR_T>& s1,
	    const std::basic_string_view<CHAR_T>& s2,
	    const std::basic_string_view<CHAR_T>& s3,
	    const std::basic_string_view<CHAR_T>& s4,
	    const std::basic_string_view<CHAR_T>& s5) noexcept {
		std::basic_string<CHAR_T> result;
		result.reserve(s1.size() + s2.size() + s3.size() + s4.size() + s5.size());
		result.append(s1).append(s2).append(s3).append(s4).append(s5);
		return result;
	}

	template<typename CHAR_T>
	inline std::basic_string<CHAR_T> _ConcatHelper(std::initializer_list<std::basic_string_view<CHAR_T>> args) noexcept {
		std::basic_string<CHAR_T> result;

		size_t size = 0;
		for (const std::basic_string_view<CHAR_T>& s : args) {
			size += s.size();
		}

		result.reserve(size);

		for (const std::basic_string_view<CHAR_T>& s : args) {
			result.append(s);
		}

		return result;
	}

	template<typename CHAR_T, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename... AV>
	inline std::basic_string<CHAR_T> _Concat(
	    T1&& s1,
	    T2&& s2,
	    T3&& s3,
	    T4&& s4,
	    T5&& s5,
	    T6&& s6,
	    AV&&... args) noexcept {
		return _ConcatHelper<CHAR_T>({std::forward<T1>(s1),
		                              std::forward<T2>(s2),
		                              std::forward<T3>(s3),
		                              std::forward<T4>(s4),
		                              std::forward<T5>(s5),
		                              std::forward<T6>(s6),
		                              std::forward<AV>(args)...});
	}
} // namespace impl

DLL_PUBLIC void Upper(std::string& str);
DLL_PUBLIC void Upper(std::wstring& str);
DLL_PUBLIC void Lower(std::string& str);
DLL_PUBLIC void Lower(std::wstring& str);
DLL_PUBLIC std::string Lower(std::string_view str);
DLL_PUBLIC std::vector<std::string_view> Split(std::string_view src, std::string_view separate_character);
#ifdef _WIN32 // 实现用到了 from_chars, clang 暂不支持
DLL_PUBLIC std::vector<float> ExtractNumbersFromString(std::string_view str);
#endif

template<typename T1, typename T2, typename... AV,
         typename CHAR_T = std::conditional_t<std::is_constructible_v<std::basic_string_view<char>, T1>, char, wchar_t>>
inline std::basic_string<CHAR_T> Concat(T1&& s1, T2&& s2, AV&&... args) noexcept {
	return impl::_Concat<CHAR_T>(std::forward<T1>(s1), std::forward<T2>(s2), std::forward<AV>(args)...);
}

} // namespace lxd
