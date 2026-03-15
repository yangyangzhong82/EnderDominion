#pragma once

#include "ll/api/io/LogLevel.h"
#include <string>
#include <vector>

namespace my_mod {

struct EnderDragonReflectLowHealthConfig {
    bool enabled = true;
    float threshold = 80.0F;
    float ratio = 0.5F;
};

struct EnderDragonKillRewardConfig {
    bool enabled = true;
    double totalMoney = 100.0;
    std::string currencyType = "money";
};

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
    // 末影龙低血量增强反伤配置
    EnderDragonReflectLowHealthConfig enderDragonReflectLowHealth{};
    // 末影龙爆炸减伤开关（方块爆炸/实体爆炸）
    bool enderDragonExplosionDamageReductionEnabled = true;
    // 末影龙爆炸减伤比例（0.0 ~ 1.0，0.6 表示减少 60% 爆炸伤害）
    float enderDragonExplosionDamageReductionRatio = 0.6F;
    // 末影龙高额伤害减免开关（仅对超过阈值的部分生效）
    bool enderDragonHighDamageReductionEnabled = true;
    // 高额伤害阈值（伤害值 <= 阈值时不触发此减免）
    float enderDragonHighDamageReductionThreshold = 10.0F;
    // 超出阈值部分的减免比例（0.0 ~ 1.0，0.5 表示超出部分减免 50%）
    float enderDragonHighDamageReductionRatio = 0.5F;
    // 末影龙死亡金币奖励配置
    EnderDragonKillRewardConfig enderDragonKillReward{};
    // 末影龙在 DragonStrafePlayerGoal 开始/结束时召唤闪电开关
    bool enderDragonStrafeLightningEnabled = true;
    // 每次开始/结束触发时召唤闪电数量
    int enderDragonStrafeLightningCount = 1;
    // 闪电落点相对目标的随机偏移半径（格）
    float enderDragonStrafeLightningSpread = 2.5F;
    // 回退目标搜索范围（当 Goal 内无有效目标时，按此范围找最近玩家）
    float enderDragonStrafeLightningFallbackRange = 48.0F;
    // 末影龙火球自动追踪玩家开关
    bool enderDragonHomingFireballEnabled = true;
    // 末影龙火球追踪玩家搜索范围（格）
    float enderDragonHomingFireballRange = 72.0F;
    // 末影龙火球每 tick 转向强度（0.0 ~ 1.0）
    float enderDragonHomingFireballTurnStrength = 0.5F;

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
    // 末影龙低血量时增强龙焰爆炸威力开关
    bool enderDragonFlamingExplosionLowHealthBoostEnabled = true;
    // 末影龙血量低于等于该值时触发爆炸威力增强
    float enderDragonFlamingExplosionLowHealthThreshold = 80.0F;
    // 低血量时龙焰爆炸威力倍率（>= 1.0）
    float enderDragonFlamingExplosionLowHealthPowerMultiplier = 1.5F;
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
    // 末影龙召唤怪物的全局存活上限（达到后不再召唤）
    int enderDragonSummonMobMaxAlive = 40;
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

    // 玩家在末地主岛死亡时，末影龙回血开关
    bool endIslandPlayerDeathHealDragonEnabled = true;
    // 每次触发恢复的生命值
    int endIslandPlayerDeathHealDragonAmount = 20;

    // 可选 debuff 列表（随机等概率抽取，仅生效负面效果）
    // 支持常见名字与别名，例如: slowness, weakness, poison, wither, blindness
    std::vector<std::string>
        enderDragonAttackDebuffTypes{"slowness", "weakness", "poison", "wither", "blindness", "hunger"};

    // ─── 末影龙落地冲击波 ───
    // 末影龙降落到喷泉时产生冲击波
    bool  enderDragonLandingShockwaveEnabled           = true;
    // 冲击波影响范围（格）
    float enderDragonLandingShockwaveRange              = 16.0F;
    // 冲击波直接伤害
    float enderDragonLandingShockwaveDamage             = 6.0F;
    // 水平击退强度
    float enderDragonLandingShockwaveKnockbackStrength  = 1.8F;
    // 向上抛飞强度
    float enderDragonLandingShockwaveUpwardStrength     = 0.6F;
    // 缓慢效果持续时间（tick）
    int   enderDragonLandingShockwaveSlownessTicks      = 80;
    // 缓慢效果等级（0 = I, 1 = II）
    int   enderDragonLandingShockwaveSlownessLevel      = 1;

    // ─── 末影龙瞬移冲刺攻击 ───
    // 末影龙定期瞬移到玩家身后发动突袭
    bool  enderDragonTeleportDashEnabled           = true;
    // 冲刺冷却间隔（tick）
    int   enderDragonTeleportDashCooldownTicks     = 200;
    // 警告到执行的延迟（tick）
    int   enderDragonTeleportDashWarningTicks      = 30;
    // 搜索玩家范围（格）
    float enderDragonTeleportDashRange             = 48.0F;
    // 冲刺伤害
    float enderDragonTeleportDashDamage            = 8.0F;
    // 击退强度
    float enderDragonTeleportDashKnockbackStrength = 1.5F;
    // 落点伤害范围（格）
    float enderDragonTeleportDashHitRange          = 6.0F;
    // 传送到玩家身后的距离（格）
    float enderDragonTeleportDashDistance           = 5.0F;

    // ─── 末地水晶增强 ───
    // 水晶被摧毁时产生额外效果
    bool  enderDragonCrystalEnhanceEnabled                = true;
    // 水晶被毁时额外爆炸威力
    float enderDragonCrystalDestroyExplosionPower          = 4.0F;
    // 额外爆炸是否破坏方块
    bool  enderDragonCrystalDestroyExplosionBreakBlocks    = false;
    // 额外爆炸是否点燃火焰
    bool  enderDragonCrystalDestroyExplosionFire           = false;
    // 水晶被毁时召唤守卫怪物开关
    bool  enderDragonCrystalDestroySpawnMobEnabled         = true;
    // 每次召唤怪物数量
    int   enderDragonCrystalDestroySpawnMobCount           = 2;
    // 召唤怪物类型列表
    std::vector<std::string> enderDragonCrystalDestroySpawnMobTypes{"minecraft:enderman"};
    // 水晶被毁时给附近玩家施加负面效果开关
    bool  enderDragonCrystalDestroyDebuffEnabled           = true;
    // 负面效果影响范围（格）
    float enderDragonCrystalDestroyDebuffRange             = 16.0F;
    // 负面效果持续时间（tick）
    int   enderDragonCrystalDestroyDebuffTicks             = 100;
    // 负面效果等级（0 = I, 1 = II）
    int   enderDragonCrystalDestroyDebuffLevel             = 0;
    // 水晶被毁时末影龙愤怒回血量
    int   enderDragonCrystalDestroyDragonHealAmount        = 10;

    // ─── 龙息弹幕 ───
    // 龙焰阶段发射多方向龙息火球弹幕
    bool  enderDragonBreathBarrageEnabled                       = true;
    // 每波发射火球数量
    int   enderDragonBreathBarrageCount                         = 4;
    // 发射间隔（tick）
    int   enderDragonBreathBarrageIntervalTicks                 = 20;
    // 火球速度
    float enderDragonBreathBarrageSpeed                         = 1.0F;
    // 弹幕模式: "fan" 扇形, "spiral" 螺旋, "ring" 环形
    std::string enderDragonBreathBarragePattern                 = "spiral";
    // 扇形展开角度（度，仅 fan 模式）
    float enderDragonBreathBarrageFanAngle                      = 90.0F;
    // 同时存在的龙息火球上限（防止粒子过多卡顿）
    int   enderDragonBreathBarrageMaxAlive                      = 12;
    // 低血量增强
    bool  enderDragonBreathBarrageLowHealthBoostEnabled         = true;
    float enderDragonBreathBarrageLowHealthThreshold            = 80.0F;
    // 低血量时额外增加的火球数量
    int   enderDragonBreathBarrageLowHealthExtraCount           = 2;

    // ─── 龙息云增强 ───
    // 增强龙息云（AreaEffectCloud）的范围、持续时间和效果
    bool  enderDragonBreathEnhanceEnabled                       = true;
    // 龙息云半径倍率
    float enderDragonBreathEnhanceRadiusMultiplier              = 1.5F;
    // 龙息云持续时间倍率
    float enderDragonBreathEnhanceDurationMultiplier            = 1.5F;
    // 龙息云额外效果列表
    std::vector<std::string> enderDragonBreathEnhanceExtraEffects{"wither", "poison"};
    // 额外效果等级（0 = I, 1 = II）
    int   enderDragonBreathEnhanceExtraEffectLevel              = 0;
    // 额外效果持续时间（tick）
    int   enderDragonBreathEnhanceExtraEffectTicks              = 100;
    // 龙息云额外伤害
    float enderDragonBreathEnhanceExtraDamage                   = 2.0F;
    // 低血量增强
    bool  enderDragonBreathEnhanceLowHealthBoostEnabled         = true;
    float enderDragonBreathEnhanceLowHealthThreshold            = 80.0F;
    // 低血量时半径额外倍率
    float enderDragonBreathEnhanceLowHealthRadiusMultiplier     = 1.5F;
};

} // namespace my_mod
