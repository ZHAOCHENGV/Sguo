// ✨ 新增 - 移动到目标任务实现
/**
 * @file SG_BTTask_MoveToTarget.cpp
 * @brief 行为树任务：移动到目标实现
 */

#include "AI/Tasks/SG_BTTask_MoveToTarget.h"
#include "AI/SG_AIControllerBase.h"
#include "Units/SG_UnitsBase.h"
#include "AbilitySystem/SG_AttributeSet.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "Debug/SG_LogCategories.h"

/**
 * @brief 构造函数
 * @details
 * 功能说明：
 * - 设置任务名称
 * - 配置节点参数
 */
USG_BTTask_MoveToTarget::USG_BTTask_MoveToTarget()
{
	// 设置任务名称
	NodeName = TEXT("移动到目标");
	
	// 配置黑板键过滤器（只接受 Object 类型）
	TargetKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(USG_BTTask_MoveToTarget, TargetKey), AActor::StaticClass());
	
	// 设置为异步任务（等待移动完成）
	bNotifyTick = true;
}

/**
 * @brief 执行任务
 * @param OwnerComp 行为树组件
 * @param NodeMemory 节点内存
 * @return 任务执行结果
 * @details
 * 功能说明：
 * - 移动到目标位置
 * - 考虑攻击范围
 */
EBTNodeResult::Type USG_BTTask_MoveToTarget::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	// 获取 AI Controller
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("❌ 移动到目标任务：AI Controller 无效"));
		return EBTNodeResult::Failed;
	}
	
	// 获取控制的单位
	ASG_UnitsBase* ControlledUnit = Cast<ASG_UnitsBase>(AIController->GetPawn());
	if (!ControlledUnit)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("❌ 移动到目标任务：控制的单位无效"));
		return EBTNodeResult::Failed;
	}
	
	// 获取黑板组件
	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (!BlackboardComp)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("❌ 移动到目标任务：黑板组件无效"));
		return EBTNodeResult::Failed;
	}
	
	// 获取目标
	AActor* Target = Cast<AActor>(BlackboardComp->GetValueAsObject(TargetKey.SelectedKeyName));
	if (!Target)
	{
		UE_LOG(LogSGGameplay, Warning, TEXT("⚠️ 移动到目标任务：目标无效"));
		return EBTNodeResult::Failed;
	}
	
	// 计算可接受半径（使用攻击范围）
	float Radius = AcceptableRadius;
	if (Radius < 0.0f)
	{
		// 从 AttributeSet 读取攻击范围
		if (ControlledUnit->AttributeSet)
		{
			Radius = ControlledUnit->AttributeSet->GetAttackRange();
		}
		else
		{
			Radius = ControlledUnit->BaseAttackRange;
		}
		
		// 减去一些余量，避免边界抖动
		Radius = FMath::Max(Radius - 50.0f, 50.0f);
	}
	
	// 移动到目标
	EPathFollowingRequestResult::Type Result = AIController->MoveToActor(
		Target,
		Radius,           // 可接受半径
		true,             // 停止时到达
		true,             // 使用寻路
		true,             // 可以跨越
		nullptr,          // 过滤器类
		true              // 允许部分路径
	);
	
	// 检查移动请求结果
	if (Result == EPathFollowingRequestResult::RequestSuccessful)
	{
		UE_LOG(LogSGGameplay, Verbose, TEXT("✓ 移动到目标任务：开始移动到 %s"), *Target->GetName());
		return EBTNodeResult::InProgress;
	}
	else if (Result == EPathFollowingRequestResult::AlreadyAtGoal)
	{
		UE_LOG(LogSGGameplay, Verbose, TEXT("✓ 移动到目标任务：已在目标位置"));
		return EBTNodeResult::Succeeded;
	}
	else
	{
		UE_LOG(LogSGGameplay, Warning, TEXT("⚠️ 移动到目标任务：移动请求失败"));
		return EBTNodeResult::Failed;
	}
}
