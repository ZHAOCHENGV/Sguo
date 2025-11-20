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
	
	// ❌ 删除 - 不能直接赋值 NodeMemory
	// NodeMemory = sizeof(FSG_BTTaskAttackMemory);
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
	// ========== 输出调试信息 ==========
	UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
	UE_LOG(LogSGGameplay, Log, TEXT("🎯 攻击目标任务：开始执行"));
	
	// 获取 AI Controller
	ASG_AIControllerBase* AIController = Cast<ASG_AIControllerBase>(OwnerComp.GetAIOwner());
	if (!AIController)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("  ❌ AI Controller 无效"));
		UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
		return EBTNodeResult::Failed;
	}
	UE_LOG(LogSGGameplay, Log, TEXT("  ✓ AI Controller 有效"));
	
	// 检查主城是否被打断
	if (AIController->bIsMainCity && AIController->bAttackInterrupted)
	{
		UE_LOG(LogSGGameplay, Warning, TEXT("  ⚠️ 主城攻击被打断"));
		UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
		return EBTNodeResult::Failed;
	}
	
	// 获取控制的单位
	ASG_UnitsBase* ControlledUnit = Cast<ASG_UnitsBase>(AIController->GetPawn());
	if (!ControlledUnit)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("  ❌ 控制的单位无效"));
		UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
		return EBTNodeResult::Failed;
	}
	UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 控制的单位：%s"), *ControlledUnit->GetName());
	
	// 触发攻击
	UE_LOG(LogSGGameplay, Log, TEXT("  调用 PerformAttack()..."));
	bool bSuccess = ControlledUnit->PerformAttack();
	
	if (bSuccess)
	{
		UE_LOG(LogSGGameplay, Log, TEXT("  ✅ 攻击触发成功"));
		
		// 计算等待时间
		FSG_BTTaskAttackMemory* Memory = reinterpret_cast<FSG_BTTaskAttackMemory*>(NodeMemory);
		
		if (AttackCooldown > 0.0f)
		{
			Memory->RemainingWaitTime = AttackCooldown;
			UE_LOG(LogSGGameplay, Log, TEXT("  使用手动冷却时间：%.2f 秒"), AttackCooldown);
		}
		else
		{
			float AttackSpeed = 1.0f;
			if (ControlledUnit->AttributeSet)
			{
				AttackSpeed = ControlledUnit->AttributeSet->GetAttackSpeed();
			}
			
			Memory->RemainingWaitTime = 1.0f / FMath::Max(AttackSpeed, 0.1f);
			UE_LOG(LogSGGameplay, Log, TEXT("  自动计算冷却时间：%.2f 秒（攻速：%.2f）"), 
				Memory->RemainingWaitTime, AttackSpeed);
		}
		
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
	
	// 🔧 修改 - 添加调试日志（每次都输出）
	UE_LOG(LogSGGameplay, Verbose, TEXT("  ⏳ 攻击冷却中：剩余 %.2f 秒"), Memory->RemainingWaitTime);
	
	// 减少等待时间
	Memory->RemainingWaitTime -= DeltaSeconds;
	
	// 等待完成
	if (Memory->RemainingWaitTime <= 0.0f)
	{
		UE_LOG(LogSGGameplay, Log, TEXT("✓ 攻击目标任务：攻击完成"));
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
	}
}
