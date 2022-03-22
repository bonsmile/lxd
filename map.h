#pragma once

#include <array>
#include <algorithm>
#include <optional>

namespace lxd {
    template <typename Key, typename Value, std::size_t Size>
    struct StaticMap {
        std::array<std::pair<Key, Value>, Size> data;

        [[nodiscard]] constexpr std::optional<Value> at(const Key& key) const {
            const auto itr =
                std::find_if(begin(data), end(data),
                             [&key](const auto& v) { return v.first == key; });
            if(itr != end(data)) {
                return itr->second;
            } else {
                return std::nullopt;
            }
        }
    };
}