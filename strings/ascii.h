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
// File: ascii.h
// -----------------------------------------------------------------------------
//
// This package contains functions operating on characters and strings
// restricted to standard ASCII. These include character classification
// functions analogous to those found in the ANSI C Standard Library <ctype.h>
// header file.
//
// C++ implementations provide <ctype.h> functionality based on their
// C environment locale. In general, reliance on such a locale is not ideal, as
// the locale standard is problematic (and may not return invariant information
// for the same character set, for example). These `ascii_*()` functions are
// hard-wired for standard ASCII, much faster, and guaranteed to behave
// consistently.  They will never be overloaded, nor will their function
// signature change.
//
// `ascii_isalnum()`, `ascii_isalpha()`, `ascii_isascii()`, `ascii_isblank()`,
// `ascii_iscntrl()`, `ascii_isdigit()`, `ascii_isgraph()`, `ascii_islower()`,
// `ascii_isprint()`, `ascii_ispunct()`, `ascii_isspace()`, `ascii_isupper()`,
// `ascii_isxdigit()`
//   Analogous to the <ctype.h> functions with similar names, these
//   functions take an unsigned char and return a bool, based on whether the
//   character matches the condition specified.
//
//   If the input character has a numerical value greater than 127, these
//   functions return `false`.
//
// `ascii_tolower()`, `ascii_toupper()`
//   Analogous to the <ctype.h> functions with similar names, these functions
//   take an unsigned char and return a char.
//
//   If the input character is not an ASCII {lower,upper}-case letter (including
//   numerical values greater than 127) then the functions return the same value
//   as the input character.

#ifndef ABSL_STRINGS_ASCII_H_
#define ABSL_STRINGS_ASCII_H_

#include <algorithm>
#include <string>

#include "../defines.h"
//#include "absl/base/attributes.h"
//#include "absl/base/config.h"
//#include "absl/strings/string_view.h"

namespace absl {
//ABSL_NAMESPACE_BEGIN
namespace ascii_internal {

    // Array of bitfields holding character information. Each bit value corresponds
    // to a particular character feature. For readability, and because the value
    // of these bits is tightly coupled to this implementation, the individual bits
    // are not named. Note that bitfields for all characters above ASCII 127 are
    // zero-initialized.
        const unsigned char kPropertyBits[256] = {
        0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,  // 0x00
        0x40, 0x68, 0x48, 0x48, 0x48, 0x48, 0x40, 0x40,
        0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,  // 0x10
        0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
        0x28, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,  // 0x20
        0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
        0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84,  // 0x30
        0x84, 0x84, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
        0x10, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x05,  // 0x40
        0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
        0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,  // 0x50
        0x05, 0x05, 0x05, 0x10, 0x10, 0x10, 0x10, 0x10,
        0x10, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x05,  // 0x60
        0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
        0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,  // 0x70
        0x05, 0x05, 0x05, 0x10, 0x10, 0x10, 0x10, 0x40,
    };

    // Array of characters for the ascii_tolower() function. For values 'A'
    // through 'Z', return the lower-case character; otherwise, return the
    // identity of the passed character.
        const char kToLower[256] = {
      '\x00', '\x01', '\x02', '\x03', '\x04', '\x05', '\x06', '\x07',
      '\x08', '\x09', '\x0a', '\x0b', '\x0c', '\x0d', '\x0e', '\x0f',
      '\x10', '\x11', '\x12', '\x13', '\x14', '\x15', '\x16', '\x17',
      '\x18', '\x19', '\x1a', '\x1b', '\x1c', '\x1d', '\x1e', '\x1f',
      '\x20', '\x21', '\x22', '\x23', '\x24', '\x25', '\x26', '\x27',
      '\x28', '\x29', '\x2a', '\x2b', '\x2c', '\x2d', '\x2e', '\x2f',
      '\x30', '\x31', '\x32', '\x33', '\x34', '\x35', '\x36', '\x37',
      '\x38', '\x39', '\x3a', '\x3b', '\x3c', '\x3d', '\x3e', '\x3f',
      '\x40',    'a',    'b',    'c',    'd',    'e',    'f',    'g',
         'h',    'i',    'j',    'k',    'l',    'm',    'n',    'o',
         'p',    'q',    'r',    's',    't',    'u',    'v',    'w',
         'x',    'y',    'z', '\x5b', '\x5c', '\x5d', '\x5e', '\x5f',
      '\x60', '\x61', '\x62', '\x63', '\x64', '\x65', '\x66', '\x67',
      '\x68', '\x69', '\x6a', '\x6b', '\x6c', '\x6d', '\x6e', '\x6f',
      '\x70', '\x71', '\x72', '\x73', '\x74', '\x75', '\x76', '\x77',
      '\x78', '\x79', '\x7a', '\x7b', '\x7c', '\x7d', '\x7e', '\x7f',
      '\x80', '\x81', '\x82', '\x83', '\x84', '\x85', '\x86', '\x87',
      '\x88', '\x89', '\x8a', '\x8b', '\x8c', '\x8d', '\x8e', '\x8f',
      '\x90', '\x91', '\x92', '\x93', '\x94', '\x95', '\x96', '\x97',
      '\x98', '\x99', '\x9a', '\x9b', '\x9c', '\x9d', '\x9e', '\x9f',
      '\xa0', '\xa1', '\xa2', '\xa3', '\xa4', '\xa5', '\xa6', '\xa7',
      '\xa8', '\xa9', '\xaa', '\xab', '\xac', '\xad', '\xae', '\xaf',
      '\xb0', '\xb1', '\xb2', '\xb3', '\xb4', '\xb5', '\xb6', '\xb7',
      '\xb8', '\xb9', '\xba', '\xbb', '\xbc', '\xbd', '\xbe', '\xbf',
      '\xc0', '\xc1', '\xc2', '\xc3', '\xc4', '\xc5', '\xc6', '\xc7',
      '\xc8', '\xc9', '\xca', '\xcb', '\xcc', '\xcd', '\xce', '\xcf',
      '\xd0', '\xd1', '\xd2', '\xd3', '\xd4', '\xd5', '\xd6', '\xd7',
      '\xd8', '\xd9', '\xda', '\xdb', '\xdc', '\xdd', '\xde', '\xdf',
      '\xe0', '\xe1', '\xe2', '\xe3', '\xe4', '\xe5', '\xe6', '\xe7',
      '\xe8', '\xe9', '\xea', '\xeb', '\xec', '\xed', '\xee', '\xef',
      '\xf0', '\xf1', '\xf2', '\xf3', '\xf4', '\xf5', '\xf6', '\xf7',
      '\xf8', '\xf9', '\xfa', '\xfb', '\xfc', '\xfd', '\xfe', '\xff',
    };

    // Array of characters for the ascii_toupper() function. For values 'a'
    // through 'z', return the upper-case character; otherwise, return the
    // identity of the passed character.
        const char kToUpper[256] = {
      '\x00', '\x01', '\x02', '\x03', '\x04', '\x05', '\x06', '\x07',
      '\x08', '\x09', '\x0a', '\x0b', '\x0c', '\x0d', '\x0e', '\x0f',
      '\x10', '\x11', '\x12', '\x13', '\x14', '\x15', '\x16', '\x17',
      '\x18', '\x19', '\x1a', '\x1b', '\x1c', '\x1d', '\x1e', '\x1f',
      '\x20', '\x21', '\x22', '\x23', '\x24', '\x25', '\x26', '\x27',
      '\x28', '\x29', '\x2a', '\x2b', '\x2c', '\x2d', '\x2e', '\x2f',
      '\x30', '\x31', '\x32', '\x33', '\x34', '\x35', '\x36', '\x37',
      '\x38', '\x39', '\x3a', '\x3b', '\x3c', '\x3d', '\x3e', '\x3f',
      '\x40', '\x41', '\x42', '\x43', '\x44', '\x45', '\x46', '\x47',
      '\x48', '\x49', '\x4a', '\x4b', '\x4c', '\x4d', '\x4e', '\x4f',
      '\x50', '\x51', '\x52', '\x53', '\x54', '\x55', '\x56', '\x57',
      '\x58', '\x59', '\x5a', '\x5b', '\x5c', '\x5d', '\x5e', '\x5f',
      '\x60',    'A',    'B',    'C',    'D',    'E',    'F',    'G',
         'H',    'I',    'J',    'K',    'L',    'M',    'N',    'O',
         'P',    'Q',    'R',    'S',    'T',    'U',    'V',    'W',
         'X',    'Y',    'Z', '\x7b', '\x7c', '\x7d', '\x7e', '\x7f',
      '\x80', '\x81', '\x82', '\x83', '\x84', '\x85', '\x86', '\x87',
      '\x88', '\x89', '\x8a', '\x8b', '\x8c', '\x8d', '\x8e', '\x8f',
      '\x90', '\x91', '\x92', '\x93', '\x94', '\x95', '\x96', '\x97',
      '\x98', '\x99', '\x9a', '\x9b', '\x9c', '\x9d', '\x9e', '\x9f',
      '\xa0', '\xa1', '\xa2', '\xa3', '\xa4', '\xa5', '\xa6', '\xa7',
      '\xa8', '\xa9', '\xaa', '\xab', '\xac', '\xad', '\xae', '\xaf',
      '\xb0', '\xb1', '\xb2', '\xb3', '\xb4', '\xb5', '\xb6', '\xb7',
      '\xb8', '\xb9', '\xba', '\xbb', '\xbc', '\xbd', '\xbe', '\xbf',
      '\xc0', '\xc1', '\xc2', '\xc3', '\xc4', '\xc5', '\xc6', '\xc7',
      '\xc8', '\xc9', '\xca', '\xcb', '\xcc', '\xcd', '\xce', '\xcf',
      '\xd0', '\xd1', '\xd2', '\xd3', '\xd4', '\xd5', '\xd6', '\xd7',
      '\xd8', '\xd9', '\xda', '\xdb', '\xdc', '\xdd', '\xde', '\xdf',
      '\xe0', '\xe1', '\xe2', '\xe3', '\xe4', '\xe5', '\xe6', '\xe7',
      '\xe8', '\xe9', '\xea', '\xeb', '\xec', '\xed', '\xee', '\xef',
      '\xf0', '\xf1', '\xf2', '\xf3', '\xf4', '\xf5', '\xf6', '\xf7',
      '\xf8', '\xf9', '\xfa', '\xfb', '\xfc', '\xfd', '\xfe', '\xff',
    };

}  // namespace ascii_internal

// ascii_isalpha()
//
// Determines whether the given character is an alphabetic character.
inline bool ascii_isalpha(unsigned char c) {
  return (ascii_internal::kPropertyBits[c] & 0x01) != 0;
}

// ascii_isalnum()
//
// Determines whether the given character is an alphanumeric character.
inline bool ascii_isalnum(unsigned char c) {
  return (ascii_internal::kPropertyBits[c] & 0x04) != 0;
}

// ascii_isspace()
//
// Determines whether the given character is a whitespace character (space,
// tab, vertical tab, formfeed, linefeed, or carriage return).
inline bool ascii_isspace(unsigned char c) {
  return (ascii_internal::kPropertyBits[c] & 0x08) != 0;
}

// ascii_ispunct()
//
// Determines whether the given character is a punctuation character.
inline bool ascii_ispunct(unsigned char c) {
  return (ascii_internal::kPropertyBits[c] & 0x10) != 0;
}

// ascii_isblank()
//
// Determines whether the given character is a blank character (tab or space).
inline bool ascii_isblank(unsigned char c) {
  return (ascii_internal::kPropertyBits[c] & 0x20) != 0;
}

// ascii_iscntrl()
//
// Determines whether the given character is a control character.
inline bool ascii_iscntrl(unsigned char c) {
  return (ascii_internal::kPropertyBits[c] & 0x40) != 0;
}

// ascii_isxdigit()
//
// Determines whether the given character can be represented as a hexadecimal
// digit character (i.e. {0-9} or {A-F}).
inline bool ascii_isxdigit(unsigned char c) {
  return (ascii_internal::kPropertyBits[c] & 0x80) != 0;
}

// ascii_isdigit()
//
// Determines whether the given character can be represented as a decimal
// digit character (i.e. {0-9}).
inline bool ascii_isdigit(unsigned char c) { return c >= '0' && c <= '9'; }

// ascii_isprint()
//
// Determines whether the given character is printable, including whitespace.
inline bool ascii_isprint(unsigned char c) { return c >= 32 && c < 127; }

// ascii_isgraph()
//
// Determines whether the given character has a graphical representation.
inline bool ascii_isgraph(unsigned char c) { return c > 32 && c < 127; }

// ascii_isupper()
//
// Determines whether the given character is uppercase.
inline bool ascii_isupper(unsigned char c) { return c >= 'A' && c <= 'Z'; }

// ascii_islower()
//
// Determines whether the given character is lowercase.
inline bool ascii_islower(unsigned char c) { return c >= 'a' && c <= 'z'; }

// ascii_isascii()
//
// Determines whether the given character is ASCII.
inline bool ascii_isascii(unsigned char c) { return c < 128; }

// ascii_tolower()
//
// Returns an ASCII character, converting to lowercase if uppercase is
// passed. Note that character values > 127 are simply returned.
inline char ascii_tolower(unsigned char c) {
  return ascii_internal::kToLower[c];
}

// Converts the characters in `s` to lowercase, changing the contents of `s`.
inline void AsciiStrToLower(std::string* s) {
    for (auto& ch : *s) {
        ch = absl::ascii_tolower(ch);
    }
}

// Creates a lowercase string from a given std::string_view.
[[nodiscard]] inline std::string AsciiStrToLower(std::string_view s) {
  std::string result(s);
  absl::AsciiStrToLower(&result);
  return result;
}

// ascii_toupper()
//
// Returns the ASCII character, converting to upper-case if lower-case is
// passed. Note that characters values > 127 are simply returned.
inline char ascii_toupper(unsigned char c) {
  return ascii_internal::kToUpper[c];
}

// Converts the characters in `s` to uppercase, changing the contents of `s`.
inline void AsciiStrToUpper(std::string* s) {
    for (auto& ch : *s) {
        ch = absl::ascii_toupper(ch);
    }
}

// Creates an uppercase string from a given std::string_view.
[[nodiscard]] inline std::string AsciiStrToUpper(std::string_view s) {
  std::string result(s);
  absl::AsciiStrToUpper(&result);
  return result;
}

// Returns std::string_view with whitespace stripped from the beginning of the
// given string_view.
[[nodiscard]] inline std::string_view StripLeadingAsciiWhitespace(
    std::string_view str) {
  auto it = std::find_if_not(str.begin(), str.end(), absl::ascii_isspace);
  return str.substr(it - str.begin());
}

// Strips in place whitespace from the beginning of the given string.
inline void StripLeadingAsciiWhitespace(std::string* str) {
  auto it = std::find_if_not(str->begin(), str->end(), absl::ascii_isspace);
  str->erase(str->begin(), it);
}

// Returns std::string_view with whitespace stripped from the end of the given
// string_view.
[[nodiscard]] inline std::string_view StripTrailingAsciiWhitespace(
    std::string_view str) {
  auto it = std::find_if_not(str.rbegin(), str.rend(), absl::ascii_isspace);
  return str.substr(0, str.rend() - it);
}

// Strips in place whitespace from the end of the given string
inline void StripTrailingAsciiWhitespace(std::string* str) {
  auto it = std::find_if_not(str->rbegin(), str->rend(), absl::ascii_isspace);
  str->erase(str->rend() - it);
}

// Returns std::string_view with whitespace stripped from both ends of the
// given string_view.
[[nodiscard]] inline std::string_view StripAsciiWhitespace(
    std::string_view str) {
  return StripTrailingAsciiWhitespace(StripLeadingAsciiWhitespace(str));
}

// Strips in place whitespace from both ends of the given string
inline void StripAsciiWhitespace(std::string* str) {
  StripTrailingAsciiWhitespace(str);
  StripLeadingAsciiWhitespace(str);
}

// Removes leading, trailing, and consecutive internal whitespace.
inline void RemoveExtraAsciiWhitespace(std::string* str) {
    auto stripped = StripAsciiWhitespace(*str);

    if (stripped.empty()) {
        str->clear();
        return;
    }

    auto input_it = stripped.begin();
    auto input_end = stripped.end();
    auto output_it = &(*str)[0];
    bool is_ws = false;

    for (; input_it < input_end; ++input_it) {
        if (is_ws) {
            // Consecutive whitespace?  Keep only the last.
            is_ws = absl::ascii_isspace(*input_it);
            if (is_ws) --output_it;
        }
        else {
            is_ws = absl::ascii_isspace(*input_it);
        }

        *output_it = *input_it;
        ++output_it;
    }

    str->erase(output_it - &(*str)[0]);
}

//ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_STRINGS_ASCII_H_
