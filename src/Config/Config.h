#pragma once

#include "ll/api/io/LogLevel.h"
#include <string>
#include <vector>

namespace my_mod {

struct Config {
    int              version                        = 1;
    ll::io::LogLevel logLevel                       = ll::io::LogLevel::Info;
    std::string      language                       = "zh_CN";
    float            enderDragonMaxHealth           = 200.0F;
    bool             enderDragonReflectEnabled      = true;
    float            enderDragonReflectRatio        = 0.25F;
    bool             enderDragonNaturalRegenEnabled = true;
    int              enderDragonRegenAmount         = 1;
    int              enderDragonRegenIntervalTicks  = 60;
    bool             enderDragonAttackDebuffEnabled = true;
    float            enderDragonAttackDebuffChance  = 0.35F;
    int              enderDragonAttackDebuffTicks   = 120;
    int              enderDragonAttackDebuffLevel   = 0;
    bool             enderDragonPeriodicDebuffEnabled       = true;
    int              enderDragonPeriodicDebuffIntervalTicks = 100;
    float            enderDragonPeriodicDebuffRange         = 20.0F;
    std::vector<std::string> enderDragonAttackDebuffTypes{
        "slowness",
        "weakness",
        "poison",
        "wither",
        "blindness"
    };
};

} // namespace my_mod
