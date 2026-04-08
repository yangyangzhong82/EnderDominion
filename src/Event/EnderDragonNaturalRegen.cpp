#include "Event/EnderDragonNaturalRegen.h"

#include "ll/api/event/EventBus.h"
#include "ll/api/event/world/ServerLevelTickEvent.h"
#include "ll/api/service/Bedrock.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/ActorType.h"
#include "mc/world/level/Level.h"
#include "mod/Global.h"
#include <algorithm>

namespace my_mod::event {

namespace {
ll::event::ListenerPtr levelTickListener;
int                    dragonRegenTickCounter = 0;

void applyDragonNaturalRegen(Level& level) {
    const auto& cfg = getConfig();
    if (!cfg.enderDragonNaturalRegenEnabled) {
        return;
    }

    int const interval = std::max(1, cfg.enderDragonRegenIntervalTicks);
    if (++dragonRegenTickCounter < interval) {
        return;
    }
    dragonRegenTickCounter = 0;

    int const healAmount = std::max(1, cfg.enderDragonRegenAmount);
    for (auto* actor : level.getRuntimeActorList()) {
        if (!actor || actor->getEntityTypeId() != ActorType::Dragon) {
            continue;
        }
        if (actor->getHealth() < actor->getMaxHealth()) {
            actor->heal(healAmount);
        }
    }
}
} // namespace

void enableEnderDragonNaturalRegen() {
    if (levelTickListener) {
        return;
    }

    levelTickListener = ll::event::EventBus::getInstance().emplaceListener<ll::event::ServerLevelTickEvent>(
        [](ll::event::ServerLevelTickEvent&) {
            ll::service::getLevel().transform([&](Level& level) {
                applyDragonNaturalRegen(level);
                return true;
            });
        }
    );
}

void disableEnderDragonNaturalRegen() {
    if (!levelTickListener) {
        return;
    }

    ll::event::EventBus::getInstance().removeListener(levelTickListener);
    levelTickListener.reset();
    dragonRegenTickCounter = 0;
}

} // namespace my_mod::event
