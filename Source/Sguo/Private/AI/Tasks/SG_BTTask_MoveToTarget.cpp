// 📄 文件：Source/Sguo/Private/AI/Tasks/SG_BTTask_MoveToTarget.cpp
// 🔧 修复 - 移除错误的 1.3 倍攻击范围扩大，改为正确的移动和攻击范围判断
// ✅ 这是完整文件

#include "AI/Tasks/SG_BTTask_MoveToTarget.h"
#include "AI/SG_AIControllerBase.h"
#include "Units/SG_UnitsBase.h"
#include "Buildings/SG_MainCityBase.h"
#include "AbilitySystem/SG_AttributeSet.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "NavigationSystem.h"
#include "AI/SG_CombatTargetManager.h"
#include "Navigation/PathFollowingComponent.h"
#include "Debug/SG_LogCategories.h"
#include "Components/BoxComponent.h"
#include "Kismet/KismetMathLibrary.h"

/**
 * @brief 构造函数
 */
USG_BTTask_MoveToTarget::USG_BTTask_MoveToTarget()
{
    NodeName = TEXT("移动到目标");
    
    TargetKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(USG_BTTask_MoveToTarget, TargetKey), AActor::StaticClass());
    TargetKey.SelectedKeyName = FName("CurrentTarget");
    
    bNotifyTick = true;
}

/**
 * @brief 计算单位到主城检测盒表面的距离
 * @param UnitLocation 单位位置
 * @param MainCity 主城对象
 * @return 到主城表面的距离（2D）
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
        
        // 计算最近点（2D）
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
 * @brief 计算单位到主城检测盒表面的最近点
 */
static FVector CalculateClosestPointOnMainCitySurface(const FVector& UnitLocation, ASG_MainCityBase* MainCity)
{
    if (!MainCity)
    {
        return UnitLocation;
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
        
        return ClosestPoint;
    }
    
    FVector CityLocation = MainCity->GetActorLocation();
    CityLocation.Z = UnitLocation.Z;
    return CityLocation;
}

/**
 * @brief 执行任务
 * @details
 * 🔧 核心修复：
 * - 使用精确的攻击范围判断（不再扩大 1.3 倍）
 * - 只有真正在攻击范围内才返回成功
 */
EBTNodeResult::Type USG_BTTask_MoveToTarget::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    // 获取 AI 控制器
    AAIController* AIController = OwnerComp.GetAIOwner();
    if (!AIController)
    {
        return EBTNodeResult::Failed;
    }

    ASG_AIControllerBase* SGAIController = Cast<ASG_AIControllerBase>(AIController);
    
    // 获取被控单位
    ASG_UnitsBase* ControlledUnit = Cast<ASG_UnitsBase>(AIController->GetPawn());
    if (!ControlledUnit)
    {
        return EBTNodeResult::Failed;
    }
    
    FString UnitName = ControlledUnit->GetName();
    
    // 获取黑板组件
    UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
    if (!BlackboardComp)
    {
        return EBTNodeResult::Failed;
    }
    
    // 从黑板获取目标
    AActor* Target = Cast<AActor>(BlackboardComp->GetValueAsObject(TargetKey.SelectedKeyName));
    if (!Target)
    {
        return EBTNodeResult::Failed;
    }

    // ========== 检查目标类型 ==========
    ASG_MainCityBase* TargetMainCity = Cast<ASG_MainCityBase>(Target);
    bool bTargetIsMainCity = (TargetMainCity != nullptr);
    
    bool bShouldOccupySlot = false;
    if (!bTargetIsMainCity && SGAIController)
    {
        bShouldOccupySlot = SGAIController->ShouldOccupyAttackSlot();
    }

    // 获取攻击范围
    float AttackRange = ControlledUnit->GetAttackRangeForAI();
    FVector UnitLocation = ControlledUnit->GetActorLocation();

    FVector MoveDestination;
    float AcceptanceRadius;

    if (bTargetIsMainCity)
    {
        // ========== 主城目标处理 ==========
        
        float CurrentDistanceToSurface = CalculateDistanceToMainCitySurface(UnitLocation, TargetMainCity);
        
        FVector BoxExtent = FVector::ZeroVector;
        FVector BoxCenter = FVector::ZeroVector;
        if (UBoxComponent* DetectionBox = TargetMainCity->GetAttackDetectionBox())
        {
            BoxExtent = DetectionBox->GetScaledBoxExtent();
            BoxCenter = DetectionBox->GetComponentLocation();
        }
        
        UE_LOG(LogSGGameplay, Log, TEXT("========== [%s] 主城移动检查 =========="), *UnitName);
        UE_LOG(LogSGGameplay, Log, TEXT("  单位位置：%s"), *UnitLocation.ToString());
        UE_LOG(LogSGGameplay, Log, TEXT("  检测盒中心：%s"), *BoxCenter.ToString());
        UE_LOG(LogSGGameplay, Log, TEXT("  检测盒尺寸：%s"), *BoxExtent.ToString());
        UE_LOG(LogSGGameplay, Log, TEXT("  攻击范围：%.0f"), AttackRange);
        UE_LOG(LogSGGameplay, Log, TEXT("  到检测盒表面距离：%.0f"), CurrentDistanceToSurface);
        
        // 🔧 核心修复：使用精确的攻击范围判断
        if (CurrentDistanceToSurface <= AttackRange)
        {
            UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 已在攻击范围内（距离 %.0f <= 范围 %.0f）！"), 
                CurrentDistanceToSurface, AttackRange);
            
            if (SGAIController)
            {
                SGAIController->SetTargetEngagementState(ESGTargetEngagementState::Engaged);
            }
            
            AIController->StopMovement();
            UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
            return EBTNodeResult::Succeeded;
        }
        
        UE_LOG(LogSGGameplay, Log, TEXT("  需要移动（距离 %.0f > 攻击范围 %.0f）"), 
            CurrentDistanceToSurface, AttackRange);
        
        // 获取检测盒表面最近点
        FVector ClosestPointOnSurface = CalculateClosestPointOnMainCitySurface(UnitLocation, TargetMainCity);
        
        // 计算方向
        FVector DirectionFromSurface = (UnitLocation - ClosestPointOnSurface).GetSafeNormal2D();
        
        if (DirectionFromSurface.IsNearlyZero())
        {
            FVector CityCenter = TargetMainCity->GetActorLocation();
            DirectionFromSurface = (UnitLocation - CityCenter).GetSafeNormal2D();
            
            if (DirectionFromSurface.IsNearlyZero())
            {
                DirectionFromSurface = FVector(1.0f, 0.0f, 0.0f);
            }
        }
        
        // 🔧 核心修复：移动目标 = 表面最近点 + 方向 * (攻击范围 * 0.8)
        // 目标距离表面 = 攻击范围 * 0.8，确保在攻击范围内
        float StandOffDistance = AttackRange * 0.6f;
        MoveDestination = ClosestPointOnSurface + DirectionFromSurface * StandOffDistance;
        MoveDestination.Z = UnitLocation.Z;
        
        // 🔧 修复：接受半径设置为较小的值
        AcceptanceRadius = 1.0f;
        
        UE_LOG(LogSGGameplay, Log, TEXT("  表面最近点：%s"), *ClosestPointOnSurface.ToString());
        UE_LOG(LogSGGameplay, Log, TEXT("  站位距离（从表面）：%.0f"), StandOffDistance);
        UE_LOG(LogSGGameplay, Log, TEXT("  移动目标：%s"), *MoveDestination.ToString());
        UE_LOG(LogSGGameplay, Log, TEXT("  接受半径：%.0f"), AcceptanceRadius);
        
        // 🔧 验证计算结果
        float ExpectedDistanceAfterMove = CalculateDistanceToMainCitySurface(MoveDestination, TargetMainCity);
        UE_LOG(LogSGGameplay, Log, TEXT("  预计到达后距表面：%.0f（应 <= 攻击范围 %.0f）"), 
            ExpectedDistanceAfterMove, AttackRange);
    }
    else if (bShouldOccupySlot)
    {
        // ========== 近战单位 - 使用槽位系统 ==========
        MoveDestination = Target->GetActorLocation();
        AcceptanceRadius = 30.0f;
        
        if (UWorld* World = GetWorld())
        {
            if (USG_CombatTargetManager* CombatManager = World->GetSubsystem<USG_CombatTargetManager>())
            {
                FVector SlotPosition;
                if (CombatManager->GetReservedSlotPosition(ControlledUnit, Target, SlotPosition))
                {
                    MoveDestination = SlotPosition;
                }
            }
        }
    }
    else
    {
        // ========== 远程单位 ==========
        float DistanceToTarget = FVector::Dist(UnitLocation, Target->GetActorLocation());

        if (DistanceToTarget <= AttackRange)
        {
            return EBTNodeResult::Succeeded;
        }

        FVector DirectionToTarget = (Target->GetActorLocation() - UnitLocation).GetSafeNormal();
        MoveDestination = Target->GetActorLocation() - DirectionToTarget * (AttackRange * 0.9f);
        AcceptanceRadius = 50.0f;
    }

    // 发起移动请求
    UE_LOG(LogSGGameplay, Log, TEXT("  🚶 发起移动请求..."));
    
    EPathFollowingRequestResult::Type Result = AIController->MoveToLocation(
        MoveDestination,
        AcceptanceRadius,
        true,
        true,
        true,
        true,
        nullptr
    );

    FString ResultStr;
    switch (Result)
    {
        case EPathFollowingRequestResult::RequestSuccessful:
            ResultStr = TEXT("RequestSuccessful");
            break;
        case EPathFollowingRequestResult::AlreadyAtGoal:
            ResultStr = TEXT("AlreadyAtGoal");
            break;
        case EPathFollowingRequestResult::Failed:
            ResultStr = TEXT("Failed");
            break;
        default:
            ResultStr = TEXT("Unknown");
            break;
    }
    
    UE_LOG(LogSGGameplay, Log, TEXT("  移动请求结果：%s"), *ResultStr);
    UE_LOG(LogSGGameplay, Log, TEXT("========================================"));

    if (Result == EPathFollowingRequestResult::RequestSuccessful)
    {
        return EBTNodeResult::InProgress;
    }
    else if (Result == EPathFollowingRequestResult::AlreadyAtGoal)
    {
        if (bTargetIsMainCity)
        {
            float DistToSurface = CalculateDistanceToMainCitySurface(UnitLocation, TargetMainCity);
            
            // 🔧 修复：使用精确攻击范围判断
            if (DistToSurface <= AttackRange)
            {
                if (SGAIController)
                {
                    SGAIController->SetTargetEngagementState(ESGTargetEngagementState::Engaged);
                }
                UE_LOG(LogSGGameplay, Log, TEXT("  ✓ AlreadyAtGoal 且在攻击范围内（距离: %.0f <= 范围: %.0f）"), 
                    DistToSurface, AttackRange);
                return EBTNodeResult::Succeeded;
            }
            else
            {
                // 导航无法更靠近，返回失败让行为树重试
                UE_LOG(LogSGGameplay, Warning, TEXT("  ⚠️ AlreadyAtGoal 但不在攻击范围内（距离: %.0f > 范围: %.0f）"), 
                    DistToSurface, AttackRange);
                return EBTNodeResult::Failed;
            }
        }
        
        return EBTNodeResult::Succeeded;
    }
    else
    {
        UE_LOG(LogSGGameplay, Warning, TEXT("❌ [%s] 移动请求失败"), *UnitName);
        return EBTNodeResult::Failed;
    }
}

/**
 * @brief Tick 更新
 * @details
 * 🔧 核心修复：
 * - 使用精确攻击范围判断
 * - 不再扩大 1.3 倍
 */
void USG_BTTask_MoveToTarget::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    Super::TickTask(OwnerComp, NodeMemory, DeltaSeconds);

    AAIController* AIController = OwnerComp.GetAIOwner();
    if (!AIController)
    {
        FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
        return;
    }

    ASG_AIControllerBase* SGAIController = Cast<ASG_AIControllerBase>(AIController);
    
    ASG_UnitsBase* ControlledUnit = Cast<ASG_UnitsBase>(AIController->GetPawn());
    if (!ControlledUnit)
    {
        FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
        return;
    }

    // 检测是否卡住
    if (SGAIController && SGAIController->IsStuck())
    {
        AActor* CurrentTarget = SGAIController->GetCurrentTarget();
        
        if (CurrentTarget && CurrentTarget->IsA(ASG_MainCityBase::StaticClass()))
        {
            SGAIController->ResetMovementTimer();
            FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
            return;
        }
        
        SGAIController->MarkCurrentTargetUnreachable();
        AIController->StopMovement();
        
        AActor* NewTarget = SGAIController->FindNearestReachableTarget();
        
        if (NewTarget && NewTarget != CurrentTarget)
        {
            SGAIController->SetCurrentTarget(NewTarget);
        }
        
        FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
        return;
    }

    // 检查是否已进入攻击范围
    UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
    if (BlackboardComp)
    {
        AActor* Target = Cast<AActor>(BlackboardComp->GetValueAsObject(FName("CurrentTarget")));
        if (Target)
        {
            float AttackRange = ControlledUnit->GetAttackRangeForAI();
            FVector UnitLocation = ControlledUnit->GetActorLocation();
            float Distance = 0.0f;
            
            ASG_MainCityBase* TargetMainCity = Cast<ASG_MainCityBase>(Target);
            if (TargetMainCity)
            {
                Distance = CalculateDistanceToMainCitySurface(UnitLocation, TargetMainCity);
            }
            else
            {
                Distance = FVector::Dist(UnitLocation, Target->GetActorLocation());
            }
            
            // 🔧 修复：使用精确攻击范围，不扩大
            if (Distance <= AttackRange)
            {
                if (SGAIController)
                {
                    SGAIController->SetTargetEngagementState(ESGTargetEngagementState::Engaged);
                }
                
                AIController->StopMovement();
                
                UE_LOG(LogSGGameplay, Log, TEXT("✓ [%s] 到达攻击范围（距离: %.0f <= 范围: %.0f）"), 
                    *ControlledUnit->GetName(), Distance, AttackRange);
                
                FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
                return;
            }
        }
    }

    // 检测移动状态
    EPathFollowingStatus::Type Status = AIController->GetMoveStatus();

    if (Status == EPathFollowingStatus::Idle)
    {
        UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
        if (BB)
        {
            AActor* Target = Cast<AActor>(BB->GetValueAsObject(FName("CurrentTarget")));
            ASG_MainCityBase* MainCity = Cast<ASG_MainCityBase>(Target);
            
            if (MainCity)
            {
                float AttackRange = ControlledUnit->GetAttackRangeForAI();
                float Distance = CalculateDistanceToMainCitySurface(ControlledUnit->GetActorLocation(), MainCity);
                
                // 🔧 修复：使用精确攻击范围
                if (Distance <= AttackRange)
                {
                    if (SGAIController)
                    {
                        SGAIController->SetTargetEngagementState(ESGTargetEngagementState::Engaged);
                    }
                    UE_LOG(LogSGGameplay, Log, TEXT("✓ [%s] 移动结束，进入攻击状态（距离: %.0f <= 范围: %.0f）"), 
                        *ControlledUnit->GetName(), Distance, AttackRange);
                    FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
                    return;
                }
                else
                {
                    UE_LOG(LogSGGameplay, Warning, TEXT("⚠️ [%s] 移动结束但未到达攻击范围（距离: %.0f > 范围: %.0f）"), 
                        *ControlledUnit->GetName(), Distance, AttackRange);
                    FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
                    return;
                }
            }
        }
        
        FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
    }
}
