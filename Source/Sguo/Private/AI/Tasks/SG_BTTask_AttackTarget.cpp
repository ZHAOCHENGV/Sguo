// 📄 文件：Source/Sguo/Private/AI/Tasks/SG_BTTask_AttackTarget.cpp
// 🔧 修复 - 统一主城距离计算逻辑，解决因主城高度偏移导致的攻击中止问题

#include "AI/Tasks/SG_BTTask_AttackTarget.h"
#include "AI/SG_AIControllerBase.h"
#include "Units/SG_UnitsBase.h"
#include "AbilitySystem/SG_AttributeSet.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Buildings/SG_MainCityBase.h"
#include "Components/BoxComponent.h"
#include "Data/Type/SG_UnitDataTable.h"
#include "Debug/SG_LogCategories.h"
#include "Animation/AnimMontage.h"

/**
 * @brief 构造函数
 */
USG_BTTask_AttackTarget::USG_BTTask_AttackTarget()
{
    NodeName = TEXT("攻击目标");
    bNotifyTick = true;
}

/**
 * @brief 获取实例内存大小
 */
uint16 USG_BTTask_AttackTarget::GetInstanceMemorySize() const
{
    return sizeof(FSG_BTTaskAttackMemory);
}

/**
 * @brief 执行任务
 */
EBTNodeResult::Type USG_BTTask_AttackTarget::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    // ========== 步骤1：获取 AI Controller ==========
    AAIController* AIController = OwnerComp.GetAIOwner();
    if (!AIController)
    {
        return EBTNodeResult::Failed;
    }
    
    // ========== 步骤2：停止移动 ==========
    AIController->StopMovement();
    
    // ========== 步骤3：获取控制的单位 ==========
    ASG_UnitsBase* ControlledUnit = Cast<ASG_UnitsBase>(AIController->GetPawn());
    if (!ControlledUnit)
    {
        return EBTNodeResult::Failed;
    }

    // 获取任务内存
    FSG_BTTaskAttackMemory* Memory = reinterpret_cast<FSG_BTTaskAttackMemory*>(NodeMemory);
    Memory->RemainingWaitTime = 0.0f;

    // ========== 步骤4：检查目标有效性 ==========
    UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
    AActor* Target = nullptr;
    if (BlackboardComp)
    {
        Target = Cast<AActor>(BlackboardComp->GetValueAsObject(FName("CurrentTarget")));
        
        if (!IsTargetAlive(Target))
        {
            return EBTNodeResult::Failed;
        }
        
        if (Target)
        {
            ControlledUnit->OnStartAttackingTarget(Target);
        }
    }

    // ========== 步骤5：检查动画僵直状态 ==========
    if (ControlledUnit->bIsAttacking)
    {
        Memory->RemainingWaitTime = ControlledUnit->AttackAnimationRemainingTime;
        return EBTNodeResult::InProgress;
    }

    // ========== 步骤6：检查是否有可用技能 ==========
    if (!ControlledUnit->HasAvailableAbility())
    {
        float MinCooldown = FLT_MAX;
        for (float CD : ControlledUnit->AbilityCooldowns)
        {
            if (CD > 0.0f && CD < MinCooldown)
            {
                MinCooldown = CD;
            }
        }
        Memory->RemainingWaitTime = (MinCooldown < FLT_MAX) ? MinCooldown : 0.1f;
        return EBTNodeResult::InProgress;
    }
    
    // ========== 步骤7：检查距离（🔧 核心修复位置） ==========
    if (Target)
    {
        FVector UnitLocation = ControlledUnit->GetActorLocation();
        float AttackRange = ControlledUnit->GetAttackRangeForAI();
        float ActualDistance = 0.0f;
        
        ASG_MainCityBase* MainCity = Cast<ASG_MainCityBase>(Target);
        if (MainCity && MainCity->GetAttackDetectionBox())
        {
            // 🔧 修复：使用与 Decorator 一致的盒体表面距离计算
            // 之前的球形估算会导致因 Box 高度偏移产生的 Z 轴误差
            UBoxComponent* DetectionBox = MainCity->GetAttackDetectionBox();
            FVector BoxCenter = DetectionBox->GetComponentLocation();
            FVector BoxExtent = DetectionBox->GetScaledBoxExtent();
            
            // 计算单位在 Box 2D 平面上的最近点
            FVector ClosestPoint;
            ClosestPoint.X = FMath::Clamp(UnitLocation.X, BoxCenter.X - BoxExtent.X, BoxCenter.X + BoxExtent.X);
            ClosestPoint.Y = FMath::Clamp(UnitLocation.Y, BoxCenter.Y - BoxExtent.Y, BoxCenter.Y + BoxExtent.Y);
            // 关键：忽略 Box 的 Z 轴高度（因为 Box 被抬高了 500），强制 Z 与单位一致
            ClosestPoint.Z = UnitLocation.Z; 
            
            // 计算 2D 距离
            ActualDistance = FVector::Dist2D(UnitLocation, ClosestPoint);
        }
        else
        {
            // 普通单位使用标准距离
            ActualDistance = FVector::Dist(UnitLocation, Target->GetActorLocation());
        }

        // 允许 50.0f 的误差缓冲
        if (ActualDistance > AttackRange + 50.0f)
        {
            UE_LOG(LogSGGameplay, Warning, TEXT("⚠️ %s 攻击中止：距离过远 (实际: %.1f, 允许: %.1f)"), 
                *ControlledUnit->GetName(), ActualDistance, AttackRange + 50.0f);
            return EBTNodeResult::Failed;
        }
    }
    
    // ========== 步骤8：触发攻击 ==========
    bool bSuccess = ControlledUnit->PerformAttack();
    
    if (bSuccess)
    {
        // 默认等待时间，实际由 TickTask 监控 bIsAttacking
        Memory->RemainingWaitTime = 0.5f;
        return EBTNodeResult::InProgress;
    }
    else
    {
        if (ControlledUnit->bIsAttacking)
        {
            Memory->RemainingWaitTime = ControlledUnit->AttackAnimationRemainingTime;
            return EBTNodeResult::InProgress;
        }
        
        return EBTNodeResult::Failed;
    }
}

/**
 * @brief Tick 更新
 */
void USG_BTTask_AttackTarget::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    Super::TickTask(OwnerComp, NodeMemory, DeltaSeconds);
    
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

    /*// 检查目标存活
    UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
    if (BlackboardComp)
    {
        AActor* Target = Cast<AActor>(BlackboardComp->GetValueAsObject(FName("CurrentTarget")));
        if (!IsTargetAlive(Target))
        {
            if (!ControlledUnit->bIsAttacking)
            {
                FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
                return;
            }
        }
    }*/

    // 🔧 修改 - 攻击锁定期间不检查目标存活状态
    // 让攻击动画正常播放完毕
    UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
    if (BlackboardComp && !ControlledUnit->IsAttackLocked())
    {
        // 只有在非锁定状态下才检查目标存活
        AActor* Target = Cast<AActor>(BlackboardComp->GetValueAsObject(FName("CurrentTarget")));
        if (!IsTargetAlive(Target))
        {
            // 目标死亡且不在攻击锁定中，结束任务
            FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
            return;
        }
    }

    FSG_BTTaskAttackMemory* Memory = reinterpret_cast<FSG_BTTaskAttackMemory*>(NodeMemory);
    
    if (Memory->RemainingWaitTime > 0.0f)
    {
        Memory->RemainingWaitTime -= DeltaSeconds;
    }
    
    // 只要动画播放完毕，任务就视为成功，以便尽快进行下一次攻击判断
    if (!ControlledUnit->bIsAttacking && Memory->RemainingWaitTime <= 0.0f)
    {
        FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
    }
}

/**
 * @brief 检查目标是否存活
 */
bool USG_BTTask_AttackTarget::IsTargetAlive(AActor* Target) const
{
    if (!Target)
    {
        return false;
    }
    
    if (ASG_UnitsBase* TargetUnit = Cast<ASG_UnitsBase>(Target))
    {
        if (TargetUnit->bIsDead) return false;
        if (TargetUnit->AttributeSet && TargetUnit->AttributeSet->GetHealth() <= 0.0f) return false;
        if (!TargetUnit->CanBeTargeted()) return false;
        return true;
    }
    
    if (ASG_MainCityBase* TargetMainCity = Cast<ASG_MainCityBase>(Target))
    {
        return TargetMainCity->IsAlive();
    }
    
    return true;
}