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
	 // ========== 步骤1：获取 AI Controller ==========
    AAIController* AIController = OwnerComp.GetAIOwner();
    if (!AIController) return EBTNodeResult::Failed;
    
    // ========== 步骤2：立即停止移动 ==========
    AIController->StopMovement();
    
    // ========== 步骤3：获取控制的单位 ==========
    ASG_UnitsBase* ControlledUnit = Cast<ASG_UnitsBase>(AIController->GetPawn());
    if (!ControlledUnit) return EBTNodeResult::Failed;
    
    // 获取任务内存
    FSG_BTTaskAttackMemory* Memory = reinterpret_cast<FSG_BTTaskAttackMemory*>(NodeMemory);
    Memory->RemainingWaitTime = 0.0f;

    // ========== 🔧 修改 - 步骤4：检查目标是否有效 ==========
    UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
    if (BlackboardComp)
    {
        AActor* Target = Cast<AActor>(BlackboardComp->GetValueAsObject(FName("CurrentTarget")));
        
        // ✨ 新增 - 检查目标是否已死亡
        if (!IsTargetAlive(Target))
        {
            UE_LOG(LogSGGameplay, Log, TEXT("  ⚠️ 目标已死亡或无效，任务失败"));
            return EBTNodeResult::Failed;
        }
    }

    // ========== 步骤5：智能检查状态 ==========
    
    // 情况A：单位正在播放攻击动画
    if (ControlledUnit->bIsAttacking)
    {
        UE_LOG(LogSGGameplay, Verbose, TEXT("  ⚠️ 单位正在攻击动画中，BT 任务进入等待状态"));
        return EBTNodeResult::InProgress;
    }

    // 情况B：单位处于数值冷却中
    if (ControlledUnit->IsAttackOnCooldown())
    {
        float Remaining = ControlledUnit->GetCooldownRemainingTime();
        UE_LOG(LogSGGameplay, Verbose, TEXT("  ⏳ 单位攻击冷却中，剩余：%.2f 秒，BT 任务进入等待状态"), Remaining);
        
        Memory->RemainingWaitTime = Remaining;
        return EBTNodeResult::InProgress;
    }
    
    // ========== 步骤6：检查距离 ==========
    if (BlackboardComp)
    {
        AActor* Target = Cast<AActor>(BlackboardComp->GetValueAsObject(FName("CurrentTarget")));
        if (Target)
        {
            FVector UnitLocation = ControlledUnit->GetActorLocation();
            float AttackRange = ControlledUnit->GetAttackRangeForAI();
            float ActualDistance = 0.0f;
            
            ASG_MainCityBase* MainCity = Cast<ASG_MainCityBase>(Target);
            if (MainCity && MainCity->GetAttackDetectionBox())
            {
                UBoxComponent* DetectionBox = MainCity->GetAttackDetectionBox();
                FVector BoxCenter = DetectionBox->GetComponentLocation();
                FVector BoxExtent = DetectionBox->GetScaledBoxExtent();
                float BoxRadius = FMath::Max3(BoxExtent.X, BoxExtent.Y, BoxExtent.Z);
                float DistanceToCenter = FVector::Dist(UnitLocation, BoxCenter);
                ActualDistance = FMath::Max(0.0f, DistanceToCenter - BoxRadius);
            }
            else
            {
                ActualDistance = FVector::Dist(UnitLocation, Target->GetActorLocation());
            }

            if (ActualDistance > AttackRange + 50.0f)
            {
                return EBTNodeResult::Failed;
            }
        }
    }
    
    // ========== 步骤7：触发攻击 ==========
    bool bSuccess = ControlledUnit->PerformAttack();
    
    if (bSuccess)
    {
        UE_LOG(LogSGGameplay, Log, TEXT("  ✅ 攻击触发成功"));
        
        FSGUnitAttackDefinition CurrentAttack = ControlledUnit->GetCurrentAttackDefinition();
        Memory->RemainingWaitTime = CurrentAttack.Cooldown;
        
        return EBTNodeResult::InProgress;
    }
    else
    {
        if (ControlledUnit->bIsAttacking)
        {
            return EBTNodeResult::InProgress;
        }
        
        UE_LOG(LogSGGameplay, Warning, TEXT("  ⚠️ 攻击触发失败"));
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
 * - 🔧 修改 - 增加目标死亡检测
 * - 等待攻击冷却时间
 * - 冷却完成后返回成功
 */
void USG_BTTask_AttackTarget::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	 Super::TickTask(OwnerComp, NodeMemory, DeltaSeconds);
    
    // 获取控制的单位
    AAIController* AIController = OwnerComp.GetAIOwner();
    if (!AIController)
    {
        FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
        return;
    }
    
    ASG_UnitsBase* ControlledUnit = Cast<ASG_UnitsBase>(AIController->GetPawn());
    if (!ControlledUnit)
    {
        FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
        return;
    }

    // ✨ 新增 - 检查目标是否已死亡
    UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
    if (BlackboardComp)
    {
        AActor* Target = Cast<AActor>(BlackboardComp->GetValueAsObject(FName("CurrentTarget")));
        
        // 如果目标已死亡，立即结束任务（让行为树重新评估）
        if (!IsTargetAlive(Target))
        {
            UE_LOG(LogSGGameplay, Log, TEXT("  🎯 目标已死亡，结束攻击任务"));
            
            // ✨ 新增 - 如果正在攻击动画中，不立即结束，等动画结束
            // 但标记需要在动画结束后立即寻找新目标
            if (!ControlledUnit->bIsAttacking)
            {
                // 不在攻击动画中，可以立即结束
                FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
                return;
            }
            else
            {
                // 正在攻击动画中，继续等待动画结束
                // 但在动画结束后立即返回 Failed 而不是 Succeeded
                UE_LOG(LogSGGameplay, Verbose, TEXT("    等待攻击动画结束后切换目标"));
            }
        }
    }

    // 获取任务内存
    FSG_BTTaskAttackMemory* Memory = reinterpret_cast<FSG_BTTaskAttackMemory*>(NodeMemory);
    
    // 1. 更新数值冷却时间
    if (Memory->RemainingWaitTime > 0.0f)
    {
        Memory->RemainingWaitTime -= DeltaSeconds;
    }
    
    // 2. 检查是否正在播放攻击动画
    bool bIsAnimating = ControlledUnit->bIsAttacking;

    // 3. 只有当 [数值冷却结束] 且 [单位不在攻击动作中] 时，才算任务完成
    if (Memory->RemainingWaitTime <= 0.0f && !bIsAnimating)
    {
        // 再次检查单位本身的冷却器（双重保险）
        if (!ControlledUnit->IsAttackOnCooldown())
        {
            // ✨ 新增 - 再次检查目标是否有效
            if (BlackboardComp)
            {
                AActor* Target = Cast<AActor>(BlackboardComp->GetValueAsObject(FName("CurrentTarget")));
                if (!IsTargetAlive(Target))
                {
                    // 目标已死亡，返回 Failed 触发重新寻找目标
                    UE_LOG(LogSGGameplay, Log, TEXT("  🎯 攻击结束，目标已死亡，返回 Failed"));
                    FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
                    return;
                }
            }
            
            FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
        }
    }
}


// ✨ 新增 - 检查目标是否存活
/**
 * @brief 检查目标是否存活
 * @param Target 目标 Actor
 * @return 目标是否存活
 * @details
 * 功能说明：
 * - 检查目标是否为空
 * - 检查单位是否已死亡
 * - 检查主城是否已摧毁
 */
bool USG_BTTask_AttackTarget::IsTargetAlive(AActor* Target) const
{
    // 空目标视为无效
    if (!Target)
    {
        return false;
    }
    
    // 检查是否是单位
    if (ASG_UnitsBase* TargetUnit = Cast<ASG_UnitsBase>(Target))
    {
        // 检查死亡标记
        if (TargetUnit->bIsDead)
        {
            return false;
        }
        
        // 检查生命值
        if (TargetUnit->AttributeSet && TargetUnit->AttributeSet->GetHealth() <= 0.0f)
        {
            return false;
        }
        
        // ✨ 新增 - 检查是否可被选为目标
        if (!TargetUnit->CanBeTargeted())
        {
            return false;
        }
        
        return true;
    }
    
    // 检查是否是主城
    if (ASG_MainCityBase* TargetMainCity = Cast<ASG_MainCityBase>(Target))
    {
        return TargetMainCity->IsAlive();
    }
    
    // 其他类型的目标，默认有效
    return true;
}