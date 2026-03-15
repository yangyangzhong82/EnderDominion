#pragma once

#include "Config/Config.h"
#include "ll/api/io/LogLevel.h"
#include <nlohmann/json.hpp>

#include <boost/pfr.hpp>
#include <string_view>
#include <type_traits>
#include <utility>

namespace nlohmann {

template <>
struct adl_serializer<my_mod::Config> {
private:
    template <size_t I>
    static void to_json_field(json& j, my_mod::Config const& value) {
        constexpr std::string_view name = boost::pfr::get_name<I, my_mod::Config>();
        auto const&                field = boost::pfr::get<I>(value);

        if constexpr (name == "enderDragonReflectLowHealth") {
            j["enderDragonReflectLowHealthBoostEnabled"] = field.enabled;
            j["enderDragonReflectLowHealthThreshold"]    = field.threshold;
            j["enderDragonReflectLowHealthRatio"]        = field.ratio;
        } else if constexpr (name == "enderDragonKillReward") {
            j["enderDragonKillRewardEnabled"]      = field.enabled;
            j["enderDragonKillRewardTotalMoney"]   = field.totalMoney;
            j["enderDragonKillRewardCurrencyType"] = field.currencyType;
        } else {
            j[name] = field;
        }
    }

    template <size_t I>
    static void from_json_field(json const& j, my_mod::Config& value, my_mod::Config const& defaultValue) {
        constexpr std::string_view name         = boost::pfr::get_name<I, my_mod::Config>();
        auto&                      field        = boost::pfr::get<I>(value);
        auto const&                defaultField = boost::pfr::get<I>(defaultValue);

        if constexpr (name == "enderDragonReflectLowHealth") {
            field = defaultField;
            if (j.contains(name)) {
                j.at(name).get_to(field);
            }
            if (j.contains("enderDragonReflectLowHealthBoostEnabled")) {
                j.at("enderDragonReflectLowHealthBoostEnabled").get_to(field.enabled);
            }
            if (j.contains("enderDragonReflectLowHealthThreshold")) {
                j.at("enderDragonReflectLowHealthThreshold").get_to(field.threshold);
            }
            if (j.contains("enderDragonReflectLowHealthRatio")) {
                j.at("enderDragonReflectLowHealthRatio").get_to(field.ratio);
            }
        } else if constexpr (name == "enderDragonKillReward") {
            field = defaultField;
            if (j.contains(name)) {
                j.at(name).get_to(field);
            }
            if (j.contains("enderDragonKillRewardEnabled")) {
                j.at("enderDragonKillRewardEnabled").get_to(field.enabled);
            }
            if (j.contains("enderDragonKillRewardTotalMoney")) {
                j.at("enderDragonKillRewardTotalMoney").get_to(field.totalMoney);
            }
            if (j.contains("enderDragonKillRewardCurrencyType")) {
                j.at("enderDragonKillRewardCurrencyType").get_to(field.currencyType);
            }
        } else {
            if (j.contains(name)) {
                j.at(name).get_to(field);
            } else {
                field = defaultField;
            }
        }
    }

public:
    static void to_json(json& j, my_mod::Config const& value) {
        j = json::object();
        constexpr auto field_count = boost::pfr::tuple_size_v<my_mod::Config>;

        [&]<size_t... I>(std::index_sequence<I...>) { (to_json_field<I>(j, value), ...); }
        (std::make_index_sequence<field_count>{});
    }

    static void from_json(json const& j, my_mod::Config& value) {
        const my_mod::Config defaultValue{};
        constexpr auto       field_count = boost::pfr::tuple_size_v<my_mod::Config>;

        [&]<size_t... I>(std::index_sequence<I...>) { (from_json_field<I>(j, value, defaultValue), ...); }
        (std::make_index_sequence<field_count>{});
    }
};

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
