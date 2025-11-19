/**
 * @file SG_StateTreeTask_FindTarget.cpp
 * @brief StateTree任务：查找目标 实现
 */

#include "AI/StateTree/SG_StateTreeTask_FindTarget.h"
#include "AI/SG_AIControllerBase.h"
#include "StateTreeExecutionContext.h"
#include "Debug/SG_LogCategories.h"

EStateTreeRunStatus FSG_StateTreeTask_FindTarget::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FSG_StateTreeTask_FindTarget::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// 获取AI Controller
	AAIController* AIController = Cast<AAIController>(Context.GetOwner());
	if (!AIController)
	{
		UE_LOG(LogSGGameplay, Warning, TEXT("❌ FindTarget: 无法获取AI Controller"));
		return EStateTreeRunStatus::Failed;
	}

	ASG_AIControllerBase* SGAIController = Cast<ASG_AIControllerBase>(AIController);
	if (!SGAIController)
	{
		UE_LOG(LogSGGameplay, Warning, TEXT("❌ FindTarget: 不是SG_AIControllerBase"));
		return EStateTreeRunStatus::Failed;
	}

	// 优先查找主城（如果设置）
	if (InstanceData.bPrioritizeMainCity)
	{
		AActor* MainCity = SGAIController->FindEnemyMainCity();
		if (MainCity)
		{
			InstanceData.FoundTarget = MainCity;
			SGAIController->SetCurrentTarget(MainCity);
			UE_LOG(LogSGGameplay, Log, TEXT("✅ 找到目标主城：%s"), *MainCity->GetName());
			return EStateTreeRunStatus::Succeeded;
		}
	}

	// 查找最近的敌人
	AActor* Enemy = SGAIController->FindNearestEnemy(InstanceData.SearchRadius);
	if (Enemy)
	{
		InstanceData.FoundTarget = Enemy;
		SGAIController->SetCurrentTarget(Enemy);
		UE_LOG(LogSGGameplay, Log, TEXT("✅ 找到目标敌人：%s"), *Enemy->GetName());
		return EStateTreeRunStatus::Succeeded;
	}

	// 没有找到目标
	UE_LOG(LogSGGameplay, Log, TEXT("❌ 未找到目标"));
	return EStateTreeRunStatus::Failed;
}
