#include "Event/EnderDragonHealth.h"

#include "ll/api/service/Bedrock.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/ActorType.h"
#include "mc/world/actor/Mob.h"
#include "mc/world/attribute/Attribute.h"
#include "mc/world/attribute/AttributeInstanceForwarder.h"
#include "mc/world/attribute/AttributeModificationContext.h"
#include "mc/world/attribute/BaseAttributeMap.h"
#include "mc/world/attribute/SharedAttributes.h"
#include "mc/world/level/Level.h"
#include "mod/Global.h"

namespace my_mod::event {

namespace {
void applyEnderDragonMaxHealth(Mob& mob) {
    if (mob.getEntityTypeId() != ActorType::Dragon) {
        return;
    }

    const auto& cfg          = getConfig();
    float const targetHealth = cfg.enderDragonMaxHealth;
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
} // namespace

int applyEnderDragonMaxHealthToExisting() {
    int updatedCount = 0;
    ll::service::getLevel().transform([&](Level& level) {
        for (auto* actor : level.getRuntimeActorList()) {
            if (!actor || actor->getEntityTypeId() != ActorType::Dragon) {
                continue;
            }
            applyEnderDragonMaxHealth(static_cast<Mob&>(*actor));
            ++updatedCount;
        }
        return true;
    });
    return updatedCount;
}

} // namespace my_mod::event
