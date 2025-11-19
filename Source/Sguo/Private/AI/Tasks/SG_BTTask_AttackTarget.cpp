// ✨ 新增 - 攻击目标任务实现
/**
 * @file SG_BTTask_AttackTarget.cpp
 * @brief 行为树任务：攻击目标实现
 */

#include "AI/Tasks/SG_BTTask_AttackTarget.h"
#include "AI/SG_AIControllerBase.h"
#include "Units/SG_UnitsBase.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Debug/SG_LogCategories.h"

/**
 * @brief 构造函数
 * @details
 * 功能说明：
 * - 设置任务名称
 * - 配置节点参数
 */
USG_BTTask_AttackTarget::USG_BTTask_AttackTarget()
{
	// 设置任务名称
	NodeName = TEXT("攻击目标");
}

/**
 * @brief 执行任务
 * @param OwnerComp 行为树组件
 * @param NodeMemory 节点内存
 * @return 任务执行结果
 * @details
 * 功能说明：
 * - 触发攻击能力
 * - 返回成功或失败
 */
EBTNodeResult::Type USG_BTTask_AttackTarget::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	// 获取 AI Controller
	ASG_AIControllerBase* AIController = Cast<ASG_AIControllerBase>(OwnerComp.GetAIOwner());
	if (!AIController)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("❌ 攻击目标任务：AI Controller 无效"));
		return EBTNodeResult::Failed;
	}
	
	// 检查主城是否被打断
	if (AIController->bIsMainCity && AIController->bAttackInterrupted)
	{
		UE_LOG(LogSGGameplay, Verbose, TEXT("⚠️ 攻击目标任务：主城攻击被打断"));
		return EBTNodeResult::Failed;
	}
	
	// 获取控制的单位
	ASG_UnitsBase* ControlledUnit = Cast<ASG_UnitsBase>(AIController->GetPawn());
	if (!ControlledUnit)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("❌ 攻击目标任务：控制的单位无效"));
		return EBTNodeResult::Failed;
	}
	
	// 触发攻击
	bool bSuccess = ControlledUnit->PerformAttack();
	
	if (bSuccess)
	{
		UE_LOG(LogSGGameplay, Verbose, TEXT("✓ 攻击目标任务：攻击成功"));
		return EBTNodeResult::Succeeded;
	}
	else
	{
		UE_LOG(LogSGGameplay, Verbose, TEXT("⚠️ 攻击目标任务：攻击失败（可能在冷却中）"));
		return EBTNodeResult::Failed;
	}
}
