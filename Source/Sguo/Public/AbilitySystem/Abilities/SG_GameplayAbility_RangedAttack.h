// ✨ 新增 - 远程攻击能力类
// Copyright notice placeholder
/**
 * @file SG_GameplayAbility_RangedAttack.h
 * @brief 远程攻击能力（弓兵、弩兵专用）
 * @details
 * 功能说明：
 * - 继承自 USG_GameplayAbility_Attack
 * - 专门用于远程单位（弓兵、弩兵）
 * - 生成投射物进行攻击
 * - 支持抛物线和直线飞行
 * 使用方式：
 * 1. 创建 Blueprint 继承此类
 * 2. 配置攻击动画蒙太奇
 * 3. 配置投射物类（BP_Projectile_Arrow）
 * 4. 在单位中授予此能力
 * 注意事项：
 * - 攻击类型已设置为 Ranged
 * - 需要配置 ProjectileClass
 */

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/SG_GameplayAbility_Attack.h"
#include "SG_GameplayAbility_RangedAttack.generated.h"

/**
 * @brief 远程攻击能力类
 * @details
 * 功能说明：
 * - 远程攻击能力的具体实现
 * - 自动设置攻击类型为 Ranged
 * - 提供远程攻击的默认配置
 * 使用示例：
 * 在 Blueprint 中创建子类（GA_Attack_Archer）：
 * - Damage Multiplier: 1.0（100% 伤害）
 * - Attack Montage: AM_Archer_Attack
 * - Projectile Class: BP_Projectile_Arrow
 * - Damage Effect Class: GE_Damage_Base
 */
UCLASS()
class SGUO_API USG_GameplayAbility_RangedAttack : public USG_GameplayAbility_Attack
{
	GENERATED_BODY()

public:
	/**
	 * @brief 构造函数
	 * @details
	 * 功能说明：
	 * - 设置默认配置
	 * - 攻击类型设为 Ranged
	 */
	USG_GameplayAbility_RangedAttack();

protected:
	// ========== 蓝图可配置参数 ==========

	/**
	 * @brief 投射物生成偏移
	 * @details
	 * 功能说明：
	 * - 投射物生成位置相对于单位的偏移
	 * - 用于调整投射物从弓弩发射的位置
	 * 建议值：
	 * - X: 50（前方）
	 * - Y: 0
	 * - Z: 80（弓的高度）
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ranged Config", meta = (DisplayName = "投射物生成偏移"))
	FVector ProjectileSpawnOffset = FVector(50.0f, 0.0f, 80.0f);

	/**
	 * @brief 目标预判系数
	 * @details
	 * 功能说明：
	 * - 是否预判移动中的目标
	 * - 0.0 = 不预判（瞄准当前位置）
	 * - 1.0 = 完全预判（瞄准预测位置）
	 * - 0.5 = 部分预判
	 * 参考值：
	 * - 新手弓兵：0.3
	 * - 熟练弓兵：0.7
	 * - 神射手：1.0
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ranged Config", meta = (DisplayName = "目标预判系数", ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float LeadTargetFactor = 0.5f;

	/**
	 * @brief 是否瞄准目标身体中心
	 * @details
	 * 功能说明：
	 * - 如果为 true，瞄准目标 Capsule 中心
	 * - 如果为 false，瞄准目标脚底位置
	 * 注意事项：
	 * - 瞄准中心更容易命中
	 * - 瞄准脚底适合抛物线投射物
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ranged Config", meta = (DisplayName = "瞄准身体中心"))
	bool bAimAtCenter = true;

	/**
	 * @brief 投射物数量（连射）
	 * @details
	 * 功能说明：
	 * - 一次攻击发射的投射物数量
	 * - 用于实现连弩、齐射等效果
	 * 参考值：
	 * - 单发：1
	 * - 连弩：3
	 * - 齐射：5
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ranged Config", meta = (DisplayName = "投射物数量", ClampMin = "1", UIMin = "1", UIMax = "10"))
	int32 ProjectileCount = 1;

	/**
	 * @brief 投射物间隔时间（秒）
	 * @details
	 * 功能说明：
	 * - 多个投射物之间的发射间隔
	 * - 仅在 ProjectileCount > 1 时生效
	 * 参考值：
	 * - 快速连射：0.1 秒
	 * - 普通连射：0.2 秒
	 * - 慢速连射：0.5 秒
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ranged Config", meta = (DisplayName = "投射物间隔时间", ClampMin = "0.0", UIMin = "0.0", UIMax = "2.0"))
	float ProjectileInterval = 0.1f;
};
