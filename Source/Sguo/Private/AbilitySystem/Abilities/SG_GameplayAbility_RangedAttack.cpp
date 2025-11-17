// ✨ 新增 - 远程攻击能力实现
// Copyright notice placeholder

#include "AbilitySystem/Abilities/SG_GameplayAbility_RangedAttack.h"
#include "Debug/SG_LogCategories.h"

/**
 * @brief 构造函数
 * @details
 * 功能说明：
 * - 设置远程攻击的默认配置
 * - 攻击类型设为 Ranged
 * - 设置默认的伤害倍率和投射物配置
 * 注意事项：
 * - 这些值可以在 Blueprint 中覆盖
 */
USG_GameplayAbility_RangedAttack::USG_GameplayAbility_RangedAttack()
{
	// ========== 设置攻击类型 ==========
	// 设置为远程攻击
	AttackType = ESGAttackAbilityType::Ranged;
	
	// ========== 设置默认伤害倍率 ==========
	// 100% 伤害（可以在 Blueprint 中修改）
	DamageMultiplier = 1.0f;
	
	// ========== 设置默认远程配置 ==========
	// 投射物生成偏移（前方 50cm，上方 80cm）
	ProjectileSpawnOffset = FVector(50.0f, 0.0f, 80.0f);
	// 目标预判系数（50% 预判）
	LeadTargetFactor = 0.5f;
	// 瞄准身体中心
	bAimAtCenter = true;
	// 单发投射物
	ProjectileCount = 1;
	// 连射间隔 0.1 秒
	ProjectileInterval = 0.1f;
	
	// 输出日志
	UE_LOG(LogSGGameplay, Log, TEXT("✓ 远程攻击能力构造完成"));
}
