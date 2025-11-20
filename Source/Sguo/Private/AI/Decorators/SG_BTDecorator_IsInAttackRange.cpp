// 🔧 完全重写 - SG_BTDecorator_IsInAttackRange.cpp
/**
 * @file SG_BTDecorator_IsInAttackRange.cpp
 * @brief 行为树装饰器：检查是否在攻击范围内实现
 */

#include "AI/Decorators/SG_BTDecorator_IsInAttackRange.h"
#include "AI/SG_AIControllerBase.h"
#include "Units/SG_UnitsBase.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Debug/SG_LogCategories.h"

/**
 * @brief 构造函数
 */
USG_BTDecorator_IsInAttackRange::USG_BTDecorator_IsInAttackRange()
{
	// 设置装饰器名称
	NodeName = TEXT("是否在攻击范围内");
	
	// 🔧 关键：启用 Tick，定期检查条件
	bNotifyTick = true;
	
	// 🔧 关键：设置为观察者模式
	bNotifyBecomeRelevant = true;
	bNotifyCeaseRelevant = true;
	
	// 🔧 关键：不自动中断，由 Tick 控制
	FlowAbortMode = EBTFlowAbortMode::None;
	
	// 配置黑板键过滤器
	TargetKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(USG_BTDecorator_IsInAttackRange, TargetKey), AActor::StaticClass());
	
	// 🔧 新增：设置默认黑板键名称
	TargetKey.SelectedKeyName = FName("CurrentTarget");
}

/**
 * @brief 计算条件
 */
bool USG_BTDecorator_IsInAttackRange::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
	// ========== 步骤1：获取 AI Controller ==========
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("❌ IsInAttackRange：AI Controller 无效"));
		return false;
	}
	
	// ========== 步骤2：获取控制的单位 ==========
	ASG_UnitsBase* ControlledUnit = Cast<ASG_UnitsBase>(AIController->GetPawn());
	if (!ControlledUnit)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("❌ IsInAttackRange：控制的单位无效"));
		return false;
	}
	
	// ========== 步骤3：获取黑板组件 ==========
	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (!BlackboardComp)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("❌ IsInAttackRange：黑板组件无效"));
		return false;
	}
	
	// ========== 步骤4：获取目标 ==========
	// 🔧 修改：先输出黑板键名称，用于调试
	FName KeyName = TargetKey.SelectedKeyName;
	if (KeyName.IsNone())
	{
		UE_LOG(LogSGGameplay, Error, TEXT("❌ IsInAttackRange：黑板键名称为空"));
		return false;
	}
	
	// 🔧 修改：输出调试信息
	UE_LOG(LogSGGameplay, Verbose, TEXT("🔍 IsInAttackRange：尝试读取黑板键 '%s'"), *KeyName.ToString());
	
	// 获取目标
	AActor* Target = Cast<AActor>(BlackboardComp->GetValueAsObject(KeyName));
	if (!Target)
	{
		UE_LOG(LogSGGameplay, Warning, TEXT("⚠️ IsInAttackRange：黑板键 '%s' 的值为空"), *KeyName.ToString());
		return false;
	}
	
	// ========== 步骤5：获取攻击范围 ==========
	float AttackRange = ControlledUnit->GetAttackRangeForAI();
	
	// ========== 步骤6：计算与目标的距离 ==========
	FVector UnitLocation = ControlledUnit->GetActorLocation();
	FVector TargetLocation = Target->GetActorLocation();
	float Distance = FVector::Dist(UnitLocation, TargetLocation);
	
	// ========== 步骤7：检查是否在攻击范围内 ==========
	bool bInRange = Distance <= (AttackRange + DistanceTolerance);
	
	// ========== 步骤8：输出详细调试日志 ==========
	UE_LOG(LogSGGameplay, Log, TEXT("🎯 IsInAttackRange 检查："));
	UE_LOG(LogSGGameplay, Log, TEXT("  单位：%s"), *ControlledUnit->GetName());
	UE_LOG(LogSGGameplay, Log, TEXT("  目标：%s"), *Target->GetName());
	UE_LOG(LogSGGameplay, Log, TEXT("  单位位置：%s"), *UnitLocation.ToString());
	UE_LOG(LogSGGameplay, Log, TEXT("  目标位置：%s"), *TargetLocation.ToString());
	UE_LOG(LogSGGameplay, Log, TEXT("  距离：%.2f"), Distance);
	UE_LOG(LogSGGameplay, Log, TEXT("  攻击范围：%.2f"), AttackRange);
	UE_LOG(LogSGGameplay, Log, TEXT("  容差：%.2f"), DistanceTolerance);
	UE_LOG(LogSGGameplay, Log, TEXT("  结果：%s"), bInRange ? TEXT("✅ 在范围内") : TEXT("❌ 不在范围内"));
	
	return bInRange;
}

/**
 * @brief Tick 更新
 * @details
 * 功能说明：
 * - 定期检查条件是否变化
 * - 条件变化时通知行为树重新评估
 */
void USG_BTDecorator_IsInAttackRange::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);
	
	// 定期检查条件
	LastCheckTime += DeltaSeconds;
	if (LastCheckTime >= CheckInterval)
	{
		LastCheckTime = 0.0f;
		
		// 计算当前条件
		bool CurrentConditionResult = CalculateRawConditionValue(OwnerComp, NodeMemory);
		
		// 🔧 新增 - 强制更新黑板值
		UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
		if (BlackboardComp)
		{
			BlackboardComp->SetValueAsBool(FName("IsInAttackRange"), CurrentConditionResult);
		}
		
		// 条件变化时，请求重新评估
		if (CurrentConditionResult != LastConditionResult)
		{
			LastConditionResult = CurrentConditionResult;
			
			// 通知行为树条件已变化
			OwnerComp.RequestExecution(this);
			
			UE_LOG(LogSGGameplay, Log, TEXT("🔄 IsInAttackRange 条件变化：%s → %s"),
				!LastConditionResult ? TEXT("不在范围内") : TEXT("在范围内"),
				CurrentConditionResult ? TEXT("在范围内") : TEXT("不在范围内"));
		}
	}
}

/**
 * @brief 获取节点描述
 */
FString USG_BTDecorator_IsInAttackRange::GetStaticDescription() const
{
	return FString::Printf(TEXT("检查是否在攻击范围内\n目标键：%s\n距离容差：%.0f\n检查间隔：%.2f秒"),
		*TargetKey.SelectedKeyName.ToString(),
		DistanceTolerance,
		CheckInterval);
}
