#include "Event/EnderDragonBreathBarrage.h"

#include "ll/api/event/EventBus.h"
#include "ll/api/event/world/LevelTickEvent.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/service/Bedrock.h"
#include "mc/legacy/ActorUniqueID.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/ActorDefinitionIdentifier.h"
#include "mc/world/actor/ActorType.h"
#include "mc/world/actor/ai/goal/DragonFlamingGoal.h"
#include "mc/world/actor/monster/EnderDragon.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/Spawner.h"
#include "mc/world/level/dimension/VanillaDimensions.h"
#include "mod/Global.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <memory>
#include <numbers>
#include <string>
#include <string_view>
#include <unordered_map>

namespace my_mod::event {

namespace {
std::unique_ptr<ll::memory::HookRegistrar<struct BarrageFlamingStartHook>> startHookRegistrar;
std::unique_ptr<ll::memory::HookRegistrar<struct BarrageFlamingStopHook>>  stopHookRegistrar;
ll::event::ListenerPtr                                                     levelTickListener;
int                                                                        globalTickCounter = 0;

struct BarrageState {
    bool  active      = false;
    int   tickCounter = 0;
    float spiralAngle = 0.0F;
};

std::unordered_map<int64, BarrageState> barrageStates;

int countActiveDragonFireballs(Level& level) {
    int count = 0;
    for (auto* actor : level.getRuntimeActorList()) {
        if (!actor || !actor->isAlive()) {
            continue;
        }
        if (actor->getEntityTypeId() == ActorType::DragonFireball) {
            ++count;
        }
    }
    return count;
}

std::string normalizePattern(std::string_view raw) {
    std::string result;
    result.reserve(raw.size());
    for (char ch : raw) {
        result += static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return result;
}

void fireBarrageWave(Level& level, EnderDragon& dragon, BarrageState& state) {
    auto const& cfg = getConfig();
    if (!dragon.isAlive()) {
        return;
    }

    // 限制同时存在的龙息火球总数，防止粒子过多导致卡顿
    int const maxFireballs = std::max(1, cfg.enderDragonBreathBarrageMaxAlive);
    int const aliveCount   = countActiveDragonFireballs(level);
    if (aliveCount >= maxFireballs) {
        return;
    }

    int   count = std::max(1, cfg.enderDragonBreathBarrageCount);
    float speed = std::max(0.1F, cfg.enderDragonBreathBarrageSpeed);

    // 低血量增强
    if (cfg.enderDragonBreathBarrageLowHealthBoostEnabled) {
        float const threshold = std::max(0.0F, cfg.enderDragonBreathBarrageLowHealthThreshold);
        if (static_cast<float>(dragon.getHealth()) <= threshold) {
            count += std::max(0, cfg.enderDragonBreathBarrageLowHealthExtraCount);
        }
    }

    // 确保不超过上限
    count = std::min(count, maxFireballs - aliveCount);
    if (count <= 0) {
        return;
    }

    Vec3 const  dragonPos = dragon.getPosition();
    Vec3 const  firePos{dragonPos.x, dragonPos.y + 2.0F, dragonPos.z};
    Vec3 const  headLook = dragon.getHeadLookVector(1.0F);
    float const baseAngle = std::atan2(headLook.z, headLook.x);

    std::string const pattern = normalizePattern(cfg.enderDragonBreathBarragePattern);

    Spawner&     spawner = level.getSpawner();
    BlockSource& region  = dragon.getDimensionBlockSource();
    ActorDefinitionIdentifier fireballId("minecraft:dragon_fireball");

    for (int i = 0; i < count; ++i) {
        float angle = 0.0F;

        if (pattern == "fan") {
            float const fanAngle =
                std::max(1.0F, cfg.enderDragonBreathBarrageFanAngle) * (std::numbers::pi_v<float> / 180.0F);
            if (count > 1) {
                angle = baseAngle - fanAngle * 0.5F + fanAngle * static_cast<float>(i) / static_cast<float>(count - 1);
            } else {
                angle = baseAngle;
            }
        } else if (pattern == "ring") {
            angle = static_cast<float>(i) * (2.0F * std::numbers::pi_v<float> / static_cast<float>(count));
        } else {
            // spiral (default)
            angle = state.spiralAngle
                  + static_cast<float>(i) * (2.0F * std::numbers::pi_v<float> / static_cast<float>(count));
        }

        Vec3 direction{
            std::cos(angle) * speed,
            -0.2F * speed,
            std::sin(angle) * speed
        };

        spawner.spawnProjectile(region, fireballId, &dragon, firePos, direction);
    }

    // 螺旋模式：每波递增角度
    if (pattern == "spiral" || pattern.empty()) {
        state.spiralAngle += 30.0F * (std::numbers::pi_v<float> / 180.0F);
        if (state.spiralAngle > 2.0F * std::numbers::pi_v<float>) {
            state.spiralAngle -= 2.0F * std::numbers::pi_v<float>;
        }
    }
}

void processBarrageTick(Level& level) {
    auto const& cfg = getConfig();
    if (!cfg.enderDragonBreathBarrageEnabled) {
        return;
    }

    int interval = std::max(1, cfg.enderDragonBreathBarrageIntervalTicks);

    for (auto& [uid, state] : barrageStates) {
        if (!state.active) {
            continue;
        }

        // 低血量时减少间隔
        int actualInterval = interval;
        ActorUniqueID actorUid{uid};
        Actor* dragonActor = level.fetchEntity(actorUid, false);
        if (!dragonActor || dragonActor->getEntityTypeId() != ActorType::Dragon || !dragonActor->isAlive()) {
            state.active = false;
            continue;
        }

        auto& dragon = *static_cast<EnderDragon*>(dragonActor);

        if (cfg.enderDragonBreathBarrageLowHealthBoostEnabled) {
            float const threshold = std::max(0.0F, cfg.enderDragonBreathBarrageLowHealthThreshold);
            if (static_cast<float>(dragon.getHealth()) <= threshold) {
                actualInterval = std::max(1, actualInterval * 2 / 3);
            }
        }

        if (++state.tickCounter < actualInterval) {
            continue;
        }
        state.tickCounter = 0;

        fireBarrageWave(level, dragon, state);
    }
}

LL_TYPE_INSTANCE_HOOK(
    BarrageFlamingStartHook,
    ll::memory::HookPriority::Normal,
    DragonFlamingGoal,
    &DragonFlamingGoal::$start,
    void
) {
    origin();
    auto const& cfg = getConfig();
    if (!cfg.enderDragonBreathBarrageEnabled) {
        return;
    }

    EnderDragon& dragon = this->mDragon;
    if (!dragon.isAlive()) {
        return;
    }

    int64 uid               = dragon.getOrCreateUniqueID().rawID;
    auto& state             = barrageStates[uid];
    state.active            = true;
    state.tickCounter       = 0;
    state.spiralAngle       = 0.0F;
}

LL_TYPE_INSTANCE_HOOK(
    BarrageFlamingStopHook,
    ll::memory::HookPriority::Normal,
    DragonFlamingGoal,
    &DragonFlamingGoal::$stop,
    void
) {
    origin();

    EnderDragon& dragon = this->mDragon;
    int64 uid           = dragon.getOrCreateUniqueID().rawID;
    auto  it            = barrageStates.find(uid);
    if (it != barrageStates.end()) {
        it->second.active = false;
    }
}
} // namespace

void enableEnderDragonBreathBarrage() {
    if (!startHookRegistrar) {
        startHookRegistrar = std::make_unique<ll::memory::HookRegistrar<BarrageFlamingStartHook>>();
    }
    if (!stopHookRegistrar) {
        stopHookRegistrar = std::make_unique<ll::memory::HookRegistrar<BarrageFlamingStopHook>>();
    }

    if (!levelTickListener) {
        levelTickListener = ll::event::EventBus::getInstance().emplaceListener<ll::event::LevelTickEvent>(
            [](ll::event::LevelTickEvent&) {
                ++globalTickCounter;
                ll::service::getLevel().transform([&](Level& level) {
                    processBarrageTick(level);
                    return true;
                });
            }
        );
    }
}

void disableEnderDragonBreathBarrage() {
    startHookRegistrar.reset();
    stopHookRegistrar.reset();
    if (levelTickListener) {
        ll::event::EventBus::getInstance().removeListener(levelTickListener);
        levelTickListener.reset();
    }
    barrageStates.clear();
    globalTickCounter = 0;
}

} // namespace my_mod::event
