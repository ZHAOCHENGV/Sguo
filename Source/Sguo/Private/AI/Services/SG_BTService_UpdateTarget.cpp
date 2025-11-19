// ✨ 新增 - 更新目标服务实现
/**
 * @file SG_BTService_UpdateTarget.cpp
 * @brief 行为树服务：更新目标实现
 */

#include "AI/Services/SG_BTService_UpdateTarget.h"
#include "AI/SG_AIControllerBase.h"
#include "Units/SG_UnitsBase.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Debug/SG_LogCategories.h"

/**
 * @brief 构造函数
 * @details
 * 功能说明：
 * - 设置服务名称
 * - 配置更新间隔
 */
USG_BTService_UpdateTarget::USG_BTService_UpdateTarget()
{
	// 设置服务名称
	NodeName = TEXT("更新目标");
	
	// 设置更新间隔（每 0.5 秒检查一次）
	Interval = 0.5f;
	RandomDeviation = 0.1f;
	
	// 配置黑板键过滤器（只接受 Object 类型）
	TargetKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(USG_BTService_UpdateTarget, TargetKey), AActor::StaticClass());
}

/**
 * @brief Tick 更新
 * @param OwnerComp 行为树组件
 * @param NodeMemory 节点内存
 * @param DeltaSeconds 时间间隔
 * @details
 * 功能说明：
 * - 检查目标有效性
 * - 目标无效时查找新目标
 * - 更新黑板数据
 */
void USG_BTService_UpdateTarget::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);
	
	// 获取 AI Controller
	ASG_AIControllerBase* AIController = Cast<ASG_AIControllerBase>(OwnerComp.GetAIOwner());
	if (!AIController)
	{
		return;
	}
	
	// 获取黑板组件
	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (!BlackboardComp)
	{
		return;
	}
	
	// 获取当前目标
	AActor* CurrentTarget = Cast<AActor>(BlackboardComp->GetValueAsObject(TargetKey.SelectedKeyName));
	
	// 检查目标是否有效
	bool bIsTargetValid = false;
	if (CurrentTarget)
	{
		// 检查目标是否已死亡
		ASG_UnitsBase* TargetUnit = Cast<ASG_UnitsBase>(CurrentTarget);
		if (TargetUnit)
		{
			bIsTargetValid = !TargetUnit->bIsDead;
		}
		else
		{
			// 如果不是单位（可能是主城），假设有效
			bIsTargetValid = true;
		}
	}
	
	// 如果目标无效，查找新目标
	if (!bIsTargetValid)
	{
		UE_LOG(LogSGGameplay, Verbose, TEXT("🔄 目标无效，查找新目标"));
		
		AActor* NewTarget = AIController->FindNearestTarget();
		if (NewTarget)
		{
			BlackboardComp->SetValueAsObject(TargetKey.SelectedKeyName, NewTarget);
			AIController->SetCurrentTarget(NewTarget);
			
			UE_LOG(LogSGGameplay, Log, TEXT("✓ 找到新目标：%s"), *NewTarget->GetName());
		}
		else
		{
			BlackboardComp->ClearValue(TargetKey.SelectedKeyName);
			AIController->SetCurrentTarget(nullptr);
			
			UE_LOG(LogSGGameplay, Verbose, TEXT("⚠️ 未找到新目标"));
		}
	}
}
