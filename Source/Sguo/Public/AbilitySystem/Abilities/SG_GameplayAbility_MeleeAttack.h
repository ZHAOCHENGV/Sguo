// ✨ 新增 - 近战攻击能力类
// Copyright notice placeholder
/**
 * @file SG_GameplayAbility_MeleeAttack.h
 * @brief 近战攻击能力（步兵、骑兵专用）
 * @details
 * 功能说明：
 * - 继承自 USG_GameplayAbility_Attack
 * - 专门用于近战单位（步兵、骑兵）
 * - 使用球形范围检测查找目标
 * - 支持攻击动画和判定
 * 使用方式：
 * 1. 创建 Blueprint 继承此类
 * 2. 配置攻击动画蒙太奇
 * 3. 配置伤害 GameplayEffect
 * 4. 在单位中授予此能力
 * 注意事项：
 * - 攻击类型已设置为 Melee
 * - 攻击范围使用单位的 BaseAttackRange
 */

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/SG_GameplayAbility_Attack.h"
#include "SG_GameplayAbility_MeleeAttack.generated.h"

/**
 * @brief 近战攻击能力类
 * @details
 * 功能说明：
 * - 近战攻击能力的具体实现
 * - 自动设置攻击类型为 Melee
 * - 提供近战攻击的默认配置
 * 使用示例：
 * 在 Blueprint 中创建子类（GA_Attack_Infantry）：
 * - Damage Multiplier: 1.0（100% 伤害）
 * - Attack Montage: AM_Infantry_Attack
 * - Damage Effect Class: GE_Damage_Base
 */
UCLASS()
class SGUO_API USG_GameplayAbility_MeleeAttack : public USG_GameplayAbility_Attack
{
	GENERATED_BODY()

public:
	/**
	 * @brief 构造函数
	 * @details
	 * 功能说明：
	 * - 设置默认配置
	 * - 攻击类型设为 Melee
	 */
	USG_GameplayAbility_MeleeAttack();

protected:
	// ========== 蓝图可配置参数 ==========

	/**
	 * @brief 近战攻击最大目标数
	 * @details
	 * 功能说明：
	 * - 一次近战攻击最多可以命中的目标数量
	 * - 适用于范围武器（如长戟、大刀）
	 * 参考值：
	 * - 单体攻击：1
	 * - 小范围攻击：3
	 * - 大范围攻击：5
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Melee Config", meta = (DisplayName = "最大目标数", ClampMin = "1", UIMin = "1", UIMax = "10"))
	int32 MaxTargets = 1;

	/**
	 * @brief 攻击扇形角度（度）
	 * @details
	 * 功能说明：
	 * - 近战攻击的扇形范围
	 * - 0 = 全方向，180 = 前方半圆，90 = 前方扇形
	 * 参考值：
	 * - 直刺武器：90 度
	 * - 横扫武器：180 度
	 * - 旋转攻击：360 度
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Melee Config", meta = (DisplayName = "攻击扇形角度", ClampMin = "0.0", UIMin = "0.0", UIMax = "360.0"))
	float AttackAngle = 180.0f;

	/**
	 * @brief 是否启用扇形检测
	 * @details
	 * 功能说明：
	 * - 如果为 false，使用球形检测（全方向）
	 * - 如果为 true，使用扇形检测（限制角度）
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Melee Config", meta = (DisplayName = "启用扇形检测"))
	bool bUseConeDetection = true;
};
