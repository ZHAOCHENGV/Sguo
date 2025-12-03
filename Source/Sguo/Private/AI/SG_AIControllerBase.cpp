// 📄 文件：Source/Sguo/Private/AI/SG_AIControllerBase.cpp
// 🔧 修改 - 完整修复
// ✅ 这是完整文件

#include "AI/SG_AIControllerBase.h"
#include "AbilitySystem/SG_AttributeSet.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Units/SG_UnitsBase.h"
#include "Kismet/GameplayStatics.h"
#include "Buildings/SG_MainCityBase.h"
#include "Debug/SG_LogCategories.h"
#include "NavigationSystem.h"
#include "AI/SG_CombatTargetManager.h"
#include "AI/SG_TargetingSubsystem.h"
#include "Components/BoxComponent.h"


// ========== 黑板键名称定义 ==========
const FName ASG_AIControllerBase::BB_CurrentTarget = TEXT("CurrentTarget");
const FName ASG_AIControllerBase::BB_IsInAttackRange = TEXT("IsInAttackRange");
const FName ASG_AIControllerBase::BB_IsTargetLocked = TEXT("IsTargetLocked");
const FName ASG_AIControllerBase::BB_IsTargetMainCity = TEXT("IsTargetMainCity");



// ========== 构造函数 ==========
ASG_AIControllerBase::ASG_AIControllerBase()
{
    PrimaryActorTick.bCanEverTick = true;
    bWantsPlayerState = false;
    bSetControlRotationFromPawnOrientation = false;
}

// ========== BeginPlay ==========
void ASG_AIControllerBase::BeginPlay()
{
    Super::BeginPlay();
}

/**
 * @brief Tick 更新
 * @param DeltaTime 帧间隔
 * @details
 * 功能说明：
 * - 更新移动计时器
 * - 周期性清理不可达列表
 * - 移动中检测更好目标
 * - ✨ 新增：攻击主城时检测敌方单位
 */
void ASG_AIControllerBase::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    // 更新移动计时器
    UpdateMovementTimer(DeltaTime);
    
    // 周期性清理不可达列表
    UnreachableClearTimer += DeltaTime;
    if (UnreachableClearTimer >= UnreachableClearInterval)
    {
        UnreachableClearTimer = 0.0f;
        ClearUnreachableTargets();
    }

    // ✨ 新增 - 攻击主城时检测敌方单位
    TargetSwitchCheckTimer += DeltaTime;
    if (TargetSwitchCheckTimer >= TargetSwitchCheckInterval)
    {
        TargetSwitchCheckTimer = 0.0f;
        
        // 移动中或攻击主城时都检测更好目标
        if (TargetEngagementState == ESGTargetEngagementState::Moving)
        {
            CheckForBetterTargetWhileMoving();
        }
        else if (TargetEngagementState == ESGTargetEngagementState::Engaged)
        {
            // 只有攻击主城时才检测敌方单位
            CheckForEnemyUnitsWhileAttackingMainCity();
        }
    }
}

// ========== OnPossess ==========
void ASG_AIControllerBase::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    
    // 初始化位置记录
    LastPosition = InPawn->GetActorLocation();
    
    // 步骤1：确定要使用的行为树
    UBehaviorTree* BehaviorTreeToUse = nullptr;
    
    ASG_UnitsBase* ControlledUnit = Cast<ASG_UnitsBase>(InPawn);
    if (ControlledUnit)
    {
        BehaviorTreeToUse = ControlledUnit->GetUnitBehaviorTree();
    }
    
    if (!BehaviorTreeToUse && DefaultBehaviorTree)
    {
        BehaviorTreeToUse = DefaultBehaviorTree;
    }
    
    if (!BehaviorTreeToUse)
    {
        UE_LOG(LogSGGameplay, Warning, TEXT("AI: %s 没有可用的行为树"), *InPawn->GetName());
        return;
    }
    
    // 步骤2：启动行为树
    StartBehaviorTree(BehaviorTreeToUse);
}

// ========== SetupBehaviorTree ==========
bool ASG_AIControllerBase::SetupBehaviorTree(UBehaviorTree* BehaviorTreeToUse)
{
    if (!BehaviorTreeToUse)
    {
        return false;
    }
    
    UBlackboardData* BlackboardAsset = BehaviorTreeToUse->BlackboardAsset;
    if (!BlackboardAsset)
    {
        return false;
    }
    
    UBlackboardComponent* BlackboardComp = nullptr;
    bool bSuccess = UseBlackboard(BlackboardAsset, BlackboardComp);
    
    if (bSuccess && BlackboardComp)
    {
        BlackboardComp->SetValueAsBool(BB_IsTargetLocked, false);
        BlackboardComp->SetValueAsBool(BB_IsInAttackRange, false);
        BlackboardComp->SetValueAsBool(BB_IsTargetMainCity, false);
        return true;
    }
    
    return false;
}

// ========== StartBehaviorTree ==========
bool ASG_AIControllerBase::StartBehaviorTree(UBehaviorTree* BehaviorTreeToRun)
{
    if (!BehaviorTreeToRun)
    {
        return false;
    }
    
    UBehaviorTreeComponent* BTComp = Cast<UBehaviorTreeComponent>(GetBrainComponent());
    
    if (BTComp && BTComp->IsRunning())
    {
        BTComp->StopTree(EBTStopMode::Safe);
    }
    
    if (!SetupBehaviorTree(BehaviorTreeToRun))
    {
        return false;
    }
    
    bool bSuccess = AAIController::RunBehaviorTree(BehaviorTreeToRun);
    
    if (bSuccess)
    {
        CurrentBehaviorTree = BehaviorTreeToRun;
    }
    
    return bSuccess;
}

/**
 * @brief 取消控制时调用
 */
void ASG_AIControllerBase::OnUnPossess()
{
    if (AActor* CurrentTarget = GetCurrentTarget())
    {
        if (ASG_UnitsBase* ControlledUnit = Cast<ASG_UnitsBase>(GetPawn()))
        {
            if (UWorld* World = GetWorld())
            {
                if (USG_TargetingSubsystem* TargetingSys = World->GetSubsystem<USG_TargetingSubsystem>())
                {
                    TargetingSys->UnregisterAttacker(ControlledUnit, CurrentTarget);
                }

                // 只有非主城目标且需要槽位时才释放槽位
                if (ShouldOccupyAttackSlot() && !CurrentTarget->IsA(ASG_MainCityBase::StaticClass()))
                {
                    if (USG_CombatTargetManager* CombatManager = World->GetSubsystem<USG_CombatTargetManager>())
                    {
                        CombatManager->ReleaseAttackSlot(ControlledUnit, CurrentTarget);
                    }
                }
            }
        }
    }

    if (CurrentListenedTarget.IsValid())
    {
        UnbindTargetDeathEvent(CurrentListenedTarget.Get());
        CurrentListenedTarget = nullptr;
    }
    
    UBehaviorTreeComponent* BTComp = Cast<UBehaviorTreeComponent>(GetBrainComponent());
    if (BTComp && BTComp->IsRunning())
    {
        BTComp->StopTree(EBTStopMode::Safe);
    }
    
    CurrentBehaviorTree = nullptr;
    UnreachableTargets.Empty();
    TargetEngagementState = ESGTargetEngagementState::Searching;
    
    Super::OnUnPossess();
}

// ========== FreezeAI ==========
void ASG_AIControllerBase::FreezeAI()
{
    UBehaviorTreeComponent* BTComp = Cast<UBehaviorTreeComponent>(GetBrainComponent());
    if (BTComp)
    {
        BTComp->StopTree(EBTStopMode::Safe);
    }
    
    StopMovement();
    
    if (CurrentListenedTarget.IsValid())
    {
        UnbindTargetDeathEvent(CurrentListenedTarget.Get());
        CurrentListenedTarget = nullptr;
    }
    
    SetCurrentTarget(nullptr);
    SetActorTickEnabled(false);
    
    TargetEngagementState = ESGTargetEngagementState::Searching;
}

/**
 * @brief 设置目标锁定状态
 * @param NewState 新状态
 */
void ASG_AIControllerBase::SetTargetEngagementState(ESGTargetEngagementState NewState)
{
    if (TargetEngagementState == NewState)
    {
        return;
    }
    
    TargetEngagementState = NewState;
}

/**
 * @brief 检查是否允许切换目标
 * @return 是否允许切换
 * @details
 * 功能说明：
 * - ✨ 新增：攻击锁定期间不允许切换目标
 * - Engaged 状态下攻击敌方单位时不允许切换（除非目标死亡且动画结束）
 * - 攻击主城时允许切换到敌方单位
 */
bool ASG_AIControllerBase::CanSwitchTarget() const
{
    // ✨ 新增 - 检查单位是否处于攻击锁定状态
    ASG_UnitsBase* ControlledUnit = Cast<ASG_UnitsBase>(GetPawn());
    if (ControlledUnit && ControlledUnit->IsAttackLocked())
    {
        // 攻击锁定期间绝对不允许切换目标
        return false;
        
    }
    // Searching、Moving、Blocked 状态都允许切换
    if (TargetEngagementState != ESGTargetEngagementState::Engaged)
    {
        return true;
    }
    
    // Engaged 状态下，检查当前目标是否是主城
    AActor* CurrentTarget = GetCurrentTarget();
    if (CurrentTarget && CurrentTarget->IsA(ASG_MainCityBase::StaticClass()))
    {
        // 攻击主城时允许切换到敌方单位
        return true;
    }
    
    // 攻击敌方单位时不允许切换
    return false;
}

// ✨ 新增 - 攻击主城时检测敌方单位
/**
 * @brief 攻击主城时检测敌方单位
 * @details
 * 功能说明：
 * - 仅在 Engaged 状态且目标是主城时调用
 * - 如果视野内有敌方单位，切换目标
 */
void ASG_AIControllerBase::CheckForEnemyUnitsWhileAttackingMainCity()
{
    // ✨ 新增 - 攻击锁定检查
    ASG_UnitsBase* ControlledUnit = Cast<ASG_UnitsBase>(GetPawn());
    if (ControlledUnit && ControlledUnit->IsAttackLocked())
    {
        return;
    }
    
    AActor* CurrentTarget = GetCurrentTarget();
    if (!CurrentTarget)
    {
        return;
    }
    
    // 只有当前目标是主城时才检测
    if (!CurrentTarget->IsA(ASG_MainCityBase::StaticClass()))
    {
        return;
    }
    
   
    
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }
    
    USG_TargetingSubsystem* TargetingSys = World->GetSubsystem<USG_TargetingSubsystem>();
    if (!TargetingSys)
    {
        return;
    }
    
    // 查找敌方单位（不包括主城）
    TArray<FSGTargetCandidate> Candidates;
    TSet<TWeakObjectPtr<AActor>> IgnoreList = UnreachableTargets;
    
    AActor* EnemyUnit = TargetingSys->FindEnemyUnitsOnly(
        ControlledUnit,
        ControlledUnit->GetDetectionRange(),
        Candidates,
        IgnoreList
    );
    
    if (EnemyUnit)
    {
        // 发现敌方单位，切换目标
        UE_LOG(LogSGGameplay, Log, TEXT("AI: %s 发现敌方单位 %s，从主城切换"),
            *ControlledUnit->GetName(), *EnemyUnit->GetName());
        
        SetCurrentTarget(EnemyUnit);
    }
}

/**
 * @brief 在移动状态下检测是否有更好的目标
 * @details
 * 功能说明：
 * - ✨ 新增：攻击锁定期间不检测
 */
void ASG_AIControllerBase::CheckForBetterTargetWhileMoving()
{
    // ✨ 新增 - 攻击锁定检查
    ASG_UnitsBase* ControlledUnit = Cast<ASG_UnitsBase>(GetPawn());
    if (ControlledUnit && ControlledUnit->IsAttackLocked())
    {
        return;
    }
    
    if (TargetEngagementState != ESGTargetEngagementState::Moving)
    {
        return;
    }

  

    AActor* CurrentTarget = GetCurrentTarget();
    if (!CurrentTarget)
    {
        return;
    }

    bool bCurrentTargetIsMainCity = CurrentTarget->IsA(ASG_MainCityBase::StaticClass());

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    USG_TargetingSubsystem* TargetingSys = World->GetSubsystem<USG_TargetingSubsystem>();
    if (!TargetingSys)
    {
        return;
    }

    FVector MyLocation = ControlledUnit->GetActorLocation();
    float CurrentDistance = FVector::Dist(MyLocation, CurrentTarget->GetActorLocation());

    TArray<FSGTargetCandidate> Candidates;
    TSet<TWeakObjectPtr<AActor>> IgnoreList = UnreachableTargets;
    
    AActor* BetterTarget = TargetingSys->FindEnemyUnitsOnly(
        ControlledUnit,
        ControlledUnit->GetDetectionRange(),
        Candidates,
        IgnoreList
    );

    if (BetterTarget && BetterTarget != CurrentTarget)
    {
        float NewDistance = FVector::Dist(MyLocation, BetterTarget->GetActorLocation());

        bool bShouldSwitch = false;
        
        if (bCurrentTargetIsMainCity)
        {
            // 当前攻击主城，发现敌方单位就切换
            bShouldSwitch = true;
        }
        else if (CurrentDistance - NewDistance > TargetSwitchDistanceThreshold)
        {
            // 新目标明显更近
            bShouldSwitch = true;
        }

        if (bShouldSwitch)
        {
            SetCurrentTarget(BetterTarget);
        }
    }
}

/**
 * @brief 标记当前目标为不可达
 */
void ASG_AIControllerBase::MarkCurrentTargetUnreachable()
{
    AActor* CurrentTarget = GetCurrentTarget();
    if (!CurrentTarget)
    {
        return;
    }
    
    // 主城不标记为不可达
    if (CurrentTarget->IsA(ASG_MainCityBase::StaticClass()))
    {
        return;
    }
    
    UnreachableTargets.Add(CurrentTarget);
    SetTargetEngagementState(ESGTargetEngagementState::Blocked);
}

/**
 * @brief 清除不可达目标列表
 */
void ASG_AIControllerBase::ClearUnreachableTargets()
{
    for (auto It = UnreachableTargets.CreateIterator(); It; ++It)
    {
        if (!It->IsValid())
        {
            It.RemoveCurrent();
        }
    }
    
    if (UnreachableTargets.Num() > 0)
    {
        UnreachableTargets.Empty();
    }
}

/**
 * @brief 检查目标是否在不可达列表中
 */
bool ASG_AIControllerBase::IsTargetUnreachable(AActor* Target) const
{
    if (!Target)
    {
        return false;
    }
    
    return UnreachableTargets.Contains(Target);
}

/**
 * @brief 检查是否卡住
 */
bool ASG_AIControllerBase::IsStuck() const
{
    return MovementTimer >= StuckThresholdTime;
}

/**
 * @brief 重置移动计时器
 */
void ASG_AIControllerBase::ResetMovementTimer()
{
    MovementTimer = 0.0f;
    if (APawn* ControlledPawn = GetPawn())
    {
        LastPosition = ControlledPawn->GetActorLocation();
    }
}

/**
 * @brief 更新移动计时器
 */
void ASG_AIControllerBase::UpdateMovementTimer(float DeltaTime)
{
    if (TargetEngagementState != ESGTargetEngagementState::Moving)
    {
        MovementTimer = 0.0f;
        return;
    }
    
    APawn* ControlledPawn = GetPawn();
    if (!ControlledPawn)
    {
        return;
    }

    float Speed = ControlledPawn->GetVelocity().Size();
    
    if (Speed < 10.0f)
    {
        MovementTimer += DeltaTime;
    }
    else
    {
        MovementTimer = 0.0f; 
    }

    if (MovementTimer > 0.5f)
    {
        MovementTimer = 0.0f;
        TryFlankingMove();
    }
}


/**
 * @brief 尝试侧面绕行
 */
void ASG_AIControllerBase::TryFlankingMove()
{
    ASG_UnitsBase* Unit = Cast<ASG_UnitsBase>(GetPawn());
    AActor* CurrentTarget = GetCurrentTarget();
    
    if (!Unit || !CurrentTarget)
    {
        return;
    }

    // 主城不需要绕行
    if (CurrentTarget->IsA(ASG_MainCityBase::StaticClass()))
    {
        return;
    }

    // 远程单位不需要绕行
    if (!ShouldOccupyAttackSlot())
    {
        return;
    }

    if (UWorld* World = GetWorld())
    {
        USG_CombatTargetManager* CombatManager = World->GetSubsystem<USG_CombatTargetManager>();
        if (CombatManager)
        {
            CombatManager->ReleaseAttackSlot(Unit, CurrentTarget);
            
            FVector NewSlotPos;
            if (CombatManager->TryReserveAttackSlot(Unit, CurrentTarget, NewSlotPos))
            {
                MoveToLocation(NewSlotPos, -1.0f, true, true, true);
            }
            else
            {
                MarkCurrentTargetUnreachable();
                StopMovement();
            }
        }
    }
}

/**
 * @brief 查找最近的可达目标
 */
AActor* ASG_AIControllerBase::FindNearestReachableTarget()
{
    ASG_UnitsBase* ControlledUnit = Cast<ASG_UnitsBase>(GetPawn());
    if (!ControlledUnit)
    {
        return nullptr;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return nullptr;
    }

    if (USG_TargetingSubsystem* TargetingSys = World->GetSubsystem<USG_TargetingSubsystem>())
    {
        TArray<FSGTargetCandidate> Candidates;

        AActor* BestTarget = TargetingSys->FindBestTarget(
            ControlledUnit,
            ControlledUnit->GetDetectionRange(),
            Candidates,
            UnreachableTargets
        );

        return BestTarget;
    }

    return nullptr;
}

/**
 * @brief 查找最近的目标
 */
AActor* ASG_AIControllerBase::FindNearestTarget()
{
    ASG_UnitsBase* ControlledUnit = Cast<ASG_UnitsBase>(GetPawn());
    if (!ControlledUnit)
    {
        return nullptr;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return nullptr;
    }

    if (USG_TargetingSubsystem* TargetingSys = World->GetSubsystem<USG_TargetingSubsystem>())
    {
        TArray<FSGTargetCandidate> Candidates;
        
        AActor* BestTarget = TargetingSys->FindBestTarget(
            ControlledUnit,
            ControlledUnit->GetDetectionRange(),
            Candidates,
            UnreachableTargets
        );

        if (BestTarget)
        {
            bool bTargetIsMainCity = BestTarget->IsA(ASG_MainCityBase::StaticClass());
            if (UBlackboardComponent* BB = GetBlackboardComponent())
            {
                BB->SetValueAsBool(BB_IsTargetMainCity, bTargetIsMainCity);
            }

            return BestTarget;
        }
    }

    if (USG_CombatTargetManager* CombatManager = World->GetSubsystem<USG_CombatTargetManager>())
    {
        AActor* Target = CombatManager->FindBestTargetWithSlot(ControlledUnit);
        if (Target)
        {
            return Target;
        }
    }

    return nullptr;
}

/**
 * @brief 检测周边威胁
 */
bool ASG_AIControllerBase::DetectNearbyThreats(float DetectionRadius)
{
    if (!CanSwitchTarget())
    {
        return false;
    }
    
    ASG_UnitsBase* ControlledUnit = Cast<ASG_UnitsBase>(GetPawn());
    if (!ControlledUnit)
    {
        return false;
    }
    
    AActor* CurrentTarget = GetCurrentTarget();
    
    // 只有攻击主城时才检测威胁
    UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
    if (BlackboardComp && !BlackboardComp->GetValueAsBool(BB_IsTargetMainCity))
    {
        return false;
    }
    
    FGameplayTag MyFaction = ControlledUnit->FactionTag;
    
    TArray<AActor*> AllUnits;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASG_UnitsBase::StaticClass(), AllUnits);
    
    for (AActor* Actor : AllUnits)
    {
        if (Actor == ControlledUnit || Actor == CurrentTarget)
        {
            continue;
        }
        
        ASG_UnitsBase* Unit = Cast<ASG_UnitsBase>(Actor);
        if (!Unit)
        {
            continue;
        }
        
        if (Unit->FactionTag != MyFaction)
        {
            if (Unit->bIsDead || !Unit->CanBeTargeted())
            {
                continue;
            }
            
            float Distance = FVector::Dist(ControlledUnit->GetActorLocation(), Unit->GetActorLocation());
            
            if (Distance <= DetectionRadius)
            {
                SetCurrentTarget(Unit);
                StopMovement();
                return true;
            }
        }
    }
    
    return false;
}

// 🔧 修改 - SetCurrentTarget 函数（在开头添加锁定检查）
/**
 * @brief 设置当前目标
 * @param NewTarget 新目标
 * @details
 * 功能说明：
 * - ✨ 新增：攻击锁定期间不允许切换目标
 */
void ASG_AIControllerBase::SetCurrentTarget(AActor* NewTarget)
{
    // ✨ 新增 - 攻击锁定检查
    ASG_UnitsBase* ControlledUnit = Cast<ASG_UnitsBase>(GetPawn());
    if (ControlledUnit && ControlledUnit->IsAttackLocked())
    {
        UE_LOG(LogSGGameplay, Verbose, TEXT("🔒 AI: %s 攻击锁定中，拒绝切换目标"), 
            *ControlledUnit->GetName());
        return;
    }
    
    
    UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
    if (!BlackboardComp)
    {
        return;
    }

    AActor* OldTarget = GetCurrentTarget();
    
    if (OldTarget == NewTarget)
    {
        return;
    }

    UWorld* World = GetWorld();
    USG_TargetingSubsystem* TargetingSys = World ? World->GetSubsystem<USG_TargetingSubsystem>() : nullptr;
    USG_CombatTargetManager* CombatManager = World ? World->GetSubsystem<USG_CombatTargetManager>() : nullptr;
    bool bShouldOccupySlot = ShouldOccupyAttackSlot();

    // 1. 处理旧目标注销
    if (OldTarget && ControlledUnit)
    {
        // 只有非主城目标且需要槽位时才释放槽位
        if (bShouldOccupySlot && !OldTarget->IsA(ASG_MainCityBase::StaticClass()))
        {
            if (CombatManager)
            {
                CombatManager->ReleaseAttackSlot(ControlledUnit, OldTarget);
            }
        }

        if (TargetingSys)
        {
            TargetingSys->UnregisterAttacker(ControlledUnit, OldTarget);
        }
    }
    
    // 解绑旧目标死亡事件
    if (CurrentListenedTarget.IsValid())
    {
        UnbindTargetDeathEvent(CurrentListenedTarget.Get());
        CurrentListenedTarget = nullptr;
    }
    
    // 更新黑板
    BlackboardComp->SetValueAsObject(BB_CurrentTarget, NewTarget);
    
    // 检查新目标是否是主城
    bool bTargetIsMainCity = false;
    ASG_MainCityBase* TargetMainCity = nullptr;
    if (NewTarget)
    {
        TargetMainCity = Cast<ASG_MainCityBase>(NewTarget);
        bTargetIsMainCity = (TargetMainCity != nullptr);
    }
    
    BlackboardComp->SetValueAsBool(BB_IsTargetMainCity, bTargetIsMainCity);
    BlackboardComp->SetValueAsBool(BB_IsTargetLocked, NewTarget != nullptr);
    
    // 更新单位的目标
    if (ControlledUnit)
    {
        ControlledUnit->SetTarget(NewTarget);
    }
    
    // 2. 处理新目标注册和移动
    if (NewTarget && ControlledUnit)
    {
        // 绑定单位死亡事件（主城没有死亡事件）
        if (ASG_UnitsBase* TargetUnit = Cast<ASG_UnitsBase>(NewTarget))
        {
            BindTargetDeathEvent(TargetUnit);
            CurrentListenedTarget = TargetUnit;
        }

        // 注册攻击者
        if (TargetingSys)
        {
            TargetingSys->RegisterAttacker(ControlledUnit, NewTarget);
        }
        
        // 根据目标类型计算移动位置
        FVector MoveDestination;
        float AcceptanceRadius;
        float AttackRange = ControlledUnit->GetAttackRangeForAI();
        
        if (bTargetIsMainCity && TargetMainCity)
        {
            // ========== 主城目标 - 不使用槽位系统 ==========
            FVector CityLocation = TargetMainCity->GetActorLocation();
            FVector UnitLocation = ControlledUnit->GetActorLocation();
            
            FVector DirectionToUnit = (UnitLocation - CityLocation);
            DirectionToUnit.Z = 0.0f;
            DirectionToUnit.Normalize();
            
            if (DirectionToUnit.IsNearlyZero())
            {
                DirectionToUnit = FVector(1.0f, 0.0f, 0.0f);
            }
            
            float CityRadius = 800.0f;
            if (TargetMainCity->GetAttackDetectionBox())
            {
                FVector BoxExtent = TargetMainCity->GetAttackDetectionBox()->GetScaledBoxExtent();
                CityRadius = FMath::Max(BoxExtent.X, BoxExtent.Y);
            }
            
            float StandDistance = CityRadius + (AttackRange * 0.7f);
            
            MoveDestination = CityLocation + (DirectionToUnit * StandDistance);
            MoveDestination.Z = UnitLocation.Z;
            
            AcceptanceRadius = AttackRange * 0.5f;
        }
        else
        {
            // ========== 普通单位目标 ==========
            MoveDestination = NewTarget->GetActorLocation();
            AcceptanceRadius = AttackRange * 0.8f;
            
            // 只有需要占用槽位的单位才预约槽位
            if (bShouldOccupySlot && CombatManager)
            {
                FVector SlotPosition;
                if (CombatManager->TryReserveAttackSlot(ControlledUnit, NewTarget, SlotPosition))
                {
                    MoveDestination = SlotPosition;
                    AcceptanceRadius = 30.0f;
                }
            }
        }
        
        // 设置状态为移动中并立即开始移动
        SetTargetEngagementState(ESGTargetEngagementState::Moving);
        ResetMovementTimer();
        
        // 立即开始移动
        MoveToLocation(MoveDestination, AcceptanceRadius, true, true, true);
    }
    else
    {
        SetTargetEngagementState(ESGTargetEngagementState::Searching);
    }
}

// ========== GetCurrentTarget ==========
AActor* ASG_AIControllerBase::GetCurrentTarget() const
{
    const UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
    if (!BlackboardComp)
    {
        return nullptr;
    }
    
    return Cast<AActor>(BlackboardComp->GetValueAsObject(BB_CurrentTarget));
}

// ========== IsTargetValid ==========
bool ASG_AIControllerBase::IsTargetValid() const
{
    AActor* CurrentTarget = GetCurrentTarget();
    if (!CurrentTarget)
    {
        return false;
    }
    
    ASG_UnitsBase* TargetUnit = Cast<ASG_UnitsBase>(CurrentTarget);
    if (TargetUnit)
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
    }
    
    ASG_MainCityBase* TargetMainCity = Cast<ASG_MainCityBase>(CurrentTarget);
    if (TargetMainCity)
    {
        if (!TargetMainCity->IsAlive())
        {
            return false;
        }
    }
    
    return true;
}

/**
 * @brief 检查当前控制的单位是否需要占用攻击槽位
 */
bool ASG_AIControllerBase::ShouldOccupyAttackSlot() const
{
    ASG_UnitsBase* ControlledUnit = Cast<ASG_UnitsBase>(GetPawn());
    return ShouldUnitOccupyAttackSlot(ControlledUnit);
}

/**
 * @brief 检查指定单位是否需要占用攻击槽位
 */
bool ASG_AIControllerBase::ShouldUnitOccupyAttackSlot(const ASG_UnitsBase* Unit) const
{
    if (!Unit)
    {
        return false;
    }

    if (SlotOccupyingUnitTypes.IsEmpty())
    {
        return true;
    }

    return SlotOccupyingUnitTypes.HasTag(Unit->UnitTypeTag);
}

// ========== InterruptAttack ==========
void ASG_AIControllerBase::InterruptAttack()
{
    if (!bIsMainCity)
    {
        return;
    }
    
    bAttackInterrupted = true;
    
    UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
    if (BlackboardComp)
    {
        BlackboardComp->SetValueAsBool(TEXT("AttackInterrupted"), true);
    }
}

// ========== ResumeAttack ==========
void ASG_AIControllerBase::ResumeAttack()
{
    if (!bIsMainCity)
    {
        return;
    }
    
    bAttackInterrupted = false;
    
    UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
    if (BlackboardComp)
    {
        BlackboardComp->SetValueAsBool(TEXT("AttackInterrupted"), false);
    }
}

/**
 * @brief 目标死亡回调
 * @param DeadUnit 死亡的单位
 * @details
 * 功能说明：
 * - ✨ 新增：如果攻击锁定中，不立即处理，等攻击动画结束后由 CheckAndFindNewTargetAfterAttack 处理
 */
void ASG_AIControllerBase::OnTargetDeath(ASG_UnitsBase* DeadUnit)
{
    AActor* CurrentTarget = GetCurrentTarget();
    if (CurrentTarget != DeadUnit)
    {
        return;
    }


    // ✨ Test --- 新增 - 攻击锁定检查
    ASG_UnitsBase* ControlledUnit = Cast<ASG_UnitsBase>(GetPawn());
    if (ControlledUnit && ControlledUnit->IsAttackLocked())
    {
        // 攻击锁定期间，只记录日志，不立即切换目标
        // 目标切换会在攻击动画结束后由 UpdateTarget 服务处理
        UE_LOG(LogSGGameplay, Log, TEXT("🔒 AI: %s 的目标 %s 死亡，但攻击锁定中，延迟处理"),
            *ControlledUnit->GetName(), *DeadUnit->GetName());
        return;
    }
    
    CurrentListenedTarget = nullptr;
    
    SetTargetEngagementState(ESGTargetEngagementState::Searching);
    
    UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
    if (BlackboardComp)
    {
        BlackboardComp->SetValueAsObject(BB_CurrentTarget, nullptr);
        BlackboardComp->SetValueAsBool(BB_IsTargetLocked, false);
        BlackboardComp->SetValueAsBool(BB_IsTargetMainCity, false);
    }
    
    if (ASG_UnitsBase* AControlledUnit = Cast<ASG_UnitsBase>(GetPawn()))
    {
        AControlledUnit->SetTarget(nullptr);
    }
    
    UnreachableTargets.Remove(DeadUnit);
    
    AActor* NewTarget = FindNearestTarget();
    if (NewTarget)
    {
        SetCurrentTarget(NewTarget);
    }
}

// ========== BindTargetDeathEvent ==========
void ASG_AIControllerBase::BindTargetDeathEvent(ASG_UnitsBase* Target)
{
    if (!Target)
    {
        return;
    }
    
    Target->OnUnitDeathEvent.AddDynamic(this, &ASG_AIControllerBase::OnTargetDeath);
}

// ========== UnbindTargetDeathEvent ==========
void ASG_AIControllerBase::UnbindTargetDeathEvent(ASG_UnitsBase* Target)
{
    if (!Target)
    {
        return;
    }
    
    Target->OnUnitDeathEvent.RemoveDynamic(this, &ASG_AIControllerBase::OnTargetDeath);
}
