#pragma once

#include <fmt/format.h>
#include <string>
#include <string_view>
#include <unordered_map>

namespace my_mod {

class I18n {
public:
    static I18n& getInstance();

    bool load(const std::string& langDir, const std::string& defaultLang = "zh_CN");
    void setLanguage(const std::string& lang);

    [[nodiscard]] const std::string& getLanguage() const;

    [[nodiscard]] std::string get(std::string_view key) const;

    template <typename... Args>
    [[nodiscard]] std::string get(std::string_view key, Args&&... args) const {
        return fmt::format(fmt::runtime(get(key)), std::forward<Args>(args)...);
    }

private:
    using TranslationTable = std::unordered_map<std::string, std::string>;

    I18n() = default;
    ~I18n() = default;
    I18n(const I18n&) = delete;
    I18n& operator=(const I18n&) = delete;

    bool loadLanguageFile(const std::string& path, const std::string& lang);

    [[nodiscard]] const std::string* find(const std::string& key, const std::string& lang) const;

    std::string                                                 mCurrentLang;
    std::string                                                 mDefaultLang;
    std::unordered_map<std::string, TranslationTable> mTranslations;
};

inline std::string tr(std::string_view key) { return I18n::getInstance().get(key); }

template <typename... Args>
inline std::string tr(std::string_view key, Args&&... args) {
    return I18n::getInstance().get(key, std::forward<Args>(args)...);
}

} // namespace my_mod
