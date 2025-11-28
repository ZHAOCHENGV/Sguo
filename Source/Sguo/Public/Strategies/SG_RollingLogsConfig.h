// Copyright (C) 2024 Sguo Project. All Rights Reserved.

#pragma once

/**
 * ============================================================================
 * 流木计 (Rolling Logs) 功能配置与集成指南
 * ============================================================================
 * 
 * @file SG_RollingLogsConfig.h
 * @brief 流木计功能的配置常量和蓝图设置指南
 * 
 * @details 功能说明:
 *          本文件提供流木计功能的配置常量定义和完整的蓝图设置指南。
 *          包含：
 *          - 默认参数常量
 *          - GameplayTag定义指南
 *          - GameplayEffect蓝图设置步骤
 *          - DataAsset配置指南
 * 
 * ============================================================================
 * 一、GameplayTag 配置
 * ============================================================================
 * 
 * 需要在项目的 GameplayTags 配置中添加以下标签：
 * 
 * 1. 策略效果标签：
 *    - Strategy.Effect.RollingLogs      (流木计效果标识)
 * 
 * 2. 伤害相关标签（如果尚未存在）：
 *    - Data.Damage                      (伤害倍率SetByCaller标签)
 *    - Data.Knockback                   (击退距离SetByCaller标签)
 * 
 * 配置路径：Project Settings > Project > GameplayTags
 * 
 * ============================================================================
 * 二、GameplayEffect 蓝图配置
 * ============================================================================
 * 
 * [A] GE_RollingLog_Damage - 流木计伤害效果
 * ----------------------------------------
 * 
 * 1. 创建蓝图：
 *    - 路径建议：/Content/Blueprints/AbilitySystem/GameplayEffects/Strategy/
 *    - 父类：GameplayEffect
 *    - 命名：GE_RollingLog_Damage
 * 
 * 2. 基础配置：
 *    - Duration Policy: Instant（瞬发）
 *    - Period: 0（无周期）
 * 
 * 3. Executions 配置：
 *    - 添加 Execution: USG_DamageExecutionCalc
 *    - 这会使用项目现有的伤害计算系统
 * 
 * 4. 伤害倍率配置（通过SetByCaller）：
 *    - 在代码中通过 SetSetByCallerMagnitude 设置
 *    - Tag: Data.Damage
 *    - 木桩类中的 DamageMultiplier 会传递到这里
 * 
 * 5. GameplayCue 配置（可选）：
 *    - 添加 GameplayCue: GameplayCue.Combat.Hit.Wood
 *    - 用于播放木桩击中效果
 * 
 * ----------------------------------------
 * 
 * [B] GE_RollingLog_Knockback - 流木计击退效果（可选）
 * ----------------------------------------
 * 
 * 如果需要通过GAS实现击退（而非代码中的Launch），可配置此GE：
 * 
 * 1. 创建蓝图：
 *    - 父类：GameplayEffect
 *    - 命名：GE_RollingLog_Knockback
 * 
 * 2. 基础配置：
 *    - Duration Policy: HasDuration
 *    - Duration Magnitude: 0.3（击退持续时间）
 * 
 * 3. Modifiers 配置：
 *    - Attribute: 无（仅用于触发GameplayCue）
 *    
 * 4. 或者使用自定义 Execution 实现击退逻辑
 * 
 * ============================================================================
 * 三、Data Asset 配置
 * ============================================================================
 * 
 * [A] 创建流木计策略卡数据资产
 * ----------------------------------------
 * 
 * 1. 创建蓝图：
 *    - 路径建议：/Content/Data/Cards/Strategy/
 *    - 父类：USG_StrategyCardData
 *    - 命名：DA_Card_RollingLogs
 * 
 * 2. 基础信息配置：
 *    - CardName: "流木计"
 *    - CardDescription: "发动后6秒内场上持续出现滚动木桩，击中敌人造成伤害并击退"
 *    - CardIcon: [选择流木计图标]
 *    - CardTypeTag: Card.Type.Strategy
 *    - CardRarityTag: Card.Rarity.Rare
 * 
 * 3. 策略效果配置：
 *    - StrategyEffectTag: Strategy.Effect.RollingLogs
 *    - TargetType: NoTarget（无需选择目标）
 *    - Duration: 6.0
 *    - EffectActorClass: BP_RollingLogsEffect（见下方）
 *    - GameplayEffectClass: 可留空（伤害在木桩类中配置）
 * 
 * ----------------------------------------
 * 
 * [B] 创建流木计效果Actor蓝图
 * ----------------------------------------
 * 
 * 1. 创建蓝图：
 *    - 路径建议：/Content/Blueprints/Strategies/
 *    - 父类：ASG_RollingLogsEffect
 *    - 命名：BP_RollingLogsEffect
 * 
 * 2. 木桩生成配置：
 *    - RollingLogClass: BP_RollingLog（见下方）
 *    - SpawnInterval: 0.5（每0.5秒生成）
 *    - InitialDelay: 0.0（立即开始）
 *    - LogsPerSpawn: 1（每次生成1个）
 *    - MaxSimultaneousLogs: 20（最多20个同时存在）
 * 
 * 3. 生成区域配置：
 *    - SpawnAreaHalfWidth: 1500.0（战场半宽）
 *    - SpawnOffsetFromMainCity: 300.0（距主城偏移）
 *    - SpawnHeightOffset: 50.0（高度偏移）
 *    - SpawnYJitter: 100.0（Y轴随机抖动）
 * 
 * 4. 伤害配置：
 *    - DamageEffectClass: GE_RollingLog_Damage
 *    - KnockbackEffectClass: 可选（如使用代码击退则留空）
 * 
 * 5. 特效音效配置：
 *    - EffectStartVFX: [选择开始特效]
 *    - EffectEndVFX: [选择结束特效]
 *    - SpawnVFX: [选择生成特效]
 *    - EffectStartSound: [选择开始音效]
 *    - EffectEndSound: [选择结束音效]
 * 
 * ----------------------------------------
 * 
 * [C] 创建木桩Actor蓝图
 * ----------------------------------------
 * 
 * 1. 创建蓝图：
 *    - 路径建议：/Content/Blueprints/Strategies/
 *    - 父类：ASG_RollingLog
 *    - 命名：BP_RollingLog
 * 
 * 2. 运动配置：
 *    - RollSpeed: 800.0（滚动速度）
 *    - RotationSpeed: 360.0（旋转速度）
 *    - MaxLifeTime: 10.0（最大存活时间）
 *    - DestroyBeyondDistance: 500.0（超界销毁距离）
 * 
 * 3. 碰撞配置：
 *    - CollisionRadius: 80.0（碰撞半径）
 * 
 * 4. 伤害配置：
 *    - DamageMultiplier: 1.0（伤害倍率）
 *    - KnockbackDistance: 200.0（击退距离）
 *    - KnockbackDuration: 0.3（击退时间）
 * 
 * 5. 网格体配置：
 *    - LogMesh > Static Mesh: [选择木桩模型]
 *    - LogMesh > Materials: [选择木桩材质]
 * 
 * 6. 特效音效配置：
 *    - HitEffect: [选择击中特效]
 *    - DestroyEffect: [选择破碎特效]
 *    - TrailEffect: [选择拖尾特效]
 *    - SpawnSound: [选择生成音效]
 *    - RollSound: [选择滚动循环音效]
 *    - HitSound: [选择击中音效]
 *    - DestroySound: [选择破碎音效]
 * 
 * ============================================================================
 * 四、调试与测试
 * ============================================================================
 * 
 * 1. 开启调试模式：
 *    - BP_RollingLogsEffect > bShowDebug = true
 *    - BP_RollingLog > bShowDebug = true
 * 
 * 2. 调试信息内容：
 *    - 生成区域边界线
 *    - 木桩移动方向箭头
 *    - 碰撞球体可视化
 *    - 主城位置标记
 * 
 * 3. 日志输出：
 *    - 搜索 "[RollingLog]" 查看木桩相关日志
 *    - 搜索 "[RollingLogsEffect]" 查看效果相关日志
 * 
 * 4. 测试检查清单：
 *    □ 木桩是否从己方主城方向生成
 *    □ 木桩是否朝敌方主城方向移动
 *    □ 木桩生成位置是否在Y轴范围内随机
 *    □ 木桩是否正确检测敌方单位
 *    □ 击中后是否造成伤害
 *    □ 击中后是否有击退效果
 *    □ 木桩击中后是否破碎
 *    □ 效果是否在6秒后正确结束
 *    □ 特效和音效是否正常播放
 * 
 * ============================================================================
 * 五、性能优化建议
 * ============================================================================
 * 
 * 1. 木桩数量控制：
 *    - MaxSimultaneousLogs 不建议超过30
 *    - SpawnInterval 不建议低于0.3秒
 * 
 * 2. 碰撞优化：
 *    - 使用球体碰撞而非复杂网格碰撞
 *    - 碰撞检测使用Overlap而非Hit
 * 
 * 3. 特效优化：
 *    - 使用Niagara而非Cascade粒子
 *    - 考虑使用特效池
 * 
 * 4. 内存管理：
 *    - 木桩使用弱引用存储
 *    - 定期清理无效引用
 * 
 * ============================================================================
 */

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

/**
 * @namespace SG_RollingLogs
 * @brief 流木计功能的默认配置常量
 */
namespace SG_RollingLogs
{
	/** 默认效果持续时间（秒） */
	constexpr float DefaultEffectDuration = 6.0f;
	
	/** 默认木桩生成间隔（秒） */
	constexpr float DefaultSpawnInterval = 0.5f;
	
	/** 默认木桩滚动速度（厘米/秒） */
	constexpr float DefaultRollSpeed = 800.0f;
	
	/** 默认木桩旋转速度（度/秒） */
	constexpr float DefaultRotationSpeed = 360.0f;
	
	/** 默认碰撞半径（厘米） */
	constexpr float DefaultCollisionRadius = 80.0f;
	
	/** 默认伤害倍率 */
	constexpr float DefaultDamageMultiplier = 1.0f;
	
	/** 默认击退距离（厘米） */
	constexpr float DefaultKnockbackDistance = 200.0f;
	
	/** 默认击退持续时间（秒） */
	constexpr float DefaultKnockbackDuration = 0.3f;
	
	/** 默认生成区域Y轴半宽（厘米） */
	constexpr float DefaultSpawnAreaHalfWidth = 1500.0f;
	
	/** 默认距主城生成偏移（厘米） */
	constexpr float DefaultSpawnOffsetFromMainCity = 300.0f;
	
	/** 默认最大同时存在木桩数量 */
	constexpr int32 DefaultMaxSimultaneousLogs = 20;
	
	/** 伤害SetByCaller标签名称 */
	const FName DamageTagName = TEXT("Data.Damage");
	
	/** 击退SetByCaller标签名称 */
	const FName KnockbackTagName = TEXT("Data.Knockback");
}
