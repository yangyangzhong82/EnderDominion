#include "Event/EnderDragonTargetImmunity.h"

#include "ll/api/event/EventBus.h"
#include "ll/api/event/world/ServerLevelTickEvent.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/service/Bedrock.h"
#include "mc/legacy/ActorUniqueID.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/ActorType.h"
#include "mc/world/actor/Mob.h"
#include "mc/world/level/Level.h"

#include <memory>

namespace my_mod::event {

namespace {
ll::event::ListenerPtr actorTargetCleanupListener;
std::unique_ptr<ll::memory::HookRegistrar<class ActorSetTargetImmunityHook>> actorSetTargetHookRegistrar;
std::unique_ptr<ll::memory::HookRegistrar<class MobSetTargetImmunityHook>>   mobSetTargetHookRegistrar;

bool isEnderDragon(Actor const* actor) {
    return actor && actor->getEntityTypeId() == ActorType::Dragon;
}

void clearExistingDragonTargets(Level& level) {
    for (Actor* actor : level.getRuntimeActorList()) {
        if (actor && isEnderDragon(level.fetchEntity(actor->mTargetId, false))) {
            actor->setTarget(nullptr);
        }
    }
}

LL_TYPE_INSTANCE_HOOK(
    ActorSetTargetImmunityHook,
    ll::memory::HookPriority::High,
    Actor,
    &Actor::$setTarget,
    void,
    Actor* target
) {
    if (!isEnderDragon(target)) {
        origin(target);
    }
}

LL_TYPE_INSTANCE_HOOK(
    MobSetTargetImmunityHook,
    ll::memory::HookPriority::High,
    Mob,
    &Mob::$setTarget,
    void,
    Actor* target
) {
    if (!isEnderDragon(target)) {
        origin(target);
    }
}
} // namespace

void enableEnderDragonTargetImmunity() {
    if (!actorSetTargetHookRegistrar) {
        actorSetTargetHookRegistrar = std::make_unique<ll::memory::HookRegistrar<ActorSetTargetImmunityHook>>();
    }
    if (!mobSetTargetHookRegistrar) {
        mobSetTargetHookRegistrar = std::make_unique<ll::memory::HookRegistrar<MobSetTargetImmunityHook>>();
    }
    if (!actorTargetCleanupListener) {
        actorTargetCleanupListener = ll::event::EventBus::getInstance().emplaceListener<ll::event::ServerLevelTickEvent>(
            [](ll::event::ServerLevelTickEvent&) {
                ll::service::getLevel().transform([&](Level& level) {
                    clearExistingDragonTargets(level);
                    return true;
                });
            }
        );
    }
}

void disableEnderDragonTargetImmunity() {
    if (actorTargetCleanupListener) {
        ll::event::EventBus::getInstance().removeListener(actorTargetCleanupListener);
        actorTargetCleanupListener.reset();
    }
    mobSetTargetHookRegistrar.reset();
    actorSetTargetHookRegistrar.reset();
}

} // namespace my_mod::event
