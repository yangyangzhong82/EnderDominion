#pragma once

#include "ll/api/io/LogLevel.h"
#include <string>
#include <vector>

namespace my_mod {

struct Config {
    // 配置版本号（用于将来兼容升级）
    int version = 1;
    // 日志级别
    ll::io::LogLevel logLevel = ll::io::LogLevel::Info;
    // 语言（对应 lang 目录内文件名，如 zh_CN / en_US）
    std::string language = "zh_CN";

    // 末影龙最大生命值
    float enderDragonMaxHealth = 200.0F;

    // 末影龙反伤开关
    bool enderDragonReflectEnabled = true;
    // 反伤比例（按末影龙受到的伤害比例反弹给玩家）
    float enderDragonReflectRatio = 0.25F;
    // 末影龙爆炸减伤开关（方块爆炸/实体爆炸）
    bool enderDragonExplosionDamageReductionEnabled = true;
    // 末影龙爆炸减伤比例（0.0 ~ 1.0，0.6 表示减少 60% 爆炸伤害）
    float enderDragonExplosionDamageReductionRatio = 0.6F;

    // 末影龙自然回血开关
    bool enderDragonNaturalRegenEnabled = true;
    // 每次回血量
    int enderDragonRegenAmount = 1;
    // 回血间隔（tick）
    int enderDragonRegenIntervalTicks = 60;

    // 末影龙锁定玩家（DragonStrafePlayerGoal）时附加 debuff 开关
    bool enderDragonAttackDebuffEnabled = true;
    // 附加 debuff 概率（0.0 ~ 1.0）
    float enderDragonAttackDebuffChance = 0.35F;
    // debuff 持续时间（tick）
    int enderDragonAttackDebuffTicks = 120;
    // debuff 等级（0 表示 I，1 表示 II）
    int enderDragonAttackDebuffLevel = 0;

    // 定时给末影龙附近最近玩家附加 debuff 开关
    bool enderDragonPeriodicDebuffEnabled = true;
    // 定时附加 debuff 的间隔（tick）
    int enderDragonPeriodicDebuffIntervalTicks = 100;
    // 定时附加 debuff 的搜索范围（格）
    float enderDragonPeriodicDebuffRange = 20.0F;

    // 龙焰阶段（DragonFlamingGoal start/stop）附近爆炸开关
    bool enderDragonFlamingExplosionEnabled = true;
    // 爆炸影响范围（用于筛选要被标记的玩家，单位：格）
    float enderDragonFlamingExplosionRange = 30.0F;
    // 警告后到爆炸的延迟（tick）
    int enderDragonFlamingExplosionDelayTicks = 40;
    // 爆炸威力（半径）
    float enderDragonFlamingExplosionPower = 3.0F;
    // 爆炸是否破坏方块
    bool enderDragonFlamingExplosionBreakBlocks = false;
    // 爆炸是否点燃火焰
    bool enderDragonFlamingExplosionFire = false;

    // 末影龙定时召唤怪物攻击玩家开关
    bool enderDragonSummonMobEnabled = true;
    // 召唤间隔（tick）
    int enderDragonSummonIntervalTicks = 120;
    // 每次召唤数量
    int enderDragonSummonCountPerWave = 2;
    // 末影龙搜索玩家范围（格）
    float enderDragonSummonPlayerRange = 40.0F;
    // 召唤怪物列表（支持完整标识符，如 minecraft:enderman）
    std::vector<std::string> enderDragonSummonMobTypes{"minecraft:enderman"};

    // 末地主岛末影人强制仇恨玩家开关
    bool endIslandEndermanAggroEnabled = true;
    // 末影人仇恨刷新间隔（tick）
    int endIslandEndermanAggroIntervalTicks = 20;
    // 末影人索敌范围（格）
    float endIslandEndermanAggroRange = 48.0F;
    // 主岛判定半径（以世界原点 XZ 为中心，单位：格）
    float endIslandRadius = 256.0F;

    // 可选 debuff 列表（随机等概率抽取，仅生效负面效果）
    // 支持常见名字与别名，例如: slowness, weakness, poison, wither, blindness
    std::vector<std::string> enderDragonAttackDebuffTypes{"slowness", "weakness", "poison", "wither", "blindness"};
};

} // namespace my_mod
