// 📄 文件：Source/Sguo/Private/AI/Tasks/SG_BTTask_MoveToTarget.cpp
// 🔧 修改 - 修复主城移动逻辑
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
    
    bNotifyTick = true;
}

/**
 * @brief 执行任务
 * @details
 * 🔧 核心修改：
 * - 主城目标不使用槽位系统
 * - 主城目标直接计算到边缘的攻击位置
 * - 修复到达判定逻辑
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
        // ========== 🔧 修复 - 主城目标移动逻辑 ==========
        FVector CityLocation = TargetMainCity->GetActorLocation();
        
        // 计算主城碰撞半径
        float CityRadius = 800.0f;  // 默认值
        if (UBoxComponent* DetectionBox = TargetMainCity->GetAttackDetectionBox())
        {
            FVector BoxExtent = DetectionBox->GetScaledBoxExtent();
            CityRadius = FMath::Max(BoxExtent.X, BoxExtent.Y);
        }
        
        // 计算从主城到单位的方向
        FVector DirectionToUnit = (UnitLocation - CityLocation);
        DirectionToUnit.Z = 0.0f;  // 忽略 Z 轴
        DirectionToUnit.Normalize();
        
        // 如果方向无效（单位在主城正中心），使用默认方向
        if (DirectionToUnit.IsNearlyZero())
        {
            DirectionToUnit = FVector(1.0f, 0.0f, 0.0f);
        }
        
        // 计算站位距离：主城边缘 + 攻击范围的 70%
        float StandDistance = CityRadius + (AttackRange * 0.7f);
        
        // 计算移动目标位置
        MoveDestination = CityLocation + (DirectionToUnit * StandDistance);
        MoveDestination.Z = UnitLocation.Z;
        
        // 接受半径设置得较大，避免频繁调整
        AcceptanceRadius = AttackRange * 0.3f;
        
        // 检查是否已经在攻击范围内
        float CurrentDistanceToSurface = FVector::Dist2D(UnitLocation, CityLocation) - CityRadius;
        if (CurrentDistanceToSurface <= AttackRange)
        {
            UE_LOG(LogSGGameplay, Verbose, TEXT("🏰 %s 已在主城攻击范围内"), *ControlledUnit->GetName());
            return EBTNodeResult::Succeeded;
        }
        
        UE_LOG(LogSGGameplay, Verbose, TEXT("🏰 %s 移动到主城边缘：距离表面=%.0f，攻击范围=%.0f"),
            *ControlledUnit->GetName(),
            CurrentDistanceToSurface,
            AttackRange);
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

        // 如果已经在攻击范围内，不需要移动
        if (DistanceToTarget <= AttackRange)
        {
            return EBTNodeResult::Succeeded;
        }

        // 计算移动到攻击范围边缘的位置
        FVector DirectionToTarget = (Target->GetActorLocation() - UnitLocation).GetSafeNormal();
        MoveDestination = Target->GetActorLocation() - DirectionToTarget * (AttackRange * 0.9f);
        AcceptanceRadius = 50.0f;
    }

    // 检查是否已经在目的地附近
    float CurrentDistance = FVector::Dist(UnitLocation, MoveDestination);
    if (CurrentDistance <= AcceptanceRadius + 20.0f)
    {
        return EBTNodeResult::Succeeded;
    }

    // 执行移动请求
    EPathFollowingRequestResult::Type Result = AIController->MoveToLocation(
        MoveDestination,
        AcceptanceRadius,
        true,   // bStopOnOverlap
        true,   // bUsePathfinding
        true,   // bProjectDestinationToNavigation
        true,   // bCanStrafe
        nullptr // FilterClass
    );

    if (Result == EPathFollowingRequestResult::RequestSuccessful)
    {
        return EBTNodeResult::InProgress;
    }
    else if (Result == EPathFollowingRequestResult::AlreadyAtGoal)
    {
        return EBTNodeResult::Succeeded;
    }
    else
    {
        UE_LOG(LogSGGameplay, Warning, TEXT("%s 移动请求失败"), *ControlledUnit->GetName());
        return EBTNodeResult::Failed;
    }
}

/**
 * @brief Tick 更新
 * @details
 * 🔧 修改：
 * - 修复主城攻击范围检测
 * - 优化到达判定
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
        
        // 主城目标不标记为不可达，而是尝试调整位置
        if (CurrentTarget && CurrentTarget->IsA(ASG_MainCityBase::StaticClass()))
        {
            // 主城目标，尝试随机偏移
            SGAIController->ResetMovementTimer();
            
            // 重新执行任务
            FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
            return;
        }
        
        SGAIController->MarkCurrentTargetUnreachable();
        AIController->StopMovement();
        
        AActor* NewTarget = SGAIController->FindNearestReachableTarget();
        
        if (NewTarget && NewTarget != CurrentTarget)
        {
            SGAIController->SetCurrentTarget(NewTarget);
            FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
        }
        else
        {
            FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
        }
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
            
            // ========== 🔧 修复 - 主城距离计算 ==========
            ASG_MainCityBase* TargetMainCity = Cast<ASG_MainCityBase>(Target);
            if (TargetMainCity)
            {
                if (UBoxComponent* DetectionBox = TargetMainCity->GetAttackDetectionBox())
                {
                    FVector BoxCenter = DetectionBox->GetComponentLocation();
                    FVector BoxExtent = DetectionBox->GetScaledBoxExtent();
                    
                    // 计算到检测盒表面的 2D 距离
                    FVector ClosestPoint;
                    ClosestPoint.X = FMath::Clamp(UnitLocation.X, BoxCenter.X - BoxExtent.X, BoxCenter.X + BoxExtent.X);
                    ClosestPoint.Y = FMath::Clamp(UnitLocation.Y, BoxCenter.Y - BoxExtent.Y, BoxCenter.Y + BoxExtent.Y);
                    ClosestPoint.Z = UnitLocation.Z;
                    
                    Distance = FVector::Dist2D(UnitLocation, ClosestPoint);
                }
                else
                {
                    float CityRadius = 800.0f;
                    Distance = FMath::Max(0.0f, FVector::Dist(UnitLocation, TargetMainCity->GetActorLocation()) - CityRadius);
                }
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
                FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
                return;
            }
        }
    }

    // 检测移动状态
    EPathFollowingStatus::Type Status = AIController->GetMoveStatus();

    if (Status == EPathFollowingStatus::Idle)
    {
        FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
    }
}
