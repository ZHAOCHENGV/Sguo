// 📄 文件：Source/Sguo/Private/AI/Decorators/SG_BTDecorator_IsInAttackRange.cpp
// 🔧 修复 - 使用精确攻击范围判断
// ✅ 这是完整文件

#include "AI/Decorators/SG_BTDecorator_IsInAttackRange.h"
#include "AI/SG_AIControllerBase.h"
#include "Units/SG_UnitsBase.h"
#include "Buildings/SG_MainCityBase.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Debug/SG_LogCategories.h"
#include "Components/BoxComponent.h"

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
 * @brief 计算单位到主城检测盒表面的距离
 */
static float CalculateDistanceToMainCitySurface(const FVector& UnitLocation, ASG_MainCityBase* MainCity)
{
    if (!MainCity)
    {
        return FLT_MAX;
    }
    
    UBoxComponent* DetectionBox = MainCity->GetAttackDetectionBox();
    if (DetectionBox)
    {
        FVector BoxCenter = DetectionBox->GetComponentLocation();
        FVector BoxExtent = DetectionBox->GetScaledBoxExtent();
        
        FVector ClosestPoint;
        ClosestPoint.X = FMath::Clamp(UnitLocation.X, BoxCenter.X - BoxExtent.X, BoxCenter.X + BoxExtent.X);
        ClosestPoint.Y = FMath::Clamp(UnitLocation.Y, BoxCenter.Y - BoxExtent.Y, BoxCenter.Y + BoxExtent.Y);
        ClosestPoint.Z = UnitLocation.Z;
        
        float Distance = FVector::Dist2D(UnitLocation, ClosestPoint);
        return FMath::Max(0.0f, Distance);
    }
    
    float DefaultRadius = 800.0f;
    float DistanceToCenter = FVector::Dist2D(UnitLocation, MainCity->GetActorLocation());
    return FMath::Max(0.0f, DistanceToCenter - DefaultRadius);
}

/**
 * @brief 计算条件
 * @details
 * 🔧 修复：使用精确的攻击范围判断
 */
bool USG_BTDecorator_IsInAttackRange::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
    // ========== 步骤1：获取基础信息 ==========
    AAIController* AIController = OwnerComp.GetAIOwner();
    if (!AIController)
    {
        return false;
    }
    
    ASG_AIControllerBase* SGAIController = Cast<ASG_AIControllerBase>(AIController);
    
    ASG_UnitsBase* ControlledUnit = Cast<ASG_UnitsBase>(AIController->GetPawn());
    if (!ControlledUnit)
    {
        return false;
    }
    
    UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
    if (!BlackboardComp)
    {
        return false;
    }
    
    FName KeyName = TargetKey.SelectedKeyName;
    if (KeyName.IsNone())
    {
        KeyName = FName("CurrentTarget");
    }
    
    AActor* Target = Cast<AActor>(BlackboardComp->GetValueAsObject(KeyName));
    if (!Target)
    {
        return false;
    }
    
    // ========== 步骤2：检查目标有效性 ==========
    ASG_UnitsBase* TargetUnit = Cast<ASG_UnitsBase>(Target);
    ASG_MainCityBase* TargetMainCity = Cast<ASG_MainCityBase>(Target);
    
    if (TargetUnit)
    {
        if (TargetUnit->bIsDead || !TargetUnit->CanBeTargeted())
        {
            return false;
        }
    }
    else if (TargetMainCity)
    {
        if (!TargetMainCity->IsAlive())
        {
            return false;
        }
    }
    else
    {
        return false;
    }
    
    // ========== 步骤3：获取单位位置和攻击范围 ==========
    FVector UnitLocation = ControlledUnit->GetActorLocation();
    float AttackRange = ControlledUnit->GetAttackRangeForAI();
    
    // ========== 步骤4：计算到目标的实际距离 ==========
    float ActualDistance = 0.0f;
    
    if (TargetMainCity)
    {
        ActualDistance = CalculateDistanceToMainCitySurface(UnitLocation, TargetMainCity);
    }
    else
    {
        ActualDistance = FVector::Dist(UnitLocation, Target->GetActorLocation());
    }
    
    // ========== 🔧 修复 - 步骤5：使用精确攻击范围判断 ==========
    // 不再扩大 1.3 倍，只加上距离容差
    float EffectiveRange = AttackRange + DistanceTolerance;
    
    bool bInRange = ActualDistance <= EffectiveRange;
    
    // ========== 步骤6：更新战斗状态 ==========
    static TMap<ASG_UnitsBase*, bool> LastInRangeStatus;
    bool bWasInRange = LastInRangeStatus.FindOrAdd(ControlledUnit, false);
    
    if (bInRange != bWasInRange)
    {
        LastInRangeStatus[ControlledUnit] = bInRange;
        
        if (bInRange)
        {
            AIController->StopMovement();
            
            if (SGAIController)
            {
                SGAIController->SetTargetEngagementState(ESGTargetEngagementState::Engaged);
            }
            
            UE_LOG(LogSGGameplay, Log, TEXT("🔒 %s 进入攻击范围（目标: %s, 距离: %.0f, 范围: %.0f）"),
                *ControlledUnit->GetName(),
                *Target->GetName(),
                ActualDistance,
                EffectiveRange);
        }
        else
        {
            if (SGAIController)
            {
                if (!ControlledUnit->bIsAttacking)
                {
                    SGAIController->SetTargetEngagementState(ESGTargetEngagementState::Moving);
                }
            }
            
            UE_LOG(LogSGGameplay, Log, TEXT("🔓 %s 离开攻击范围（目标: %s, 距离: %.0f）"),
                *ControlledUnit->GetName(),
                *Target->GetName(),
                ActualDistance);
        }
    }
    
    return bInRange;
}

/**
 * @brief Tick 更新
 */
void USG_BTDecorator_IsInAttackRange::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);
    
    LastCheckTime += DeltaSeconds;
    if (LastCheckTime >= CheckInterval)
    {
        LastCheckTime = 0.0f;
        
        bool CurrentConditionResult = CalculateRawConditionValue(OwnerComp, NodeMemory);
        
        UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
        if (BlackboardComp)
        {
            BlackboardComp->SetValueAsBool(FName("IsInAttackRange"), CurrentConditionResult);
        }
        
        if (CurrentConditionResult != LastConditionResult)
        {
            LastConditionResult = CurrentConditionResult;
            OwnerComp.RequestExecution(this);
        }
    }
}

/**
 * @brief 获取节点描述
 */
FString USG_BTDecorator_IsInAttackRange::GetStaticDescription() const
{
    return FString::Printf(TEXT("检查是否在攻击范围内\n目标键：%s\n距离容差：%.0f"),
        *TargetKey.SelectedKeyName.ToString(),
        DistanceTolerance);
}
