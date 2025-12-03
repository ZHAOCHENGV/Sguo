// 📄 文件：Source/Sguo/Private/AI/Tasks/SG_BTTask_MoveToTarget.cpp
// 🔧 修复 - 主城攻击范围判断和移动逻辑
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
 * @brief 计算单位到主城表面的距离
 * @param UnitLocation 单位位置
 * @param MainCity 主城对象
 * @return 到主城表面的距离
 */
static float CalculateDistanceToMainCitySurface(const FVector& UnitLocation, ASG_MainCityBase* MainCity)
{
    if (!MainCity)
    {
        return FLT_MAX;
    }
    
    // 使用检测盒计算
    if (UBoxComponent* DetectionBox = MainCity->GetAttackDetectionBox())
    {
        FVector BoxCenter = DetectionBox->GetComponentLocation();
        FVector BoxExtent = DetectionBox->GetScaledBoxExtent();
        
        // 计算最近点（2D）
        FVector ClosestPoint;
        ClosestPoint.X = FMath::Clamp(UnitLocation.X, BoxCenter.X - BoxExtent.X, BoxCenter.X + BoxExtent.X);
        ClosestPoint.Y = FMath::Clamp(UnitLocation.Y, BoxCenter.Y - BoxExtent.Y, BoxCenter.Y + BoxExtent.Y);
        ClosestPoint.Z = UnitLocation.Z;
        
        float Distance = FVector::Dist2D(UnitLocation, ClosestPoint);
        
        // 如果单位在盒子内部，距离为 0
        return FMath::Max(0.0f, Distance);
    }
    
    // 没有检测盒，使用简单的圆形计算
    float CityRadius = 800.0f;
    float DistanceToCenter = FVector::Dist2D(UnitLocation, MainCity->GetActorLocation());
    return FMath::Max(0.0f, DistanceToCenter - CityRadius);
}

/**
 * @brief 执行任务
 * @details
 * 🔧 核心修复：
 * - 先检查是否已在攻击范围内，是则直接返回成功
 * - 只有不在攻击范围内时才计算移动目标
 * - 移动目标应该让单位更靠近主城，而不是远离
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
    
    // 检查是否需要占用槽位（只有非主城目标才检查）
    bool bShouldOccupySlot = false;
    if (!bTargetIsMainCity && SGAIController)
    {
        bShouldOccupySlot = SGAIController->ShouldOccupyAttackSlot();
    }

    // 获取攻击范围
    float AttackRange = ControlledUnit->GetAttackRangeForAI();
    FVector UnitLocation = ControlledUnit->GetActorLocation();

    // 初始化移动目标和接受半径
    FVector MoveDestination;
    float AcceptanceRadius;

    if (bTargetIsMainCity)
    {
        // ========== 🔧 核心修复 - 主城目标处理 ==========
        
        // 🔧 第一步：计算到主城表面的实际距离
        float CurrentDistanceToSurface = CalculateDistanceToMainCitySurface(UnitLocation, TargetMainCity);
        
        UE_LOG(LogSGGameplay, Log, TEXT("========== [%s] 主城移动检查 =========="), *UnitName);
        UE_LOG(LogSGGameplay, Log, TEXT("  攻击范围：%.0f"), AttackRange);
        UE_LOG(LogSGGameplay, Log, TEXT("  到主城表面距离：%.0f"), CurrentDistanceToSurface);
        
        // 🔧 第二步：如果已在攻击范围内，直接返回成功
        if (CurrentDistanceToSurface <= AttackRange)
        {
            UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 已在攻击范围内！设置为 Engaged 状态"));
            
            // 设置为 Engaged 状态
            if (SGAIController)
            {
                SGAIController->SetTargetEngagementState(ESGTargetEngagementState::Engaged);
            }
            
            // 停止移动（如果正在移动）
            AIController->StopMovement();
            
            UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
            return EBTNodeResult::Succeeded;
        }
        
        // 🔧 第三步：不在攻击范围内，需要移动
        UE_LOG(LogSGGameplay, Log, TEXT("  需要移动（距离 %.0f > 攻击范围 %.0f）"), CurrentDistanceToSurface, AttackRange);
        
        // 获取主城信息
        FVector CityLocation = TargetMainCity->GetActorLocation();
        float CityRadius = 576.0f;  // 默认值
        
        if (UBoxComponent* DetectionBox = TargetMainCity->GetAttackDetectionBox())
        {
            FVector BoxExtent = DetectionBox->GetScaledBoxExtent();
            CityRadius = FMath::Max(BoxExtent.X, BoxExtent.Y);
        }
        
        // 🔧 计算移动方向：从单位指向主城
        FVector DirectionToCity = (CityLocation - UnitLocation);
        DirectionToCity.Z = 0.0f;
        DirectionToCity.Normalize();
        
        if (DirectionToCity.IsNearlyZero())
        {
            DirectionToCity = FVector(1.0f, 0.0f, 0.0f);
        }
        
        // 🔧 核心修复：计算目标位置
        // 目标 = 主城中心 - 方向 * (主城半径 + 攻击范围 * 0.7)
        // 这样单位到达后，距主城表面约为 AttackRange * 0.7
        float TargetDistanceFromCenter = CityRadius + (AttackRange * 0.7f);
        MoveDestination = CityLocation - DirectionToCity * TargetDistanceFromCenter;
        MoveDestination.Z = UnitLocation.Z;
        
        // 接受半径
        AcceptanceRadius = AttackRange * 0.3f;
        
        UE_LOG(LogSGGameplay, Log, TEXT("  主城中心：%s"), *CityLocation.ToString());
        UE_LOG(LogSGGameplay, Log, TEXT("  主城半径：%.0f"), CityRadius);
        UE_LOG(LogSGGameplay, Log, TEXT("  目标距离中心：%.0f"), TargetDistanceFromCenter);
        UE_LOG(LogSGGameplay, Log, TEXT("  移动目标：%s"), *MoveDestination.ToString());
        UE_LOG(LogSGGameplay, Log, TEXT("  接受半径：%.0f"), AcceptanceRadius);
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
        // ========== 远程单位 - 移动到攻击范围边缘 ==========
        float DistanceToTarget = FVector::Dist(UnitLocation, Target->GetActorLocation());

        if (DistanceToTarget <= AttackRange)
        {
            return EBTNodeResult::Succeeded;
        }

        FVector DirectionToTarget = (Target->GetActorLocation() - UnitLocation).GetSafeNormal();
        MoveDestination = Target->GetActorLocation() - DirectionToTarget * (AttackRange * 0.9f);
        AcceptanceRadius = 50.0f;
    }

    // 🔧 发起移动请求
    UE_LOG(LogSGGameplay, Log, TEXT("  🚶 发起移动请求..."));
    
    EPathFollowingRequestResult::Type Result = AIController->MoveToLocation(
        MoveDestination,
        AcceptanceRadius,
        true,   // bStopOnOverlap
        true,   // bUsePathfinding
        true,   // bProjectDestinationToNavigation
        true,   // bCanStrafe
        nullptr // FilterClass
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
        // 🔧 修复：AlreadyAtGoal 时检查主城攻击范围
        if (bTargetIsMainCity)
        {
            float DistToSurface = CalculateDistanceToMainCitySurface(UnitLocation, TargetMainCity);
            
            if (DistToSurface <= AttackRange)
            {
                // 在攻击范围内
                if (SGAIController)
                {
                    SGAIController->SetTargetEngagementState(ESGTargetEngagementState::Engaged);
                }
                UE_LOG(LogSGGameplay, Log, TEXT("  ✓ AlreadyAtGoal 且在攻击范围内"));
                return EBTNodeResult::Succeeded;
            }
            else
            {
                // 🔧 关键修复：不在攻击范围内，但导航认为已到达
                // 这意味着导航网格可能不允许更靠近主城
                // 尝试直接移动到主城边缘
                UE_LOG(LogSGGameplay, Warning, TEXT("  ⚠️ AlreadyAtGoal 但不在攻击范围内（距离: %.0f > 范围: %.0f）"), 
                    DistToSurface, AttackRange);
                UE_LOG(LogSGGameplay, Warning, TEXT("  尝试简单移动..."));
                
                // 🔧 尝试不使用导航的直接移动
                FVector CityLocation = TargetMainCity->GetActorLocation();
                float CityRadius = 576.0f;
                if (UBoxComponent* DetectionBox = TargetMainCity->GetAttackDetectionBox())
                {
                    FVector BoxExtent = DetectionBox->GetScaledBoxExtent();
                    CityRadius = FMath::Max(BoxExtent.X, BoxExtent.Y);
                }
                
                FVector DirectionToCity = (CityLocation - UnitLocation).GetSafeNormal2D();
                FVector SimpleTarget = CityLocation - DirectionToCity * (CityRadius + AttackRange * 0.5f);
                SimpleTarget.Z = UnitLocation.Z;
                
                // 使用 SimpleMoveToLocation（不使用导航）
                // 但这可能导致穿墙，所以还是用 MoveToLocation
                Result = AIController->MoveToLocation(
                    SimpleTarget,
                    AttackRange * 0.3f,
                    false,  // bStopOnOverlap
                    false,  // bUsePathfinding - 关闭导航！
                    false,  // bProjectDestinationToNavigation
                    true,   // bCanStrafe
                    nullptr
                );
                
                if (Result == EPathFollowingRequestResult::RequestSuccessful)
                {
                    return EBTNodeResult::InProgress;
                }
                
                // 如果还是失败，就认为已经尽可能靠近了
                UE_LOG(LogSGGameplay, Warning, TEXT("  直接移动也失败，认为已尽可能靠近"));
                
                // 🔧 最后的处理：如果距离差距不大，就认为可以攻击
                if (DistToSurface <= AttackRange * 1.5f)
                {
                    if (SGAIController)
                    {
                        SGAIController->SetTargetEngagementState(ESGTargetEngagementState::Engaged);
                    }
                    UE_LOG(LogSGGameplay, Warning, TEXT("  距离在 1.5 倍攻击范围内，允许攻击"));
                    return EBTNodeResult::Succeeded;
                }
                
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
            
            // 如果已经在攻击范围内
            if (Distance <= AttackRange)
            {
                if (SGAIController)
                {
                    SGAIController->SetTargetEngagementState(ESGTargetEngagementState::Engaged);
                }
                
                AIController->StopMovement();
                
                UE_LOG(LogSGGameplay, Log, TEXT("✓ [%s] 到达攻击范围（距离: %.0f, 范围: %.0f）"), 
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
        // 移动结束后再次检查
        UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
        if (BB)
        {
            AActor* Target = Cast<AActor>(BB->GetValueAsObject(FName("CurrentTarget")));
            ASG_MainCityBase* MainCity = Cast<ASG_MainCityBase>(Target);
            
            if (MainCity)
            {
                float AttackRange = ControlledUnit->GetAttackRangeForAI();
                float Distance = CalculateDistanceToMainCitySurface(ControlledUnit->GetActorLocation(), MainCity);
                
                // 🔧 修复：使用 1.2 倍攻击范围作为容差
                if (Distance <= AttackRange * 1.5f)
                {
                    if (SGAIController)
                    {
                        SGAIController->SetTargetEngagementState(ESGTargetEngagementState::Engaged);
                    }
                    UE_LOG(LogSGGameplay, Log, TEXT("✓ [%s] 移动结束，进入攻击状态（距离: %.0f, 范围: %.0f）"), 
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
