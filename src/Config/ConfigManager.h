#pragma once

#include "Config/Config.h"
#include <filesystem>
#include <string>

namespace my_mod {

class ConfigManager {
public:
    static ConfigManager& getInstance();

    bool load(const std::string& path);
    bool save() const;

    Config&       get();
    const Config& get() const;

private:
    ConfigManager() = default;
    ~ConfigManager() = default;

    ConfigManager(const ConfigManager&)            = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;
    ConfigManager(ConfigManager&&)                 = delete;
    ConfigManager& operator=(ConfigManager&&)      = delete;

    Config               mConfig{};
    std::filesystem::path mConfigPath;
};

} // namespace my_mod
