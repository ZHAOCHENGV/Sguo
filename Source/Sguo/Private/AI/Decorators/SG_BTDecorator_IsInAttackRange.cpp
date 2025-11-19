// ✨ 新增 - 攻击范围检查装饰器实现
/**
 * @file SG_BTDecorator_IsInAttackRange.cpp
 * @brief 行为树装饰器：检查是否在攻击范围内实现
 */

#include "AI/Decorators/SG_BTDecorator_IsInAttackRange.h"
#include "AI/SG_AIControllerBase.h"
#include "Units/SG_UnitsBase.h"
#include "AbilitySystem/SG_AttributeSet.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Debug/SG_LogCategories.h"

/**
 * @brief 构造函数
 * @details
 * 功能说明：
 * - 设置装饰器名称
 * - 配置观察者中断模式
 */
USG_BTDecorator_IsInAttackRange::USG_BTDecorator_IsInAttackRange()
{
	// 设置装饰器名称
	NodeName = TEXT("是否在攻击范围内");
	
	// 配置黑板键过滤器（只接受 Object 类型）
	TargetKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(USG_BTDecorator_IsInAttackRange, TargetKey), AActor::StaticClass());
}

/**
 * @brief 计算条件
 * @param OwnerComp 行为树组件
 * @param NodeMemory 节点内存
 * @return 条件是否满足
 * @details
 * 功能说明：
 * - 计算与目标的距离
 * - 比较是否在攻击范围内
 */
bool USG_BTDecorator_IsInAttackRange::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
	// 获取 AI Controller
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		return false;
	}
	
	// 获取控制的单位
	ASG_UnitsBase* ControlledUnit = Cast<ASG_UnitsBase>(AIController->GetPawn());
	if (!ControlledUnit)
	{
		return false;
	}
	
	// 获取黑板组件
	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (!BlackboardComp)
	{
		return false;
	}
	
	// 获取目标
	AActor* Target = Cast<AActor>(BlackboardComp->GetValueAsObject(TargetKey.SelectedKeyName));
	if (!Target)
	{
		return false;
	}
	
	// 获取攻击范围
	float AttackRange = ControlledUnit->BaseAttackRange;
	if (ControlledUnit->AttributeSet)
	{
		AttackRange = ControlledUnit->AttributeSet->GetAttackRange();
	}
	
	// 计算与目标的距离
	float Distance = FVector::Dist(ControlledUnit->GetActorLocation(), Target->GetActorLocation());
	
	// 检查是否在攻击范围内
	return Distance <= AttackRange;
}
