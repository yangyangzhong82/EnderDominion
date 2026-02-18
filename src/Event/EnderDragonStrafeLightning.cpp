#include "Event/EnderDragonStrafeLightning.h"

#include "ll/api/memory/Hook.h"
#include "ll/api/service/Bedrock.h"
#include "mc/deps/ecs/gamerefs_entity/EntityContext.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/ActorDefinitionIdentifier.h"
#include "mc/world/actor/ActorFactory.h"
#include "mc/world/actor/ActorType.h"
#include "mc/world/actor/ai/goal/DragonStrafePlayerGoal.h"
#include "mc/world/actor/monster/EnderDragon.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/dimension/VanillaDimensions.h"
#include "mod/Global.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <random>

namespace my_mod::event {

namespace {
std::unique_ptr<ll::memory::HookRegistrar<class DragonStrafeStartLightningHook>> startHookRegistrar;
std::unique_ptr<ll::memory::HookRegistrar<class DragonStrafeStopLightningHook>>  stopHookRegistrar;

bool isInTheEnd(Actor const& actor) { return actor.getDimensionId() == VanillaDimensions::TheEnd(); }

Actor* findNearestPlayerInRange(Level& level, Actor const& center, float rangeSquared) {
    Actor* nearestPlayer = nullptr;
    float  bestDistSq    = rangeSquared;
    Vec3 const centerPos = center.getPosition();

    for (auto* actor : level.getRuntimeActorList()) {
        if (!actor || !actor->hasType(ActorType::Player) || !actor->isAlive()) {
            continue;
        }
        if (actor->getDimensionId() != center.getDimensionId()) {
            continue;
        }

        Vec3 const pos = actor->getPosition();
        float const dx = pos.x - centerPos.x;
        float const dy = pos.y - centerPos.y;
        float const dz = pos.z - centerPos.z;
        float const d2 = dx * dx + dy * dy + dz * dz;
        if (d2 <= bestDistSq) {
            bestDistSq    = d2;
            nearestPlayer = actor;
        }
    }
    return nearestPlayer;
}

Actor* resolveGoalTargetPlayer(DragonStrafePlayerGoal& goal, Level& level, float fallbackRange) {
    WeakEntityRef const& targetRef = goal.mAttackTarget;
    auto                 locked    = targetRef.lock();
    if (locked) {
        if (Actor* actor = Actor::tryGetFromEntity(*locked, false); actor && actor->hasType(ActorType::Player)
            && actor->isAlive()) {
            return actor;
        }
    }

    float const range = std::max(1.0F, fallbackRange);
    return findNearestPlayerInRange(level, goal.mDragon, range * range);
}

void summonLightning(Level& level, Actor& dragon, Actor& target) {
    const auto& cfg = getConfig();
    if (!cfg.enderDragonStrafeLightningEnabled) {
        return;
    }
    if (!dragon.isAlive() || !target.isAlive() || !target.hasType(ActorType::Player)) {
        return;
    }
    if (!isInTheEnd(dragon) || !isInTheEnd(target)) {
        return;
    }

    int const   strikeCount = std::clamp(cfg.enderDragonStrafeLightningCount, 1, 12);
    float const spread      = std::max(0.0F, cfg.enderDragonStrafeLightningSpread);

    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<float> offsetDist(-spread, spread);

    ActorDefinitionIdentifier lightningId("minecraft:lightning_bolt");
    BlockSource&              region = target.getDimensionBlockSource();
    for (int i = 0; i < strikeCount; ++i) {
        Vec3 strikePos = target.getPosition();
        strikePos.x += offsetDist(rng);
        strikePos.z += offsetDist(rng);

        auto lightningEntity = level.getActorFactory().createSummonedActor(lightningId, &dragon, strikePos);
        if (!lightningEntity) {
            continue;
        }
        level.addEntity(region, std::move(lightningEntity));
    }
}

void triggerStrafeLightning(DragonStrafePlayerGoal& goal) {
    ll::service::getLevel().transform([&](Level& level) {
        Actor* target = resolveGoalTargetPlayer(goal, level, getConfig().enderDragonStrafeLightningFallbackRange);
        if (target) {
            summonLightning(level, goal.mDragon, *target);
        }
        return true;
    });
}

LL_TYPE_INSTANCE_HOOK(
    DragonStrafeStartLightningHook,
    ll::memory::HookPriority::Normal,
    DragonStrafePlayerGoal,
    &DragonStrafePlayerGoal::$start,
    void
) {
    origin();
    triggerStrafeLightning(*this);
}

LL_TYPE_INSTANCE_HOOK(
    DragonStrafeStopLightningHook,
    ll::memory::HookPriority::Normal,
    DragonStrafePlayerGoal,
    &DragonStrafePlayerGoal::$stop,
    void
) {
    origin();
    triggerStrafeLightning(*this);
}
} // namespace

void enableEnderDragonStrafeLightning() {
    if (!startHookRegistrar) {
        startHookRegistrar = std::make_unique<ll::memory::HookRegistrar<DragonStrafeStartLightningHook>>();
    }
    if (!stopHookRegistrar) {
        stopHookRegistrar = std::make_unique<ll::memory::HookRegistrar<DragonStrafeStopLightningHook>>();
    }
}

void disableEnderDragonStrafeLightning() {
    startHookRegistrar.reset();
    stopHookRegistrar.reset();
}

} // namespace my_mod::event
