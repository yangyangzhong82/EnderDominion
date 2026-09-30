#include "Event/EnderDragonBreathEnhance.h"

#include "ll/api/memory/Hook.h"
#include "ll/api/service/Bedrock.h"
#include "mc/legacy/ActorUniqueID.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/ActorDamageByActorSource.h"
#include "mc/world/actor/ActorHurtResult.h"
#include "mc/world/actor/ActorType.h"
#include "mc/world/actor/AreaEffectCloud.h"
#include "mc/world/actor/HurtParameters.h"
#include "mc/world/actor/Mob.h"
#include "mc/world/actor/monster/EnderDragon.h"
#include "mc/world/effect/EffectDuration.h"
#include "mc/world/effect/MobEffect.h"
#include "mc/world/effect/MobEffectInstance.h"
#include "mc/world/level/Level.h"
#include "mod/Global.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace my_mod::event {

namespace {
std::unique_ptr<ll::memory::HookRegistrar<struct BreathCloudEnhanceHook>> hookRegistrar;

std::unordered_set<ActorUniqueID> enhancedClouds;
std::unordered_map<ActorUniqueID, int> cloudDamageTickCounters;

struct EffectAlias {
    std::string_view name;
    MobEffectIds     id;
};

std::string normalizeEffectName(std::string_view rawName) {
    size_t begin = 0;
    size_t end   = rawName.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(rawName[begin]))) {
        ++begin;
    }
    while (end > begin && std::isspace(static_cast<unsigned char>(rawName[end - 1]))) {
        --end;
    }

    std::string name{rawName.substr(begin, end - begin)};
    for (char& ch : name) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        if (ch == ' ' || ch == '-') {
            ch = '_';
        }
    }

    constexpr std::string_view minecraftPrefix = "minecraft:";
    if (name.rfind(minecraftPrefix, 0) == 0) {
        name.erase(0, minecraftPrefix.size());
    }
    return name;
}

MobEffect const* resolveEffect(std::string_view rawName) {
    if (rawName.empty()) {
        return nullptr;
    }

    // NOLINTBEGIN(cppcoreguidelines-interfaces-global-init)
    static const std::array<EffectAlias, 17> aliases{
        {{"slowness", MobEffectIds::MovementSlowdown},
         {"movement_slowdown", MobEffectIds::MovementSlowdown},
         {"mining_fatigue", MobEffectIds::DigSlowdown},
         {"dig_slowdown", MobEffectIds::DigSlowdown},
         {"weakness", MobEffectIds::Weakness},
         {"poison", MobEffectIds::Poison},
         {"wither", MobEffectIds::Wither},
         {"blindness", MobEffectIds::Blindness},
         {"nausea", MobEffectIds::Confusion},
         {"confusion", MobEffectIds::Confusion},
         {"hunger", MobEffectIds::Hunger},
         {"darkness", MobEffectIds::Darkness},
         {"levitation", MobEffectIds::Levitation},
         {"bad_omen", MobEffectIds::BadOmen},
         {"raid_omen", MobEffectIds::RaidOmen},
         {"trial_omen", MobEffectIds::TrialOmen},
         {"fatal_poison", MobEffectIds::FatalPoison}}
    };
    // NOLINTEND(cppcoreguidelines-interfaces-global-init)

    std::string const name = normalizeEffectName(rawName);
    if (name.empty()) {
        return nullptr;
    }

    for (auto const& alias : aliases) {
        if (alias.name == name) {
            return MobEffect::mMobEffects()[alias.id].get();
        }
    }

    if (MobEffect const* effect = MobEffect::getByName(name); effect != nullptr) {
        return effect;
    }
    return MobEffect::getByName(std::string(rawName));
}

bool isOwnerDragon(AreaEffectCloud& cloud, Level& level) {
    ActorUniqueID ownerId = cloud.mOwnerId;
    if (ownerId.rawID == 0) {
        return false;
    }

    Actor* owner = level.fetchEntity(ownerId, false);
    if (!owner) {
        return false;
    }
    return owner->getEntityTypeId() == ActorType::Dragon;
}

Actor* findOwnerDragon(AreaEffectCloud& cloud, Level& level) {
    ActorUniqueID ownerId = cloud.mOwnerId;
    if (ownerId.rawID == 0) {
        return nullptr;
    }

    Actor* owner = level.fetchEntity(ownerId, false);
    if (!owner || owner->getEntityTypeId() != ActorType::Dragon || !owner->isAlive()) {
        return nullptr;
    }
    return owner;
}

void enhanceCloud(AreaEffectCloud& cloud, Level& level) {
    auto const& cfg = getConfig();

    // 扩大半径
    float radiusMultiplier = std::max(1.0F, cfg.enderDragonBreathEnhanceRadiusMultiplier);

    // 低血量额外增加半径
    if (cfg.enderDragonBreathEnhanceLowHealthBoostEnabled) {
        float const threshold = std::max(0.0F, cfg.enderDragonBreathEnhanceLowHealthThreshold);
        Actor*      dragon    = findOwnerDragon(cloud, level);
        if (dragon && static_cast<float>(dragon->getHealth()) <= threshold) {
            radiusMultiplier *= std::max(1.0F, cfg.enderDragonBreathEnhanceLowHealthRadiusMultiplier);
        }
    }

    float const currentRadius = cloud.getEffectiveRadius();
    cloud.setInitialRadius(currentRadius * radiusMultiplier);

    // 延长持续时间
    float const durationMultiplier = std::max(1.0F, cfg.enderDragonBreathEnhanceDurationMultiplier);
    EffectDuration newDuration{};
    newDuration.mValue = static_cast<int>(600.0F * durationMultiplier);
    cloud.setDuration(newDuration);

    // 添加额外负面效果
    int const effectLevel = std::max(0, cfg.enderDragonBreathEnhanceExtraEffectLevel);
    int const effectTicks = std::max(1, cfg.enderDragonBreathEnhanceExtraEffectTicks);

    for (auto const& effectName : cfg.enderDragonBreathEnhanceExtraEffects) {
        MobEffect const* effect = resolveEffect(effectName);
        if (!effect) {
            continue;
        }

        MobEffectInstance effectInstance(static_cast<uint>(effect->mId), EffectDuration{effectTicks});
        effectInstance.mAmplifier        = effectLevel;
        effectInstance.mAmbient          = true;
        effectInstance.mEffectVisible    = true;
        cloud.addAreaEffect(effectInstance);
    }
}

void applyExtraDamage(AreaEffectCloud& cloud, Level& level) {
    auto const& cfg = getConfig();
    float const extraDamage = std::max(0.0F, cfg.enderDragonBreathEnhanceExtraDamage);
    if (extraDamage <= 0.0F) {
        return;
    }

    Actor* dragon = findOwnerDragon(cloud, level);
    if (!dragon) {
        return;
    }

    float const radius   = cloud.getEffectiveRadius();
    float const radiusSq = radius * radius;
    Vec3 const  cloudPos = cloud.getPosition();
    auto const  cloudDim = cloud.getDimensionId();

    for (auto* actor : level.getRuntimeActorList()) {
        if (!actor || !actor->hasType(ActorType::Player) || !actor->isAlive()) {
            continue;
        }
        if (actor->getDimensionId() != cloudDim) {
            continue;
        }

        Vec3 const  pos = actor->getPosition();
        float const dx  = pos.x - cloudPos.x;
        float const dz  = pos.z - cloudPos.z;
        float const d2  = dx * dx + dz * dz;
        if (d2 > radiusSq) {
            continue;
        }

        // Y 轴范围检查
        float const dy = pos.y - cloudPos.y;
        if (dy < -1.0F || dy > 3.0F) {
            continue;
        }

        ActorDamageByActorSource dmgSource{*dragon, SharedTypes::Legacy::ActorDamageCause::Magic};
        static_cast<Mob*>(actor)->_hurt(dmgSource, extraDamage, HurtParameters{true, false, {}, 0.0F});
    }
}

LL_TYPE_INSTANCE_HOOK(
    BreathCloudEnhanceHook,
    ll::memory::HookPriority::Normal,
    AreaEffectCloud,
    &AreaEffectCloud::$normalTick,
    void
) {
    origin();

    auto const& cfg = getConfig();
    if (!cfg.enderDragonBreathEnhanceEnabled) {
        return;
    }

    auto* cloud = this;
    if (!cloud->isAlive()) {
        return;
    }

    ActorUniqueID cloudUid = cloud->getOrCreateUniqueID();

    ll::service::getLevel().transform([&](Level& level) {
        // 判断是否为龙息云
        if (!isOwnerDragon(*cloud, level)) {
            return true;
        }

        // 首次 tick 时增强，防止重复
        if (enhancedClouds.find(cloudUid) == enhancedClouds.end()) {
            enhanceCloud(*cloud, level);
            enhancedClouds.insert(cloudUid);
        }

        // 间隔触发额外伤害（每 20 tick），避免每 tick 都遍历玩家列表
        constexpr int damageInterval = 20;
        int& tickCounter = cloudDamageTickCounters[cloudUid];
        if (++tickCounter >= damageInterval) {
            tickCounter = 0;
            applyExtraDamage(*cloud, level);
        }

        return true;
    });
}
} // namespace

void enableEnderDragonBreathEnhance() {
    if (!hookRegistrar) {
        hookRegistrar = std::make_unique<ll::memory::HookRegistrar<BreathCloudEnhanceHook>>();
    }
}

void disableEnderDragonBreathEnhance() {
    hookRegistrar.reset();
    enhancedClouds.clear();
    cloudDamageTickCounters.clear();
}

} // namespace my_mod::event
