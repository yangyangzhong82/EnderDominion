#include "Event/EnderDragonTeleportDash.h"

#include "ll/api/event/EventBus.h"
#include "ll/api/event/world/LevelTickEvent.h"
#include "ll/api/service/Bedrock.h"
#include "mc/deps/ecs/gamerefs_entity/GameRefsEntity.h"
#include "mc/legacy/ActorUniqueID.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/ActorDamageByActorSource.h"
#include "mc/world/actor/ActorDefinitionIdentifier.h"
#include "mc/world/actor/ActorFactory.h"
#include "mc/world/actor/ActorType.h"
#include "mc/world/actor/Mob.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/dimension/VanillaDimensions.h"
#include "mc/network/packet/TextPacket.h"
#include "mc/network/packet/TextPacketType.h"
#include "mod/Global.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <random>
#include <utility>
#include <vector>

namespace my_mod::event {

namespace {
ll::event::ListenerPtr levelTickListener;
int                    globalTickCounter = 0;
int                    dashCooldownCounter = 0;

struct PendingDash {
    ActorUniqueID dragonUid;
    ActorUniqueID playerUid;
    int           executeTick;
};

std::vector<PendingDash> pendingDashes;

bool isInTheEnd(Actor const& actor) { return actor.getDimensionId() == VanillaDimensions::TheEnd(); }

void sendActionbarTip(Player const& player, std::string_view msg) {
    TextPacket                     packet{};
    TextPacketPayload::MessageOnly message{};
    message.mType = TextPacketType::Tip;
    message.mMessage->assign(msg);
    packet.mBody = message;
    packet.sendTo(player);
}

Actor* findDragonInTheEnd(Level& level) {
    for (auto* actor : level.getRuntimeActorList()) {
        if (!actor || actor->getEntityTypeId() != ActorType::Dragon || !actor->isAlive()) {
            continue;
        }
        if (isInTheEnd(*actor)) {
            return actor;
        }
    }
    return nullptr;
}

std::vector<Actor*> collectPlayersInRange(Level& level, Actor const& center, float rangeSquared) {
    std::vector<Actor*> players;
    Vec3 const          centerPos = center.getPosition();
    auto const          centerDim = center.getDimensionId();

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
        if (d2 <= rangeSquared) {
            players.push_back(actor);
        }
    }
    return players;
}

void tryQueueDash(Level& level) {
    const auto& cfg = getConfig();
    if (!cfg.enderDragonTeleportDashEnabled) {
        return;
    }

    int const cooldown = std::max(1, cfg.enderDragonTeleportDashCooldownTicks);
    if (++dashCooldownCounter < cooldown) {
        return;
    }
    dashCooldownCounter = 0;

    Actor* dragon = findDragonInTheEnd(level);
    if (!dragon) {
        return;
    }

    float const range   = std::max(1.0F, cfg.enderDragonTeleportDashRange);
    float const rangeSq = range * range;

    std::vector<Actor*> players = collectPlayersInRange(level, *dragon, rangeSq);
    if (players.empty()) {
        return;
    }

    // 随机选一名玩家作为目标
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<size_t> pickDist(0, players.size() - 1);
    Actor* target = players[pickDist(rng)];
    if (!target) {
        return;
    }

    // 发送警告
    sendActionbarTip(
        *static_cast<Player*>(target),
        "[EnderDominion] The dragon is locking onto you..."
    );

    int const delay = std::max(0, cfg.enderDragonTeleportDashWarningTicks);
    pendingDashes.push_back(PendingDash{
        dragon->getOrCreateUniqueID(),
        target->getOrCreateUniqueID(),
        globalTickCounter + delay
    });
}

void processPendingDashes(Level& level) {
    const auto& cfg = getConfig();
    if (!cfg.enderDragonTeleportDashEnabled || pendingDashes.empty()) {
        return;
    }

    float const damage       = std::max(0.0F, cfg.enderDragonTeleportDashDamage);
    float const knockback    = std::max(0.0F, cfg.enderDragonTeleportDashKnockbackStrength);
    float const hitRange     = std::max(1.0F, cfg.enderDragonTeleportDashHitRange);
    float const hitRangeSq   = hitRange * hitRange;
    float const dashDistance  = std::max(1.0F, cfg.enderDragonTeleportDashDistance);

    pendingDashes.erase(
        std::remove_if(
            pendingDashes.begin(),
            pendingDashes.end(),
            [&](PendingDash const& task) {
                if (task.executeTick > globalTickCounter) {
                    return false;
                }

                Actor* dragon = level.fetchEntity(task.dragonUid, false);
                if (!dragon || !dragon->isAlive() || dragon->getEntityTypeId() != ActorType::Dragon) {
                    return true;
                }

                Actor* target = level.fetchEntity(task.playerUid, false);
                if (!target || !target->hasType(ActorType::Player) || !target->isAlive()) {
                    return true;
                }
                if (dragon->getDimensionId() != target->getDimensionId()) {
                    return true;
                }

                // 根据玩家朝向计算身后位置
                Vec2 const  rot     = target->getRotation();
                float const yawRad  = rot.y * (std::numbers::pi_v<float> / 180.0F);
                // 玩家面朝方向
                float const facingX = -std::sin(yawRad);
                float const facingZ = std::cos(yawRad);
                // 身后位置 = 玩家位置 - 面朝方向 * 距离
                Vec3 const playerPos = target->getPosition();
                Vec3 const dashPos{
                    playerPos.x - facingX * dashDistance,
                    playerPos.y,
                    playerPos.z - facingZ * dashDistance
                };

                // 传送前在原位召唤闪电（离开特效）
                {
                    ActorDefinitionIdentifier lightningId("minecraft:lightning_bolt");
                    BlockSource&              region = dragon->getDimensionBlockSource();
                    auto bolt = level.getActorFactory().createSummonedActor(
                        lightningId,
                        dragon,
                        dragon->getPosition()
                    );
                    if (bolt) {
                        level.addEntity(region, std::move(bolt));
                    }
                }

                // 传送末影龙到玩家身后
                dragon->teleportTo(dashPos, false, 0, 0, false);

                // 在新位置召唤闪电（到达特效）
                {
                    ActorDefinitionIdentifier lightningId("minecraft:lightning_bolt");
                    BlockSource&              region = dragon->getDimensionBlockSource();
                    auto bolt = level.getActorFactory().createSummonedActor(
                        lightningId,
                        dragon,
                        dashPos
                    );
                    if (bolt) {
                        level.addEntity(region, std::move(bolt));
                    }
                }

                // 对落点附近玩家施加伤害和击退
                for (auto* actor : level.getRuntimeActorList()) {
                    if (!actor || !actor->hasType(ActorType::Player) || !actor->isAlive()) {
                        continue;
                    }
                    if (actor->getDimensionId() != dragon->getDimensionId()) {
                        continue;
                    }

                    Vec3 const  pPos = actor->getPosition();
                    float const dx   = pPos.x - dashPos.x;
                    float const dy   = pPos.y - dashPos.y;
                    float const dz   = pPos.z - dashPos.z;
                    float const d2   = dx * dx + dy * dy + dz * dz;
                    if (d2 > hitRangeSq) {
                        continue;
                    }

                    // 伤害
                    if (damage > 0.0F) {
                        ActorDamageByActorSource dmgSource{
                            *dragon,
                            SharedTypes::Legacy::ActorDamageCause::EntityAttack
                        };
                        static_cast<Mob*>(actor)->_hurt(dmgSource, damage, true, false);
                    }

                    // 击退（方向从冲刺点指向玩家）
                    if (knockback > 0.0F && d2 > 0.001F) {
                        float const dist    = std::sqrt(d2);
                        float const invDist = 1.0F / dist;
                        Vec3 impulse{
                            dx * invDist * knockback,
                            0.4F,
                            dz * invDist * knockback
                        };
                        actor->applyImpulse(impulse);
                    }
                }

                return true;
            }
        ),
        pendingDashes.end()
    );
}

} // namespace

void enableEnderDragonTeleportDash() {
    if (levelTickListener) {
        return;
    }

    levelTickListener = ll::event::EventBus::getInstance().emplaceListener<ll::event::LevelTickEvent>(
        [](ll::event::LevelTickEvent&) {
            ++globalTickCounter;
            ll::service::getLevel().transform([&](Level& level) {
                tryQueueDash(level);
                processPendingDashes(level);
                return true;
            });
        }
    );
}

void disableEnderDragonTeleportDash() {
    if (levelTickListener) {
        ll::event::EventBus::getInstance().removeListener(levelTickListener);
        levelTickListener.reset();
    }
    pendingDashes.clear();
    globalTickCounter   = 0;
    dashCooldownCounter = 0;
}

} // namespace my_mod::event
