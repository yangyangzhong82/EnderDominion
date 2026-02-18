#include "Event/EnderDragonReflect.h"

#include "ll/api/memory/Hook.h"
#include "ll/api/service/Bedrock.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/ActorDamageByActorSource.h"
#include "mc/world/actor/ActorDamageSource.h"
#include "mc/world/actor/ActorType.h"
#include "mc/world/actor/Mob.h"
#include "mc/world/level/Level.h"
#include "mod/Global.h"
#include <algorithm>
#include <memory>

namespace my_mod::event {

namespace {
std::unique_ptr<ll::memory::HookRegistrar<class MobHurtHook>> hookRegistrar;

LL_TYPE_INSTANCE_HOOK(
    MobHurtHook,
    ll::memory::HookPriority::Normal,
    Mob,
    &Mob::$_hurt,
    bool,
    ::ActorDamageSource const& source,
    float                      damage,
    bool                       knock,
    bool                       ignite
) {
    auto* selfMob = this->thisFor<Mob>();
    if (!selfMob || selfMob->getEntityTypeId() != ActorType::Dragon) {
        return origin(source, damage, knock, ignite);
    }

    float finalDamage = std::max(0.0F, damage);
    const auto& cfg   = getConfig();
    if (cfg.enderDragonExplosionDamageReductionEnabled) {
        auto const cause = source.mCause;
        if (cause == SharedTypes::Legacy::ActorDamageCause::BlockExplosion
            || cause == SharedTypes::Legacy::ActorDamageCause::EntityExplosion) {
            float const reduction = std::clamp(cfg.enderDragonExplosionDamageReductionRatio, 0.0F, 1.0F);
            finalDamage           = finalDamage * (1.0F - reduction);
        }
    }

    bool const result = origin(source, finalDamage, knock, ignite);
    if (!result) {
        return result;
    }
    if (!cfg.enderDragonReflectEnabled) {
        return result;
    }

    float const reflectRatio  = cfg.enderDragonReflectRatio;
    float const reflectDamage = finalDamage * reflectRatio;
    if (reflectRatio <= 0.0F || reflectDamage <= 0.0F) {
        return result;
    }

    Actor* damageSource = nullptr;
    if (source.isEntitySource()) {
        auto uid = source.isChildEntitySource() ? source.getEntityUniqueID() : source.getDamagingEntityUniqueID();
        if (uid != ActorUniqueID::INVALID_ID()) {
            ll::service::getLevel().transform([&](Level& level) {
                damageSource = level.fetchEntity(uid, false);
                return true;
            });
        }
    }
    if (!damageSource || !damageSource->hasType(ActorType::Player)) {
        return result;
    }

    ActorDamageByActorSource reflectSource{*selfMob, SharedTypes::Legacy::ActorDamageCause::Thorns};
    static_cast<Mob*>(damageSource)->_hurt(reflectSource, reflectDamage, false, false);
    return result;
}
} // namespace

void enableEnderDragonReflect() {
    if (hookRegistrar) {
        return;
    }
    hookRegistrar = std::make_unique<ll::memory::HookRegistrar<MobHurtHook>>();
}

void disableEnderDragonReflect() {
    hookRegistrar.reset();
}

} // namespace my_mod::event
