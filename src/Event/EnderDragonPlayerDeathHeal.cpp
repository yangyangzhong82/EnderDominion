#include "Event/EnderDragonPlayerDeathHeal.h"

#include "ll/api/event/EventBus.h"
#include "ll/api/event/player/PlayerDieEvent.h"
#include "ll/api/service/Bedrock.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/ActorType.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/dimension/VanillaDimensions.h"
#include "mod/Global.h"
#include <algorithm>

namespace my_mod::event {

namespace {
ll::event::ListenerPtr playerDieListener;

bool isInTheEnd(Actor const& actor) { return actor.getDimensionId() == VanillaDimensions::TheEnd(); }

bool isOnEndMainIsland(Vec3 const& pos, float islandRadiusSquared) {
    float const distXZSquared = pos.x * pos.x + pos.z * pos.z;
    return distXZSquared <= islandRadiusSquared;
}

void tryHealDragonWhenPlayerDiesOnMainIsland(Player& player) {
    auto const& cfg = getConfig();
    if (!cfg.endIslandPlayerDeathHealDragonEnabled) {
        return;
    }
    if (!isInTheEnd(player)) {
        return;
    }

    float const islandRadius   = std::max(1.0F, cfg.endIslandRadius);
    float const islandRadiusSq = islandRadius * islandRadius;
    if (!isOnEndMainIsland(player.getPosition(), islandRadiusSq)) {
        return;
    }

    int const healAmount = std::max(1, cfg.endIslandPlayerDeathHealDragonAmount);
    ll::service::getLevel().transform([&](Level& level) {
        for (auto* actor : level.getRuntimeActorList()) {
            if (!actor || actor->getEntityTypeId() != ActorType::Dragon || !actor->isAlive()) {
                continue;
            }
            if (!isInTheEnd(*actor)) {
                continue;
            }
            actor->heal(healAmount);
        }
        return true;
    });
}
} // namespace

void enableEnderDragonPlayerDeathHeal() {
    if (playerDieListener) {
        return;
    }

    playerDieListener = ll::event::EventBus::getInstance().emplaceListener<ll::event::PlayerDieEvent>(
        [](ll::event::PlayerDieEvent& ev) { tryHealDragonWhenPlayerDiesOnMainIsland(ev.self()); }
    );
}

void disableEnderDragonPlayerDeathHeal() {
    if (!playerDieListener) {
        return;
    }

    ll::event::EventBus::getInstance().removeListener(playerDieListener);
    playerDieListener.reset();
}

} // namespace my_mod::event
