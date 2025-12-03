// 📄 文件：Source/Sguo/Private/AI/Decorators/SG_BTDecorator_IsInAttackRange.cpp
// 🔧 修改 - 优化性能，减少日志输出
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
 * @brief 计算条件
 * @details
 * 🔧 修改：
 * - 大幅减少日志输出
 * - 优化状态缓存机制
 */
bool USG_BTDecorator_IsInAttackRange::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
    AAIController* AIController = OwnerComp.GetAIOwner();
    if (!AIController) return false;
    
    ASG_AIControllerBase* SGAIController = Cast<ASG_AIControllerBase>(AIController);
    
    ASG_UnitsBase* ControlledUnit = Cast<ASG_UnitsBase>(AIController->GetPawn());
    if (!ControlledUnit) return false;
    
    UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
    if (!BlackboardComp) return false;
    
    FName KeyName = TargetKey.SelectedKeyName;
    if (KeyName.IsNone()) return false;
    
    AActor* Target = Cast<AActor>(BlackboardComp->GetValueAsObject(KeyName));
    
    if (!Target)
    {
        return false;
    }
    
    // 检查目标有效性
    if (ASG_UnitsBase* TargetUnit = Cast<ASG_UnitsBase>(Target))
    {
        if (TargetUnit->bIsDead || !TargetUnit->CanBeTargeted())
        {
            return false;
        }
    }
    
    if (ASG_MainCityBase* TargetMainCity = Cast<ASG_MainCityBase>(Target))
    {
        if (!TargetMainCity->IsAlive())
        {
            return false;
        }
    }
    
    // 获取单位位置和攻击范围
    FVector UnitLocation = ControlledUnit->GetActorLocation();
    float AttackRange = ControlledUnit->GetAttackRangeForAI();
    
    // 计算到目标的实际距离
    float ActualDistance = 0.0f;
    
    ASG_MainCityBase* MainCity = Cast<ASG_MainCityBase>(Target);
    if (MainCity && MainCity->GetAttackDetectionBox())
    {
        UBoxComponent* DetectionBox = MainCity->GetAttackDetectionBox();
        FVector BoxCenter = DetectionBox->GetComponentLocation();
        FVector BoxExtent = DetectionBox->GetScaledBoxExtent();
        
        float DistanceToCenter = FVector::Dist(UnitLocation, BoxCenter);
        float BoxRadius = FMath::Max3(BoxExtent.X, BoxExtent.Y, BoxExtent.Z);
        
        ActualDistance = FMath::Max(0.0f, DistanceToCenter - BoxRadius);
    }
    else
    {
        ActualDistance = FVector::Dist(UnitLocation, Target->GetActorLocation());
    }
    
    // 判断是否在攻击范围内
    bool bInRange = ActualDistance <= (AttackRange + DistanceTolerance);
    
    // 🔧 修改 - 使用静态变量缓存状态，只在变化时处理
    static TMap<ASG_UnitsBase*, bool> LastInRangeStatus;
    bool bWasInRange = LastInRangeStatus.FindOrAdd(ControlledUnit, false);
    
    if (bInRange && !bWasInRange)
    {
        // 刚进入攻击范围
        AIController->StopMovement();
        
        if (SGAIController)
        {
            SGAIController->SetTargetEngagementState(ESGTargetEngagementState::Engaged);
        }
        
        // 🔧 修改 - 只在状态变化时输出日志
        UE_LOG(LogSGGameplay, Verbose, TEXT("🔒 %s 进入攻击范围"),
            *ControlledUnit->GetName());
    }
    else if (!bInRange && bWasInRange)
    {
        // 离开攻击范围
        if (SGAIController)
        {
            SGAIController->SetTargetEngagementState(ESGTargetEngagementState::Moving);
        }
    }
    
    LastInRangeStatus[ControlledUnit] = bInRange;
    
    return bInRange;
}

/**
 * @brief Tick 更新
 * @details
 * 🔧 修改：
 * - 减少日志输出
 * - 优化检查频率
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
        
        // 条件变化时，强制重新评估
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
