// 🔧 完全修复 - SG_BTDecorator_IsInAttackRange.cpp

/**
 * @file SG_BTDecorator_IsInAttackRange.cpp
 * @brief 行为树装饰器：检查是否在攻击范围内实现
 */

#include "AI/Decorators/SG_BTDecorator_IsInAttackRange.h"
#include "AI/SG_AIControllerBase.h"
#include "Units/SG_UnitsBase.h"
#include "Buildings/SG_MainCityBase.h"  // ✨ 新增
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Debug/SG_LogCategories.h"
#include "Components/BoxComponent.h"  // ✨ 新增

/**
 * @brief 构造函数
 */
USG_BTDecorator_IsInAttackRange::USG_BTDecorator_IsInAttackRange()
{
	NodeName = TEXT("是否在攻击范围内");
	bNotifyTick = true;
	bNotifyBecomeRelevant = true;
	bNotifyCeaseRelevant = true;
	FlowAbortMode = EBTFlowAbortMode::LowerPriority;
	
	TargetKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(USG_BTDecorator_IsInAttackRange, TargetKey), AActor::StaticClass());
	TargetKey.SelectedKeyName = FName("CurrentTarget");
}

/**
 * @brief 计算条件
 * @details
 * 功能说明：
 * - 🔧 修复：主城使用检测盒表面距离
 * - ✨ 新增：进入攻击范围时立即停止移动
 */
bool USG_BTDecorator_IsInAttackRange::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
	
	// ========== 步骤1-5：获取基础信息（保持不变）==========
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController) return false;
	
	ASG_UnitsBase* ControlledUnit = Cast<ASG_UnitsBase>(AIController->GetPawn());
	if (!ControlledUnit) return false;
	
	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (!BlackboardComp) return false;
	
	FName KeyName = TargetKey.SelectedKeyName;
	if (KeyName.IsNone()) return false;
	
	AActor* Target = Cast<AActor>(BlackboardComp->GetValueAsObject(KeyName));
	if (!Target) return false;
	
	// ========== 步骤6：获取单位位置和攻击范围 ==========
	FVector UnitLocation = ControlledUnit->GetActorLocation();
	float AttackRange = ControlledUnit->GetAttackRangeForAI();
	
	// ========== 步骤7：计算到目标的实际距离 ==========
	float ActualDistance = 0.0f;
	bool bIsMainCity = false;
	
	ASG_MainCityBase* MainCity = Cast<ASG_MainCityBase>(Target);
	if (MainCity && MainCity->GetAttackDetectionBox())
	{
		bIsMainCity = true;
		
		UBoxComponent* DetectionBox = MainCity->GetAttackDetectionBox();
		FVector BoxCenter = DetectionBox->GetComponentLocation();
		FVector BoxExtent = DetectionBox->GetScaledBoxExtent();
		
		float DistanceToCenter = FVector::Dist(UnitLocation, BoxCenter);
		float BoxRadius = FMath::Max3(BoxExtent.X, BoxExtent.Y, BoxExtent.Z);
		
		ActualDistance = DistanceToCenter - BoxRadius;
		
		if (ActualDistance < 0.0f)
		{
			ActualDistance = 0.0f;
		}
	}
	else
	{
		ActualDistance = FVector::Dist(UnitLocation, Target->GetActorLocation());
	}
	
	// ========== 步骤8：判断是否在攻击范围内 ==========
	bool bInRange = ActualDistance <= (AttackRange + DistanceTolerance);
	
	// ========== ✨ 新增 - 步骤9：进入攻击范围时立即停止移动 ==========
	static TMap<ASG_UnitsBase*, bool> LastInRangeStatus;
	bool bWasInRange = LastInRangeStatus.FindOrAdd(ControlledUnit, false);
	
	if (bInRange && !bWasInRange)
	{
		// 刚进入攻击范围，立即停止移动
		AIController->StopMovement();
		UE_LOG(LogSGGameplay, Warning, TEXT("🛑 %s 进入攻击范围，立即停止移动"), *ControlledUnit->GetName());
	}
	
	LastInRangeStatus[ControlledUnit] = bInRange;
	
	// ========== 步骤10：输出详细调试日志 ==========
	UE_LOG(LogSGGameplay, Log, TEXT("🎯 IsInAttackRange 检查："));
	UE_LOG(LogSGGameplay, Log, TEXT("  单位：%s"), *ControlledUnit->GetName());
	UE_LOG(LogSGGameplay, Log, TEXT("  目标：%s%s"), *Target->GetName(), bIsMainCity ? TEXT("（主城）") : TEXT(""));
	UE_LOG(LogSGGameplay, Log, TEXT("  单位位置：%s"), *UnitLocation.ToString());
	
	if (bIsMainCity)
	{
		UBoxComponent* DetectionBox = MainCity->GetAttackDetectionBox();
		FVector BoxCenter = DetectionBox->GetComponentLocation();
		FVector BoxExtent = DetectionBox->GetScaledBoxExtent();
		float BoxRadius = FMath::Max3(BoxExtent.X, BoxExtent.Y, BoxExtent.Z);
		float DistanceToCenter = FVector::Dist(UnitLocation, BoxCenter);
		
		UE_LOG(LogSGGameplay, Log, TEXT("  检测盒中心：%s"), *BoxCenter.ToString());
		UE_LOG(LogSGGameplay, Log, TEXT("  检测盒半径：%.2f"), BoxRadius);
		UE_LOG(LogSGGameplay, Log, TEXT("  到中心距离：%.2f"), DistanceToCenter);
		UE_LOG(LogSGGameplay, Log, TEXT("  到表面距离：%.2f"), ActualDistance);
	}
	else
	{
		UE_LOG(LogSGGameplay, Log, TEXT("  目标位置：%s"), *Target->GetActorLocation().ToString());
		UE_LOG(LogSGGameplay, Log, TEXT("  距离：%.2f"), ActualDistance);
	}
	
	UE_LOG(LogSGGameplay, Log, TEXT("  单位攻击范围：%.2f"), AttackRange);
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
		
		// 🔧 强制更新黑板值
		UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
		if (BlackboardComp)
		{
			BlackboardComp->SetValueAsBool(FName("IsInAttackRange"), CurrentConditionResult);
		}
		
		// 条件变化时，强制重新评估
		if (CurrentConditionResult != LastConditionResult)
		{
			LastConditionResult = CurrentConditionResult;
			OwnerComp.RequestExecution(this);
			
			UE_LOG(LogSGGameplay, Warning, TEXT("🔄 IsInAttackRange 条件变化：%s → %s，请求重新评估"),
				!LastConditionResult ? TEXT("不在范围内") : TEXT("在范围内"),
				CurrentConditionResult ? TEXT("在范围内") : TEXT("不在范围内"));
		}
		
		// ✨ 强制定期评估（防止卡住）
		if (CurrentConditionResult)
		{
			static TMap<UBehaviorTreeComponent*, int32> ForceEvaluateCounters;
			int32& Counter = ForceEvaluateCounters.FindOrAdd(&OwnerComp, 0);
			Counter++;
			
			if (Counter >= 5)  // 每5次检查（0.5秒）
			{
				Counter = 0;
				OwnerComp.RequestExecution(this);
				
				UE_LOG(LogSGGameplay, Verbose, TEXT("🔄 IsInAttackRange 强制请求评估（防止卡住）"));
			}
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
