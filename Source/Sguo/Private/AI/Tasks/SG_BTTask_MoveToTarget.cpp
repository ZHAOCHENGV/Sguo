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
	
	// ========== 检查目标是否为主城 ==========
	ASG_MainCityBase* TargetMainCity = Cast<ASG_MainCityBase>(Target);
	bool bIsTargetMainCity = (TargetMainCity != nullptr);
	
	// ========== 🔧 修复 - 计算目标位置 ==========
	FVector TargetLocation;
	
	if (bIsTargetMainCity)
	{
		// 🔧 关键修复：主城使用 Actor 位置（地面），而不是检测盒位置（空中）
		TargetLocation = TargetMainCity->GetActorLocation();
		
		// ✨ 新增 - 确保目标位置在地面上（投影到导航网格）
		FNavLocation NavLocation;
		UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
		if (NavSys)
		{
			// 尝试在目标位置附近找到可导航的位置
			if (NavSys->ProjectPointToNavigation(TargetLocation, NavLocation, FVector(500.0f, 500.0f, 1000.0f)))
			{
				TargetLocation = NavLocation.Location;
				UE_LOG(LogSGGameplay, Log, TEXT("  主城目标，使用投影后的导航位置：%s"), *TargetLocation.ToString());
			}
			else
			{
				UE_LOG(LogSGGameplay, Warning, TEXT("  ⚠️ 无法将主城位置投影到导航网格"));
			}
		}
	}
	else
	{
		// 普通单位，使用 Actor 位置
		TargetLocation = Target->GetActorLocation();
	}
	
	// ========== 计算停止距离 ==========
	float Radius = AcceptableRadius;
	
	if (Radius < 0.0f)
	{
		// 从单位获取攻击范围
		Radius = ControlledUnit->GetAttackRangeForAI();
		
		// 🔧 修改：主城使用更大的停止距离
		if (bIsTargetMainCity)
		{
			// 主城很大，停止距离应该更大
			// 攻击范围 + 主城检测盒半径的估算值
			Radius = Radius + 600.0f;  // 攻击范围 + 主城大小
			UE_LOG(LogSGGameplay, Log, TEXT("  主城目标，停止距离：%.0f"), Radius);
		}
		else
		{
			// 普通单位
			Radius = FMath::Max(Radius - 100.0f, 50.0f);
		}
	}
	
	// ========== 检查是否已在攻击范围内 ==========
	float DistanceToTarget = FVector::Dist(ControlledUnit->GetActorLocation(), TargetLocation);
	float AttackRange = ControlledUnit->GetAttackRangeForAI();
	
	// 🔧 修改：主城的攻击范围判定更宽松
	float EffectiveAttackRange = bIsTargetMainCity ? (AttackRange + 600.0f) : AttackRange;
	
	if (DistanceToTarget <= EffectiveAttackRange)
	{
		UE_LOG(LogSGGameplay, Log, TEXT("✓ 移动到目标任务：已在攻击范围内（距离：%.0f，有效攻击范围：%.0f）"), 
			DistanceToTarget, EffectiveAttackRange);
		return EBTNodeResult::Succeeded;
	}
	
	// ========== 移动到目标位置 ==========
	EPathFollowingRequestResult::Type Result = AIController->MoveToLocation(
		TargetLocation,
		Radius,
		true,   // 停止时到达
		true,   // 使用寻路
		true,   // 可以跨越
		true,   // 允许部分路径
		nullptr // 过滤器类
	);
	
	// 检查移动请求结果
	if (Result == EPathFollowingRequestResult::RequestSuccessful)
	{
		UE_LOG(LogSGGameplay, Log, TEXT("✓ 移动到目标任务：开始移动到 %s（停止距离：%.0f，目标类型：%s）"), 
			*Target->GetName(), Radius, bIsTargetMainCity ? TEXT("主城") : TEXT("单位"));
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
		UE_LOG(LogSGGameplay, Warning, TEXT("  目标位置：%s"), *TargetLocation.ToString());
		UE_LOG(LogSGGameplay, Warning, TEXT("  单位位置：%s"), *ControlledUnit->GetActorLocation().ToString());
		UE_LOG(LogSGGameplay, Warning, TEXT("  停止距离：%.0f"), Radius);
		UE_LOG(LogSGGameplay, Warning, TEXT("  距离：%.0f"), DistanceToTarget);
		
		// ✨ 新增 - 如果是主城且距离合理，尝试直接成功
		if (bIsTargetMainCity && DistanceToTarget <= 2000.0f)
		{
			UE_LOG(LogSGGameplay, Log, TEXT("  主城目标，距离合理，尝试部分路径移动"));
			// 可以考虑返回成功，或者尝试其他策略
		}
		
		return EBTNodeResult::Failed;
	}
}
