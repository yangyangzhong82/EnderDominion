#include "Config/ConfigManager.h"

#include "Config/ConfigSerialization.h"
#include "I18n/I18n.h"
#include "mod/Global.h"
#include <fstream>
#include <nlohmann/json.hpp>
#include <vector>

namespace my_mod {

namespace {

ll::io::Logger& modLogger() { return getLogger(); }

void findMissingKeys(
    const nlohmann::json& defaultJson,
    const nlohmann::json& userJson,
    const std::string&    pathPrefix,
    std::vector<std::string>& missingKeys
) {
    for (const auto& item : defaultJson.items()) {
        const std::string keyPath = pathPrefix + item.key();
        if (!userJson.contains(item.key())) {
            missingKeys.push_back(keyPath);
            continue;
        }

        if (item.value().is_object() && userJson.at(item.key()).is_object()) {
            findMissingKeys(item.value(), userJson.at(item.key()), keyPath + ".", missingKeys);
        }
    }
}

void warnMissingKeys(const nlohmann::json& userJson) {
    const nlohmann::json    defaultJson = Config{};
    std::vector<std::string> missingKeys;
    findMissingKeys(defaultJson, userJson, "", missingKeys);

    if (missingKeys.empty()) {
        return;
    }

    auto& log = modLogger();
    log.warn(tr("config.missing_keys"));
    log.warn(tr("config.missing_keys_hint"));
    for (const auto& key : missingKeys) {
        log.warn(tr("config.missing_key_item", key));
    }
    log.warn(tr("config.regenerate_hint"));
}

} // namespace

ConfigManager& ConfigManager::getInstance() {
    static ConfigManager instance;
    return instance;
}

bool ConfigManager::load(const std::string& path) {
    mConfigPath = path;

    std::ifstream file(mConfigPath);
    if (!file.is_open()) {
        modLogger().info(tr("config.not_found", path));
        return save();
    }

    nlohmann::json userJson;
    try {
        const std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        userJson = nlohmann::json::parse(content, nullptr, true, true);
        mConfig  = userJson.get<Config>();
    } catch (const nlohmann::json::exception& e) {
        modLogger().error(tr("config.parse_error", e.what()));
        return false;
    }

    warnMissingKeys(userJson);
    modLogger().setLevel(mConfig.logLevel);
    return true;
}

bool ConfigManager::save() const {
    try {
        std::ofstream file(mConfigPath);
        if (!file.is_open()) {
            modLogger().error(tr("config.save_error", "open failed"));
            return false;
        }

        file << nlohmann::json(mConfig).dump(4);
        if (!file.good()) {
            modLogger().error(tr("config.save_error", "write failed"));
            return false;
        }
    } catch (const std::exception& e) {
        modLogger().error(tr("config.save_error", e.what()));
        return false;
    }

    return true;
}

Config& ConfigManager::get() { return mConfig; }

const Config& ConfigManager::get() const { return mConfig; }

} // namespace my_mod
