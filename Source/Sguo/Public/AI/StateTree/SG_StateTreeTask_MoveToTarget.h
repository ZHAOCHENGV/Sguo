/**
 * @file SG_StateTreeTask_MoveToTarget.h
 * @brief StateTree任务：移动到目标
 * @details
 * 功能说明：
 * - 使用导航系统移动到目标Actor
 * - 支持配置接受半径
 */

#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "SG_StateTreeTask_MoveToTarget.generated.h"

/**
 * @brief MoveToTarget任务实例数据
 */
USTRUCT()
struct SGUO_API FSG_StateTreeTask_MoveToTargetInstanceData
{
	GENERATED_BODY()

	/** 目标Actor */
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<AActor> TargetActor = nullptr;

	/** 接受半径（到达此距离即认为成功） */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float AcceptanceRadius = 150.0f;

	/** 是否使用攻击范围作为接受半径 */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bUseAttackRangeAsAcceptance = true;
};

/**
 * @brief StateTree任务：移动到目标
 */
USTRUCT(meta = (DisplayName = "Move To Target", Category = "AI|Movement"))
struct SGUO_API FSG_StateTreeTask_MoveToTarget : public FStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FSG_StateTreeTask_MoveToTargetInstanceData;

	FSG_StateTreeTask_MoveToTarget() = default;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
