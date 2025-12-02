// 📄 SG_BTTask_MoveToTarget.cpp - 完整文件

/**
 * @file SG_BTTask_MoveToTarget.cpp
 * @brief 行为树任务：移动到目标实现
 */

#include "AI/Tasks/SG_BTTask_MoveToTarget.h"
#include "AI/SG_AIControllerBase.h"
#include "Units/SG_UnitsBase.h"
#include "Buildings/SG_MainCityBase.h"  // ✨ 新增
#include "AbilitySystem/SG_AttributeSet.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "Debug/SG_LogCategories.h"
#include "Components/BoxComponent.h"  // ✨ 新增
#include "Kismet/KismetMathLibrary.h"

/**
 * @brief 构造函数
 * @details
 * 功能说明：
 * - 设置任务名称
 * - 配置节点参数
 */
USG_BTTask_MoveToTarget::USG_BTTask_MoveToTarget()
{
	// 设置任务名称
	NodeName = TEXT("移动到目标");
	
	// 配置黑板键过滤器（只接受 Object 类型）
	TargetKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(USG_BTTask_MoveToTarget, TargetKey), AActor::StaticClass());
	
	// 设置为异步任务（等待移动完成）
	bNotifyTick = true;
}

/**
 * @brief 执行任务
 * @param OwnerComp 行为树组件
 * @param NodeMemory 节点内存
 * @return 任务执行结果
 * @details
 * 🔧 核心修改：
 * - 开始移动时重置移动计时器
 * - 设置目标状态为 Moving
 */
EBTNodeResult::Type USG_BTTask_MoveToTarget::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	   AAIController* AIController = OwnerComp.GetAIOwner();
    if (!AIController)
    {
        return EBTNodeResult::Failed;
    }
    
    ASG_UnitsBase* ControlledUnit = Cast<ASG_UnitsBase>(AIController->GetPawn());
    if (!ControlledUnit)
    {
        return EBTNodeResult::Failed;
    }
    
    UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
    if (!BlackboardComp)
    {
        return EBTNodeResult::Failed;
    }
    
    AActor* Target = Cast<AActor>(BlackboardComp->GetValueAsObject(TargetKey.SelectedKeyName));
    if (!Target)
    {
        return EBTNodeResult::Failed;
    }

    // ✨ 新增 - 获取预约的槽位位置
    FVector MoveDestination = Target->GetActorLocation();
    float AcceptanceRadius = 50.0f;

    if (UWorld* World = GetWorld())
    {
        USG_CombatTargetManager* CombatManager = World->GetSubsystem<USG_CombatTargetManager>();
        if (CombatManager)
        {
            FVector SlotPosition;
            if (CombatManager->GetReservedSlotPosition(ControlledUnit, Target, SlotPosition))
            {
                // 使用槽位位置作为目的地
                MoveDestination = SlotPosition;
                AcceptanceRadius = 30.0f;  // 槽位位置需要更精确

                UE_LOG(LogSGGameplay, Verbose, TEXT("%s 移动到槽位位置：%s"),
                    *ControlledUnit->GetName(), *SlotPosition.ToString());
            }
        }
    }

    // 检查是否已经在目的地附近
    float CurrentDistance = FVector::Dist(ControlledUnit->GetActorLocation(), MoveDestination);
    if (CurrentDistance <= AcceptanceRadius + 20.0f)
    {
        return EBTNodeResult::Succeeded;
    }

    // 执行移动
    EPathFollowingRequestResult::Type Result = AIController->MoveToLocation(
        MoveDestination,
        AcceptanceRadius,
        true,   // bStopOnOverlap
        true,   // bUsePathfinding
        true,   // bProjectDestinationToNavigation
        true,   // bCanStrafe
        nullptr
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
        // 移动失败，可能需要切换目标
        UE_LOG(LogSGGameplay, Warning, TEXT("%s 移动到槽位失败"), *ControlledUnit->GetName());
        return EBTNodeResult::Failed;
    }
}

/**
 * @brief Tick 更新
 * @param OwnerComp 行为树组件
 * @param NodeMemory 节点内存
 * @param DeltaSeconds 帧间隔
 * @details
 * 🔧 核心修改：
 * - 检测是否卡住
 * - 卡住时标记目标不可达，切换目标
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

    // ✨ 新增 - 转换为我们的 AI 控制器
    ASG_AIControllerBase* SGAIController = Cast<ASG_AIControllerBase>(AIController);
    
    ASG_UnitsBase* ControlledUnit = Cast<ASG_UnitsBase>(AIController->GetPawn());
    if (!ControlledUnit)
    {
        FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
        return;
    }

    // ✨ 新增 - 检测是否卡住
    if (SGAIController && SGAIController->IsStuck())
    {
        UE_LOG(LogSGGameplay, Warning, TEXT("🚧 %s 被卡住，标记目标不可达并切换目标"),
            *ControlledUnit->GetName());
        
        // 标记当前目标不可达
        SGAIController->MarkCurrentTargetUnreachable();
        
        // 停止当前移动
        AIController->StopMovement();
        
        // 查找新的可达目标
        AActor* NewTarget = SGAIController->FindNearestReachableTarget();
        if (NewTarget)
        {
            SGAIController->SetCurrentTarget(NewTarget);
            UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 切换到新目标：%s"), *NewTarget->GetName());
        }
        else
        {
            UE_LOG(LogSGGameplay, Warning, TEXT("  ⚠️ 没有可达目标，继续等待"));
        }
        
        // 任务失败，让行为树重新评估
        FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
        return;
    }

    // ✨ 新增 - 检查是否已进入攻击范围
    UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
    if (BlackboardComp)
    {
        AActor* Target = Cast<AActor>(BlackboardComp->GetValueAsObject(FName("CurrentTarget")));
        if (Target)
        {
            float AttackRange = ControlledUnit->GetAttackRangeForAI();
            float Distance = FVector::Dist(ControlledUnit->GetActorLocation(), Target->GetActorLocation());
            
            // 主城特殊处理
            ASG_MainCityBase* TargetMainCity = Cast<ASG_MainCityBase>(Target);
            if (TargetMainCity && TargetMainCity->GetAttackDetectionBox())
            {
                UBoxComponent* DetectionBox = TargetMainCity->GetAttackDetectionBox();
                FVector BoxCenter = DetectionBox->GetComponentLocation();
                FVector BoxExtent = DetectionBox->GetScaledBoxExtent();
                float BoxRadius = FMath::Max3(BoxExtent.X, BoxExtent.Y, BoxExtent.Z);
                Distance = FMath::Max(0.0f, FVector::Dist(ControlledUnit->GetActorLocation(), BoxCenter) - BoxRadius);
            }
            
            // 如果已经在攻击范围内
            if (Distance <= AttackRange)
            {
                // 设置为战斗状态
                if (SGAIController)
                {
                    SGAIController->SetTargetEngagementState(ESGTargetEngagementState::Engaged);
                }
                
                // 停止移动
                AIController->StopMovement();
                
                // 任务成功
                FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
                return;
            }
        }
    }

    // 获取当前的移动状态
    EPathFollowingStatus::Type Status = AIController->GetMoveStatus();

    // 如果状态是 Idle，说明移动已经结束
    if (Status == EPathFollowingStatus::Idle)
    {
        FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
    }
}
