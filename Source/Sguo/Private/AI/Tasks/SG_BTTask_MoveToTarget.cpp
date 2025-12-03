// 📄 文件：Source/Sguo/Private/AI/Tasks/SG_BTTask_MoveToTarget.cpp
// 🔧 修改 - 添加远程单位支持
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
 * @details
 * 功能说明：
 * - 设置任务名称
 * - 配置黑板键过滤器
 * - 启用 Tick 通知
 */
USG_BTTask_MoveToTarget::USG_BTTask_MoveToTarget()
{
    // 设置任务名称（在行为树编辑器中显示）
    NodeName = TEXT("移动到目标");
    
    // 配置黑板键过滤器（只接受 Actor 类型的对象）
    TargetKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(USG_BTTask_MoveToTarget, TargetKey), AActor::StaticClass());
    
    // 设置为异步任务（等待移动完成，需要 Tick 通知）
    bNotifyTick = true;
}

/**
 * @brief 执行任务
 * @param OwnerComp 行为树组件
 * @param NodeMemory 节点内存
 * @return 任务执行结果
 * @details
 * 功能说明：
 * - 🔧 修改 - 根据单位类型决定是否使用槽位系统
 * - 远程单位直接移动到攻击范围内，不使用槽位
 * - 近战单位使用槽位系统，移动到预约的槽位位置
 * 详细流程：
 * 1. 获取 AI 控制器和被控单位
 * 2. 从黑板获取目标
 * 3. 根据单位类型计算移动目标位置
 * 4. 执行移动请求
 */
EBTNodeResult::Type USG_BTTask_MoveToTarget::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    // 获取 AI 控制器
    AAIController* AIController = OwnerComp.GetAIOwner();
    if (!AIController)
    {
        return EBTNodeResult::Failed;
    }

    // ✨ 新增 - 转换为我们的 AI 控制器（用于检查槽位占用配置）
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

    // ✨ 新增 - 检查是否需要占用槽位
    bool bShouldOccupySlot = SGAIController ? SGAIController->ShouldOccupyAttackSlot() : true;

    // 初始化移动目标和接受半径
    FVector MoveDestination = Target->GetActorLocation();
    float AcceptanceRadius = 50.0f;

    if (bShouldOccupySlot)
    {
        // ========== 近战单位 - 使用槽位系统 ==========
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
                    // 槽位位置需要更精确的到达判定
                    AcceptanceRadius = 30.0f;

                    UE_LOG(LogSGGameplay, Verbose, TEXT("%s 移动到槽位位置：%s"),
                        *ControlledUnit->GetName(), *SlotPosition.ToString());
                }
            }
        }
    }
    else
    {
        // ========== ✨ 新增 - 远程单位 - 移动到攻击范围边缘 ==========
        float AttackRange = ControlledUnit->GetAttackRangeForAI();
        float DistanceToTarget = FVector::Dist(ControlledUnit->GetActorLocation(), Target->GetActorLocation());

        // 如果已经在攻击范围内，不需要移动
        if (DistanceToTarget <= AttackRange)
        {
            UE_LOG(LogSGGameplay, Log, TEXT("🏹 %s 已在攻击范围内，无需移动"),
                *ControlledUnit->GetName());
            return EBTNodeResult::Succeeded;
        }

        // 计算移动到攻击范围边缘的位置
        FVector DirectionToTarget = (Target->GetActorLocation() - ControlledUnit->GetActorLocation()).GetSafeNormal();
        
        // 目标位置 = 目标 - 攻击范围 * 0.9（留一点余量）
        MoveDestination = Target->GetActorLocation() - DirectionToTarget * (AttackRange * 0.9f);
        AcceptanceRadius = 50.0f;

        UE_LOG(LogSGGameplay, Verbose, TEXT("🏹 %s 移动到攻击范围边缘：%s"),
            *ControlledUnit->GetName(), *MoveDestination.ToString());
    }

    // 检查是否已经在目的地附近
    float CurrentDistance = FVector::Dist(ControlledUnit->GetActorLocation(), MoveDestination);
    if (CurrentDistance <= AcceptanceRadius + 20.0f)
    {
        return EBTNodeResult::Succeeded;
    }

    // 执行移动请求
    EPathFollowingRequestResult::Type Result = AIController->MoveToLocation(
        MoveDestination,
        AcceptanceRadius,
        true,   // bStopOnOverlap - 到达时停止
        true,   // bUsePathfinding - 使用导航系统
        true,   // bProjectDestinationToNavigation - 将目标投射到导航网格
        true,   // bCanStrafe - 允许侧移
        nullptr // FilterClass
    );

    // 根据移动请求结果返回任务状态
    if (Result == EPathFollowingRequestResult::RequestSuccessful)
    {
        // 移动请求成功，任务进行中
        return EBTNodeResult::InProgress;
    }
    else if (Result == EPathFollowingRequestResult::AlreadyAtGoal)
    {
        // 已经在目标位置，任务成功
        return EBTNodeResult::Succeeded;
    }
    else
    {
        // 移动请求失败
        UE_LOG(LogSGGameplay, Warning, TEXT("%s 移动失败"), *ControlledUnit->GetName());
        return EBTNodeResult::Failed;
    }
}

/**
 * @brief Tick 更新
 * @param OwnerComp 行为树组件
 * @param NodeMemory 节点内存
 * @param DeltaSeconds 帧间隔
 * @details
 * 功能说明：
 * - 检测是否卡住，卡住时标记目标不可达并切换目标
 * - 检测是否已进入攻击范围
 * - 检测移动状态是否完成
 */
void USG_BTTask_MoveToTarget::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    Super::TickTask(OwnerComp, NodeMemory, DeltaSeconds);

    // 获取 AI 控制器
    AAIController* AIController = OwnerComp.GetAIOwner();
    if (!AIController)
    {
        FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
        return;
    }

    // 转换为我们的 AI 控制器
    ASG_AIControllerBase* SGAIController = Cast<ASG_AIControllerBase>(AIController);
    
    // 获取被控单位
    ASG_UnitsBase* ControlledUnit = Cast<ASG_UnitsBase>(AIController->GetPawn());
    if (!ControlledUnit)
    {
        FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
        return;
    }

    // ========== 检测是否卡住 ==========
    if (SGAIController && SGAIController->IsStuck())
    {
        AActor* CurrentTarget = SGAIController->GetCurrentTarget();

        UE_LOG(LogSGGameplay, Warning, TEXT("🚧 %s 移动卡住 (目标: %s)，正在尝试切换目标..."),
            *ControlledUnit->GetName(),
            CurrentTarget ? *CurrentTarget->GetName() : TEXT("None"));
        
        // 1. 标记当前目标不可达（加入黑名单）
        SGAIController->MarkCurrentTargetUnreachable();
        
        // 2. 停止当前移动
        AIController->StopMovement();
        
        // 3. 查找新的可达目标
        AActor* NewTarget = SGAIController->FindNearestReachableTarget();
        
        if (NewTarget && NewTarget != CurrentTarget)
        {
            // 找到新目标，切换
            SGAIController->SetCurrentTarget(NewTarget);
            UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 成功切换到新目标：%s"), *NewTarget->GetName());
            
            // 任务失败，让行为树重置并重新执行 MoveTo
            FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
        }
        else
        {
            UE_LOG(LogSGGameplay, Warning, TEXT("  ⚠️ 没有其他可达目标，只能待机"));
            FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
        }
        return;
    }

    // ========== 检查是否已进入攻击范围 ==========
    UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
    if (BlackboardComp)
    {
        AActor* Target = Cast<AActor>(BlackboardComp->GetValueAsObject(FName("CurrentTarget")));
        if (Target)
        {
            float AttackRange = ControlledUnit->GetAttackRangeForAI();
            float Distance = FVector::Dist(ControlledUnit->GetActorLocation(), Target->GetActorLocation());
            
            // 主城特殊处理（考虑主城体积）
            ASG_MainCityBase* TargetMainCity = Cast<ASG_MainCityBase>(Target);
            if (TargetMainCity && TargetMainCity->GetAttackDetectionBox())
            {
                UBoxComponent* DetectionBox = TargetMainCity->GetAttackDetectionBox();
                
                // 获取检测盒中心和范围
                FVector BoxCenter = DetectionBox->GetComponentLocation();
                FVector BoxExtent = DetectionBox->GetScaledBoxExtent();
                float BoxRadius = FMath::Max3(BoxExtent.X, BoxExtent.Y, BoxExtent.Z);
                
                // 计算到主城边缘的距离
                Distance = FMath::Max(0.0f, FVector::Dist(ControlledUnit->GetActorLocation(), BoxCenter) - BoxRadius);
            }
            
            // 如果已经在攻击范围内
            if (Distance <= AttackRange)
            {
                // 设置状态为战斗锁定
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

    // ========== 检测移动状态 ==========
    EPathFollowingStatus::Type Status = AIController->GetMoveStatus();

    // 如果状态是 Idle，说明移动已经结束
    if (Status == EPathFollowingStatus::Idle)
    {
        FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
    }
}
