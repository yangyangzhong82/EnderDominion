#include "Event/EnderDragonAttackDebuff.h"

#include "czmoney/money_api.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/world/ServerLevelTickEvent.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/service/Bedrock.h"
#include "mc/deps/ecs/gamerefs_entity/EntityContext.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/ActorType.h"
#include "mc/world/actor/ai/goal/DragonStrafePlayerGoal.h"
#include "mc/world/effect/EffectDuration.h"
#include "mc/world/effect/MobEffect.h"
#include "mc/world/effect/MobEffectInstance.h"
#include "mc/world/level/Level.h"
#include "mod/Global.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <memory>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace my_mod::event {

namespace {
std::unique_ptr<ll::memory::HookRegistrar<class DragonStrafeStartDebuffHook>> hookRegistrar;
ll::event::ListenerPtr                                                      levelTickListener;
int                                                                         dragonPeriodicDebuffTickCounter = 0;

struct EffectAlias {
    std::string_view name;
    MobEffectIds     id;
};

std::string normalizeEffectName(std::string_view rawName) {
    size_t begin = 0;
    size_t end   = rawName.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(rawName[begin]))) {
        ++begin;
    }
    while (end > begin && std::isspace(static_cast<unsigned char>(rawName[end - 1]))) {
        --end;
    }

    std::string name{rawName.substr(begin, end - begin)};
    for (char& ch : name) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        if (ch == ' ' || ch == '-') {
            ch = '_';
        }
    }

    constexpr std::string_view minecraftPrefix = "minecraft:";
    if (name.rfind(minecraftPrefix, 0) == 0) {
        name.erase(0, minecraftPrefix.size());
    }
    return name;
}

MobEffect const* resolveEffect(std::string_view rawName) {
    if (rawName.empty()) {
        return nullptr;
    }

    static constexpr std::array<EffectAlias, 17> aliases{
        {{"slowness", MobEffectIds::MovementSlowdown},
         {"movement_slowdown", MobEffectIds::MovementSlowdown},
         {"mining_fatigue", MobEffectIds::DigSlowdown},
         {"dig_slowdown", MobEffectIds::DigSlowdown},
         {"weakness", MobEffectIds::Weakness},
         {"poison", MobEffectIds::Poison},
         {"wither", MobEffectIds::Wither},
         {"blindness", MobEffectIds::Blindness},
         {"nausea", MobEffectIds::Confusion},
         {"confusion", MobEffectIds::Confusion},
         {"hunger", MobEffectIds::Hunger},
         {"darkness", MobEffectIds::Darkness},
         {"levitation", MobEffectIds::Levitation},
         {"bad_omen", MobEffectIds::BadOmen},
         {"raid_omen", MobEffectIds::RaidOmen},
         {"trial_omen", MobEffectIds::TrialOmen},
         {"fatal_poison", MobEffectIds::FatalPoison}}
    };

    std::string const name = normalizeEffectName(rawName);
    if (name.empty()) {
        return nullptr;
    }

    for (auto const& alias : aliases) {
        if (alias.name == name) {
            return MobEffect::mMobEffects()[alias.id].get();
        }
    }

    if (MobEffect const* effect = MobEffect::getByName(name); effect != nullptr) {
        return effect;
    }
    return MobEffect::getByName(std::string(rawName));
}

std::vector<MobEffect const*> collectConfiguredDebuffs() {
    static constexpr std::array<std::string_view, 6>
        defaultDebuffNames{"slowness", "weakness", "poison", "wither", "blindness", "hunger"};

    const auto& cfg = getConfig();

    std::vector<MobEffect const*> debuffs;
    debuffs.reserve(cfg.enderDragonAttackDebuffTypes.size());
    for (auto const& effectName : cfg.enderDragonAttackDebuffTypes) {
        MobEffect const* effect = resolveEffect(effectName);
        if (!effect || !effect->mIsHarmful) {
            continue;
        }
        if (std::find(debuffs.begin(), debuffs.end(), effect) == debuffs.end()) {
            debuffs.push_back(effect);
        }
    }

    if (!debuffs.empty()) {
        return debuffs;
    }

    debuffs.reserve(defaultDebuffNames.size());
    for (auto const effectName : defaultDebuffNames) {
        MobEffect const* effect = resolveEffect(effectName);
        if (effect && effect->mIsHarmful) {
            debuffs.push_back(effect);
        }
    }
    return debuffs;
}

MobEffect const* getRandomDebuff(std::mt19937& rng, std::vector<MobEffect const*> const& debuffs) {
    if (debuffs.empty()) {
        return nullptr;
    }
    std::uniform_int_distribution<size_t> pickDist(0, debuffs.size() - 1);
    return debuffs[pickDist(rng)];
}

Actor* findNearestPlayerInRange(Level& level, Actor const& center, float rangeSquared) {
    Actor* nearestPlayer = nullptr;
    float  bestDistSq    = rangeSquared;

    Vec3 const centerPos = center.getPosition();
    auto const centerDim = center.getDimensionId();
    for (auto* actor : level.getRuntimeActorList()) {
        if (!actor || !actor->hasType(ActorType::Player) || !actor->isAlive()) {
            continue;
        }
        if (actor->getDimensionId() != centerDim) {
            continue;
        }

        Vec3 const  pos = actor->getPosition();
        float const dx  = pos.x - centerPos.x;
        float const dy  = pos.y - centerPos.y;
        float const dz  = pos.z - centerPos.z;
        float const d2  = dx * dx + dy * dy + dz * dz;
        if (d2 <= bestDistSq) {
            bestDistSq    = d2;
            nearestPlayer = actor;
        }
    }

    return nearestPlayer;
}

void tryApplyDebuff(Actor& target, bool checkChance, std::vector<MobEffect const*> const* providedDebuffs = nullptr) {
    const auto& cfg = getConfig();
    if (!target.canReceiveMobEffectsFromGameplay()) {
        return;
    }

    static thread_local std::mt19937 rng{std::random_device{}()};
    if (checkChance) {
        float const chance = std::clamp(cfg.enderDragonAttackDebuffChance, 0.0F, 1.0F);
        if (chance <= 0.0F) {
            return;
        }

        std::uniform_real_distribution<float> chanceDist(0.0F, 1.0F);
        if (chanceDist(rng) > chance) {
            return;
        }
    }

    std::vector<MobEffect const*> localDebuffs;
    auto const*             debuffs = providedDebuffs;
    if (!debuffs) {
        localDebuffs = collectConfiguredDebuffs();
        debuffs      = &localDebuffs;
    }

    MobEffect const* debuff = getRandomDebuff(rng, *debuffs);
    if (!debuff) {
        return;
    }

    int const ticks     = std::max(1, cfg.enderDragonAttackDebuffTicks);
    int const amplifier = std::max(0, cfg.enderDragonAttackDebuffLevel);

    MobEffectInstance effectInstance(static_cast<uint>(debuff->mId), EffectDuration{ticks});
    effectInstance.mAmplifier        = amplifier;
    effectInstance.mAmbient          = false;
    effectInstance.mEffectVisible    = true;
    target.addEffect(effectInstance);
}

void applyPeriodicDragonDebuff(Level& level) {
    const auto& cfg = getConfig();
    if (!cfg.enderDragonPeriodicDebuffEnabled) {
        return;
    }

    int const interval = std::max(1, cfg.enderDragonPeriodicDebuffIntervalTicks);
    if (++dragonPeriodicDebuffTickCounter < interval) {
        return;
    }
    dragonPeriodicDebuffTickCounter = 0;

    float const range = std::max(0.0F, cfg.enderDragonPeriodicDebuffRange);
    if (range <= 0.0F) {
        return;
    }
    float const rangeSquared = range * range;

    std::vector<MobEffect const*> debuffs = collectConfiguredDebuffs();
    if (debuffs.empty()) {
        return;
    }

    for (auto* actor : level.getRuntimeActorList()) {
        if (!actor || actor->getEntityTypeId() != ActorType::Dragon || !actor->isAlive()) {
            continue;
        }

        Actor* nearestPlayer = findNearestPlayerInRange(level, *actor, rangeSquared);
        if (!nearestPlayer) {
            continue;
        }

        tryApplyDebuff(*nearestPlayer, true, &debuffs);
    }
}

LL_TYPE_INSTANCE_HOOK(
    DragonStrafeStartDebuffHook,
    ll::memory::HookPriority::Normal,
    DragonStrafePlayerGoal,
    &DragonStrafePlayerGoal::$start,
    void
) {
    origin();
    if (!getConfig().enderDragonAttackDebuffEnabled) {
        return;
    }
    // The target setter is no longer exposed; resolve the selected target when strafing starts.
    auto targetRef = mAttackTarget->lock();
    if (!targetRef) {
        return;
    }
    Actor* target = Actor::tryGetFromEntity(*targetRef, false);
    if (target && target->hasType(ActorType::Player) && target->isAlive()) {
        tryApplyDebuff(*target, true);
    }
}
} // namespace

void enableEnderDragonAttackDebuff() {
    if (!hookRegistrar) {
        hookRegistrar = std::make_unique<ll::memory::HookRegistrar<DragonStrafeStartDebuffHook>>();
    }

    if (!levelTickListener) {
        levelTickListener = ll::event::EventBus::getInstance().emplaceListener<ll::event::ServerLevelTickEvent>(
            [](ll::event::ServerLevelTickEvent&) {
                ll::service::getLevel().transform([&](Level& level) {
                    applyPeriodicDragonDebuff(level);
                    return true;
                });
            }
        );
    }
}

void disableEnderDragonAttackDebuff() {
    hookRegistrar.reset();
    if (levelTickListener) {
        ll::event::EventBus::getInstance().removeListener(levelTickListener);
        levelTickListener.reset();
    }
    dragonPeriodicDebuffTickCounter = 0;
}

} // namespace my_mod::event
