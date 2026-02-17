#include "I18n/I18n.h"

#include "mod/Global.h"
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace my_mod {

I18n& I18n::getInstance() {
    static I18n instance;
    return instance;
}

bool I18n::load(const std::string& langDir, const std::string& defaultLang) {
    mDefaultLang = defaultLang;
    mCurrentLang = defaultLang;
    mTranslations.clear();

    namespace fs = std::filesystem;
    std::error_code ec;
    if (!fs::exists(langDir, ec)) {
        fs::create_directories(langDir, ec);
        getLogger().warn(get("i18n.lang_dir_not_found", langDir));
        return false;
    }

    bool loaded = false;
    for (const auto& entry : fs::directory_iterator(langDir)) {
        if (entry.path().extension() != ".json") {
            continue;
        }

        const std::string lang = entry.path().stem().string();
        if (loadLanguageFile(entry.path().string(), lang)) {
            loaded = true;
        }
    }
    return loaded;
}

bool I18n::loadLanguageFile(const std::string& path, const std::string& lang) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    try {
        nlohmann::json json;
        file >> json;

        auto& translations = mTranslations[lang];
        translations.clear();

        for (const auto& [key, value] : json.items()) {
            if (value.is_string()) {
                translations[key] = value.get<std::string>();
            }
        }
        getLogger().info(get("i18n.loaded", lang));
        return true;
    } catch (const std::exception& e) {
        getLogger().error(get("i18n.load_error", path, e.what()));
        return false;
    }
}

void I18n::setLanguage(const std::string& lang) {
    if (mTranslations.contains(lang)) {
        mCurrentLang = lang;
        return;
    }

    getLogger().warn(get("i18n.lang_not_found", lang, mDefaultLang));
    mCurrentLang = mDefaultLang;
}

const std::string& I18n::getLanguage() const { return mCurrentLang; }

const std::string* I18n::find(const std::string& key, const std::string& lang) const {
    const auto langIt = mTranslations.find(lang);
    if (langIt == mTranslations.end()) {
        return nullptr;
    }

    const auto keyIt = langIt->second.find(key);
    if (keyIt == langIt->second.end()) {
        return nullptr;
    }

    return &keyIt->second;
}

std::string I18n::get(std::string_view key) const {
    const std::string keyStr(key);

    if (const auto* hit = find(keyStr, mCurrentLang)) {
        return *hit;
    }

    if (mCurrentLang != mDefaultLang) {
        if (const auto* fallback = find(keyStr, mDefaultLang)) {
            return *fallback;
        }
    }

    return keyStr;
}

} // namespace my_mod
