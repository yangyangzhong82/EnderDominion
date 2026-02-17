#pragma once

#include "Config/Config.h"
#include "ll/api/io/LogLevel.h"
#include <nlohmann/json.hpp>

#include <boost/pfr.hpp>
#include <string_view>
#include <type_traits>
#include <utility>

namespace nlohmann {

template <typename T>
    requires std::is_aggregate_v<T>
struct adl_serializer<T> {
    static void to_json(json& j, const T& value) {
        j = json::object();
        constexpr auto field_count = boost::pfr::tuple_size_v<T>;

        [&]<size_t... I>(std::index_sequence<I...>) {
            ((j[boost::pfr::get_name<I, T>()] = boost::pfr::get<I>(value)), ...);
        }(std::make_index_sequence<field_count>{});
    }

    static void from_json(const json& j, T& value) {
        const T        default_value{};
        constexpr auto field_count = boost::pfr::tuple_size_v<T>;

        [&]<size_t... I>(std::index_sequence<I...>) {
            (([&] {
                 constexpr std::string_view name = boost::pfr::get_name<I, T>();

                 if (j.contains(name)) {
                     j.at(name).get_to(boost::pfr::get<I>(value));
                 } else {
                     boost::pfr::get<I>(value) = boost::pfr::get<I>(default_value);
                 }
             }()),
             ...);
        }(std::make_index_sequence<field_count>{});
    }
};

} // namespace nlohmann
