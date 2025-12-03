// 📄 文件：Source/Sguo/Private/AI/SG_AIControllerBase.cpp
// 🔧 修改 - 修复目标管理和性能优化
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
    // 🔧 修改 - 减少日志输出
    UE_LOG(LogSGGameplay, Verbose, TEXT("✓ AI 控制器 BeginPlay 完成"));
}

/**
 * @brief Tick 更新
 * @param DeltaTime 帧间隔
 * @details
 * 功能说明：
 * - 更新移动计时器
 * - 周期性清理不可达列表
 * - ✨ 新增：移动中检测更好目标
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

    // ✨ 新增 - 移动中检测更好目标
    if (TargetEngagementState == ESGTargetEngagementState::Moving)
    {
        TargetSwitchCheckTimer += DeltaTime;
        if (TargetSwitchCheckTimer >= TargetSwitchCheckInterval)
        {
            TargetSwitchCheckTimer = 0.0f;
            CheckForBetterTargetWhileMoving();
        }
    }
    else
    {
        TargetSwitchCheckTimer = 0.0f;
    }
}

// ========== OnPossess ==========
void ASG_AIControllerBase::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    
    // 🔧 修改 - 减少日志输出
    UE_LOG(LogSGGameplay, Verbose, TEXT("AI 控制器 OnPossess: %s"), *InPawn->GetName());
    
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
        UE_LOG(LogSGGameplay, Warning, TEXT("⚠️ %s 没有可用的行为树"), *InPawn->GetName());
        return;
    }
    
    // 步骤2：启动行为树
    bool bSuccess = StartBehaviorTree(BehaviorTreeToUse);
    
    if (!bSuccess)
    {
        UE_LOG(LogSGGameplay, Error, TEXT("❌ %s 行为树启动失败"), *InPawn->GetName());
    }
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
        UE_LOG(LogSGGameplay, Error, TEXT("❌ 行为树没有关联的黑板资产"));
        return false;
    }
    
    UBlackboardComponent* BlackboardComp = nullptr;
    bool bSuccess = UseBlackboard(BlackboardAsset, BlackboardComp);
    
    if (bSuccess && BlackboardComp)
    {
        // 初始化黑板数据
        BlackboardComp->SetValueAsBool(BB_IsTargetLocked, false);
        BlackboardComp->SetValueAsBool(BB_IsInAttackRange, false);
        BlackboardComp->SetValueAsBool(BB_IsTargetMainCity, false);
        return true;
    }
    
    UE_LOG(LogSGGameplay, Error, TEXT("❌ 黑板初始化失败"));
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
    // 确保注销当前的攻击记录
    if (AActor* CurrentTarget = GetCurrentTarget())
    {
        if (ASG_UnitsBase* ControlledUnit = Cast<ASG_UnitsBase>(GetPawn()))
        {
            if (UWorld* World = GetWorld())
            {
                // 注销攻击者身份
                if (USG_TargetingSubsystem* TargetingSys = World->GetSubsystem<USG_TargetingSubsystem>())
                {
                    TargetingSys->UnregisterAttacker(ControlledUnit, CurrentTarget);
                }

                // 只有需要占用槽位的单位才释放槽位
                if (ShouldOccupyAttackSlot())
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
    
    // 清理状态
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
    
    UE_LOG(LogSGGameplay, Verbose, TEXT("🥶 AI 已冻结：%s"), 
        GetPawn() ? *GetPawn()->GetName() : TEXT("None"));
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
    
    ESGTargetEngagementState OldState = TargetEngagementState;
    TargetEngagementState = NewState;
    
    // 🔧 修改 - 只在状态变化时输出日志，减少日志量
    #if !UE_BUILD_SHIPPING
    static const TCHAR* StateNames[] = { TEXT("搜索中"), TEXT("移动中"), TEXT("战斗中"), TEXT("被阻挡") };
    UE_LOG(LogSGGameplay, Verbose, TEXT("🎯 %s 目标状态：%s → %s"),
        GetPawn() ? *GetPawn()->GetName() : TEXT("Unknown"),
        StateNames[static_cast<uint8>(OldState)],
        StateNames[static_cast<uint8>(NewState)]);
    #endif
}

// ✨ 新增 - 检查是否允许切换目标
/**
 * @brief 检查是否允许切换目标
 * @return 是否允许切换
 * @details
 * 功能说明：
 * - Engaged 状态（正在攻击）时不允许切换
 * - 其他状态都允许切换
 */
bool ASG_AIControllerBase::CanSwitchTarget() const
{
    // 只有在 Engaged 状态时不允许切换
    return TargetEngagementState != ESGTargetEngagementState::Engaged;
}

// ✨ 新增 - 移动中检测更好目标
/**
 * @brief 在移动状态下检测是否有更好的目标
 * @details
 * 功能说明：
 * - 仅在 Moving 状态下调用
 * - 检测是否有更近的敌方单位
 * - 如果新目标比当前目标近超过阈值，则切换
 */
void ASG_AIControllerBase::CheckForBetterTargetWhileMoving()
{
    // 只在移动状态下检测
    if (TargetEngagementState != ESGTargetEngagementState::Moving)
    {
        return;
    }

    ASG_UnitsBase* ControlledUnit = Cast<ASG_UnitsBase>(GetPawn());
    if (!ControlledUnit)
    {
        return;
    }

    AActor* CurrentTarget = GetCurrentTarget();
    if (!CurrentTarget)
    {
        return;
    }

    // 如果当前目标是主城，检测是否有敌方单位出现
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

    // 查找敌方单位（不包括主城）
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

        // 如果当前目标是主城，任何敌方单位都优先
        // 否则新目标必须比当前目标近超过阈值
        bool bShouldSwitch = false;
        
        if (bCurrentTargetIsMainCity)
        {
            // 当前攻击主城，发现敌方单位就切换
            bShouldSwitch = true;
            UE_LOG(LogSGGameplay, Log, TEXT("🔄 %s 发现敌方单位，从主城切换到 %s"),
                *ControlledUnit->GetName(), *BetterTarget->GetName());
        }
        else if (CurrentDistance - NewDistance > TargetSwitchDistanceThreshold)
        {
            // 新目标明显更近
            bShouldSwitch = true;
            UE_LOG(LogSGGameplay, Log, TEXT("🔄 %s 发现更近目标：%s (距离差: %.0f)"),
                *ControlledUnit->GetName(), *BetterTarget->GetName(), CurrentDistance - NewDistance);
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
    
    UnreachableTargets.Add(CurrentTarget);
    
    UE_LOG(LogSGGameplay, Verbose, TEXT("🚫 %s 标记目标 %s 为不可达"),
        GetPawn() ? *GetPawn()->GetName() : TEXT("Unknown"),
        *CurrentTarget->GetName());
    
    SetTargetEngagementState(ESGTargetEngagementState::Blocked);
}

/**
 * @brief 清除不可达目标列表
 */
void ASG_AIControllerBase::ClearUnreachableTargets()
{
    // 清理无效引用
    for (auto It = UnreachableTargets.CreateIterator(); It; ++It)
    {
        if (!It->IsValid())
        {
            It.RemoveCurrent();
        }
    }
    
    // 如果列表不为空，清空
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
    // 只在移动状态下检测
    if (TargetEngagementState != ESGTargetEngagementState::Moving)
    {
        MovementTimer = 0.0f;
        return;
    }
    
    APawn* ControlledPawn = GetPawn();
    if (!ControlledPawn) return;

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
    
    if (!Unit || !CurrentTarget) return;

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
 * @return 最佳目标 Actor
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

    // 优先使用 TargetingSubsystem
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

    // 备选：使用 CombatTargetManager
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
    // ✨ 修改 - 只有在允许切换目标时才检测威胁
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
            if (Unit->bIsDead)
            {
                continue;
            }
            
            if (!Unit->CanBeTargeted())
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

/**
 * @brief 设置当前目标
 * @param NewTarget 新目标
 * @details
 * 🔧 修改：
 * - 设置目标后立即触发移动
 * - 优化日志输出
 */
void ASG_AIControllerBase::SetCurrentTarget(AActor* NewTarget)
{
    UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
    if (!BlackboardComp)
    {
        return;
    }
    
    ASG_UnitsBase* ControlledUnit = Cast<ASG_UnitsBase>(GetPawn());
    AActor* OldTarget = GetCurrentTarget();
    
    // 如果目标没变，不处理
    if (OldTarget == NewTarget)
    {
        return;
    }

    USG_TargetingSubsystem* TargetingSys = GetWorld() ? GetWorld()->GetSubsystem<USG_TargetingSubsystem>() : nullptr;
    bool bShouldOccupySlot = ShouldOccupyAttackSlot();

    // 1. 处理旧目标注销
    if (OldTarget)
    {
        if (UWorld* World = GetWorld())
        {
            if (bShouldOccupySlot)
            {
                if (USG_CombatTargetManager* CombatManager = World->GetSubsystem<USG_CombatTargetManager>())
                {
                    CombatManager->ReleaseAttackSlot(ControlledUnit, OldTarget);
                }
            }

            if (TargetingSys && ControlledUnit)
            {
                TargetingSys->UnregisterAttacker(ControlledUnit, OldTarget);
            }
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
    
    bool bTargetIsMainCity = false;
    if (NewTarget)
    {
        bTargetIsMainCity = NewTarget->IsA(ASG_MainCityBase::StaticClass());
    }
    BlackboardComp->SetValueAsBool(BB_IsTargetMainCity, bTargetIsMainCity);
    BlackboardComp->SetValueAsBool(BB_IsTargetLocked, NewTarget != nullptr);
    
    // 更新单位的目标
    if (ControlledUnit)
    {
        ControlledUnit->SetTarget(NewTarget);
    }
    
    // 2. 处理新目标注册
    if (NewTarget)
    {
        if (ASG_UnitsBase* TargetUnit = Cast<ASG_UnitsBase>(NewTarget))
        {
            BindTargetDeathEvent(TargetUnit);
            CurrentListenedTarget = TargetUnit;
        }

        if (TargetingSys && ControlledUnit)
        {
            TargetingSys->RegisterAttacker(ControlledUnit, NewTarget);
        }
        
        // 🔧 修改 - 预约槽位并立即开始移动
        FVector MoveDestination = NewTarget->GetActorLocation();
        
        if (bShouldOccupySlot)
        {
            if (UWorld* World = GetWorld())
            {
                if (USG_CombatTargetManager* CombatManager = World->GetSubsystem<USG_CombatTargetManager>())
                {
                    FVector SlotPosition;
                    if (CombatManager->TryReserveAttackSlot(ControlledUnit, NewTarget, SlotPosition))
                    {
                        MoveDestination = SlotPosition;
                    }
                }
            }
        }
        
        // ✨ 新增 - 设置状态为移动中并立即开始移动
        SetTargetEngagementState(ESGTargetEngagementState::Moving);
        ResetMovementTimer();
        
        // 立即开始移动到目标/槽位位置
        float AttackRange = ControlledUnit ? ControlledUnit->GetAttackRangeForAI() : 150.0f;
        MoveToLocation(MoveDestination, AttackRange * 0.8f, true, true, true);
        
        // 🔧 修改 - 减少日志
        UE_LOG(LogSGGameplay, Verbose, TEXT("🎯 %s 设置目标：%s"),
            ControlledUnit ? *ControlledUnit->GetName() : TEXT("Unknown"),
            *NewTarget->GetName());
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
        float MainCityHealth = TargetMainCity->GetCurrentHealth();
        if (MainCityHealth <= 0.0f)
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

// ========== OnTargetDeath ==========
void ASG_AIControllerBase::OnTargetDeath(ASG_UnitsBase* DeadUnit)
{
    AActor* CurrentTarget = GetCurrentTarget();
    if (CurrentTarget != DeadUnit)
    {
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
    
    if (ASG_UnitsBase* ControlledUnit = Cast<ASG_UnitsBase>(GetPawn()))
    {
        ControlledUnit->SetTarget(nullptr);
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
