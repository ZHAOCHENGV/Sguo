// 📄 文件：Source/Sguo/Private/AI/Services/SG_BTService_UpdateTarget.cpp
// 🔧 修改 - 优化目标更新逻辑
// ✅ 这是完整文件

#include "AI/Services/SG_BTService_UpdateTarget.h"

#include "AbilitySystem/SG_AttributeSet.h"
#include "AI/SG_AIControllerBase.h"
#include "Units/SG_UnitsBase.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Buildings/SG_MainCityBase.h"
#include "Debug/SG_LogCategories.h"

/**
 * @brief 构造函数
 */
USG_BTService_UpdateTarget::USG_BTService_UpdateTarget()
{
    NodeName = TEXT("更新目标");
    
    // 🔧 修改 - 增加更新间隔，减少性能开销
    Interval = 0.3f;  // 从 0.2 秒改为 0.3 秒
    RandomDeviation = 0.1f;
    
    TargetKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(USG_BTService_UpdateTarget, TargetKey), AActor::StaticClass());
}

/**
 * @brief Tick 更新
 * @details
 * 🔧 核心修改：
 * 1. 检查是否允许切换目标（使用 CanSwitchTarget）
 * 2. Engaged 状态下只验证目标有效性，不切换
 * 3. 减少不必要的日志输出
 */
void USG_BTService_UpdateTarget::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);
    
    ASG_AIControllerBase* AIController = Cast<ASG_AIControllerBase>(OwnerComp.GetAIOwner());
    if (!AIController)
    {
        return;
    }
    
    ASG_UnitsBase* ControlledUnit = Cast<ASG_UnitsBase>(AIController->GetPawn());
    if (!ControlledUnit)
    {
        return;
    }
    
    UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
    if (!BlackboardComp)
    {
        return;
    }
    
    AActor* CurrentTarget = Cast<AActor>(BlackboardComp->GetValueAsObject(TargetKey.SelectedKeyName));
    
    // ========== 1. 验证当前目标是否有效 ==========
    bool bIsTargetValid = false;
    if (CurrentTarget)
    {
        ASG_UnitsBase* TargetUnit = Cast<ASG_UnitsBase>(CurrentTarget);
        if (TargetUnit)
        {
            bIsTargetValid = !TargetUnit->bIsDead && 
                             TargetUnit->CanBeTargeted() && 
                             (!TargetUnit->AttributeSet || TargetUnit->AttributeSet->GetHealth() > 0.0f);
        }
        else
        {
            ASG_MainCityBase* TargetMainCity = Cast<ASG_MainCityBase>(CurrentTarget);
            if (TargetMainCity)
            {
                bIsTargetValid = TargetMainCity->IsAlive();
            }
        }
    }

    // ========== 2. 目标无效时查找新目标 ==========
    if (!bIsTargetValid)
    {
        // 目标无效，必须查找新目标
        AActor* NewTarget = AIController->FindNearestTarget();
        if (NewTarget)
        {
            BlackboardComp->SetValueAsObject(TargetKey.SelectedKeyName, NewTarget);
            AIController->SetCurrentTarget(NewTarget);
            
            // 重置攻击状态
            if (ControlledUnit->bIsAttacking)
            {
                BlackboardComp->SetValueAsBool(FName("IsInAttackRange"), false);
                ControlledUnit->bIsAttacking = false;
            }
        }
        else
        {
            if (CurrentTarget != nullptr)
            {
                BlackboardComp->ClearValue(TargetKey.SelectedKeyName);
                AIController->SetCurrentTarget(nullptr);
            }
        }
        return;
    }

    // ========== 3. 🔧 修改 - 检查是否允许切换目标 ==========
    // 如果处于 Engaged 状态（正在攻击），不允许切换目标
    if (!AIController->CanSwitchTarget())
    {
        // 正在攻击，不切换目标
        return;
    }

    // ========== 4. 动态择优（仅在非攻击状态下） ==========
    // 只有在移动中或搜索中才检测更好的目标
    // 这部分逻辑已经移到 AIController::CheckForBetterTargetWhileMoving
    // 这里不再重复处理，避免性能开销
}
