#include "Event/EnderDragonFlamingExplosion.h"

#include "ll/api/event/EventBus.h"
#include "ll/api/event/world/ServerLevelTickEvent.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/service/Bedrock.h"
#include "mc/legacy/ActorUniqueID.h"
#include "mc/network/packet/TextPacket.h"
#include "mc/network/packet/TextPacketType.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/ActorType.h"
#include "mc/world/actor/ai/goal/DragonFlamingGoal.h"
#include "mc/world/actor/monster/EnderDragon.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/Level.h"
#include "mod/Global.h"
#include <algorithm>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace my_mod::event {

namespace {
std::unique_ptr<ll::memory::HookRegistrar<class DragonFlamingStartHook>> startHookRegistrar;
std::unique_ptr<ll::memory::HookRegistrar<class DragonFlamingStopHook>>  stopHookRegistrar;
ll::event::ListenerPtr                                                   levelTickListener;
int                                                                      globalTickCounter = 0;

struct PendingExplosion {
    ActorUniqueID dragonUid;
    ActorUniqueID playerUid;
    int           executeTick;
};

std::vector<PendingExplosion> pendingExplosions;

std::string buildWarningMessage(bool fromStart, int delayTicks) {
    std::string msg  = "[EnderDominion] Warning: Dragon flame phase ";
    msg             += fromStart ? "started" : "ended";
    if (delayTicks > 0) {
        msg += ", explosion near you in " + std::to_string(delayTicks) + " ticks.";
    } else {
        msg += ", explosion near you now.";
    }
    return msg;
}

void sendActionbarTip(Player const& player, std::string_view msg) {
    TextPacket                     packet{};
    TextPacketPayload::MessageOnly message{};
    message.mType = TextPacketType::Tip;
    message.mMessage->assign(msg);
    packet.mBody = message;
    packet.sendTo(player);
}

bool isWithinRange(Actor const& center, Actor const& target, float rangeSquared) {
    if (center.getDimensionId() != target.getDimensionId()) {
        return false;
    }
    Vec3 const  a  = center.getPosition();
    Vec3 const  b  = target.getPosition();
    float const dx = a.x - b.x;
    float const dy = a.y - b.y;
    float const dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz <= rangeSquared;
}

void queueExplosionsForNearbyPlayers(EnderDragon& dragon, bool fromStart) {
    const auto& cfg = getConfig();
    if (!cfg.enderDragonFlamingExplosionEnabled || !dragon.isAlive()) {
        return;
    }

    float const range = std::max(0.0F, cfg.enderDragonFlamingExplosionRange);
    if (range <= 0.0F) {
        return;
    }
    float const rangeSquared = range * range;
    int const   delayTicks   = std::max(0, cfg.enderDragonFlamingExplosionDelayTicks);
    std::string warning      = buildWarningMessage(fromStart, delayTicks);

    ll::service::getLevel().transform([&](Level& level) {
        for (auto* actor : level.getRuntimeActorList()) {
            if (!actor || !actor->hasType(ActorType::Player) || !actor->isAlive()) {
                continue;
            }
            if (!isWithinRange(dragon, *actor, rangeSquared)) {
                continue;
            }

            sendActionbarTip(*static_cast<Player*>(actor), warning);
            pendingExplosions.push_back(PendingExplosion{
                dragon.getOrCreateUniqueID(),
                actor->getOrCreateUniqueID(),
                globalTickCounter + delayTicks
            });
        }
        return true;
    });
}

void processPendingExplosions(Level& level) {
    const auto& cfg = getConfig();
    if (!cfg.enderDragonFlamingExplosionEnabled || pendingExplosions.empty()) {
        return;
    }

    float const baseRadius = std::max(0.1F, cfg.enderDragonFlamingExplosionPower);
    pendingExplosions.erase(
        std::remove_if(
            pendingExplosions.begin(),
            pendingExplosions.end(),
            [&](PendingExplosion const& task) {
                if (task.executeTick > globalTickCounter) {
                    return false;
                }

                Actor* playerActor = level.fetchEntity(task.playerUid, false);
                if (!playerActor || !playerActor->hasType(ActorType::Player) || !playerActor->isAlive()) {
                    return true;
                }

                Actor* sourceDragon = level.fetchEntity(task.dragonUid, false);
                if (sourceDragon
                    && (!sourceDragon->isAlive() || sourceDragon->getEntityTypeId() != ActorType::Dragon)) {
                    sourceDragon = nullptr;
                }
                if (sourceDragon && sourceDragon->getDimensionId() != playerActor->getDimensionId()) {
                    sourceDragon = nullptr;
                }

                float radius = baseRadius;
                if (sourceDragon && cfg.enderDragonFlamingExplosionLowHealthBoostEnabled) {
                    float const threshold = std::max(0.0F, cfg.enderDragonFlamingExplosionLowHealthThreshold);
                    if (sourceDragon->getHealth() <= threshold) {
                        float const multiplier =
                            std::max(1.0F, cfg.enderDragonFlamingExplosionLowHealthPowerMultiplier);
                        radius *= multiplier;
                    }
                }

                level.explode(
                    playerActor->getDimensionBlockSource(),
                    sourceDragon,
                    playerActor->getPosition(),
                    radius,
                    cfg.enderDragonFlamingExplosionFire,
                    cfg.enderDragonFlamingExplosionBreakBlocks,
                    std::numeric_limits<float>::max(),
                    true
                );
                return true;
            }
        ),
        pendingExplosions.end()
    );
}

LL_TYPE_INSTANCE_HOOK(
    DragonFlamingStartHook,
    ll::memory::HookPriority::Normal,
    DragonFlamingGoal,
    &DragonFlamingGoal::$start,
    void
) {
    origin();
    queueExplosionsForNearbyPlayers(this->mDragon, true);
}

LL_TYPE_INSTANCE_HOOK(
    DragonFlamingStopHook,
    ll::memory::HookPriority::Normal,
    DragonFlamingGoal,
    &DragonFlamingGoal::$stop,
    void
) {
    origin();
    queueExplosionsForNearbyPlayers(this->mDragon, false);
}
} // namespace

void enableEnderDragonFlamingExplosion() {
    if (!startHookRegistrar) {
        startHookRegistrar = std::make_unique<ll::memory::HookRegistrar<DragonFlamingStartHook>>();
    }
    if (!stopHookRegistrar) {
        stopHookRegistrar = std::make_unique<ll::memory::HookRegistrar<DragonFlamingStopHook>>();
    }

    if (!levelTickListener) {
        levelTickListener = ll::event::EventBus::getInstance().emplaceListener<ll::event::ServerLevelTickEvent>(
            [](ll::event::ServerLevelTickEvent& event) {
                ++globalTickCounter;
                ll::service::getLevel().transform([&](Level& level) {
                    processPendingExplosions(level);
                    return true;
                });
            }
        );
    }
}

void disableEnderDragonFlamingExplosion() {
    startHookRegistrar.reset();
    stopHookRegistrar.reset();
    if (levelTickListener) {
        ll::event::EventBus::getInstance().removeListener(levelTickListener);
        levelTickListener.reset();
    }
    pendingExplosions.clear();
    globalTickCounter = 0;
}

} // namespace my_mod::event
