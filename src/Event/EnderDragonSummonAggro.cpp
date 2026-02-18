#include "Event/EnderDragonSummonAggro.h"

#include "ll/api/event/EventBus.h"
#include "ll/api/event/world/LevelTickEvent.h"
#include "ll/api/service/Bedrock.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/ActorDefinitionIdentifier.h"
#include "mc/world/actor/ActorType.h"
#include "mc/world/actor/Mob.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/Spawner.h"
#include "mc/world/level/dimension/VanillaDimensions.h"
#include "mod/Global.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <memory>
#include <numbers>
#include <random>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace my_mod::event {

namespace {
ll::event::ListenerPtr levelTickListener;
int                    dragonSummonTickCounter  = 0;
int                    endermanAggroTickCounter = 0;

std::string trimAndNormalizeMobName(std::string_view rawName) {
    size_t begin = 0;
    size_t end   = rawName.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(rawName[begin]))) {
        ++begin;
    }
    while (end > begin && std::isspace(static_cast<unsigned char>(rawName[end - 1]))) {
        --end;
    }
    if (begin >= end) {
        return {};
    }

    std::string normalized{rawName.substr(begin, end - begin)};
    for (char& ch : normalized) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        if (ch == ' ') {
            ch = '_';
        }
    }

    if (normalized.find(':') == std::string::npos) {
        normalized = "minecraft:" + normalized;
    }
    return normalized;
}

std::vector<std::string> collectSummonMobTypes() {
    std::vector<std::string> mobTypes;
    for (auto const& rawName : getConfig().enderDragonSummonMobTypes) {
        std::string normalized = trimAndNormalizeMobName(rawName);
        if (normalized.empty()) {
            continue;
        }
        if (std::find(mobTypes.begin(), mobTypes.end(), normalized) == mobTypes.end()) {
            mobTypes.push_back(std::move(normalized));
        }
    }
    if (mobTypes.empty()) {
        mobTypes.emplace_back("minecraft:enderman");
    }
    return mobTypes;
}

int countAliveSummonedMobs(Level& level, std::unordered_set<std::string> const& mobTypeSet) {
    if (mobTypeSet.empty()) {
        return 0;
    }

    int aliveCount = 0;
    for (auto* actor : level.getRuntimeActorList()) {
        if (!actor || !actor->isAlive()) {
            continue;
        }
        if (actor->getDimensionId() != VanillaDimensions::TheEnd()) {
            continue;
        }

        std::string normalizedType = trimAndNormalizeMobName(actor->getTypeName());
        if (mobTypeSet.contains(normalizedType)) {
            ++aliveCount;
        }
    }
    return aliveCount;
}

bool isInTheEnd(Actor const& actor) { return actor.getDimensionId() == VanillaDimensions::TheEnd(); }

bool isOnEndMainIsland(Vec3 const& pos, float islandRadiusSquared) {
    float const distXZSquared = pos.x * pos.x + pos.z * pos.z;
    return distXZSquared <= islandRadiusSquared;
}

bool hasAliveDragonInTheEnd(Level& level) {
    for (auto* actor : level.getRuntimeActorList()) {
        if (!actor || actor->getEntityTypeId() != ActorType::Dragon || !actor->isAlive()) {
            continue;
        }
        if (isInTheEnd(*actor)) {
            return true;
        }
    }
    return false;
}

std::vector<Actor*> collectCandidatePlayers(Level& level, bool requireMainIsland, float islandRadiusSquared) {
    std::vector<Actor*> players;
    for (auto* actor : level.getRuntimeActorList()) {
        if (!actor || !actor->hasType(ActorType::Player) || !actor->isAlive()) {
            continue;
        }
        if (!isInTheEnd(*actor)) {
            continue;
        }
        if (requireMainIsland && !isOnEndMainIsland(actor->getPosition(), islandRadiusSquared)) {
            continue;
        }
        players.push_back(actor);
    }
    return players;
}

Actor* findNearestPlayerInRange(std::vector<Actor*> const& players, Vec3 const& center, float rangeSquared) {
    Actor* nearestPlayer = nullptr;
    float  bestDistSq    = rangeSquared;

    for (Actor* player : players) {
        if (!player) {
            continue;
        }
        Vec3 const pos = player->getPosition();
        float const dx = pos.x - center.x;
        float const dy = pos.y - center.y;
        float const dz = pos.z - center.z;
        float const d2 = dx * dx + dy * dy + dz * dz;
        if (d2 <= bestDistSq) {
            bestDistSq    = d2;
            nearestPlayer = player;
        }
    }

    return nearestPlayer;
}

Mob* summonOneMobNearTarget(Level& level, Actor& dragon, Actor& target, std::vector<std::string> const& mobTypes) {
    if (mobTypes.empty()) {
        return nullptr;
    }

    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<size_t> mobTypeDist(0, mobTypes.size() - 1);
    std::uniform_real_distribution<float> angleDist(0.0F, std::numbers::pi_v<float> * 2.0F);
    std::uniform_real_distribution<float> radiusDist(3.0F, 7.0F);
    std::uniform_real_distribution<float> yOffsetDist(-1.0F, 1.0F);

    Spawner&     spawner = level.getSpawner();
    BlockSource& region  = target.getDimensionBlockSource();

    for (int attempt = 0; attempt < 6; ++attempt) {
        std::string const& mobName = mobTypes[mobTypeDist(rng)];
        ActorDefinitionIdentifier id(mobName);

        float const angle  = angleDist(rng);
        float const radius = radiusDist(rng);
        Vec3 const  targetPos = target.getPosition();
        Vec3        spawnPos{
            targetPos.x + std::cos(angle) * radius,
            targetPos.y + yOffsetDist(rng),
            targetPos.z + std::sin(angle) * radius
        };

        Mob* mob = spawner.spawnMob(region, id, &dragon, spawnPos, false, false, true);
        if (!mob || !mob->isAlive()) {
            continue;
        }

        mob->setTarget(&target);
        return mob;
    }

    return nullptr;
}

void processDragonSummon(Level& level) {
    auto const& cfg = getConfig();
    if (!cfg.enderDragonSummonMobEnabled) {
        return;
    }

    int const interval = std::max(1, cfg.enderDragonSummonIntervalTicks);
    if (++dragonSummonTickCounter < interval) {
        return;
    }
    dragonSummonTickCounter = 0;

    float const playerRange = std::max(1.0F, cfg.enderDragonSummonPlayerRange);
    float const rangeSq     = playerRange * playerRange;
    int const   summonCount = std::clamp(cfg.enderDragonSummonCountPerWave, 1, 16);
    int const   maxAlive    = std::max(0, cfg.enderDragonSummonMobMaxAlive);

    std::vector<Actor*> const players = collectCandidatePlayers(level, false, 0.0F);
    if (players.empty()) {
        return;
    }

    std::vector<std::string> const mobTypes = collectSummonMobTypes();
    std::unordered_set<std::string> mobTypeSet(mobTypes.begin(), mobTypes.end());
    int aliveCount = countAliveSummonedMobs(level, mobTypeSet);
    if (maxAlive > 0 && aliveCount >= maxAlive) {
        return;
    }

    for (auto* actor : level.getRuntimeActorList()) {
        if (!actor || actor->getEntityTypeId() != ActorType::Dragon || !actor->isAlive()) {
            continue;
        }
        if (!isInTheEnd(*actor)) {
            continue;
        }

        Actor* targetPlayer = findNearestPlayerInRange(players, actor->getPosition(), rangeSq);
        if (!targetPlayer) {
            continue;
        }

        int allowedSpawnCount = summonCount;
        if (maxAlive > 0) {
            allowedSpawnCount = std::min(allowedSpawnCount, maxAlive - aliveCount);
            if (allowedSpawnCount <= 0) {
                break;
            }
        }

        int spawnedCount = 0;
        int attempts     = 0;
        int maxAttempts  = std::max(3, allowedSpawnCount * 3);
        while (spawnedCount < allowedSpawnCount && attempts < maxAttempts) {
            if (summonOneMobNearTarget(level, *actor, *targetPlayer, mobTypes)) {
                ++spawnedCount;
                ++aliveCount;
            }
            ++attempts;
        }
    }
}

void processEndIslandEndermanAggro(Level& level) {
    auto const& cfg = getConfig();
    if (!cfg.endIslandEndermanAggroEnabled) {
        return;
    }
    if (!hasAliveDragonInTheEnd(level)) {
        return;
    }

    int const interval = std::max(1, cfg.endIslandEndermanAggroIntervalTicks);
    if (++endermanAggroTickCounter < interval) {
        return;
    }
    endermanAggroTickCounter = 0;

    float const aggroRange      = std::max(1.0F, cfg.endIslandEndermanAggroRange);
    float const aggroRangeSq    = aggroRange * aggroRange;
    float const islandRadius    = std::max(1.0F, cfg.endIslandRadius);
    float const islandRadiusSq  = islandRadius * islandRadius;

    std::vector<Actor*> const players = collectCandidatePlayers(level, true, islandRadiusSq);
    if (players.empty()) {
        return;
    }

    for (auto* actor : level.getRuntimeActorList()) {
        if (!actor || actor->getEntityTypeId() != ActorType::EnderMan || !actor->isAlive()) {
            continue;
        }
        if (!isInTheEnd(*actor)) {
            continue;
        }
        if (!isOnEndMainIsland(actor->getPosition(), islandRadiusSq)) {
            continue;
        }

        Actor* targetPlayer = findNearestPlayerInRange(players, actor->getPosition(), aggroRangeSq);
        if (!targetPlayer) {
            continue;
        }
        actor->setTarget(targetPlayer);
    }
}

void onLevelTick(Level& level) {
    processDragonSummon(level);
    processEndIslandEndermanAggro(level);
}
} // namespace

void enableEnderDragonSummonAggro() {
    if (levelTickListener) {
        return;
    }

    levelTickListener = ll::event::EventBus::getInstance().emplaceListener<ll::event::LevelTickEvent>(
        [](ll::event::LevelTickEvent&) {
            ll::service::getLevel().transform([&](Level& level) {
                onLevelTick(level);
                return true;
            });
        }
    );
}

void disableEnderDragonSummonAggro() {
    if (!levelTickListener) {
        return;
    }

    ll::event::EventBus::getInstance().removeListener(levelTickListener);
    levelTickListener.reset();
    dragonSummonTickCounter  = 0;
    endermanAggroTickCounter = 0;
}

} // namespace my_mod::event
