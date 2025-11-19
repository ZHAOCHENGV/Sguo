// ✨ 新增 - 目标有效性检查装饰器实现
/**
 * @file SG_BTDecorator_IsTargetValid.cpp
 * @brief 行为树装饰器：检查目标是否有效实现
 */

#include "AI/Decorators/SG_BTDecorator_IsTargetValid.h"
#include "AI/SG_AIControllerBase.h"
#include "Units/SG_UnitsBase.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Debug/SG_LogCategories.h"

/**
 * @brief 构造函数
 * @details
 * 功能说明：
 * - 设置装饰器名称
 * - 配置观察者中断模式
 */
USG_BTDecorator_IsTargetValid::USG_BTDecorator_IsTargetValid()
{
	// 设置装饰器名称
	NodeName = TEXT("目标是否有效");
	
	// 配置观察者中断模式（目标变化时中断）
	bNotifyBecomeRelevant = true;
	bNotifyCeaseRelevant = true;
	
	// 配置黑板键过滤器（只接受 Object 类型）
	TargetKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(USG_BTDecorator_IsTargetValid, TargetKey), AActor::StaticClass());
}

/**
 * @brief 计算条件
 * @param OwnerComp 行为树组件
 * @param NodeMemory 节点内存
 * @return 条件是否满足
 * @details
 * 功能说明：
 * - 检查目标是否有效
 * - 返回 true 或 false
 */
bool USG_BTDecorator_IsTargetValid::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
	// 获取 AI Controller
	ASG_AIControllerBase* AIController = Cast<ASG_AIControllerBase>(OwnerComp.GetAIOwner());
	if (!AIController)
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
	
	// 检查目标是否已死亡
	ASG_UnitsBase* TargetUnit = Cast<ASG_UnitsBase>(Target);
	if (TargetUnit && TargetUnit->bIsDead)
	{
		return false;
	}
	
	return true;
}
