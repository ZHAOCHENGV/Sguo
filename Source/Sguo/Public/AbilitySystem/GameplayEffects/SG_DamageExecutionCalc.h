// ✨ 新增 - 伤害计算执行类
// Copyright notice placeholder
/**
 * @file SG_DamageExecutionCalc.h
 * @brief 伤害计算执行类
 * @details
 * 功能说明：
 * - 使用 GAS 的 Execution Calculation 系统计算伤害
 * - 支持基础伤害、伤害倍率、护甲减伤等
 * - 将最终伤害应用到 IncomingDamage 属性
 * 详细流程：
 * 1. 从 Source（攻击者）读取 AttackDamage 属性
 * 2. 从 EffectSpec 读取伤害倍率（SetByCaller）
 * 3. 计算最终伤害
 * 4. 应用到 Target（被攻击者）的 IncomingDamage 属性
 * 注意事项：
 * - IncomingDamage 在 PostGameplayEffectExecute 中会转换为实际生命值扣除
 * - 伤害倍率通过 SetByCaller 传递，使用 GameplayTag "Data.Damage"
 */

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "SG_DamageExecutionCalc.generated.h"

/**
 * @brief 伤害计算执行类
 * @details
 * 功能说明：
 * - 计算最终伤害值
 * - 支持伤害倍率和减伤计算
 * 使用方式：
 * 1. 在 GameplayEffect 蓝图中选择此类作为 Execution
 * 2. 使用 SetByCaller 设置伤害倍率（Tag: Data.Damage）
 * 3. GE 应用时会自动调用 Execute_Implementation
 */
UCLASS()
class SGUO_API USG_DamageExecutionCalc : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
	/**
	 * @brief 构造函数
	 * @details
	 * 功能说明：
	 * - 声明需要捕获的属性（Source 和 Target）
	 * - Source：攻击者的 AttackDamage
	 * - Target：被攻击者的 IncomingDamage
	 */
	USG_DamageExecutionCalc();

	/**
	 * @brief 执行伤害计算
	 * @param ExecutionParams 执行参数（包含 Source 和 Target）
	 * @param OutExecutionOutput 输出参数（修改 Target 的属性）
	 * @details
	 * 功能说明：
	 * - 从 Source 读取攻击力
	 * - 从 EffectSpec 读取伤害倍率
	 * - 计算最终伤害
	 * - 应用到 Target 的 IncomingDamage
	 * 详细流程：
	 * 1. 获取攻击者的 AttackDamage 属性值
	 * 2. 从 SetByCaller 读取伤害倍率（默认 1.0）
	 * 3. 计算：最终伤害 = AttackDamage * 伤害倍率
	 * 4. 应用到 Target 的 IncomingDamage 属性
	 * 注意事项：
	 * - 伤害倍率使用 GameplayTag "Data.Damage"
	 * - 如果未设置倍率，默认为 1.0（100%伤害）
	 */
	virtual void Execute_Implementation(
		const FGameplayEffectCustomExecutionParameters& ExecutionParams,
		OUT FGameplayEffectCustomExecutionOutput& OutExecutionOutput
	) const override;
};
