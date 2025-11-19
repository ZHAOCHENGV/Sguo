/**
 * @file SG_StateTreeTask_MoveToTarget.cpp
 * @brief StateTree任务：移动到目标 实现
 */

#include "AI/StateTree/SG_StateTreeTask_MoveToTarget.h"
#include "AI/SG_AIControllerBase.h"
#include "Units/SG_UnitsBase.h"
#include "StateTreeExecutionContext.h"
#include "Debug/SG_LogCategories.h"

EStateTreeRunStatus FSG_StateTreeTask_MoveToTarget::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// 获取AI Controller
	AAIController* AIController = Cast<AAIController>(Context.GetOwner());
	if (!AIController)
	{
		UE_LOG(LogSGGameplay, Warning, TEXT("❌ MoveToTarget: 无法获取AI Controller"));
		return EStateTreeRunStatus::Failed;
	}

	ASG_AIControllerBase* SGAIController = Cast<ASG_AIControllerBase>(AIController);
	if (!SGAIController)
	{
		UE_LOG(LogSGGameplay, Warning, TEXT("❌ MoveToTarget: 不是SG_AIControllerBase"));
		return EStateTreeRunStatus::Failed;
	}

	// 检查目标是否有效
	if (!InstanceData.TargetActor)
	{
		// 尝试从AI Controller获取当前目标
		InstanceData.TargetActor = SGAIController->GetCurrentTarget();
		
		if (!InstanceData.TargetActor)
		{
			UE_LOG(LogSGGameplay, Warning, TEXT("❌ MoveToTarget: 目标为空"));
			return EStateTreeRunStatus::Failed;
		}
	}

	// 确定接受半径
	float AcceptanceRadius = InstanceData.AcceptanceRadius;
	
	if (InstanceData.bUseAttackRangeAsAcceptance)
	{
		if (ASG_UnitsBase* Unit = Cast<ASG_UnitsBase>(AIController->GetPawn()))
		{
			AcceptanceRadius = Unit->BaseAttackRange * 0.9f; // 稍微小一点，确保在攻击范围内
		}
	}

	// 开始移动
	bool bSuccess = SGAIController->MoveToTargetActor(InstanceData.TargetActor, AcceptanceRadius);
	
	if (bSuccess)
	{
		UE_LOG(LogSGGameplay, Log, TEXT("✅ 开始移动到目标：%s，接受半径：%.1f"), 
			*InstanceData.TargetActor->GetName(), AcceptanceRadius);
		return EStateTreeRunStatus::Running;
	}
	else
	{
		UE_LOG(LogSGGameplay, Warning, TEXT("❌ 移动失败"));
		return EStateTreeRunStatus::Failed;
	}
}

EStateTreeRunStatus FSG_StateTreeTask_MoveToTarget::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// 获取AI Controller
	AAIController* AIController = Cast<AAIController>(Context.GetOwner());
	if (!AIController)
	{
		return EStateTreeRunStatus::Failed;
	}

	ASG_AIControllerBase* SGAIController = Cast<ASG_AIControllerBase>(AIController);
	if (!SGAIController)
	{
		return EStateTreeRunStatus::Failed;
	}

	// 检查目标是否仍然有效
	if (!SGAIController->IsTargetValid())
	{
		UE_LOG(LogSGGameplay, Log, TEXT("❌ 目标无效，停止移动"));
		return EStateTreeRunStatus::Failed;
	}

	// 确定接受半径
	float AcceptanceRadius = InstanceData.AcceptanceRadius;
	
	if (InstanceData.bUseAttackRangeAsAcceptance)
	{
		if (ASG_UnitsBase* Unit = Cast<ASG_UnitsBase>(AIController->GetPawn()))
		{
			AcceptanceRadius = Unit->BaseAttackRange * 0.9f;
		}
	}

	// 检查是否到达目标
	if (SGAIController->IsInAttackRange(InstanceData.TargetActor, AcceptanceRadius))
	{
		UE_LOG(LogSGGameplay, Log, TEXT("✅ 已到达目标"));
		return EStateTreeRunStatus::Succeeded;
	}

	return EStateTreeRunStatus::Running;
}

void FSG_StateTreeTask_MoveToTarget::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	// 获取AI Controller
	AAIController* AIController = Cast<AAIController>(Context.GetOwner());
	if (!AIController)
	{
		return;
	}

	ASG_AIControllerBase* SGAIController = Cast<ASG_AIControllerBase>(AIController);
	if (SGAIController)
	{
		// 停止移动
		SGAIController->StopMovement();
		UE_LOG(LogSGGameplay, Log, TEXT("🛑 停止移动任务"));
	}
}
