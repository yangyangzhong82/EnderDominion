#include "mod/Entry.h"

#include "Command/EnderDragonHealthCommand.h"
#include "Config/ConfigManager.h"
#include "Event/EnderDragonAttackDebuff.h"
#include "Event/EnderDragonBreathBarrage.h"
#include "Event/EnderDragonBreathEnhance.h"
#include "Event/EnderDragonCrystalEnhance.h"
#include "Event/EnderDragonFlamingExplosion.h"
#include "Event/EnderDragonHomingFireball.h"
#include "Event/EnderDragonLandingShockwave.h"
#include "Event/EnderDragonNaturalRegen.h"
#include "Event/EnderDragonPlayerDeathHeal.h"
#include "Event/EnderDragonReflect.h"
#include "Event/EnderDragonStrafeLightning.h"
#include "Event/EnderDragonSummonAggro.h"
#include "Event/EnderDragonTeleportDash.h"
#include "I18n/I18n.h"
#include "mod/Global.h"
#include "ll/api/mod/RegisterHelper.h"

namespace my_mod {

ll::io::Logger& getLogger() { return Entry::getInstance().getSelf().getLogger(); }

Config& getConfig() { return ConfigManager::getInstance().get(); }

Entry& Entry::getInstance() {
    static Entry instance;
    return instance;
}

bool Entry::load() {
    getSelf().getLogger().debug("Loading...");

    const auto langPath = getSelf().getLangDir();
    I18n::getInstance().load(langPath.string(), "zh_CN");

    auto configPath = getSelf().getConfigDir();
    if (!std::filesystem::exists(configPath)) {
        std::filesystem::create_directories(configPath);
    }
    configPath /= "config.json";
    configPath.make_preferred();

    if (!ConfigManager::getInstance().load(configPath.string())) {
        getSelf().getLogger().error(tr("config.save_error", "load failed"));
        return false;
    }

    I18n::getInstance().setLanguage(getConfig().language);

    getLogger().info(tr("plugin.loaded"));
    return true;
}

bool Entry::enable() {
    getSelf().getLogger().debug("Enabling...");
    command::registerEnderDragonHealthCommand();
    event::enableEnderDragonAttackDebuff();
    event::enableEnderDragonBreathBarrage();
    event::enableEnderDragonBreathEnhance();
    event::enableEnderDragonCrystalEnhance();
    event::enableEnderDragonFlamingExplosion();
    event::enableEnderDragonHomingFireball();
    event::enableEnderDragonLandingShockwave();
    event::enableEnderDragonNaturalRegen();
    event::enableEnderDragonPlayerDeathHeal();
    event::enableEnderDragonReflect();
    event::enableEnderDragonStrafeLightning();
    event::enableEnderDragonSummonAggro();
    event::enableEnderDragonTeleportDash();
    return true;
}

bool Entry::disable() {
    getSelf().getLogger().debug("Disabling...");
    command::unregisterEnderDragonHealthCommand();
    event::disableEnderDragonAttackDebuff();
    event::disableEnderDragonBreathBarrage();
    event::disableEnderDragonBreathEnhance();
    event::disableEnderDragonCrystalEnhance();
    event::disableEnderDragonFlamingExplosion();
    event::disableEnderDragonHomingFireball();
    event::disableEnderDragonLandingShockwave();
    event::disableEnderDragonPlayerDeathHeal();
    event::disableEnderDragonReflect();
    event::disableEnderDragonStrafeLightning();
    event::disableEnderDragonNaturalRegen();
    event::disableEnderDragonSummonAggro();
    event::disableEnderDragonTeleportDash();
    getLogger().info(tr("plugin.unloaded"));
    return true;
}

} // namespace my_mod

LL_REGISTER_MOD(my_mod::Entry, my_mod::Entry::getInstance());
