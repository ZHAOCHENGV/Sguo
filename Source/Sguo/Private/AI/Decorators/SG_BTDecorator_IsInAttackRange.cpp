// 📄 文件：Source/Sguo/Private/AI/Decorators/SG_BTDecorator_IsInAttackRange.cpp
// 🔧 修改 - 修复主城攻击范围检测
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
 * 🔧 核心修改：
 * - 主城使用检测盒表面距离计算
 * - 普通单位使用中心点距离计算
 * - 修复进入/离开攻击范围的状态切换
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
        return false;
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
        // 未知目标类型
        return false;
    }
    
    // ========== 步骤3：获取单位位置和攻击范围 ==========
    FVector UnitLocation = ControlledUnit->GetActorLocation();
    float AttackRange = ControlledUnit->GetAttackRangeForAI();
    
    // ========== 步骤4：计算到目标的实际距离 ==========
    float ActualDistance = 0.0f;
    
    if (TargetMainCity)
    {
        // ========== 🔧 修复 - 主城距离计算 ==========
        if (UBoxComponent* DetectionBox = TargetMainCity->GetAttackDetectionBox())
        {
            FVector BoxCenter = DetectionBox->GetComponentLocation();
            FVector BoxExtent = DetectionBox->GetScaledBoxExtent();
            
            // 计算到检测盒表面的距离
            // 使用简化的轴对齐包围盒距离计算
            FVector ClosestPoint;
            
            // 计算最近点
            ClosestPoint.X = FMath::Clamp(UnitLocation.X, BoxCenter.X - BoxExtent.X, BoxCenter.X + BoxExtent.X);
            ClosestPoint.Y = FMath::Clamp(UnitLocation.Y, BoxCenter.Y - BoxExtent.Y, BoxCenter.Y + BoxExtent.Y);
            ClosestPoint.Z = UnitLocation.Z;  // 忽略 Z 轴
            
            // 计算到最近点的距离
            ActualDistance = FVector::Dist2D(UnitLocation, ClosestPoint);
            
            // 如果单位在检测盒内部，距离为 0
            if (ActualDistance < 0.0f)
            {
                ActualDistance = 0.0f;
            }
        }
        else
        {
            // 没有检测盒，使用默认计算
            float CityRadius = 800.0f;
            float DistanceToCenter = FVector::Dist(UnitLocation, TargetMainCity->GetActorLocation());
            ActualDistance = FMath::Max(0.0f, DistanceToCenter - CityRadius);
        }
    }
    else
    {
        // ========== 普通单位距离计算 ==========
        ActualDistance = FVector::Dist(UnitLocation, Target->GetActorLocation());
    }
    
    // ========== 步骤5：判断是否在攻击范围内 ==========
    // 🔧 修改 - 主城使用更大的容差
    float EffectiveTolerance = TargetMainCity ? (DistanceTolerance + 100.0f) : DistanceTolerance;
    bool bInRange = ActualDistance <= (AttackRange + EffectiveTolerance);
    
    // ========== 步骤6：更新战斗状态 ==========
    // 使用实例内存或静态变量缓存上次状态
    static TMap<ASG_UnitsBase*, bool> LastInRangeStatus;
    bool bWasInRange = LastInRangeStatus.FindOrAdd(ControlledUnit, false);
    
    if (bInRange != bWasInRange)
    {
        LastInRangeStatus[ControlledUnit] = bInRange;
        
        if (bInRange)
        {
            // 刚进入攻击范围
            AIController->StopMovement();
            
            if (SGAIController)
            {
                SGAIController->SetTargetEngagementState(ESGTargetEngagementState::Engaged);
            }
            
            UE_LOG(LogSGGameplay, Verbose, TEXT("🔒 %s 进入攻击范围（目标: %s, 距离: %.0f, 范围: %.0f）"),
                *ControlledUnit->GetName(),
                *Target->GetName(),
                ActualDistance,
                AttackRange);
        }
        else
        {
            // 离开攻击范围
            if (SGAIController)
            {
                // 只有在非攻击状态时才切换到移动状态
                if (!ControlledUnit->bIsAttacking)
                {
                    SGAIController->SetTargetEngagementState(ESGTargetEngagementState::Moving);
                }
            }
            
            UE_LOG(LogSGGameplay, Verbose, TEXT("🔓 %s 离开攻击范围（目标: %s, 距离: %.0f）"),
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
        
        // 更新黑板
        UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
        if (BlackboardComp)
        {
            BlackboardComp->SetValueAsBool(FName("IsInAttackRange"), CurrentConditionResult);
        }
        
        // 条件变化时请求重新评估
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
