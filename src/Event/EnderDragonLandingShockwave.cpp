#include "Event/EnderDragonLandingShockwave.h"

#include "ll/api/memory/Hook.h"
#include "ll/api/service/Bedrock.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/ActorDamageByActorSource.h"
#include "mc/world/actor/ActorType.h"
#include "mc/world/actor/Mob.h"
#include "mc/world/actor/ai/goal/DragonLandingGoal.h"
#include "mc/world/actor/monster/EnderDragon.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/effect/MobEffect.h"
#include "mc/world/effect/MobEffectInstance.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/dimension/VanillaDimensions.h"
#include "mc/network/packet/TextPacket.h"
#include "mc/network/packet/TextPacketType.h"
#include "mod/Global.h"
#include <algorithm>
#include <cmath>
#include <memory>

namespace my_mod::event {

namespace {
std::unique_ptr<ll::memory::HookRegistrar<class DragonLandingStopShockwaveHook>> hookRegistrar;

bool isInTheEnd(Actor const& actor) { return actor.getDimensionId() == VanillaDimensions::TheEnd(); }

void sendActionbarTip(Player const& player, std::string_view msg) {
    TextPacket                     packet{};
    TextPacketPayload::MessageOnly message{};
    message.mType = TextPacketType::Tip;
    message.mMessage->assign(msg);
    packet.mBody = message;
    packet.sendTo(player);
}

void applyShockwave(EnderDragon& dragon) {
    const auto& cfg = getConfig();
    if (!cfg.enderDragonLandingShockwaveEnabled || !dragon.isAlive()) {
        return;
    }
    if (!isInTheEnd(dragon)) {
        return;
    }

    float const range   = std::max(0.0F, cfg.enderDragonLandingShockwaveRange);
    float const rangeSq = range * range;
    if (range <= 0.0F) {
        return;
    }

    float const damage          = std::max(0.0F, cfg.enderDragonLandingShockwaveDamage);
    float const knockback       = std::max(0.0F, cfg.enderDragonLandingShockwaveKnockbackStrength);
    float const upward          = std::max(0.0F, cfg.enderDragonLandingShockwaveUpwardStrength);
    int   const slownessTicks   = std::max(1, cfg.enderDragonLandingShockwaveSlownessTicks);
    int   const slownessLevel   = std::max(0, cfg.enderDragonLandingShockwaveSlownessLevel);

    Vec3 const dragonPos = dragon.getPosition();

    ll::service::getLevel().transform([&](Level& level) {
        for (auto* actor : level.getRuntimeActorList()) {
            if (!actor || !actor->hasType(ActorType::Player) || !actor->isAlive()) {
                continue;
            }
            if (actor->getDimensionId() != dragon.getDimensionId()) {
                continue;
            }

            Vec3 const  playerPos = actor->getPosition();
            float const dx        = playerPos.x - dragonPos.x;
            float const dy        = playerPos.y - dragonPos.y;
            float const dz        = playerPos.z - dragonPos.z;
            float const distSq    = dx * dx + dy * dy + dz * dz;
            if (distSq > rangeSq || distSq < 0.001F) {
                continue;
            }

            // 发送提示
            sendActionbarTip(*static_cast<Player*>(actor), "[EnderDominion] Dragon landing shockwave!");

            // 计算击退方向（从龙指向玩家，归一化）
            float const dist    = std::sqrt(distSq);
            float const invDist = 1.0F / dist;
            Vec3 const  dir{dx * invDist, 0.0F, dz * invDist};

            // 距离衰减：越近越强
            float const ratio = 1.0F - (dist / range);

            // 施加击退
            if (knockback > 0.0F || upward > 0.0F) {
                Vec3 impulse{
                    dir.x * knockback * ratio,
                    upward * ratio,
                    dir.z * knockback * ratio
                };
                actor->applyImpulse(impulse);
            }

            // 施加伤害
            if (damage > 0.0F) {
                ActorDamageByActorSource dmgSource{dragon, SharedTypes::Legacy::ActorDamageCause::EntityAttack};
                static_cast<Mob*>(actor)->_hurt(dmgSource, damage * ratio, true, false);
            }

            // 施加缓慢效果
            if (actor->canReceiveMobEffectsFromGameplay()) {
                MobEffect* slowness = MobEffect::MOVEMENT_SLOWDOWN();
                if (slowness) {
                    MobEffectInstance effectInstance(static_cast<uint>(slowness->mId));
                    effectInstance.mDuration->mValue = slownessTicks;
                    effectInstance.mAmplifier        = slownessLevel;
                    effectInstance.mAmbient          = false;
                    effectInstance.mEffectVisible    = true;
                    actor->addEffect(effectInstance);
                }
            }
        }
        return true;
    });
}

LL_TYPE_INSTANCE_HOOK(
    DragonLandingStopShockwaveHook,
    ll::memory::HookPriority::Normal,
    DragonLandingGoal,
    &DragonLandingGoal::$stop,
    void
) {
    origin();
    applyShockwave(this->mDragon);
}
} // namespace

void enableEnderDragonLandingShockwave() {
    if (!hookRegistrar) {
        hookRegistrar = std::make_unique<ll::memory::HookRegistrar<DragonLandingStopShockwaveHook>>();
    }
}

void disableEnderDragonLandingShockwave() {
    hookRegistrar.reset();
}

} // namespace my_mod::event
