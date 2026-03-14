#include "Event/EnderDragonCrystalEnhance.h"

#include "ll/api/memory/Hook.h"
#include "ll/api/service/Bedrock.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/ActorDamageSource.h"
#include "mc/world/actor/ActorDefinitionIdentifier.h"
#include "mc/world/actor/ActorType.h"
#include "mc/world/actor/Mob.h"
#include "mc/world/actor/monster/EnderCrystal.h"
#include "mc/world/actor/monster/EnderDragon.h"
#include "mc/world/effect/MobEffect.h"
#include "mc/world/effect/MobEffectInstance.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/Spawner.h"
#include "mc/world/level/dimension/VanillaDimensions.h"
#include "mod/Global.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <memory>
#include <numbers>
#include <random>
#include <string>
#include <vector>

namespace my_mod::event {

namespace {
std::unique_ptr<ll::memory::HookRegistrar<class DragonCrystalDestroyedHook>> hookRegistrar;

bool isInTheEnd(Actor const& actor) { return actor.getDimensionId() == VanillaDimensions::TheEnd(); }

std::string normalizeMobName(std::string_view rawName) {
    size_t begin = 0;
    size_t end   = rawName.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(rawName[begin]))) {
        ++begin;
    }
    while (end > begin && std::isspace(static_cast<unsigned char>(rawName[end - 1]))) {
        --end;
    }
    if (begin >= end) {
        return {};
    }

    std::string normalized{rawName.substr(begin, end - begin)};
    for (char& ch : normalized) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        if (ch == ' ') {
            ch = '_';
        }
    }

    if (normalized.find(':') == std::string::npos) {
        normalized = "minecraft:" + normalized;
    }
    return normalized;
}

void spawnGuardMobs(Level& level, EnderDragon& dragon, Vec3 const& crystalPos) {
    const auto& cfg = getConfig();
    if (!cfg.enderDragonCrystalDestroySpawnMobEnabled) {
        return;
    }

    int const spawnCount = std::clamp(cfg.enderDragonCrystalDestroySpawnMobCount, 1, 16);

    // 收集怪物类型
    std::vector<std::string> mobTypes;
    for (auto const& rawName : cfg.enderDragonCrystalDestroySpawnMobTypes) {
        std::string name = normalizeMobName(rawName);
        if (!name.empty() && std::find(mobTypes.begin(), mobTypes.end(), name) == mobTypes.end()) {
            mobTypes.push_back(std::move(name));
        }
    }
    if (mobTypes.empty()) {
        mobTypes.emplace_back("minecraft:enderman");
    }

    static thread_local std::mt19937            rng{std::random_device{}()};
    std::uniform_int_distribution<size_t>        typeDist(0, mobTypes.size() - 1);
    std::uniform_real_distribution<float>        angleDist(0.0F, std::numbers::pi_v<float> * 2.0F);
    std::uniform_real_distribution<float>        radiusDist(2.0F, 5.0F);

    Spawner&     spawner = level.getSpawner();
    BlockSource& region  = dragon.getDimensionBlockSource();

    int spawned  = 0;
    int attempts = 0;
    int maxAttempts = std::max(3, spawnCount * 3);

    while (spawned < spawnCount && attempts < maxAttempts) {
        std::string const& mobName = mobTypes[typeDist(rng)];
        ActorDefinitionIdentifier id(mobName);

        float const angle  = angleDist(rng);
        float const radius = radiusDist(rng);
        Vec3 spawnPos{
            crystalPos.x + std::cos(angle) * radius,
            crystalPos.y,
            crystalPos.z + std::sin(angle) * radius
        };

        Mob* mob = spawner.spawnMob(region, id, &dragon, spawnPos, false, false, true);
        if (mob && mob->isAlive()) {
            // 找最近玩家设为目标
            Actor* nearestPlayer = nullptr;
            float  bestDistSq    = std::numeric_limits<float>::max();
            for (auto* actor : level.getRuntimeActorList()) {
                if (!actor || !actor->hasType(ActorType::Player) || !actor->isAlive()) {
                    continue;
                }
                if (actor->getDimensionId() != dragon.getDimensionId()) {
                    continue;
                }
                Vec3 const  pPos = actor->getPosition();
                float const dx   = pPos.x - spawnPos.x;
                float const dy   = pPos.y - spawnPos.y;
                float const dz   = pPos.z - spawnPos.z;
                float const d2   = dx * dx + dy * dy + dz * dz;
                if (d2 < bestDistSq) {
                    bestDistSq    = d2;
                    nearestPlayer = actor;
                }
            }
            if (nearestPlayer) {
                mob->setTarget(nearestPlayer);
            }
            ++spawned;
        }
        ++attempts;
    }
}

void debuffNearbyPlayers(Level& level, EnderDragon& dragon, Vec3 const& crystalPos) {
    const auto& cfg = getConfig();
    if (!cfg.enderDragonCrystalDestroyDebuffEnabled) {
        return;
    }

    float const range   = std::max(0.0F, cfg.enderDragonCrystalDestroyDebuffRange);
    float const rangeSq = range * range;
    if (range <= 0.0F) {
        return;
    }

    int const ticks     = std::max(1, cfg.enderDragonCrystalDestroyDebuffTicks);
    int const amplifier = std::max(0, cfg.enderDragonCrystalDestroyDebuffLevel);

    // 施加缓慢 + 虚弱
    MobEffect* slowness = MobEffect::MOVEMENT_SLOWDOWN();
    MobEffect* weakness = MobEffect::WEAKNESS();

    for (auto* actor : level.getRuntimeActorList()) {
        if (!actor || !actor->hasType(ActorType::Player) || !actor->isAlive()) {
            continue;
        }
        if (actor->getDimensionId() != dragon.getDimensionId()) {
            continue;
        }
        if (!actor->canReceiveMobEffectsFromGameplay()) {
            continue;
        }

        Vec3 const  pPos = actor->getPosition();
        float const dx   = pPos.x - crystalPos.x;
        float const dy   = pPos.y - crystalPos.y;
        float const dz   = pPos.z - crystalPos.z;
        float const d2   = dx * dx + dy * dy + dz * dz;
        if (d2 > rangeSq) {
            continue;
        }

        if (slowness) {
            MobEffectInstance effect(static_cast<uint>(slowness->mId));
            effect.mDuration->mValue = ticks;
            effect.mAmplifier        = amplifier;
            effect.mAmbient          = false;
            effect.mEffectVisible    = true;
            actor->addEffect(effect);
        }
        if (weakness) {
            MobEffectInstance effect(static_cast<uint>(weakness->mId));
            effect.mDuration->mValue = ticks;
            effect.mAmplifier        = amplifier;
            effect.mAmbient          = false;
            effect.mEffectVisible    = true;
            actor->addEffect(effect);
        }
    }
}

void healDragon(EnderDragon& dragon) {
    const auto& cfg = getConfig();
    int const healAmount = std::max(0, cfg.enderDragonCrystalDestroyDragonHealAmount);
    if (healAmount <= 0 || !dragon.isAlive()) {
        return;
    }

    int const currentHealth = dragon.getHealth();
    int const maxHealth     = dragon.getMaxHealth();
    if (currentHealth >= maxHealth) {
        return;
    }

    int const canHeal = maxHealth - currentHealth;
    dragon.heal(std::min(healAmount, canHeal));
}

LL_TYPE_INSTANCE_HOOK(
    DragonCrystalDestroyedHook,
    ll::memory::HookPriority::Normal,
    EnderDragon,
    &EnderDragon::onCrystalDestroyed,
    void,
    ::EnderCrystal const& crystal,
    ::BlockPos            pos,
    ::ActorDamageSource const& source
) {
    origin(crystal, pos, source);

    const auto& cfg = getConfig();
    if (!cfg.enderDragonCrystalEnhanceEnabled) {
        return;
    }

    auto* dragon = this;
    if (!dragon->isAlive() || !isInTheEnd(*dragon)) {
        return;
    }

    Vec3 const crystalPos{
        static_cast<float>(pos.x) + 0.5F,
        static_cast<float>(pos.y),
        static_cast<float>(pos.z) + 0.5F
    };

    ll::service::getLevel().transform([&](Level& level) {
        // 额外爆炸
        float const explosionPower = std::max(0.1F, cfg.enderDragonCrystalDestroyExplosionPower);
        level.explode(
            dragon->getDimensionBlockSource(),
            dragon,
            crystalPos,
            explosionPower,
            cfg.enderDragonCrystalDestroyExplosionFire,
            cfg.enderDragonCrystalDestroyExplosionBreakBlocks,
            std::numeric_limits<float>::max(),
            true
        );

        // 召唤守卫怪物
        spawnGuardMobs(level, *dragon, crystalPos);

        // 给附近玩家施加负面效果
        debuffNearbyPlayers(level, *dragon, crystalPos);

        // 龙愤怒回血
        healDragon(*dragon);

        return true;
    });
}
} // namespace

void enableEnderDragonCrystalEnhance() {
    if (!hookRegistrar) {
        hookRegistrar = std::make_unique<ll::memory::HookRegistrar<DragonCrystalDestroyedHook>>();
    }
}

void disableEnderDragonCrystalEnhance() {
    hookRegistrar.reset();
}

} // namespace my_mod::event
