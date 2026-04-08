#include "Event/EnderDragonReflect.h"

#include "czmoney/money_api.h"
#include "I18n/I18n.h"
#include "fmt/format.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/service/Bedrock.h"
#include "mc/legacy/ActorUniqueID.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/ActorDamageByActorSource.h"
#include "mc/world/actor/ActorHurtResult.h"
#include "mc/world/actor/ActorDamageSource.h"
#include "mc/world/actor/ActorType.h"
#include "mc/world/actor/Mob.h"
#include "mc/world/actor/monster/EnderDragon.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/Level.h"
#include "mod/Global.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace my_mod::event {

namespace {
std::unique_ptr<ll::memory::HookRegistrar<class MobHurtHook>> hookRegistrar;
std::unique_ptr<ll::memory::HookRegistrar<class EnderDragonDieHook>> dragonDieHookRegistrar;

struct DragonDamageContribution {
    std::string playerName;
    ActorUniqueID playerUid;
    double      damage = 0.0;
};

std::unordered_map<ActorUniqueID, std::unordered_map<std::string, DragonDamageContribution>> dragonDamageContributions;

[[nodiscard]] bool isFinitePositive(double value) { return std::isfinite(value) && value > 0.0; }

[[nodiscard]] std::string_view moneyApiResultToString(czmoney::api::MoneyApiResult result) {
    using czmoney::api::MoneyApiResult;

    switch (result) {
    case MoneyApiResult::Success:
        return "Success";
    case MoneyApiResult::AccountNotFound:
        return "AccountNotFound";
    case MoneyApiResult::InvalidAmount:
        return "InvalidAmount";
    case MoneyApiResult::InsufficientBalance:
        return "InsufficientBalance";
    case MoneyApiResult::DatabaseError:
        return "DatabaseError";
    case MoneyApiResult::MoneyManagerNotAvailable:
        return "MoneyManagerNotAvailable";
    case MoneyApiResult::UnknownError:
        return "UnknownError";
    }
    return "Unknown";
}

[[nodiscard]] Player* resolveDamageSourcePlayer(ActorDamageSource const& source) {
    if (!source.isEntitySource()) {
        return nullptr;
    }

    ActorUniqueID const uid = source.isChildEntitySource() ? source.getEntityUniqueID() : source.getDamagingEntityUniqueID();
    if (uid == ActorUniqueID::INVALID_ID()) {
        return nullptr;
    }

    Actor* damageSource = nullptr;
    ll::service::getLevel().transform([&](Level& level) {
        damageSource = level.fetchEntity(uid, false);
        return true;
    });
    if (!damageSource || !damageSource->hasType(ActorType::Player)) {
        return nullptr;
    }

    return static_cast<Player*>(damageSource);
}

void recordDragonDamage(ActorUniqueID dragonUid, Player& player, double damage) {
    if (!isFinitePositive(damage)) {
        return;
    }

    auto& contribution = dragonDamageContributions[dragonUid][player.getUuid().asString()];
    contribution.playerName = player.getRealName();
    contribution.playerUid  = player.getOrCreateUniqueID();
    contribution.damage += damage;
}

void notifyPlayerReward(ActorUniqueID playerUid, std::string_view message) {
    ll::service::getLevel().transform([&](Level& level) {
        Actor* playerActor = level.fetchEntity(playerUid, false);
        if (playerActor && playerActor->hasType(ActorType::Player)) {
            static_cast<Player*>(playerActor)->sendMessage(message);
        }
        return true;
    });
}

void settleDragonKillReward(ActorUniqueID dragonUid) {
    auto contributionIt = dragonDamageContributions.find(dragonUid);
    if (contributionIt == dragonDamageContributions.end()) {
        return;
    }

    auto contributions = std::move(contributionIt->second);
    dragonDamageContributions.erase(contributionIt);

    auto const& cfg = getConfig();
    if (!cfg.enderDragonKillReward.enabled) {
        return;
    }

    double const totalReward = cfg.enderDragonKillReward.totalMoney;
    if (!isFinitePositive(totalReward)) {
        return;
    }
    if (cfg.enderDragonKillReward.currencyType.empty()) {
        getLogger().warn("Ender Dragon kill reward skipped because currency type is empty");
        return;
    }

    double totalDamage = 0.0;
    for (auto const& [uuid, contribution] : contributions) {
        (void)uuid;
        if (isFinitePositive(contribution.damage)) {
            totalDamage += contribution.damage;
        }
    }
    if (!isFinitePositive(totalDamage)) {
        return;
    }

    for (auto const& [uuid, contribution] : contributions) {
        if (!isFinitePositive(contribution.damage)) {
            continue;
        }

        double const reward = totalReward * (contribution.damage / totalDamage);
        if (!isFinitePositive(reward)) {
            continue;
        }

        auto const result = czmoney::api::addPlayerBalance(
            uuid,
            cfg.enderDragonKillReward.currencyType,
            reward,
            "EnderDominion",
            "EnderDragonKillReward",
            contribution.playerName
        );
        if (result != czmoney::api::MoneyApiResult::Success) {
            getLogger().warn(
                "Failed to grant Ender Dragon kill reward to {} (uuid={}, amount={}, currency={}): {}",
                contribution.playerName,
                uuid,
                reward,
                cfg.enderDragonKillReward.currencyType,
                moneyApiResultToString(result)
            );
            continue;
        }

        notifyPlayerReward(
            contribution.playerUid,
            tr(
                "reward.ender_dragon_kill_money",
                fmt::format("{:.2f}", reward),
                cfg.enderDragonKillReward.currencyType
            )
        );
    }
}

LL_TYPE_INSTANCE_HOOK(
    EnderDragonDieHook,
    ll::memory::HookPriority::Normal,
    EnderDragon,
    &EnderDragon::$die,
    void,
    ::ActorDamageSource const& source
) {
    auto* dragon = this->thisFor<EnderDragon>();
    ActorUniqueID const dragonUid = dragon ? dragon->getOrCreateUniqueID() : ActorUniqueID::INVALID_ID();

    origin(source);

    if (dragonUid != ActorUniqueID::INVALID_ID()) {
        settleDragonKillReward(dragonUid);
    }
}

LL_TYPE_INSTANCE_HOOK(
    MobHurtHook,
    ll::memory::HookPriority::Normal,
    Mob,
    &Mob::$_hurt,
    ActorHurtResult,
    ::ActorDamageSource const& source,
    float                      damage,
    bool                       knock,
    bool                       ignite
) {
    auto* selfMob = this->thisFor<Mob>();
    if (!selfMob || selfMob->getEntityTypeId() != ActorType::Dragon) {
        return origin(source, damage, knock, ignite);
    }

    ActorUniqueID const dragonUid = selfMob->getOrCreateUniqueID();
    float const healthBeforeHit = std::max(0.0F, static_cast<float>(selfMob->getHealth()));

    float finalDamage = std::max(0.0F, damage);
    const auto& cfg   = getConfig();
    if (cfg.enderDragonExplosionDamageReductionEnabled) {
        auto const cause = source.mCause;
        if (cause == SharedTypes::Legacy::ActorDamageCause::BlockExplosion
            || cause == SharedTypes::Legacy::ActorDamageCause::EntityExplosion) {
            float const reduction = std::clamp(cfg.enderDragonExplosionDamageReductionRatio, 0.0F, 1.0F);
            finalDamage           = finalDamage * (1.0F - reduction);
        }
    }
    if (cfg.enderDragonHighDamageReductionEnabled) {
        float const threshold = std::max(0.0F, cfg.enderDragonHighDamageReductionThreshold);
        float const reduction = std::clamp(cfg.enderDragonHighDamageReductionRatio, 0.0F, 1.0F);
        if (finalDamage > threshold && reduction > 0.0F) {
            float const exceed = finalDamage - threshold;
            finalDamage        = threshold + exceed * (1.0F - reduction);
        }
    }

    ActorHurtResult const result = origin(source, finalDamage, knock, ignite);
    if (!result) {
        return result;
    }

    Player* damageSourcePlayer = resolveDamageSourcePlayer(source);
    float const actualDamage = std::max(
        0.0F,
        healthBeforeHit - std::max(0.0F, static_cast<float>(selfMob->getHealth()))
    );
    if (damageSourcePlayer && actualDamage > 0.0F) {
        recordDragonDamage(dragonUid, *damageSourcePlayer, static_cast<double>(actualDamage));
    }

    float reflectRatio = cfg.enderDragonReflectRatio;
    if (cfg.enderDragonReflectLowHealth.enabled) {
        float const threshold = std::max(0.0F, cfg.enderDragonReflectLowHealth.threshold);
        if (selfMob->getHealth() <= threshold) {
            reflectRatio = std::max(reflectRatio, cfg.enderDragonReflectLowHealth.ratio);
        }
    }
    float const reflectDamage = finalDamage * reflectRatio;
    if (cfg.enderDragonReflectEnabled && reflectRatio > 0.0F && reflectDamage > 0.0F && damageSourcePlayer) {
        ActorDamageByActorSource reflectSource{*selfMob, SharedTypes::Legacy::ActorDamageCause::Thorns};
        static_cast<Mob*>(damageSourcePlayer)->_hurt(reflectSource, reflectDamage, false, false);
    }
    return result;
}
} // namespace

void enableEnderDragonReflect() {
    if (hookRegistrar) {
        return;
    }
    hookRegistrar = std::make_unique<ll::memory::HookRegistrar<MobHurtHook>>();
    dragonDieHookRegistrar = std::make_unique<ll::memory::HookRegistrar<EnderDragonDieHook>>();
}

void disableEnderDragonReflect() {
    hookRegistrar.reset();
    dragonDieHookRegistrar.reset();
    dragonDamageContributions.clear();
}

} // namespace my_mod::event
