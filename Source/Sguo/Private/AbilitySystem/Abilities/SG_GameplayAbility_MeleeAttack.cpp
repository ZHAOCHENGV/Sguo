// ✨ 新增 - 近战攻击能力实现
// Copyright notice placeholder

#include "AbilitySystem/Abilities/SG_GameplayAbility_MeleeAttack.h"
#include "Debug/SG_LogCategories.h"

/**
 * @brief 构造函数
 * @details
 * 功能说明：
 * - 设置近战攻击的默认配置
 * - 攻击类型设为 Melee
 * - 设置默认的伤害倍率
 * 注意事项：
 * - 这些值可以在 Blueprint 中覆盖
 */
USG_GameplayAbility_MeleeAttack::USG_GameplayAbility_MeleeAttack()
{
	// ========== 设置攻击类型 ==========
	// 设置为近战攻击
	AttackType = ESGAttackAbilityType::Melee;
	
	// ========== 设置默认伤害倍率 ==========
	// 100% 伤害（可以在 Blueprint 中修改）
	DamageMultiplier = 1.0f;
	
	// ========== 设置默认近战配置 ==========
	// 默认单体攻击
	MaxTargets = 1;
	// 默认前方 180 度攻击范围
	AttackAngle = 180.0f;
	// 默认启用扇形检测
	bUseConeDetection = true;
	
	// 输出日志
	UE_LOG(LogSGGameplay, Log, TEXT("✓ 近战攻击能力构造完成"));
}
