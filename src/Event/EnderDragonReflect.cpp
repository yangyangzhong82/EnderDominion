#include "Event/EnderDragonReflect.h"

#include "ll/api/event/EventBus.h"
#include "ll/api/event/entity/ActorHurtEvent.h"
#include "ll/api/service/Bedrock.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/ActorDamageByActorSource.h"
#include "mc/world/actor/ActorType.h"
#include "mc/world/actor/Mob.h"
#include "mc/world/level/Level.h"
#include "mod/Global.h"

namespace my_mod::event {

namespace {
ll::event::ListenerPtr actorHurtListener;

void handleActorHurt(ll::event::ActorHurtEvent& event) {
    if (event.isCancelled()) {
        return;
    }
    if (event.self().getEntityTypeId() != ActorType::Dragon) {
        return;
    }

    auto const& source = event.source();

    const auto& cfg = getConfig();
    if (!cfg.enderDragonReflectEnabled) {
        return;
    }

    float const reflectRatio  = cfg.enderDragonReflectRatio;
    float const reflectDamage = event.damage() * reflectRatio;
    if (reflectRatio <= 0.0F || reflectDamage <= 0.0F) {
        return;
    }

    auto uid = source.getDamagingEntityUniqueID();
    if (uid == ActorUniqueID::INVALID_ID()) {
        uid = source.getEntityUniqueID();
    }
    if (uid == ActorUniqueID::INVALID_ID()) {
        return;
    }

    ll::service::getLevel().transform([&](Level& level) {
        auto* attacker = level.fetchEntity(uid, false);
        if (!attacker || !attacker->hasType(ActorType::Player)) {
            return true;
        }

        ActorDamageByActorSource source{event.self(), SharedTypes::Legacy::ActorDamageCause::Thorns};
        static_cast<Mob*>(attacker)->_hurt(source, reflectDamage, false, false);
        return true;
    });
}
} // namespace

void enableEnderDragonReflect() {
    if (actorHurtListener) {
        return;
    }

    actorHurtListener = ll::event::EventBus::getInstance().emplaceListener<ll::event::ActorHurtEvent>(
        [](ll::event::ActorHurtEvent& event) { handleActorHurt(event); }
    );
}

void disableEnderDragonReflect() {
    if (!actorHurtListener) {
        return;
    }

    ll::event::EventBus::getInstance().removeListener(actorHurtListener);
    actorHurtListener.reset();
}

} // namespace my_mod::event
