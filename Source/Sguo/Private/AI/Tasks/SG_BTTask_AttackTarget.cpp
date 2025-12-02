// 📄 文件：Source/Sguo/Private/AI/Tasks/SG_BTTask_AttackTarget.cpp
// 🔧 修改 - 简化攻击任务逻辑

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
 * @details
 * 🔧 核心修改：
 * - 不再检查全局冷却，由 PerformAttack 内部处理
 * - 只检查动画僵直状态
 * - PerformAttack 内部会选择最优技能并启动独立冷却
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
    if (BlackboardComp)
    {
        AActor* Target = Cast<AActor>(BlackboardComp->GetValueAsObject(FName("CurrentTarget")));
        
        if (!IsTargetAlive(Target))
        {
            return EBTNodeResult::Failed;
        }
        
        // ✨ 新增 - 注册为攻击者
        if (Target)
        {
            ControlledUnit->OnStartAttackingTarget(Target);
        }
    }

    // ========== 步骤5：检查动画僵直状态 ==========
    // 🔧 修改 - 只检查动画状态，不检查技能冷却
    if (ControlledUnit->bIsAttacking)
    {
        // 正在播放动画，进入等待状态
        Memory->RemainingWaitTime = ControlledUnit->AttackAnimationRemainingTime;
        return EBTNodeResult::InProgress;
    }

    // ========== 步骤6：检查是否有可用技能 ==========
    // ✨ 新增 - 如果所有技能都在冷却，等待最短冷却时间
    if (!ControlledUnit->HasAvailableAbility())
    {
        // 找到最短的冷却时间
        float MinCooldown = FLT_MAX;
        for (float CD : ControlledUnit->AbilityCooldowns)
        {
            if (CD > 0.0f && CD < MinCooldown)
            {
                MinCooldown = CD;
            }
        }
        
        // 设置等待时间
        Memory->RemainingWaitTime = (MinCooldown < FLT_MAX) ? MinCooldown : 0.1f;
        return EBTNodeResult::InProgress;
    }
    
    // ========== 步骤7：检查距离 ==========
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
    
    // ========== 步骤8：触发攻击 ==========
    // PerformAttack 内部会：
    // 1. 选择优先级最高的可用技能
    // 2. 激活 GA
    // 3. 启动该技能的独立冷却
    bool bSuccess = ControlledUnit->PerformAttack();
    
    if (bSuccess)
    {
        // 等待动画播放完毕
        // 动画时长会在 GA 中通过 StartAttackAnimation 设置
        // 这里先设置一个默认值，实际值会在 TickTask 中更新
        Memory->RemainingWaitTime = 0.5f;
        return EBTNodeResult::InProgress;
    }
    else
    {
        // 如果是因为正在动画中失败，等待
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
 * @details
 * 🔧 核心修改：
 * - 只等待动画僵直结束
 * - 动画结束后立即返回成功，让行为树重新执行攻击任务
 * - 这样可以立刻检查是否有其他技能可用
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

    // 检查目标是否已死亡
    UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
    if (BlackboardComp)
    {
        AActor* Target = Cast<AActor>(BlackboardComp->GetValueAsObject(FName("CurrentTarget")));
        
        if (!IsTargetAlive(Target))
        {
            // 如果正在播放动画，等待动画播完
            if (!ControlledUnit->bIsAttacking)
            {
                FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
                return;
            }
        }
    }

    // 获取任务内存
    FSG_BTTaskAttackMemory* Memory = reinterpret_cast<FSG_BTTaskAttackMemory*>(NodeMemory);
    
    // 更新等待时间
    if (Memory->RemainingWaitTime > 0.0f)
    {
        Memory->RemainingWaitTime -= DeltaSeconds;
    }
    
    // ✨ 核心修改 - 结束条件
    // 只要动画播放完毕（bIsAttacking = false），任务就成功
    // 行为树会立刻重新进入攻击任务，检查是否有可用技能
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
        if (TargetUnit->bIsDead)
        {
            return false;
        }
        
        if (TargetUnit->AttributeSet && TargetUnit->AttributeSet->GetHealth() <= 0.0f)
        {
            return false;
        }
        
        if (!TargetUnit->CanBeTargeted())
        {
            return false;
        }
        
        return true;
    }
    
    if (ASG_MainCityBase* TargetMainCity = Cast<ASG_MainCityBase>(Target))
    {
        return TargetMainCity->IsAlive();
    }
    
    return true;
}
