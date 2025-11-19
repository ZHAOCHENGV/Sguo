/**
 * @file SG_StateTreeTask_PerformAttack.h
 * @brief StateTree任务：执行攻击
 * @details
 * 功能说明：
 * - 触发单位的GAS攻击能力
 * - 自动面向目标
 * - 处理攻击冷却
 */

#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "SG_StateTreeTask_PerformAttack.generated.h"

/**
 * @brief PerformAttack任务实例数据
 */
USTRUCT()
struct SGUO_API FSG_StateTreeTask_PerformAttackInstanceData
{
	GENERATED_BODY()

	/** 是否在攻击前面向目标 */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bFaceTargetBeforeAttack = true;

	/** 攻击间隔时间（秒） */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float AttackInterval = 1.0f;

	/** 上次攻击时间 */
	float LastAttackTime = 0.0f;
};

/**
 * @brief StateTree任务：执行攻击
 */
USTRUCT()
struct SGUO_API FSG_StateTreeTask_PerformAttack : public FStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FSG_StateTreeTask_PerformAttackInstanceData;

	FSG_StateTreeTask_PerformAttack() = default;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};
