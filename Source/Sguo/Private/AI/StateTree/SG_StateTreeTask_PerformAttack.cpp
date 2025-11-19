/**
 * @file SG_StateTreeTask_PerformAttack.cpp
 * @brief StateTree任务：执行攻击 实现
 */

#include "AI/StateTree/SG_StateTreeTask_PerformAttack.h"
#include "AI/SG_AIControllerBase.h"
#include "StateTreeExecutionContext.h"
#include "Debug/SG_LogCategories.h"

EStateTreeRunStatus FSG_StateTreeTask_PerformAttack::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	
	// 重置上次攻击时间
	InstanceData.LastAttackTime = 0.0f;
	
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FSG_StateTreeTask_PerformAttack::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// 获取AI Controller
	AAIController* AIController = Cast<AAIController>(Context.GetOwner());
	if (!AIController)
	{
		UE_LOG(LogSGGameplay, Warning, TEXT("❌ PerformAttack: 无法获取AI Controller"));
		return EStateTreeRunStatus::Failed;
	}

	ASG_AIControllerBase* SGAIController = Cast<ASG_AIControllerBase>(AIController);
	if (!SGAIController)
	{
		UE_LOG(LogSGGameplay, Warning, TEXT("❌ PerformAttack: 不是SG_AIControllerBase"));
		return EStateTreeRunStatus::Failed;
	}

	// 检查目标是否有效
	if (!SGAIController->IsTargetValid())
	{
		UE_LOG(LogSGGameplay, Log, TEXT("❌ PerformAttack: 目标无效"));
		return EStateTreeRunStatus::Failed;
	}

	// 获取当前目标
	AActor* Target = SGAIController->GetCurrentTarget();
	if (!Target)
	{
		return EStateTreeRunStatus::Failed;
	}

	// 检查是否在攻击范围内
	if (!SGAIController->IsInAttackRange(Target))
	{
		UE_LOG(LogSGGameplay, Log, TEXT("❌ PerformAttack: 目标不在攻击范围内"));
		return EStateTreeRunStatus::Failed;
	}

	// 检查攻击冷却
	float CurrentTime = Context.GetWorld()->GetTimeSeconds();
	if (CurrentTime - InstanceData.LastAttackTime < InstanceData.AttackInterval)
	{
		// 仍在冷却中，继续等待
		return EStateTreeRunStatus::Running;
	}

	// 面向目标（如果设置）
	if (InstanceData.bFaceTargetBeforeAttack)
	{
		SGAIController->FaceTarget(Target);
	}

	// 执行攻击
	bool bSuccess = SGAIController->PerformAttack();
	
	if (bSuccess)
	{
		// 更新上次攻击时间
		InstanceData.LastAttackTime = CurrentTime;
		UE_LOG(LogSGGameplay, Log, TEXT("⚔️ 执行攻击成功"));
		return EStateTreeRunStatus::Running; // 继续攻击状态
	}
	else
	{
		UE_LOG(LogSGGameplay, Warning, TEXT("❌ 执行攻击失败"));
		return EStateTreeRunStatus::Failed;
	}
}
