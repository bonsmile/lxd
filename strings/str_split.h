//
// Copyright 2017 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// -----------------------------------------------------------------------------
// File: str_split.h
// -----------------------------------------------------------------------------
//
// This file contains functions for splitting strings. It defines the main
// `StrSplit()` function, several delimiters for determining the boundaries on
// which to split the string, and predicates for filtering delimited results.
// `StrSplit()` adapts the returned collection to the type specified by the
// caller.
//
// Example:
//
//   // Splits the given string on commas. Returns the results in a
//   // vector of strings.
//   std::vector<std::string> v = absl::StrSplit("a,b,c", ',');
//   // Can also use ","
//   // v[0] == "a", v[1] == "b", v[2] == "c"
//
// See StrSplit() below for more information.
#ifndef ABSL_STRINGS_STR_SPLIT_H_
#define ABSL_STRINGS_STR_SPLIT_H_

#include <algorithm>
#include <cstddef>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <cassert>

#include "str_split_internal.h"
#include "ascii.h"

namespace absl {
//ABSL_NAMESPACE_BEGIN

//------------------------------------------------------------------------------
// Delimiters
//------------------------------------------------------------------------------
//
// `StrSplit()` uses delimiters to define the boundaries between elements in the
// provided input. Several `Delimiter` types are defined below. If a string
// (`const char*`, `std::string`, or `std::string_view`) is passed in place of
// an explicit `Delimiter` object, `StrSplit()` treats it the same way as if it
// were passed a `ByString` delimiter.
//
// A `Delimiter` is an object with a `Find()` function that knows how to find
// the first occurrence of itself in a given `std::string_view`.
//
// The following `Delimiter` types are available for use within `StrSplit()`:
//
//   - `ByString` (default for string arguments)
//   - `ByChar` (default for a char argument)
//   - `ByAnyChar`
//   - `ByLength`
//   - `MaxSplits`
//
// A Delimiter's `Find()` member function will be passed an input `text` that is
// to be split and a position (`pos`) to begin searching for the next delimiter
// in `text`. The returned std::string_view should refer to the next occurrence
// (after `pos`) of the represented delimiter; this returned std::string_view
// represents the next location where the input `text` should be broken.
//
// The returned std::string_view may be zero-length if the Delimiter does not
// represent a part of the string (e.g., a fixed-length delimiter). If no
// delimiter is found in the input `text`, a zero-length std::string_view
// referring to `text.end()` should be returned (e.g.,
// `text.substr(text.size())`). It is important that the returned
// std::string_view always be within the bounds of the input `text` given as an
// argument--it must not refer to a string that is physically located outside of
// the given string.
//
// The following example is a simple Delimiter object that is created with a
// single char and will look for that char in the text passed to the `Find()`
// function:
//
//   struct SimpleDelimiter {
//     const char c_;
//     explicit SimpleDelimiter(char c) : c_(c) {}
//     std::string_view Find(std::string_view text, size_t pos) {
//       auto found = text.find(c_, pos);
//       if (found == std::string_view::npos)
//         return text.substr(text.size());
//
//       return text.substr(found, 1);
//     }
//   };

// This GenericFind() template function encapsulates the finding algorithm
// shared between the ByString and ByAnyChar delimiters. The FindPolicy
// template parameter allows each delimiter to customize the actual find
// function to use and the length of the found delimiter. For example, the
// Literal delimiter will ultimately use std::string_view::find(), and the
// AnyOf delimiter will use std::string_view::find_first_of().
template <typename CharT, typename FindPolicy>
std::basic_string_view<CharT> GenericFind(std::basic_string_view<CharT> text,
                              std::basic_string_view<CharT> delimiter, size_t pos,
                              FindPolicy find_policy) {
    if (delimiter.empty() && text.length() > 0) {
        // Special case for empty string delimiters: always return a zero-length
        // std::string_view referring to the item at position 1 past pos.
        return std::basic_string_view<CharT>(text.data() + pos + 1, 0);
    }
    size_t found_pos = std::basic_string_view<CharT>::npos;
    std::basic_string_view<CharT> found(text.data() + text.size(),
                            0);  // By default, not found
    found_pos = find_policy.Find(text, delimiter, pos);
    if (found_pos != std::basic_string_view<CharT>::npos) {
        found = std::basic_string_view<CharT>(text.data() + found_pos,
                                  find_policy.Length(delimiter));
    }
    return found;
}

// Finds using std::string_view::find(), therefore the length of the found
// delimiter is delimiter.length().
template <typename CharT>
struct LiteralPolicy {
    size_t Find(std::basic_string_view<CharT> text, std::basic_string_view<CharT> delimiter, size_t pos) {
        return text.find(delimiter, pos);
    }
    size_t Length(std::basic_string_view<CharT> delimiter) { return delimiter.length(); }
};

// Finds using std::string_view::find_first_of(), therefore the length of the
// found delimiter is 1.
template <typename CharT>
struct AnyOfPolicy {
    size_t Find(std::basic_string_view<CharT> text, std::basic_string_view<CharT> delimiter, size_t pos) {
        return text.find_first_of(delimiter, pos);
    }
    size_t Length(std::basic_string_view<CharT> /* delimiter */) { return 1; }
};

// ByString
//
// A sub-string delimiter. If `StrSplit()` is passed a string in place of a
// `Delimiter` object, the string will be implicitly converted into a
// `ByString` delimiter.
//
// Example:
//
//   // Because a string literal is converted to an `absl::ByString`,
//   // the following two splits are equivalent.
//
//   std::vector<std::string> v1 = absl::StrSplit("a, b, c", ", ");
//
//   using absl::ByString;
//   std::vector<std::string> v2 = absl::StrSplit("a, b, c",
//                                                ByString(", "));
//   // v[0] == "a", v[1] == "b", v[2] == "c"
template <typename CharT>
class ByString {
public:
    explicit ByString(std::basic_string_view<CharT> sp) : delimiter_(sp) {}
    std::basic_string_view<CharT> Find(std::basic_string_view<CharT> text, size_t pos) const {
        if (delimiter_.length() == 1) {
            // Much faster to call find on a single character than on an
            // std::string_view.
            size_t found_pos = text.find(delimiter_[0], pos);
            if (found_pos == std::basic_string_view<CharT>::npos)
                return std::basic_string_view<CharT>(text.data() + text.size(), 0);
            return text.substr(found_pos, 1);
        }
        return GenericFind<CharT>(text, delimiter_, pos, LiteralPolicy<CharT>());
    }

private:
    const std::basic_string<CharT> delimiter_;
};

// ByChar
//
// A single character delimiter. `ByChar` is functionally equivalent to a
// 1-char string within a `ByString` delimiter, but slightly more efficient.
//
// Example:
//
//   // Because a char literal is converted to a absl::ByChar,
//   // the following two splits are equivalent.
//   std::vector<std::string> v1 = absl::StrSplit("a,b,c", ',');
//   using absl::ByChar;
//   std::vector<std::string> v2 = absl::StrSplit("a,b,c", ByChar(','));
//   // v[0] == "a", v[1] == "b", v[2] == "c"
//
// `ByChar` is also the default delimiter if a single character is given
// as the delimiter to `StrSplit()`. For example, the following calls are
// equivalent:
//
//   std::vector<std::string> v = absl::StrSplit("a-b", '-');
//
//   using absl::ByChar;
//   std::vector<std::string> v = absl::StrSplit("a-b", ByChar('-'));
//
template <typename CharT>
class ByChar {
 public:
  explicit ByChar(char c) : c_(c) {}

  std::basic_string_view<CharT> Find(std::basic_string_view<CharT> text, size_t pos) const {
      size_t found_pos = text.find(c_, pos);
      if (found_pos == std::string_view::npos)
          return std::string_view(text.data() + text.size(), 0);
      return text.substr(found_pos, 1);
  }

 private:
     CharT c_;
};

// ByAnyChar
//
// A delimiter that will match any of the given byte-sized characters within
// its provided string.
//
// Note: this delimiter works with single-byte string data, but does not work
// with variable-width encodings, such as UTF-8.
//
// Example:
//
//   using absl::ByAnyChar;
//   std::vector<std::string> v = absl::StrSplit("a,b=c", ByAnyChar(",="));
//   // v[0] == "a", v[1] == "b", v[2] == "c"
//
// If `ByAnyChar` is given the empty string, it behaves exactly like
// `ByString` and matches each individual character in the input string.
//
class ByAnyChar {
 public:
     explicit ByAnyChar(std::string_view sp) : delimiters_(sp) {}
  std::string_view Find(std::string_view text, size_t pos) const {
      return GenericFind<char>(text, delimiters_, pos, AnyOfPolicy<char>());
  }

 private:
  const std::string delimiters_;
};

// ByLength
//
// A delimiter for splitting into equal-length strings. The length argument to
// the constructor must be greater than 0.
//
// Note: this delimiter works with single-byte string data, but does not work
// with variable-width encodings, such as UTF-8.
//
// Example:
//
//   using absl::ByLength;
//   std::vector<std::string> v = absl::StrSplit("123456789", ByLength(3));

//   // v[0] == "123", v[1] == "456", v[2] == "789"
//
// Note that the string does not have to be a multiple of the fixed split
// length. In such a case, the last substring will be shorter.
//
//   using absl::ByLength;
//   std::vector<std::string> v = absl::StrSplit("12345", ByLength(2));
//
//   // v[0] == "12", v[1] == "34", v[2] == "5"
class ByLength {
 public:
     //
     // ByLength
     //
     explicit ByLength(ptrdiff_t length) : length_(length) {
         assert(length > 0);
     }
  std::string_view Find(std::string_view text, size_t pos) const {
      pos = std::min(pos, text.size());  // truncate `pos`
      std::string_view substr = text.substr(pos);
      // If the string is shorter than the chunk size we say we
      // "can't find the delimiter" so this will be the last chunk.
      if (substr.length() <= static_cast<size_t>(length_))
          return std::string_view(text.data() + text.size(), 0);

      return std::string_view(substr.data() + length_, 0);
  }

 private:
  const ptrdiff_t length_;
};

namespace strings_internal {

// A traits-like metafunction for selecting the default Delimiter object type
// for a particular Delimiter type. The base case simply exposes type Delimiter
// itself as the delimiter's Type. However, there are specializations for
// string-like objects that map them to the ByString delimiter object.
// This allows functions like absl::StrSplit() and absl::MaxSplits() to accept
// string-like objects (e.g., ',') as delimiter arguments but they will be
// treated as if a ByString delimiter was given.
template <typename Delimiter>
struct SelectDelimiter {
  using type = Delimiter;
};

template <>
struct SelectDelimiter<char> {
  using type = ByChar<char>;
};
template <>
struct SelectDelimiter<wchar_t> {
    using type = ByChar<wchar_t>;
};
template <>
struct SelectDelimiter<char*> {
  using type = ByString<char>;
};
template <>
struct SelectDelimiter<wchar_t*> {
    using type = ByString<wchar_t>;
};
template <>
struct SelectDelimiter<const char*> {
  using type = ByString<char>;
};
template <>
struct SelectDelimiter<const wchar_t*> {
    using type = ByString<wchar_t>;
};
template <>
struct SelectDelimiter<std::string_view> {
  using type = ByString<char>;
};
template <>
struct SelectDelimiter<std::wstring_view> {
    using type = ByString<wchar_t>;
};
template <>
struct SelectDelimiter<std::string> {
  using type = ByString<char>;
};
template <>
struct SelectDelimiter<std::wstring> {
    using type = ByString<wchar_t>;
};

// Wraps another delimiter and sets a max number of matches for that delimiter.
template <typename Delimiter>
class MaxSplitsImpl {
 public:
  MaxSplitsImpl(Delimiter delimiter, int limit)
      : delimiter_(delimiter), limit_(limit), count_(0) {}
  std::string_view Find(std::string_view text, size_t pos) {
    if (count_++ == limit_) {
      return std::string_view(text.data() + text.size(),
                               0);  // No more matches.
    }
    return delimiter_.Find(text, pos);
  }

 private:
  Delimiter delimiter_;
  const int limit_;
  int count_;
};

}  // namespace strings_internal

// MaxSplits()
//
// A delimiter that limits the number of matches which can occur to the passed
// `limit`. The last element in the returned collection will contain all
// remaining unsplit pieces, which may contain instances of the delimiter.
// The collection will contain at most `limit` + 1 elements.
// Example:
//
//   using absl::MaxSplits;
//   std::vector<std::string> v = absl::StrSplit("a,b,c", MaxSplits(',', 1));
//
//   // v[0] == "a", v[1] == "b,c"
template <typename Delimiter>
inline strings_internal::MaxSplitsImpl<
    typename strings_internal::SelectDelimiter<Delimiter>::type>
MaxSplits(Delimiter delimiter, int limit) {
  typedef
      typename strings_internal::SelectDelimiter<Delimiter>::type DelimiterType;
  return strings_internal::MaxSplitsImpl<DelimiterType>(
      DelimiterType(delimiter), limit);
}

//------------------------------------------------------------------------------
// Predicates
//------------------------------------------------------------------------------
//
// Predicates filter the results of a `StrSplit()` by determining whether or not
// a resultant element is included in the result set. A predicate may be passed
// as an optional third argument to the `StrSplit()` function.
//
// Predicates are unary functions (or functors) that take a single
// `std::string_view` argument and return a bool indicating whether the
// argument should be included (`true`) or excluded (`false`).
//
// Predicates are useful when filtering out empty substrings. By default, empty
// substrings may be returned by `StrSplit()`, which is similar to the way split
// functions work in other programming languages.

// AllowEmpty()
//
// Always returns `true`, indicating that all strings--including empty
// strings--should be included in the split output. This predicate is not
// strictly needed because this is the default behavior of `StrSplit()`;
// however, it might be useful at some call sites to make the intent explicit.
//
// Example:
//
//  std::vector<std::string> v = absl::StrSplit(" a , ,,b,", ',', AllowEmpty());
//
//  // v[0] == " a ", v[1] == " ", v[2] == "", v[3] = "b", v[4] == ""
template <typename CharT>
struct AllowEmpty {
  bool operator()(std::basic_string_view<CharT>) const { return true; }
};

// SkipEmpty()
//
// Returns `false` if the given `std::string_view` is empty, indicating that
// `StrSplit()` should omit the empty string.
//
// Example:
//
//   std::vector<std::string> v = absl::StrSplit(",a,,b,", ',', SkipEmpty());
//
//   // v[0] == "a", v[1] == "b"
//
// Note: `SkipEmpty()` does not consider a string containing only whitespace
// to be empty. To skip such whitespace as well, use the `SkipWhitespace()`
// predicate.

struct SkipEmpty {
  template<typename CharT>
  bool operator()(std::basic_string_view<CharT> sp) const { return !sp.empty(); }
};

// SkipWhitespace()
//
// Returns `false` if the given `std::string_view` is empty *or* contains only
// whitespace, indicating that `StrSplit()` should omit the string.
//
// Example:
//
//   std::vector<std::string> v = absl::StrSplit(" a , ,,b,",
//                                               ',', SkipWhitespace());
//   // v[0] == " a ", v[1] == "b"
//
//   // SkipEmpty() would return whitespace elements
//   std::vector<std::string> v = absl::StrSplit(" a , ,,b,", ',', SkipEmpty());
//   // v[0] == " a ", v[1] == " ", v[2] == "b"

struct SkipWhitespace {
  template<typename CharT>
  bool operator()(std::basic_string_view<CharT> sp) const {
    sp = absl::StripAsciiWhitespace(sp);
    return !sp.empty();
  }
};

template <typename T>
using EnableSplitIfString =
    typename std::enable_if<std::is_same<T, std::string>::value ||
                            std::is_same<T, const std::string>::value || 
                            std::is_same<T, std::wstring>::value ||
                            std::is_same<T, const std::wstring>::value,
                            int>::type;

//------------------------------------------------------------------------------
//                                  StrSplit()
//------------------------------------------------------------------------------

// StrSplit()
//
// Splits a given string based on the provided `Delimiter` object, returning the
// elements within the type specified by the caller. Optionally, you may pass a
// `Predicate` to `StrSplit()` indicating whether to include or exclude the
// resulting element within the final result set. (See the overviews for
// Delimiters and Predicates above.)
//
// Example:
//
//   std::vector<std::string> v = absl::StrSplit("a,b,c,d", ',');
//   // v[0] == "a", v[1] == "b", v[2] == "c", v[3] == "d"
//
// You can also provide an explicit `Delimiter` object:
//
// Example:
//
//   using absl::ByAnyChar;
//   std::vector<std::string> v = absl::StrSplit("a,b=c", ByAnyChar(",="));
//   // v[0] == "a", v[1] == "b", v[2] == "c"
//
// See above for more information on delimiters.
//
// By default, empty strings are included in the result set. You can optionally
// include a third `Predicate` argument to apply a test for whether the
// resultant element should be included in the result set:
//
// Example:
//
//   std::vector<std::string> v = absl::StrSplit(" a , ,,b,",
//                                               ',', SkipWhitespace());
//   // v[0] == " a ", v[1] == "b"
//
// See above for more information on predicates.
//
//------------------------------------------------------------------------------
// StrSplit() Return Types
//------------------------------------------------------------------------------
//
// The `StrSplit()` function adapts the returned collection to the collection
// specified by the caller (e.g. `std::vector` above). The returned collections
// may contain `std::string`, `std::string_view` (in which case the original
// string being split must ensure that it outlives the collection), or any
// object that can be explicitly created from an `std::string_view`. This
// behavior works for:
//
// 1) All standard STL containers including `std::vector`, `std::list`,
//    `std::deque`, `std::set`,`std::multiset`, 'std::map`, and `std::multimap`
// 2) `std::pair` (which is not actually a container). See below.
//
// Example:
//
//   // The results are returned as `std::string_view` objects. Note that we
//   // have to ensure that the input string outlives any results.
//   std::vector<std::string_view> v = absl::StrSplit("a,b,c", ',');
//
//   // Stores results in a std::set<std::string>, which also performs
//   // de-duplication and orders the elements in ascending order.
//   std::set<std::string> a = absl::StrSplit("b,a,c,a,b", ',');
//   // v[0] == "a", v[1] == "b", v[2] = "c"
//
//   // `StrSplit()` can be used within a range-based for loop, in which case
//   // each element will be of type `std::string_view`.
//   std::vector<std::string> v;
//   for (const auto sv : absl::StrSplit("a,b,c", ',')) {
//     if (sv != "b") v.emplace_back(sv);
//   }
//   // v[0] == "a", v[1] == "c"
//
//   // Stores results in a map. The map implementation assumes that the input
//   // is provided as a series of key/value pairs. For example, the 0th element
//   // resulting from the split will be stored as a key to the 1st element. If
//   // an odd number of elements are resolved, the last element is paired with
//   // a default-constructed value (e.g., empty string).
//   std::map<std::string, std::string> m = absl::StrSplit("a,b,c", ',');
//   // m["a"] == "b", m["c"] == ""     // last component value equals ""
//
// Splitting to `std::pair` is an interesting case because it can hold only two
// elements and is not a collection type. When splitting to a `std::pair` the
// first two split strings become the `std::pair` `.first` and `.second`
// members, respectively. The remaining split substrings are discarded. If there
// are less than two split substrings, the empty string is used for the
// corresponding
// `std::pair` member.
//
// Example:
//
//   // Stores first two split strings as the members in a std::pair.
//   std::pair<std::string, std::string> p = absl::StrSplit("a,b,c", ',');
//   // p.first == "a", p.second == "b"       // "c" is omitted.
//
// The `StrSplit()` function can be used multiple times to perform more
// complicated splitting logic, such as intelligently parsing key-value pairs.
//
// Example:
//
//   // The input string "a=b=c,d=e,f=,g" becomes
//   // { "a" => "b=c", "d" => "e", "f" => "", "g" => "" }
//   std::map<std::string, std::string> m;
//   for (std::string_view sp : absl::StrSplit("a=b=c,d=e,f=,g", ',')) {
//     m.insert(absl::StrSplit(sp, absl::MaxSplits('=', 1)));
//   }
//   EXPECT_EQ("b=c", m.find("a")->second);
//   EXPECT_EQ("e", m.find("d")->second);
//   EXPECT_EQ("", m.find("f")->second);
//   EXPECT_EQ("", m.find("g")->second);
//
// WARNING: Due to a legacy bug that is maintained for backward compatibility,
// splitting the following empty string_views produces different results:
//
//   absl::StrSplit(std::string_view(""), '-');  // {""}
//   absl::StrSplit(std::string_view(), '-');    // {}, but should be {""}
//
// Try not to depend on this distinction because the bug may one day be fixed.

template <typename Delimiter>
strings_internal::Splitter<
    typename strings_internal::SelectDelimiter<Delimiter>::type, AllowEmpty<char>,
    std::string_view, char>
DLL_PUBLIC StrSplit(strings_internal::ConvertibleToStringView<char> text, Delimiter d) {
  using DelimiterType =
      typename strings_internal::SelectDelimiter<Delimiter>::type;
  return strings_internal::Splitter<DelimiterType, AllowEmpty<char>,
                                    std::string_view, char>(
	  text.value(), DelimiterType(d), AllowEmpty<char>());
}

template <typename Delimiter>
strings_internal::Splitter<
    typename strings_internal::SelectDelimiter<Delimiter>::type, AllowEmpty<wchar_t>,
    std::wstring_view, wchar_t>
DLL_PUBLIC StrSplit(strings_internal::ConvertibleToStringView<wchar_t> text, Delimiter d) {
    using DelimiterType =
        typename strings_internal::SelectDelimiter<Delimiter>::type;
    return strings_internal::Splitter<DelimiterType, AllowEmpty<wchar_t>,
        std::wstring_view, wchar_t>(
text.value(), DelimiterType(d), AllowEmpty<wchar_t>());
}

template <typename Delimiter, typename StringType,
          EnableSplitIfString<StringType>>
strings_internal::Splitter<
    typename strings_internal::SelectDelimiter<Delimiter>::type, AllowEmpty<char>,
    std::string, char>
DLL_PUBLIC StrSplit(StringType&& text, Delimiter d) {
  using DelimiterType =
      typename strings_internal::SelectDelimiter<Delimiter>::type;
  return strings_internal::Splitter<DelimiterType, AllowEmpty<char>, std::string, char>(
      std::move(text), DelimiterType(d), AllowEmpty<char>());
}

template <typename Delimiter, typename StringType,
    EnableSplitIfString<StringType>>
    strings_internal::Splitter<
    typename strings_internal::SelectDelimiter<Delimiter>::type, AllowEmpty<wchar_t>,
    std::wstring, wchar_t>
   DLL_PUBLIC StrSplit(StringType&& text, Delimiter d) {
    using DelimiterType =
        typename strings_internal::SelectDelimiter<Delimiter>::type;
    return strings_internal::Splitter<DelimiterType, AllowEmpty<wchar_t>, std::wstring, wchar_t>(
        std::move(text), DelimiterType(d), AllowEmpty<wchar_t>());
}

template <typename Delimiter, typename Predicate>
strings_internal::Splitter<
    typename strings_internal::SelectDelimiter<Delimiter>::type, Predicate,
    std::string_view, char>
DLL_PUBLIC StrSplit(strings_internal::ConvertibleToStringView<char> text, Delimiter d,
         Predicate p) {
  using DelimiterType =
      typename strings_internal::SelectDelimiter<Delimiter>::type;
  return strings_internal::Splitter<DelimiterType, Predicate,
                                    std::string_view, char>(
      text.value(), DelimiterType(d), std::move(p));
}

template <typename Delimiter, typename Predicate>
strings_internal::Splitter<
    typename strings_internal::SelectDelimiter<Delimiter>::type, Predicate,
    std::wstring_view, wchar_t>
   DLL_PUBLIC StrSplit(strings_internal::ConvertibleToStringView<wchar_t> text, Delimiter d,
             Predicate p) {
    using DelimiterType =
        typename strings_internal::SelectDelimiter<Delimiter>::type;
    return strings_internal::Splitter<DelimiterType, Predicate,
        std::wstring_view, wchar_t>(
text.value(), DelimiterType(d), std::move(p));
}

template <typename Delimiter, typename Predicate, typename StringType,
          EnableSplitIfString<StringType>>
strings_internal::Splitter<
    typename strings_internal::SelectDelimiter<Delimiter>::type, Predicate,
    std::string, char>
DLL_PUBLIC StrSplit(StringType&& text, Delimiter d, Predicate p) {
  using DelimiterType =
      typename strings_internal::SelectDelimiter<Delimiter>::type;
  return strings_internal::Splitter<DelimiterType, Predicate, std::string, char>(
      std::move(text), DelimiterType(d), std::move(p));
}

template <typename Delimiter, typename Predicate, typename StringType,
    EnableSplitIfString<StringType>>
    strings_internal::Splitter<
    typename strings_internal::SelectDelimiter<Delimiter>::type, Predicate,
    std::wstring, wchar_t>
DLL_PUBLIC StrSplit(StringType&& text, Delimiter d, Predicate p) {
    using DelimiterType =
        typename strings_internal::SelectDelimiter<Delimiter>::type;
    return strings_internal::Splitter<DelimiterType, Predicate, std::wstring, wchar_t>(
        std::move(text), DelimiterType(d), std::move(p));
}

//ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_STRINGS_STR_SPLIT_H_
