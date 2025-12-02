// 📄 文件：Source/Sguo/Private/AI/SG_AIControllerBase.cpp
// 🔧 修改 - 完整文件

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
    UE_LOG(LogSGGameplay, Log, TEXT("✓ AI 控制器 BeginPlay 完成"));
}

// ✨ 新增 - Tick 函数
/**
 * @brief Tick 更新
 * @param DeltaTime 帧间隔
 * @details
 * 功能说明：
 * - 更新移动计时器
 * - 周期性清理不可达列表
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
}

// ========== OnPossess ==========
void ASG_AIControllerBase::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    
    UE_LOG(LogSGGameplay, Log, TEXT("========== AI 控制器 OnPossess =========="));
    UE_LOG(LogSGGameplay, Log, TEXT("  控制的 Pawn：%s"), *InPawn->GetName());
    
    // ✨ 新增 - 初始化位置记录
    LastPosition = InPawn->GetActorLocation();
    
    // 步骤1：确定要使用的行为树
    UBehaviorTree* BehaviorTreeToUse = nullptr;
    
    ASG_UnitsBase* ControlledUnit = Cast<ASG_UnitsBase>(InPawn);
    if (ControlledUnit)
    {
        BehaviorTreeToUse = ControlledUnit->GetUnitBehaviorTree();
        
        if (BehaviorTreeToUse)
        {
            UE_LOG(LogSGGameplay, Log, TEXT("  📋 使用单位自定义行为树：%s"), *BehaviorTreeToUse->GetName());
        }
    }
    
    if (!BehaviorTreeToUse && DefaultBehaviorTree)
    {
        BehaviorTreeToUse = DefaultBehaviorTree;
        UE_LOG(LogSGGameplay, Log, TEXT("  📋 使用控制器默认行为树：%s"), *BehaviorTreeToUse->GetName());
    }
    
    if (!BehaviorTreeToUse)
    {
        UE_LOG(LogSGGameplay, Warning, TEXT("  ⚠️ 没有可用的行为树！"));
        return;
    }
    
    // 步骤2：启动行为树
    bool bSuccess = StartBehaviorTree(BehaviorTreeToUse);
    
    if (bSuccess)
    {
        UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 行为树启动成功"));
    }
    else
    {
        UE_LOG(LogSGGameplay, Error, TEXT("  ❌ 行为树启动失败"));
    }
    
    UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
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
        UE_LOG(LogSGGameplay, Error, TEXT("  ❌ 行为树没有关联的黑板资产"));
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
        // ❌ 删除这行
        // BlackboardComp->SetValueAsEnum(BB_TargetEngagementState, ...);
        
        UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 黑板初始化成功"));
        return true;
    }
    
    UE_LOG(LogSGGameplay, Error, TEXT("  ❌ 黑板初始化失败"));
    return false;
}

// ========== StartBehaviorTree ==========
bool ASG_AIControllerBase::StartBehaviorTree(UBehaviorTree* BehaviorTreeToRun)
{
    if (!BehaviorTreeToRun)
    {
        UE_LOG(LogSGGameplay, Error, TEXT("  ❌ StartBehaviorTree：行为树为空"));
        return false;
    }
    
    UBehaviorTreeComponent* BTComp = Cast<UBehaviorTreeComponent>(GetBrainComponent());
    
    if (BTComp && BTComp->IsRunning())
    {
        BTComp->StopTree(EBTStopMode::Safe);
        UE_LOG(LogSGGameplay, Verbose, TEXT("  🛑 停止当前行为树"));
    }
    
    if (!SetupBehaviorTree(BehaviorTreeToRun))
    {
        UE_LOG(LogSGGameplay, Error, TEXT("  ❌ 黑板初始化失败，无法启动行为树"));
        return false;
    }
    
    bool bSuccess = AAIController::RunBehaviorTree(BehaviorTreeToRun);
    
    if (bSuccess)
    {
        CurrentBehaviorTree = BehaviorTreeToRun;
        UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 行为树启动成功：%s"), *BehaviorTreeToRun->GetName());
    }
    else
    {
        UE_LOG(LogSGGameplay, Error, TEXT("  ❌ RunBehaviorTree 失败：%s"), *BehaviorTreeToRun->GetName());
    }
    
    return bSuccess;
}

// ========== OnUnPossess ==========
void ASG_AIControllerBase::OnUnPossess()
{
    // ✨ 新增 - 确保注销当前的攻击记录，防止单位死亡/回收后仍占据攻击名额
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
    
    // ✨ 新增 - 重置状态
    TargetEngagementState = ESGTargetEngagementState::Searching;
    
    UE_LOG(LogSGGameplay, Log, TEXT("🥶 AI 已冻结：%s"), 
        GetPawn() ? *GetPawn()->GetName() : TEXT("None"));
}

/**
 * @brief 设置目标锁定状态
 * @param NewState 新状态
 * @details
 * 功能说明：
 * - 更新内部状态
 * - 同步到黑板（使用 Enum 类型）
 * - 输出日志
 */
void ASG_AIControllerBase::SetTargetEngagementState(ESGTargetEngagementState NewState)
{
    if (TargetEngagementState == NewState)
    {
        return;
    }
    
    ESGTargetEngagementState OldState = TargetEngagementState;
    TargetEngagementState = NewState;
    
    // ✨ 简化 - 只更新内部状态，不同步到黑板
    // 行为树通过调用 IsEngagedInCombat() 等函数来判断状态
    
    // 输出状态变化日志
    static const TCHAR* StateNames[] = { TEXT("搜索中"), TEXT("移动中"), TEXT("战斗中"), TEXT("被阻挡") };
    UE_LOG(LogSGGameplay, Log, TEXT("🎯 %s 目标状态变化：%s → %s"),
        GetPawn() ? *GetPawn()->GetName() : TEXT("Unknown"),
        StateNames[static_cast<uint8>(OldState)],
        StateNames[static_cast<uint8>(NewState)]);
}

// ✨ 新增 - 标记当前目标为不可达
/**
 * @brief 标记当前目标为不可达
 * @details
 * 功能说明：
 * - 将当前目标加入不可达列表
 * - 下次寻敌时会跳过这些目标
 * - 列表会周期性清理
 */
void ASG_AIControllerBase::MarkCurrentTargetUnreachable()
{
    AActor* CurrentTarget = GetCurrentTarget();
    if (!CurrentTarget)
    {
        return;
    }
    
    UnreachableTargets.Add(CurrentTarget);
    
    UE_LOG(LogSGGameplay, Log, TEXT("🚫 %s 标记目标 %s 为不可达"),
        GetPawn() ? *GetPawn()->GetName() : TEXT("Unknown"),
        *CurrentTarget->GetName());
    
    // 设置状态为被阻挡
    SetTargetEngagementState(ESGTargetEngagementState::Blocked);
}

// ✨ 新增 - 清除不可达目标列表
/**
 * @brief 清除不可达目标列表
 * @details
 * 功能说明：
 * - 周期性调用，给目标第二次机会
 * - 清理无效的弱引用
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
    
    // 如果列表不为空，清空并输出日志
    if (UnreachableTargets.Num() > 0)
    {
        UE_LOG(LogSGGameplay, Verbose, TEXT("🔄 %s 清除不可达列表（共 %d 个目标）"),
            GetPawn() ? *GetPawn()->GetName() : TEXT("Unknown"),
            UnreachableTargets.Num());
        UnreachableTargets.Empty();
    }
}

// ✨ 新增 - 检查目标是否在不可达列表中
/**
 * @brief 检查目标是否在不可达列表中
 * @param Target 要检查的目标
 * @return 是否不可达
 */
bool ASG_AIControllerBase::IsTargetUnreachable(AActor* Target) const
{
    if (!Target)
    {
        return false;
    }
    
    return UnreachableTargets.Contains(Target);
}

// ✨ 新增 - 检查是否卡住
/**
 * @brief 检查是否卡住（移动超时）
 * @return 是否被判定为卡住
 * @details
 * 功能说明：
 * - 如果移动超过 StuckThresholdTime 但位移小于 MinMovementDistance
 * - 则判定为卡住
 */
bool ASG_AIControllerBase::IsStuck() const
{
    return MovementTimer >= StuckThresholdTime;
}

// ✨ 新增 - 重置移动计时器
/**
 * @brief 重置移动计时器
 * @details 开始新的移动时调用
 */
void ASG_AIControllerBase::ResetMovementTimer()
{
    MovementTimer = 0.0f;
    if (APawn* ControlledPawn = GetPawn())
    {
        LastPosition = ControlledPawn->GetActorLocation();
    }
}

// ✨ 新增 - 更新移动计时器
/**
 * @brief 更新移动计时器
 * @param DeltaTime 帧间隔
 * @details
 * 功能说明：
 * - 仅在移动状态下更新
 * - 检测实际移动距离
 * - 如果移动了足够距离则重置计时器
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

    // 1. 获取当前速度
    float Speed = ControlledPawn->GetVelocity().Size();
    
    // 2. 速度极低（被挡住）
    if (Speed < 10.0f)
    {
        MovementTimer += DeltaTime;
    }
    else
    {
        // 只要动起来了，就重置，说明 RVO 正在起作用
        MovementTimer = 0.0f; 
    }

    // 3. 判定为卡住（例如超过 0.5 秒没动）
    // 阈值要短，反应才快
    if (MovementTimer > 0.5f)
    {
        // 🚨 触发侧面绕行逻辑 🚨
        UE_LOG(LogSGGameplay, Warning, TEXT("🚧 %s 被人墙阻挡，尝试重新规划侧面路线..."), *ControlledPawn->GetName());
        
        // 重置计时器，防止连续触发
        MovementTimer = 0.0f;
        
        TryFlankingMove();
    }
}


// ✨ 新增函数 - TryFlankingMove (尝试侧面绕行)
// 需要在 .h 文件中声明: void TryFlankingMove();
void ASG_AIControllerBase::TryFlankingMove()
{
    ASG_UnitsBase* Unit = Cast<ASG_UnitsBase>(GetPawn());
    AActor* CurrentTarget = GetCurrentTarget();
    
    if (!Unit || !CurrentTarget) return;

    if (UWorld* World = GetWorld())
    {
        USG_CombatTargetManager* CombatManager = World->GetSubsystem<USG_CombatTargetManager>();
        if (CombatManager)
        {
            // 1. 释放当前死磕的槽位
            CombatManager->ReleaseAttackSlot(Unit, CurrentTarget);
            
            // 2. 重新预约一个槽位
            // 注意：由于我们修改了 FindNearestAvailableSlot，
            // 它现在会根据 Unit 的当前位置重新计算。
            // 既然当前位置被堵住了，Unit 会稍微被挤偏一点，这会导致算出不同的最优槽位。
            FVector NewSlotPos;
            if (CombatManager->TryReserveAttackSlot(Unit, CurrentTarget, NewSlotPos))
            {
                // 3. 强制移动到新槽位
                MoveToLocation(NewSlotPos, -1.0f, true, true, true);
                UE_LOG(LogSGGameplay, Log, TEXT("  ↪️ 切换到侧翼槽位: %s"), *NewSlotPos.ToString());
            }
            else
            {
                // 4. 真的没位置了（所有侧面都满了）
                // 这时候才考虑标记为不可达，去打别人
                MarkCurrentTargetUnreachable();
                StopMovement();
                // 行为树会在下一帧自动处理 FindNearestTarget
            }
        }
    }
}

// ✨ 新增 - 查找最近的可达目标
/**
 * @brief 查找最近的可达目标
 * @return 可达的目标 Actor
 * @details
 * 功能说明：
 * - 排除不可达列表中的目标
 * - 使用导航系统检查路径可达性
 * - 优先选择可到达的敌人
 */
AActor* ASG_AIControllerBase::FindNearestReachableTarget()
{
    ASG_UnitsBase* ControlledUnit = Cast<ASG_UnitsBase>(GetPawn());
    if (!ControlledUnit) 
    {
        return nullptr;
    }

    // ✨ 修改 - 优先使用 TargetingSubsystem
    if (USG_TargetingSubsystem* TargetingSys = GetWorld()->GetSubsystem<USG_TargetingSubsystem>())
    {
        TArray<FSGTargetCandidate> Candidates;
        
        // 调用子系统，并传入当前的 UnreachableTargets 集合作为忽略列表
        // 这样可以确保这次查找会避开之前标记为“不可达”的那些目标
        AActor* BestTarget = TargetingSys->FindBestTarget(
            ControlledUnit, 
            ControlledUnit->GetDetectionRange(), 
            Candidates, 
            UnreachableTargets
        );

        if (BestTarget)
        {
            UE_LOG(LogSGGameplay, Log, TEXT("FindNearestReachableTarget: %s 通过子系统找到新目标 %s"),
                *ControlledUnit->GetName(), *BestTarget->GetName());
            return BestTarget;
        }
    }

    // 如果子系统没找到，或者不存在，回退到简单的查找逻辑（可选，为了稳健性）
    UE_LOG(LogSGGameplay, Warning, TEXT("FindNearestReachableTarget: 子系统未找到目标，返回空"));
    return nullptr;

}

// ========== FindNearestTarget ==========
// 保持原有逻辑不变，但新增一个调用 FindNearestReachableTarget 的选项
AActor* ASG_AIControllerBase::FindNearestTarget()
{
    ASG_UnitsBase* ControlledUnit = Cast<ASG_UnitsBase>(GetPawn());
    if (!ControlledUnit)
    {
        return nullptr;
    }

    // ✨ 使用战斗目标管理器
    if (UWorld* World = GetWorld())
    {
        USG_CombatTargetManager* CombatManager = World->GetSubsystem<USG_CombatTargetManager>();
        if (CombatManager)
        {
            return CombatManager->FindBestTargetWithSlot(ControlledUnit);
        }
    }

    // 备用：使用原有逻辑
    UE_LOG(LogSGGameplay, Warning, TEXT("FindNearestTarget: 无法获取目标管理子系统，使用原有逻辑"));
    
    // ✨ 修改 - 如果当前处于被阻挡状态，使用可达性检测
    if (TargetEngagementState == ESGTargetEngagementState::Blocked)
    {
        return FindNearestReachableTarget();
    }
    
    // 原有逻辑保持不变
    ASG_UnitsBase* AControlledUnit = Cast<ASG_UnitsBase>(GetPawn());
    if (!AControlledUnit) 
    {
        UE_LOG(LogSGGameplay, Error, TEXT("FindNearestTarget: 控制的单位为空"));
        return nullptr;
    }

    FGameplayTag MyFaction = ControlledUnit->FactionTag;
    FVector MyLoc = ControlledUnit->GetActorLocation();
    
    float DetectionRadius = ControlledUnit->GetDetectionRange();
    ESGTargetSearchShape SearchShape = ControlledUnit->TargetSearchShape;
    bool bPrioritizeFrontmost = ControlledUnit->bPrioritizeFrontmost;

    UE_LOG(LogSGGameplay, Verbose, TEXT("FindNearestTarget: %s 开始寻找目标"), *ControlledUnit->GetName());

    // 准备候选列表
    TArray<AActor*> AllUnits;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASG_UnitsBase::StaticClass(), AllUnits);
    
    TArray<AActor*> AllMainCities;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASG_MainCityBase::StaticClass(), AllMainCities);

    // 筛选有效的敌方单位
    TArray<AActor*> ValidEnemyUnits;
    
    for (AActor* Actor : AllUnits)
    {
        if (Actor == ControlledUnit) continue;

        ASG_UnitsBase* Unit = Cast<ASG_UnitsBase>(Actor);
        if (!Unit) continue;
        if (Unit->bIsDead) continue;
        if (Unit->FactionTag == MyFaction) continue;
        if (!Unit->CanBeTargeted()) continue;

        FVector TargetLoc = Unit->GetActorLocation();
        bool bInRange = false;
        
        if (SearchShape == ESGTargetSearchShape::Square)
        {
            float DiffX = FMath::Abs(TargetLoc.X - MyLoc.X);
            float DiffY = FMath::Abs(TargetLoc.Y - MyLoc.Y);
            bInRange = (DiffX <= DetectionRadius && DiffY <= DetectionRadius);
        }
        else
        {
            bInRange = (FVector::DistSquared(TargetLoc, MyLoc) <= (DetectionRadius * DetectionRadius));
        }

        if (bInRange)
        {
            ValidEnemyUnits.Add(Unit);
        }
    }

    // 如果有敌方单位，选择最佳目标
    if (ValidEnemyUnits.Num() > 0)
    {
        AActor* BestTarget = nullptr;
        
        if (bPrioritizeFrontmost)
        {
            float BestXDiff = FLT_MAX;
            for (AActor* Target : ValidEnemyUnits)
            {
                float DistX = FMath::Abs(Target->GetActorLocation().X - MyLoc.X);
                if (DistX < BestXDiff)
                {
                    BestXDiff = DistX;
                    BestTarget = Target;
                }
            }
        }
        else
        {
            float BestDistSq = FLT_MAX;
            for (AActor* Target : ValidEnemyUnits)
            {
                float DistSq = FVector::DistSquared(Target->GetActorLocation(), MyLoc);
                if (DistSq < BestDistSq)
                {
                    BestDistSq = DistSq;
                    BestTarget = Target;
                }
            }
        }
        
        if (BestTarget)
        {
            UE_LOG(LogSGGameplay, Log, TEXT("FindNearestTarget: 选中敌方单位 %s"), *BestTarget->GetName());
            return BestTarget;
        }
    }

    // 如果没有敌方单位，查找敌方主城
    AActor* NearestMainCity = nullptr;
    float NearestMainCityDist = FLT_MAX;
    
    for (AActor* Actor : AllMainCities)
    {
        ASG_MainCityBase* City = Cast<ASG_MainCityBase>(Actor);
        if (!City) continue;
        if (!City->IsAlive()) continue;
        if (City->FactionTag == MyFaction) continue;
        
        float Dist = FVector::Dist(MyLoc, City->GetActorLocation());
        
        if (Dist < NearestMainCityDist)
        {
            NearestMainCityDist = Dist;
            NearestMainCity = City;
        }
    }
    
    if (NearestMainCity)
    {
        UE_LOG(LogSGGameplay, Log, TEXT("FindNearestTarget: 选中敌方主城 %s"), *NearestMainCity->GetName());
        return NearestMainCity;
    }

    UE_LOG(LogSGGameplay, Warning, TEXT("FindNearestTarget: 未找到任何敌方目标"));
    return nullptr;
}

// ========== DetectNearbyThreats ==========
bool ASG_AIControllerBase::DetectNearbyThreats(float DetectionRadius)
{
    // ✨ 修改 - 如果已经处于战斗锁定状态，不切换目标
    if (IsEngagedInCombat())
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

                UE_LOG(LogSGGameplay, Log, TEXT("🔄 %s 检测到周边威胁，转移目标到：%s"), 
                    *ControlledUnit->GetName(), *Unit->GetName());
                return true;
            }
        }
    }
    
    return false;
}

// ========== SetCurrentTarget ==========
void ASG_AIControllerBase::SetCurrentTarget(AActor* NewTarget)
{
 UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
    if (!BlackboardComp)
    {
        return;
    }
    
    ASG_UnitsBase* ControlledUnit = Cast<ASG_UnitsBase>(GetPawn());
    AActor* OldTarget = GetCurrentTarget();
    USG_TargetingSubsystem* TargetingSys = GetWorld() ? GetWorld()->GetSubsystem<USG_TargetingSubsystem>() : nullptr;

    // 1. 处理旧目标注销
    if (OldTarget && OldTarget != NewTarget)
    {
        if (UWorld* World = GetWorld())
        {
            // 释放 CombatManager 槽位
            if (USG_CombatTargetManager* CombatManager = World->GetSubsystem<USG_CombatTargetManager>())
            {
                CombatManager->ReleaseAttackSlot(ControlledUnit, OldTarget);
            }

            // ✨ 新增 - 向目标子系统注销攻击者身份
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

        // ✨ 新增 - 向目标子系统注册攻击者身份
        if (TargetingSys && ControlledUnit)
        {
            TargetingSys->RegisterAttacker(ControlledUnit, NewTarget);
        }
        
        // 预约槽位
        if (UWorld* World = GetWorld())
        {
            if (USG_CombatTargetManager* CombatManager = World->GetSubsystem<USG_CombatTargetManager>())
            {
                FVector SlotPosition;
                if (CombatManager->TryReserveAttackSlot(ControlledUnit, NewTarget, SlotPosition))
                {
                    UE_LOG(LogSGGameplay, Log, TEXT("🎯 %s 预约了槽位，位置: %s"),
                        *ControlledUnit->GetName(), *SlotPosition.ToString());
                }
            }
        }
        
        SetTargetEngagementState(ESGTargetEngagementState::Moving);
        ResetMovementTimer();
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
    
    UE_LOG(LogSGGameplay, Log, TEXT("🛑 主城攻击被打断"));
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
    
    UE_LOG(LogSGGameplay, Log, TEXT("▶️ 主城恢复攻击"));
}

// ========== OnTargetDeath ==========
void ASG_AIControllerBase::OnTargetDeath(ASG_UnitsBase* DeadUnit)
{
    AActor* CurrentTarget = GetCurrentTarget();
    if (CurrentTarget != DeadUnit)
    {
        return;
    }
    
    UE_LOG(LogSGGameplay, Log, TEXT("🎯 目标死亡，需要重新寻找目标"));
    
    CurrentListenedTarget = nullptr;
    
    // ✨ 修改 - 重置状态为搜索中
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
    
    // ✨ 修改 - 从不可达列表中移除死亡的目标
    UnreachableTargets.Remove(DeadUnit);
    
    AActor* NewTarget = FindNearestTarget();
    if (NewTarget)
    {
        SetCurrentTarget(NewTarget);
        UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 找到新目标：%s"), *NewTarget->GetName());
    }
    else
    {
        UE_LOG(LogSGGameplay, Log, TEXT("  ⚠️ 未找到新目标"));
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
    
    UE_LOG(LogSGGameplay, Verbose, TEXT("  ✓ 绑定目标死亡事件：%s"), *Target->GetName());
}

// ========== UnbindTargetDeathEvent ==========
void ASG_AIControllerBase::UnbindTargetDeathEvent(ASG_UnitsBase* Target)
{
    if (!Target)
    {
        return;
    }
    
    Target->OnUnitDeathEvent.RemoveDynamic(this, &ASG_AIControllerBase::OnTargetDeath);
    
    UE_LOG(LogSGGameplay, Verbose, TEXT("  ✓ 解绑目标死亡事件：%s"), *Target->GetName());
}
