#include "Event/EnderDragonHealth.h"

#include "ll/api/event/EventBus.h"
#include "ll/api/event/entity/ActorHurtEvent.h"
#include "ll/api/event/world/LevelTickEvent.h"
#include "ll/api/event/world/SpawnMobEvent.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/service/Bedrock.h"
#include "mc/world/actor/Mob.h"
#include "mc/world/actor/ActorType.h"
#include "mc/world/attribute/Attribute.h"
#include "mc/world/attribute/AttributeInstanceForwarder.h"
#include "mc/world/attribute/AttributeModificationContext.h"
#include "mc/world/attribute/BaseAttributeMap.h"
#include "mc/world/attribute/SharedAttributes.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/dimension/VanillaDimensions.h"
#include "mc/world/level/dimension/end/EndDragonFight.h"
#include "mod/Global.h"
#include <algorithm>
#include <memory>
#include <unordered_set>

namespace my_mod::event {

namespace {
ll::event::ListenerPtr                                                      spawnedMobListener;
ll::event::ListenerPtr                                                      levelTickListener;
ll::event::ListenerPtr                                                      actorHurtListener;
std::unique_ptr<ll::memory::HookRegistrar<class EndDragonRespawnStageHook>> respawnStageHookRegistrar;
std::unordered_set<ActorUniqueID>                                           playersInEnd;
int                                                                          dragonRegenTickCounter = 0;

void applyEnderDragonMaxHealth(Mob& mob) {
    if (mob.getEntityTypeId() != ActorType::Dragon) {
        return;
    }

    const auto&  cfg          = getConfig();
    float const  targetHealth = cfg.enderDragonMaxHealth;
    if (targetHealth <= 0.0F) {
        return;
    }

    auto*                      attributeMap = const_cast<BaseAttributeMap*>(mob.getAttributes().get());
    AttributeInstanceForwarder healthAttribute{
        attributeMap->getMutableInstance(SharedAttributes::HEALTH().mIDValue),
        AttributeModificationContext{attributeMap}
    };

    healthAttribute.setMaxValue(targetHealth);
    healthAttribute.setCurrentValue(targetHealth);
}

void applyToExistingEnderDragons() {
    ll::service::getLevel().transform([](Level& level) {
        for (auto* actor : level.getRuntimeActorList()) {
            if (!actor || actor->getEntityTypeId() != ActorType::Dragon) {
                continue;
            }
            applyEnderDragonMaxHealth(static_cast<Mob&>(*actor));
        }
        return true;
    });
}

LL_TYPE_INSTANCE_HOOK(
    EndDragonRespawnStageHook,
    ll::memory::HookPriority::Normal,
    EndDragonFight,
    &EndDragonFight::_setRespawnStage,
    void,
    RespawnAnimation stage
) {
    origin(stage);

    if (stage == RespawnAnimation::SummoningDragon || stage == RespawnAnimation::End) {
        applyToExistingEnderDragons();
    }
}

void rebuildPlayersInEndSnapshot() {
    playersInEnd.clear();
    ll::service::getLevel().transform([](Level& level) {
        for (auto* actor : level.getRuntimeActorList()) {
            if (!actor || !actor->hasType(ActorType::Player)) {
                continue;
            }
            if (actor->getDimensionId() == VanillaDimensions::TheEnd()) {
                playersInEnd.insert(actor->getOrCreateUniqueID());
            }
        }
        return true;
    });
}

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

void handleLevelTick(Level&) {
    ll::service::getLevel().transform([](Level& level) {
        std::unordered_set<ActorUniqueID> currentPlayersInEnd;
        bool                              hasPlayerEnteredEnd = false;

        for (auto* actor : level.getRuntimeActorList()) {
            if (!actor || !actor->hasType(ActorType::Player)) {
                continue;
            }
            if (actor->getDimensionId() != VanillaDimensions::TheEnd()) {
                continue;
            }

            auto const uid = actor->getOrCreateUniqueID();
            currentPlayersInEnd.insert(uid);
            if (!playersInEnd.contains(uid)) {
                hasPlayerEnteredEnd = true;
            }
        }

        playersInEnd = std::move(currentPlayersInEnd);
        if (hasPlayerEnteredEnd) {
            applyToExistingEnderDragons();
        }
        applyDragonNaturalRegen(level);
        return true;
    });
}

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

    auto        uid    = source.getDamagingEntityUniqueID();
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

        attacker->hurtByCause(reflectDamage, SharedTypes::Legacy::ActorDamageCause::Thorns, event.self());
        return true;
    });
}
} // namespace

void enableEnderDragonHealthControl() {
    if (spawnedMobListener || levelTickListener || actorHurtListener || respawnStageHookRegistrar) {
        return;
    }

    spawnedMobListener = ll::event::EventBus::getInstance().emplaceListener<ll::event::SpawnedMobEvent>(
        [](ll::event::SpawnedMobEvent& event) {
            if (auto mob = event.mob()) {
                applyEnderDragonMaxHealth(*mob);
            }
        }
    );

    levelTickListener = ll::event::EventBus::getInstance().emplaceListener<ll::event::LevelTickEvent>(
        [](ll::event::LevelTickEvent& event) { handleLevelTick(event.level()); }
    );
    actorHurtListener = ll::event::EventBus::getInstance().emplaceListener<ll::event::ActorHurtEvent>(
        [](ll::event::ActorHurtEvent& event) { handleActorHurt(event); }
    );
    respawnStageHookRegistrar = std::make_unique<ll::memory::HookRegistrar<EndDragonRespawnStageHook>>();

    rebuildPlayersInEndSnapshot();
}

void disableEnderDragonHealthControl() {
    if (spawnedMobListener) {
        ll::event::EventBus::getInstance().removeListener(spawnedMobListener);
        spawnedMobListener.reset();
    }
    if (levelTickListener) {
        ll::event::EventBus::getInstance().removeListener(levelTickListener);
        levelTickListener.reset();
    }
    if (actorHurtListener) {
        ll::event::EventBus::getInstance().removeListener(actorHurtListener);
        actorHurtListener.reset();
    }
    respawnStageHookRegistrar.reset();
    dragonRegenTickCounter = 0;
    playersInEnd.clear();
}

} // namespace my_mod::event
