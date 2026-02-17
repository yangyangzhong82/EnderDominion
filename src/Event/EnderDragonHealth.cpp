#include "Event/EnderDragonHealth.h"

#include "ll/api/event/EventBus.h"
#include "ll/api/event/world/LevelTickEvent.h"
#include "ll/api/event/world/SpawnMobEvent.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/service/Bedrock.h"
#include "mc/world/actor/ActorType.h"
#include "mc/world/actor/Mob.h"
#include "mc/world/attribute/Attribute.h"
#include "mc/world/attribute/AttributeInstanceForwarder.h"
#include "mc/world/attribute/AttributeModificationContext.h"
#include "mc/world/attribute/BaseAttributeMap.h"
#include "mc/world/attribute/SharedAttributes.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/dimension/VanillaDimensions.h"
#include "mc/world/level/dimension/end/EndDragonFight.h"
#include "mod/Global.h"
#include <memory>
#include <unordered_set>

namespace my_mod::event {

namespace {
ll::event::ListenerPtr                                                      spawnedMobListener;
ll::event::ListenerPtr                                                      levelTickListener;
std::unique_ptr<ll::memory::HookRegistrar<class EndDragonRespawnStageHook>> respawnStageHookRegistrar;
std::unordered_set<ActorUniqueID>                                           playersInEnd;

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
        return true;
    });
}
} // namespace

void enableEnderDragonHealthControl() {
    if (spawnedMobListener || levelTickListener || respawnStageHookRegistrar) {
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
    respawnStageHookRegistrar.reset();
    playersInEnd.clear();
}

} // namespace my_mod::event
