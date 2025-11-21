// 📄 SG_BTTask_MoveToTarget.cpp - 完整文件

/**
 * @file SG_BTTask_MoveToTarget.cpp
 * @brief 行为树任务：移动到目标实现
 */

#include "AI/Tasks/SG_BTTask_MoveToTarget.h"
#include "AI/SG_AIControllerBase.h"
#include "Units/SG_UnitsBase.h"
#include "Buildings/SG_MainCityBase.h"  // ✨ 新增
#include "AbilitySystem/SG_AttributeSet.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "Debug/SG_LogCategories.h"
#include "Components/BoxComponent.h"  // ✨ 新增

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
 * - 🔧 修改：移动到主城的攻击检测盒位置
 */
EBTNodeResult::Type USG_BTTask_MoveToTarget::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	// ========== 步骤1-5：获取基础对象（保持不变）==========
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("❌ 移动到目标任务：AI Controller 无效"));
		return EBTNodeResult::Failed;
	}
	
	ASG_UnitsBase* ControlledUnit = Cast<ASG_UnitsBase>(AIController->GetPawn());
	if (!ControlledUnit)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("❌ 移动到目标任务：控制的单位无效"));
		return EBTNodeResult::Failed;
	}
	
	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (!BlackboardComp)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("❌ 移动到目标任务：黑板组件无效"));
		return EBTNodeResult::Failed;
	}
	
	AActor* Target = Cast<AActor>(BlackboardComp->GetValueAsObject(TargetKey.SelectedKeyName));
	if (!Target)
	{
		UE_LOG(LogSGGameplay, Warning, TEXT("⚠️ 移动到目标任务：目标无效"));
		return EBTNodeResult::Failed;
	}
	
	ASG_MainCityBase* TargetMainCity = Cast<ASG_MainCityBase>(Target);
	bool bIsTargetMainCity = (TargetMainCity != nullptr);
	
	// ========== 步骤6：计算实际距离 ==========
	FVector UnitLocation = ControlledUnit->GetActorLocation();
	float AttackRange = ControlledUnit->GetAttackRangeForAI();
	float ActualDistance = 0.0f;
	
	// 用于移动的目标位置
	FVector MoveTargetLocation;
	
	if (bIsTargetMainCity && TargetMainCity->GetAttackDetectionBox())
	{
		// 主城：计算到检测盒表面的实际距离
		UBoxComponent* DetectionBox = TargetMainCity->GetAttackDetectionBox();
		FVector BoxCenter = DetectionBox->GetComponentLocation();
		FVector BoxExtent = DetectionBox->GetScaledBoxExtent();
		float BoxRadius = FMath::Max3(BoxExtent.X, BoxExtent.Y, BoxExtent.Z);
		
		float DistanceToCenter = FVector::Dist(UnitLocation, BoxCenter);
		ActualDistance = FMath::Max(0.0f, DistanceToCenter - BoxRadius);
		
		// 移动目标位置 = 主城地面位置（投影到导航网格）
		MoveTargetLocation = TargetMainCity->GetActorLocation();
		
		// 投影到导航网格
		FNavLocation NavLocation;
		UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
		if (NavSys && NavSys->ProjectPointToNavigation(MoveTargetLocation, NavLocation, FVector(500.0f, 500.0f, 1000.0f)))
		{
			MoveTargetLocation = NavLocation.Location;
		}
		
		UE_LOG(LogSGGameplay, Log, TEXT("  主城目标距离计算："));
		UE_LOG(LogSGGameplay, Log, TEXT("    单位位置：%s"), *UnitLocation.ToString());
		UE_LOG(LogSGGameplay, Log, TEXT("    检测盒中心：%s"), *BoxCenter.ToString());
		UE_LOG(LogSGGameplay, Log, TEXT("    检测盒半径：%.2f"), BoxRadius);
		UE_LOG(LogSGGameplay, Log, TEXT("    到中心距离：%.2f"), DistanceToCenter);
		UE_LOG(LogSGGameplay, Log, TEXT("    到表面距离：%.2f"), ActualDistance);
		UE_LOG(LogSGGameplay, Log, TEXT("    攻击范围：%.2f"), AttackRange);
		UE_LOG(LogSGGameplay, Log, TEXT("    移动目标位置：%s"), *MoveTargetLocation.ToString());
	}
	else
	{
		// 普通单位：直接计算距离
		MoveTargetLocation = Target->GetActorLocation();
		ActualDistance = FVector::Dist(UnitLocation, MoveTargetLocation);
	}
	
	// ========== 步骤7：检查是否已在攻击范围内 ==========
	if (ActualDistance <= AttackRange)
	{
		UE_LOG(LogSGGameplay, Log, TEXT("✓ 移动到目标任务：已在攻击范围内"));
		UE_LOG(LogSGGameplay, Log, TEXT("  实际距离：%.2f / 攻击范围：%.2f"), ActualDistance, AttackRange);
		
		// ❌ 删除 - 不再在这里停止移动
		// AIController->StopMovement();
		
		return EBTNodeResult::Succeeded;
	}
	
	// ========== 步骤8：计算停止距离 ==========
	float StopDistance = AcceptableRadius;
	
	if (StopDistance < 0.0f)
	{
		if (bIsTargetMainCity)
		{
			// 主城停止距离 = 攻击范围
			StopDistance = AttackRange;
			UE_LOG(LogSGGameplay, Log, TEXT("  主城停止距离：%.2f（= 攻击范围）"), StopDistance);
		}
		else
		{
			// 普通单位：攻击范围 - 100（提前一点停止）
			StopDistance = FMath::Max(AttackRange - 100.0f, 50.0f);
		}
	}
	
	// ========== 步骤9：移动到目标位置 ==========
	UE_LOG(LogSGGameplay, Log, TEXT("  开始移动到目标："));
	UE_LOG(LogSGGameplay, Log, TEXT("    当前位置：%s"), *UnitLocation.ToString());
	UE_LOG(LogSGGameplay, Log, TEXT("    目标位置：%s"), *MoveTargetLocation.ToString());
	UE_LOG(LogSGGameplay, Log, TEXT("    停止距离：%.2f"), StopDistance);
	UE_LOG(LogSGGameplay, Log, TEXT("    当前距离：%.2f"), ActualDistance);
	
	EPathFollowingRequestResult::Type Result = AIController->MoveToLocation(
		MoveTargetLocation,
		StopDistance,
		true,   // 停止时到达
		true,   // 使用寻路
		true,   // 可以跨越
		true,   // 允许部分路径
		nullptr // 过滤器类
	);
	
	// ========== 步骤10：检查移动请求结果 ==========
	if (Result == EPathFollowingRequestResult::RequestSuccessful)
	{
		const TCHAR* TargetTypeStr = bIsTargetMainCity ? TEXT("主城") : TEXT("单位");
		
		UE_LOG(LogSGGameplay, Log, TEXT("✓ 移动到目标任务：移动请求成功"));
		UE_LOG(LogSGGameplay, Log, TEXT("  目标：%s（%s）"), *Target->GetName(), TargetTypeStr);
		
		return EBTNodeResult::InProgress;
	}
	else if (Result == EPathFollowingRequestResult::AlreadyAtGoal)
	{
		UE_LOG(LogSGGameplay, Log, TEXT("✓ 移动到目标任务：已在目标位置"));
		return EBTNodeResult::Succeeded;
	}
	else
	{
		UE_LOG(LogSGGameplay, Warning, TEXT("⚠️ 移动到目标任务：移动请求失败"));
		UE_LOG(LogSGGameplay, Warning, TEXT("  目标：%s"), *Target->GetName());
		UE_LOG(LogSGGameplay, Warning, TEXT("  移动目标位置：%s"), *MoveTargetLocation.ToString());
		UE_LOG(LogSGGameplay, Warning, TEXT("  单位位置：%s"), *UnitLocation.ToString());
		UE_LOG(LogSGGameplay, Warning, TEXT("  停止距离：%.2f"), StopDistance);
		UE_LOG(LogSGGameplay, Warning, TEXT("  实际距离：%.2f"), ActualDistance);
		
		return EBTNodeResult::Failed;
	}
}
