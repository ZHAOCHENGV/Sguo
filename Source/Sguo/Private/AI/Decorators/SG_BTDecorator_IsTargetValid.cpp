// ✨ 新增 - 目标有效性检查装饰器实现
/**
 * @file SG_BTDecorator_IsTargetValid.cpp
 * @brief 行为树装饰器：检查目标是否有效实现
 */

#include "AI/Decorators/SG_BTDecorator_IsTargetValid.h"

#include "AbilitySystem/SG_AttributeSet.h"
#include "AI/SG_AIControllerBase.h"
#include "Units/SG_UnitsBase.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Buildings/SG_MainCityBase.h"
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
	
	// ========== 检查单位是否已死亡 ==========
	ASG_UnitsBase* TargetUnit = Cast<ASG_UnitsBase>(Target);
	if (TargetUnit)
	{
		if (TargetUnit->bIsDead)
		{
			UE_LOG(LogSGGameplay, Verbose, TEXT("目标单位已死亡：%s"), *TargetUnit->GetName());
			return false;
		}
		
		if (TargetUnit->AttributeSet && TargetUnit->AttributeSet->GetHealth() <= 0.0f)
		{
			UE_LOG(LogSGGameplay, Verbose, TEXT("目标单位生命值为 0：%s"), *TargetUnit->GetName());
			return false;
		}
	}
	
	// ========== ✨ 新增 - 检查主城是否被摧毁 ==========
	ASG_MainCityBase* TargetMainCity = Cast<ASG_MainCityBase>(Target);
	if (TargetMainCity)
	{
		float MainCityHealth = TargetMainCity->GetCurrentHealth();
		if (MainCityHealth <= 0.0f)
		{
			UE_LOG(LogSGGameplay, Log, TEXT("✗ 目标主城已被摧毁：%s（生命值：%.0f）"), 
				*TargetMainCity->GetName(), MainCityHealth);
			return false;
		}
	}
	
	return true;
}
