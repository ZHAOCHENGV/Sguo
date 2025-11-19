/**
 * @file SG_StateTreeTask_FindTarget.h
 * @brief StateTree任务：查找目标
 * @details
 * 功能说明：
 * - 查找最近的敌人或敌方主城
 * - 将找到的目标保存到AI Controller
 */

#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "SG_StateTreeTask_FindTarget.generated.h"

/**
 * @brief FindTarget任务实例数据
 */
USTRUCT()
struct SGUO_API FSG_StateTreeTask_FindTargetInstanceData
{
	GENERATED_BODY()

	/** 搜索半径 */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float SearchRadius = 2000.0f;

	/** 是否优先查找主城 */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bPrioritizeMainCity = false;

	/** 找到的目标（输出） */
	UPROPERTY(EditAnywhere, Category = "Output")
	TObjectPtr<AActor> FoundTarget = nullptr;
};

/**
 * @brief StateTree任务：查找目标
 */
USTRUCT(meta = (DisplayName = "Find Target", Category = "AI|Combat"))
struct SGUO_API FSG_StateTreeTask_FindTarget : public FStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FSG_StateTreeTask_FindTargetInstanceData;

	FSG_StateTreeTask_FindTarget() = default;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};
