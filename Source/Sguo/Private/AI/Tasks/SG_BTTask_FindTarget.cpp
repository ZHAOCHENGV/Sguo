// ✨ 新增 - 查找目标任务实现
/**
 * @file SG_BTTask_FindTarget.cpp
 * @brief 行为树任务：查找目标实现
 */

#include "AI/Tasks/SG_BTTask_FindTarget.h"
#include "AI/SG_AIControllerBase.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Debug/SG_LogCategories.h"

/**
 * @brief 构造函数
 * @details
 * 功能说明：
 * - 设置任务名称
 * - 配置节点参数
 */
USG_BTTask_FindTarget::USG_BTTask_FindTarget()
{
	// 设置任务名称
	NodeName = TEXT("查找目标");
	
	// 配置黑板键过滤器（只接受 Object 类型）
	TargetKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(USG_BTTask_FindTarget, TargetKey), AActor::StaticClass());
}

/**
 * @brief 执行任务
 * @param OwnerComp 行为树组件
 * @param NodeMemory 节点内存
 * @return 任务执行结果
 * @details
 * 功能说明：
 * - 查找最近的目标
 * - 更新黑板
 * - 返回成功或失败
 */
EBTNodeResult::Type USG_BTTask_FindTarget::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	// 获取 AI Controller
	ASG_AIControllerBase* AIController = Cast<ASG_AIControllerBase>(OwnerComp.GetAIOwner());
	if (!AIController)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("❌ 查找目标任务：AI Controller 无效"));
		return EBTNodeResult::Failed;
	}
	
	// 查找最近的目标
	AActor* NewTarget = AIController->FindNearestTarget();
	
	// 获取黑板组件
	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (!BlackboardComp)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("❌ 查找目标任务：黑板组件无效"));
		return EBTNodeResult::Failed;
	}
	
	// 更新黑板
	if (NewTarget)
	{
		BlackboardComp->SetValueAsObject(TargetKey.SelectedKeyName, NewTarget);
		AIController->SetCurrentTarget(NewTarget);
		
		UE_LOG(LogSGGameplay, Verbose, TEXT("✓ 查找目标任务：找到目标 %s"), *NewTarget->GetName());
		return EBTNodeResult::Succeeded;
	}
	else
	{
		BlackboardComp->ClearValue(TargetKey.SelectedKeyName);
		AIController->SetCurrentTarget(nullptr);
		
		UE_LOG(LogSGGameplay, Verbose, TEXT("⚠️ 查找目标任务：未找到目标"));
		return EBTNodeResult::Failed;
	}
}
