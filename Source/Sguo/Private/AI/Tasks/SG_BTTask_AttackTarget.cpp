// 🔧 修改 - SG_BTTask_AttackTarget.cpp
/**
 * @file SG_BTTask_AttackTarget.cpp
 * @brief 行为树任务：攻击目标实现
 */

#include "AI/Tasks/SG_BTTask_AttackTarget.h"
#include "AI/SG_AIControllerBase.h"
#include "Units/SG_UnitsBase.h"
#include "AbilitySystem/SG_AttributeSet.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Buildings/SG_MainCityBase.h"
#include "Components/BoxComponent.h"
#include "Data/Type/SG_UnitDataTable.h"
#include "Debug/SG_LogCategories.h"

/**
 * @brief 构造函数
 */
USG_BTTask_AttackTarget::USG_BTTask_AttackTarget()
{
	// 设置任务名称
	NodeName = TEXT("攻击目标");
	
	// ✨ 新增 - 启用 Tick，等待攻击完成
	bNotifyTick = true;
	
	
}

// ✨ 新增 - 获取实例内存大小
/**
 * @brief 获取实例内存大小
 * @return 内存大小（字节）
 */
uint16 USG_BTTask_AttackTarget::GetInstanceMemorySize() const
{
	return sizeof(FSG_BTTaskAttackMemory);
}

/**
 * @brief 执行任务
 * @param OwnerComp 行为树组件
 * @param NodeMemory 节点内存
 * @return 任务执行结果
 */
EBTNodeResult::Type USG_BTTask_AttackTarget::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
	UE_LOG(LogSGGameplay, Log, TEXT("🎯 攻击目标任务：开始执行"));
	
	// ========== 步骤1：获取 AI Controller ==========
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("  ❌ AI Controller 无效"));
		UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
		return EBTNodeResult::Failed;
	}
	UE_LOG(LogSGGameplay, Log, TEXT("  ✓ AI Controller 有效"));
	
	// ========== 步骤2：立即停止移动 ==========
	AIController->StopMovement();
	UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 已停止移动"));
	
	// ========== 步骤3：获取控制的单位 ==========
	ASG_UnitsBase* ControlledUnit = Cast<ASG_UnitsBase>(AIController->GetPawn());
	if (!ControlledUnit)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("  ❌ 控制的单位无效"));
		UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
		return EBTNodeResult::Failed;
	}
	UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 控制的单位：%s"), *ControlledUnit->GetName());
	
	// ========== ✨ 新增 - 步骤4：检查是否在冷却中 ==========
	if (ControlledUnit->IsAttackOnCooldown())
	{
		UE_LOG(LogSGGameplay, Warning, TEXT("  ⏳ 单位攻击冷却中，剩余：%.2f 秒"), 
			ControlledUnit->GetCooldownRemainingTime());
		UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
		return EBTNodeResult::Failed;
	}
	
	// ========== 步骤5：检查距离 ==========
	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (BlackboardComp)
	{
		AActor* Target = Cast<AActor>(BlackboardComp->GetValueAsObject(FName("CurrentTarget")));
		if (Target)
		{
			FVector UnitLocation = ControlledUnit->GetActorLocation();
			float AttackRange = ControlledUnit->GetAttackRangeForAI();
			float ActualDistance = 0.0f;
			
			// ========== 🔧 关键修复开始：主城距离判定 ==========
			ASG_MainCityBase* MainCity = Cast<ASG_MainCityBase>(Target);
			
			// 如果目标是主城，且有检测盒，计算到盒体表面的距离
			if (MainCity && MainCity->GetAttackDetectionBox())
			{
				UBoxComponent* DetectionBox = MainCity->GetAttackDetectionBox();
				FVector BoxCenter = DetectionBox->GetComponentLocation();
				FVector BoxExtent = DetectionBox->GetScaledBoxExtent();
				// 计算近似半径（取最大轴）
				float BoxRadius = FMath::Max3(BoxExtent.X, BoxExtent.Y, BoxExtent.Z);
				
				float DistanceToCenter = FVector::Dist(UnitLocation, BoxCenter);
				// 表面距离 = 中心距离 - 半径 (最小为0)
				ActualDistance = FMath::Max(0.0f, DistanceToCenter - BoxRadius);
				
				UE_LOG(LogSGGameplay, Log, TEXT("  🏰 主城目标距离检查："));
				UE_LOG(LogSGGameplay, Log, TEXT("    检测盒半径：%.2f"), BoxRadius);
				UE_LOG(LogSGGameplay, Log, TEXT("    到表面距离：%.2f"), ActualDistance);
			}
			else
			{
				// 普通单位：直接计算到 Actor 中心的距离
				ActualDistance = FVector::Dist(UnitLocation, Target->GetActorLocation());
				UE_LOG(LogSGGameplay, Log, TEXT("  👤 普通单位距离：%.2f"), ActualDistance);
			}
			// ========== 🔧 关键修复结束 ==========
			UE_LOG(LogSGGameplay, Log, TEXT("  攻击范围：%.2f (容差 +50.0)"), AttackRange);
			if (ActualDistance > AttackRange + 50.0f)
			{
				UE_LOG(LogSGGameplay, Warning, TEXT("  ⚠️ 不在攻击范围内，任务失败"));
				UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
				return EBTNodeResult::Failed;
			}
			
			UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 在攻击范围内"));
		}
	}
	
	// ========== 步骤6：触发攻击 ==========
	UE_LOG(LogSGGameplay, Log, TEXT("  调用 PerformAttack()..."));
	bool bSuccess = ControlledUnit->PerformAttack();
	
	if (bSuccess)
	{
		UE_LOG(LogSGGameplay, Log, TEXT("  ✅ 攻击触发成功"));
		
		// ========== 🔧 修改 - 使用 DataTable 的冷却时间 ==========
		FSG_BTTaskAttackMemory* Memory = reinterpret_cast<FSG_BTTaskAttackMemory*>(NodeMemory);
		
		// 从单位获取当前攻击的冷却时间
		FSGUnitAttackDefinition CurrentAttack = ControlledUnit->GetCurrentAttackDefinition();
		Memory->RemainingWaitTime = CurrentAttack.Cooldown;
		
		UE_LOG(LogSGGameplay, Log, TEXT("  使用 DataTable 冷却时间：%.2f 秒"), Memory->RemainingWaitTime);
		UE_LOG(LogSGGameplay, Log, TEXT("  返回：InProgress（等待冷却）"));
		UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
		return EBTNodeResult::InProgress;
	}
	else
	{
		UE_LOG(LogSGGameplay, Warning, TEXT("  ⚠️ 攻击触发失败"));
		UE_LOG(LogSGGameplay, Log, TEXT("  返回：Failed"));
		UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
		return EBTNodeResult::Failed;
	}
}

/**
 * @brief Tick 更新
 * @param OwnerComp 行为树组件
 * @param NodeMemory 节点内存
 * @param DeltaSeconds 时间间隔
 * @details
 * 功能说明：
 * - 等待攻击冷却时间
 * - 冷却完成后返回成功
 */
void USG_BTTask_AttackTarget::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickTask(OwnerComp, NodeMemory, DeltaSeconds);
	
	// 获取任务内存
	FSG_BTTaskAttackMemory* Memory = reinterpret_cast<FSG_BTTaskAttackMemory*>(NodeMemory);
	
	// 减少等待时间
	Memory->RemainingWaitTime -= DeltaSeconds;
	
	// ✨ 新增 - 每 0.5 秒输出一次调试日志
	static float DebugLogTimer = 0.0f;
	DebugLogTimer += DeltaSeconds;
	if (DebugLogTimer >= 0.5f)
	{
		DebugLogTimer = 0.0f;
		UE_LOG(LogSGGameplay, Verbose, TEXT("  ⏳ 攻击冷却中：剩余 %.2f 秒"), Memory->RemainingWaitTime);
	}
	
	// 等待完成
	if (Memory->RemainingWaitTime <= 0.0f)
	{
		UE_LOG(LogSGGameplay, Log, TEXT("✓ 攻击目标任务：攻击完成"));
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
	}
}
