#include "Event/EnderDragonHomingFireball.h"

#include "ll/api/memory/Hook.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/ActorType.h"
#include "mc/world/actor/projectile/Fireball.h"
#include "mc/world/level/Level.h"
#include "mod/Global.h"
#include <algorithm>
#include <cmath>
#include <memory>

namespace my_mod::event {

namespace {
std::unique_ptr<ll::memory::HookRegistrar<class DragonFireballHomingHook>> hookRegistrar;

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

void applyHoming(Fireball& fireball) {
    auto const& cfg = getConfig();
    if (!cfg.enderDragonHomingFireballEnabled || !fireball.isAlive()) {
        return;
    }
    if (fireball.getEntityTypeId() != ActorType::DragonFireball) {
        return;
    }

    float const range     = std::max(1.0F, cfg.enderDragonHomingFireballRange);
    float const rangeSq   = range * range;
    float const turn      = std::clamp(cfg.enderDragonHomingFireballTurnStrength, 0.0F, 1.0F);
    if (turn <= 0.0F) {
        return;
    }

    Level& level = fireball.getLevel();
    Actor* targetPlayer = findNearestPlayerInRange(level, fireball, rangeSq);
    if (!targetPlayer) {
        return;
    }

    Vec3 const toTarget = targetPlayer->getPosition() - fireball.getPosition();
    float const toTargetLenSq = toTarget.lengthSquared();
    if (toTargetLenSq <= 1.0e-6F) {
        return;
    }

    Vec3 const currentVel   = fireball.getPosDelta();
    float const speedSq     = currentVel.lengthSquared();
    float const speed       = std::max(0.08F, std::sqrt(std::max(0.0F, speedSq)));
    Vec3 const desiredDir   = toTarget.normalized();
    Vec3 const currentDir   = speedSq > 1.0e-6F ? currentVel.normalized() : desiredDir;
    Vec3       blendedDir   = currentDir * (1.0F - turn) + desiredDir * turn;
    float const blendedLen2 = blendedDir.lengthSquared();
    if (blendedLen2 <= 1.0e-6F) {
        blendedDir = desiredDir;
    } else {
        blendedDir = blendedDir.normalized();
    }

    fireball.mBuiltInComponents->mStateVectorComponent->mPosDelta = blendedDir * speed;
}

LL_TYPE_INSTANCE_HOOK(
    DragonFireballHomingHook,
    ll::memory::HookPriority::Normal,
    Fireball,
    &Fireball::$normalTick,
    void
) {
    origin();
    if (auto* self = this->thisFor<Fireball>()) {
        applyHoming(*self);
    }
}
} // namespace

void enableEnderDragonHomingFireball() {
    if (hookRegistrar) {
        return;
    }
    hookRegistrar = std::make_unique<ll::memory::HookRegistrar<DragonFireballHomingHook>>();
}

void disableEnderDragonHomingFireball() {
    hookRegistrar.reset();
}

} // namespace my_mod::event
