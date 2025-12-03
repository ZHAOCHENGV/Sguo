// 📄 文件：Source/Sguo/Private/AI/SG_CombatTargetManager.cpp
// 🔧 修改 - 添加调试可视化和标签过滤功能完整实现

#include "AI/SG_CombatTargetManager.h"
#include "Units/SG_UnitsBase.h"
#include "Buildings/SG_MainCityBase.h"
#include "Debug/SG_LogCategories.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "Engine/OverlapResult.h"
#include "TimerManager.h"
#include "Components/BoxComponent.h"
#include "DrawDebugHelpers.h"  // ✨ 新增

void USG_CombatTargetManager::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // 定期清理（每3秒）
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            CleanupTimerHandle,
            this,
            &USG_CombatTargetManager::CleanupInvalidData,
            3.0f,
            true
        );
    }

    // ✨ 新增 - 初始化默认需要槽位的标签
    if (SlotRequiredTags.IsEmpty())
    {
        // 默认近战单位需要槽位
        FGameplayTag InfantryTag = FGameplayTag::RequestGameplayTag(FName("Unit.Type.Infantry"), false);
        FGameplayTag CavalryTag = FGameplayTag::RequestGameplayTag(FName("Unit.Type.Cavalry"), false);
        
        if (InfantryTag.IsValid())
        {
            SlotRequiredTags.AddTag(InfantryTag);
        }
        if (CavalryTag.IsValid())
        {
            SlotRequiredTags.AddTag(CavalryTag);
        }
        
        UE_LOG(LogSGGameplay, Log, TEXT("✓ 初始化默认槽位标签：%s"), *SlotRequiredTags.ToString());
    }

    UE_LOG(LogSGGameplay, Log, TEXT("✓ 战斗目标管理器初始化完成"));
}

void USG_CombatTargetManager::Deinitialize()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(CleanupTimerHandle);
    }
    TargetCombatInfoMap.Empty();
    Super::Deinitialize();
}

// ✨ 新增 - Tick 函数用于调试绘制
/**
 * @brief Tick 更新
 * @param DeltaTime 帧间隔
 * @details
 * 功能说明：
 * - 每帧绘制调试可视化
 */
void USG_CombatTargetManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    // 绘制调试槽位
    if (bShowAttackSlots)
    {
        DrawDebugSlots();
    }
}

// ✨ 新增 - 获取统计 ID
TStatId USG_CombatTargetManager::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(USG_CombatTargetManager, STATGROUP_Tickables);
}

// ✨ 新增 - 检查单位是否需要占用槽位
/**
 * @brief 检查单位是否需要占用攻击槽位
 * @param Unit 单位
 * @return 是否需要占用槽位
 * @details
 * 功能说明：
 * - 检查单位的 UnitTypeTag 是否在 SlotRequiredTags 中
 * - 如果 SlotRequiredTags 为空，所有单位都需要槽位（向后兼容）
 * - 远程单位（弓箭手、弩手）通常不在列表中
 */
bool USG_CombatTargetManager::DoesUnitRequireSlot(ASG_UnitsBase* Unit) const
{
    if (!Unit)
    {
        return false;
    }
    
    // 如果没有配置标签，所有单位都需要槽位（向后兼容）
    if (SlotRequiredTags.IsEmpty())
    {
        return true;
    }
    
    // 检查单位的类型标签是否匹配
    bool bRequiresSlot = Unit->UnitTypeTag.MatchesAny(SlotRequiredTags);
    
    UE_LOG(LogSGGameplay, Verbose, TEXT("  检查单位 %s (标签: %s) 是否需要槽位: %s"),
        *Unit->GetName(),
        *Unit->UnitTypeTag.ToString(),
        bRequiresSlot ? TEXT("是") : TEXT("否"));
    
    return bRequiresSlot;
}

/**
 * @brief 为单位查找最佳目标（带槽位检查 + 路径可达性检查）
 * @details
 * 🔧 修改：
 * - 远程单位不检查槽位
 * - 只有近战单位才需要槽位
 */
AActor* USG_CombatTargetManager::FindBestTargetWithSlot(ASG_UnitsBase* Querier)
{
    if (!Querier) return nullptr;

    FVector QuerierLocation = Querier->GetActorLocation();
    FGameplayTag QuerierFaction = Querier->FactionTag;
    float SearchRadius = Querier->GetDetectionRange();

    // 获取导航系统
    UWorld* World = GetWorld();
    UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);

    // ✨ 新增 - 检查此单位是否需要槽位
    bool bNeedsSlot = DoesUnitRequireSlot(Querier);

    // 1. 获取范围内所有敌人
    TArray<AActor*> NearbyEnemies;
    QueryEnemiesInRange(Querier, SearchRadius, NearbyEnemies);

    // 2. 候选列表预处理结构体
    struct FCandidateInfo
    {
        AActor* Actor;
        float DistSq;
        int32 OccupiedSlots;
    };
    TArray<FCandidateInfo> Candidates;

    // 3. 初步筛选单位
    for (AActor* Enemy : NearbyEnemies)
    {
        // 🔧 修改 - 只有需要槽位的单位才检查槽位
        if (bNeedsSlot && !Enemy->IsA(ASG_MainCityBase::StaticClass()))
        {
            if (!HasAvailableSlot(Enemy)) continue;
        }

        float DistSq = FVector::DistSquared(QuerierLocation, Enemy->GetActorLocation());
        int32 Slots = GetOccupiedSlotCount(Enemy);
        Candidates.Add({Enemy, DistSq, Slots});
    }

    // 如果没找到单位，尝试找主城
    if (Candidates.Num() == 0)
    {
        TArray<AActor*> AllMainCities;
        UGameplayStatics::GetAllActorsOfClass(World, ASG_MainCityBase::StaticClass(), AllMainCities);
        for (AActor* Actor : AllMainCities)
        {
            ASG_MainCityBase* City = Cast<ASG_MainCityBase>(Actor);
            if (!City || !City->IsAlive() || City->FactionTag == QuerierFaction) continue;

            float DistSq = FVector::DistSquared(QuerierLocation, City->GetActorLocation());
            Candidates.Add({City, DistSq, 0});
        }
    }

    // 4. 排序：距离优先
    Candidates.Sort([](const FCandidateInfo& A, const FCandidateInfo& B) {
        return A.DistSq < B.DistSq;
    });

    // 5. 精确筛选：路径可达性检测
    int32 CheckLimit = 5; 
    int32 CheckedCount = 0;

    for (const FCandidateInfo& Candidate : Candidates)
    {
        if (CheckedCount >= CheckLimit) break;
        CheckedCount++;

        bool bIsReachable = true;
        
        if (Candidate.Actor->IsA(ASG_MainCityBase::StaticClass()))
        {
            bIsReachable = true; 
        }
        else if (NavSys)
        {
            FPathFindingQuery Query;
            Query.StartLocation = QuerierLocation;
            Query.EndLocation = Candidate.Actor->GetActorLocation();
            Query.NavData = NavSys->GetDefaultNavDataInstance();
            Query.Owner = Querier;
            
            bIsReachable = NavSys->TestPathSync(Query);
        }

        if (bIsReachable)
        {
            UE_LOG(LogSGGameplay, Log, TEXT("🎯 %s 选中最佳目标：%s (距离: %.0f, 需要槽位: %s)"),
                *Querier->GetName(), *Candidate.Actor->GetName(), FMath::Sqrt(Candidate.DistSq),
                bNeedsSlot ? TEXT("是") : TEXT("否"));
            return Candidate.Actor;
        }
    }

    return nullptr;
}

/**
 * @brief 尝试预约目标的攻击槽位
 * @details
 * 🔧 修改：
 * - 远程单位不占用槽位，直接返回目标位置
 * - 只有近战单位才占用槽位
 */
bool USG_CombatTargetManager::TryReserveAttackSlot(ASG_UnitsBase* Attacker, AActor* Target, FVector& OutSlotPosition)
{
    if (!Attacker || !Target)
    {
        return false;
    }

    // ✨ 新增 - 检查单位是否需要占用槽位
    if (!DoesUnitRequireSlot(Attacker))
    {
        // 远程单位不占用槽位，直接返回当前位置或目标方向的位置
        FVector AttackerLocation = Attacker->GetActorLocation();
        FVector TargetLocation = Target->GetActorLocation();
        
        // 计算站位点：保持当前距离，或使用攻击范围
        float AttackRange = Attacker->GetAttackRangeForAI();
        FVector Direction = (AttackerLocation - TargetLocation).GetSafeNormal();
        
        // 远程单位站在攻击范围边缘
        OutSlotPosition = TargetLocation + Direction * (AttackRange * 0.9f);
        OutSlotPosition.Z = AttackerLocation.Z;
        
        UE_LOG(LogSGGameplay, Log, TEXT("🏹 %s 是远程单位，不占用槽位，站位: %s"),
            *Attacker->GetName(), *OutSlotPosition.ToString());
        
        return true;
    }

    // 主城特殊处理
    if (ASG_MainCityBase* MainCity = Cast<ASG_MainCityBase>(Target))
    {
        FVector CityLocation = MainCity->GetActorLocation();
        FVector AttackerLocation = Attacker->GetActorLocation();
        FVector Direction = (AttackerLocation - CityLocation).GetSafeNormal();

        float CityRadius = 800.0f; 
        if (MainCity->GetAttackDetectionBox())
        {
            CityRadius = MainCity->GetAttackDetectionBox()->GetScaledBoxExtent().X; 
        }

        float AttackRange = Attacker->GetAttackRangeForAI();
        float StandDistance = CityRadius + (AttackRange * 0.8f);
        
        OutSlotPosition = CityLocation + (Direction * StandDistance);
        OutSlotPosition.Z = AttackerLocation.Z;

        return true; 
    }

    // ========== 普通单位的槽位逻辑 ==========

    FSGTargetCombatInfo& CombatInfo = GetOrCreateCombatInfo(Target);

    // 检查是否已经预约了槽位
    for (FSGAttackSlot& Slot : CombatInfo.AttackSlots)
    {
        if (Slot.OccupyingUnit.Get() == Attacker)
        {
            OutSlotPosition = Slot.GetWorldPosition(Target);
            return true;
        }
    }

    // 查找最近的可用槽位
    int32 SlotIndex = FindNearestAvailableSlot(Target, Attacker->GetActorLocation());
    if (SlotIndex == INDEX_NONE)
    {
        UE_LOG(LogSGGameplay, Warning, TEXT("❌ %s 无法预约 %s 的槽位：已满"),
            *Attacker->GetName(), *Target->GetName());
        return false;
    }

    // 预约槽位
    CombatInfo.AttackSlots[SlotIndex].OccupyingUnit = Attacker;
    // ✨ 新增 - 设置状态为已预约
    CombatInfo.AttackSlots[SlotIndex].Status = ESGSlotStatus::Reserved;
    OutSlotPosition = CombatInfo.AttackSlots[SlotIndex].GetWorldPosition(Target);

    UE_LOG(LogSGGameplay, Log, TEXT("✅ %s 预约了 %s 的槽位 #%d (状态: 已预约)"),
        *Attacker->GetName(), *Target->GetName(), SlotIndex);

    return true;
}

// ✨ 新增 - 标记槽位为已占用
/**
 * @brief 更新槽位状态为已占用
 * @param Attacker 攻击者
 * @param Target 目标
 * @details
 * 功能说明：
 * - 当单位到达攻击位置时调用
 * - 将槽位状态从 Reserved 改为 Occupied
 */
void USG_CombatTargetManager::MarkSlotAsOccupied(ASG_UnitsBase* Attacker, AActor* Target)
{
    if (!Attacker || !Target)
    {
        return;
    }

    // 远程单位不需要处理
    if (!DoesUnitRequireSlot(Attacker))
    {
        return;
    }

    // 主城不需要处理
    if (Target->IsA(ASG_MainCityBase::StaticClass()))
    {
        return;
    }

    FSGTargetCombatInfo* CombatInfo = TargetCombatInfoMap.Find(Target);
    if (!CombatInfo)
    {
        return;
    }

    for (FSGAttackSlot& Slot : CombatInfo->AttackSlots)
    {
        if (Slot.OccupyingUnit.Get() == Attacker)
        {
            Slot.Status = ESGSlotStatus::Occupied;
            UE_LOG(LogSGGameplay, Verbose, TEXT("🔴 %s 到达 %s 的槽位 (状态: 已占用)"),
                *Attacker->GetName(), *Target->GetName());
            return;
        }
    }
}

/**
 * @brief 释放攻击槽位
 */
void USG_CombatTargetManager::ReleaseAttackSlot(ASG_UnitsBase* Attacker, AActor* Target)
{
    if (!Attacker || !Target)
    {
        return;
    }

    // 远程单位不需要释放
    if (!DoesUnitRequireSlot(Attacker))
    {
        return;
    }

    // 主城不需要释放槽位
    if (Target->IsA(ASG_MainCityBase::StaticClass()))
    {
        return;
    }

    FSGTargetCombatInfo* CombatInfo = TargetCombatInfoMap.Find(Target);
    if (!CombatInfo)
    {
        return;
    }

    for (FSGAttackSlot& Slot : CombatInfo->AttackSlots)
    {
        if (Slot.OccupyingUnit.Get() == Attacker)
        {
            Slot.OccupyingUnit = nullptr;
            // ✨ 新增 - 重置状态为空闲
            Slot.Status = ESGSlotStatus::Free;
            UE_LOG(LogSGGameplay, Verbose, TEXT("🟢 %s 释放了 %s 的槽位 (状态: 空闲)"),
                *Attacker->GetName(), *Target->GetName());
            return;
        }
    }
}

/**
 * @brief 释放单位的所有槽位
 */
void USG_CombatTargetManager::ReleaseAllSlots(ASG_UnitsBase* Attacker)
{
    if (!Attacker)
    {
        return;
    }

    // 远程单位不需要释放
    if (!DoesUnitRequireSlot(Attacker))
    {
        return;
    }

    for (auto& Pair : TargetCombatInfoMap)
    {
        if (AActor* TargetActor = Pair.Key.Get())
        {
            if (TargetActor->IsA(ASG_MainCityBase::StaticClass())) continue;
        }

        for (FSGAttackSlot& Slot : Pair.Value.AttackSlots)
        {
            if (Slot.OccupyingUnit.Get() == Attacker)
            {
                Slot.OccupyingUnit = nullptr;
                // ✨ 新增 - 重置状态
                Slot.Status = ESGSlotStatus::Free;
            }
        }
    }
}

/**
 * @brief 检查目标是否有可用槽位
 */
bool USG_CombatTargetManager::HasAvailableSlot(AActor* Target) const
{
    if (Target && Target->IsA(ASG_MainCityBase::StaticClass()))
    {
        return true;
    }

    const FSGTargetCombatInfo* CombatInfo = TargetCombatInfoMap.Find(Target);
    if (!CombatInfo)
    {
        return true;
    }
    return CombatInfo->GetAvailableSlotCount() > 0;
}

/**
 * @brief 获取目标的已占用槽位数量
 */
int32 USG_CombatTargetManager::GetOccupiedSlotCount(AActor* Target) const
{
    if (Target && Target->IsA(ASG_MainCityBase::StaticClass()))
    {
        return 0;
    }

    const FSGTargetCombatInfo* CombatInfo = TargetCombatInfoMap.Find(Target);
    if (!CombatInfo)
    {
        return 0;
    }
    return CombatInfo->GetOccupiedSlotCount();
}

/**
 * @brief 获取单位当前预约的槽位位置
 */
bool USG_CombatTargetManager::GetReservedSlotPosition(ASG_UnitsBase* Attacker, AActor* Target, FVector& OutPosition) const
{
    if (!Attacker || !Target)
    {
        return false;
    }

    // 远程单位特殊处理
    if (!DoesUnitRequireSlot(Attacker))
    {
        FVector AttackerLocation = Attacker->GetActorLocation();
        FVector TargetLocation = Target->GetActorLocation();
        float AttackRange = Attacker->GetAttackRangeForAI();
        FVector Direction = (AttackerLocation - TargetLocation).GetSafeNormal();
        OutPosition = TargetLocation + Direction * (AttackRange * 0.9f);
        OutPosition.Z = AttackerLocation.Z;
        return true;
    }

    // 主城特殊处理
    if (ASG_MainCityBase* MainCity = Cast<ASG_MainCityBase>(Target))
    {
        FVector CityLocation = MainCity->GetActorLocation();
        FVector AttackerLocation = Attacker->GetActorLocation();
        FVector Direction = (AttackerLocation - CityLocation).GetSafeNormal();

        float CityRadius = 800.0f; 
        if (MainCity->GetAttackDetectionBox())
        {
            CityRadius = MainCity->GetAttackDetectionBox()->GetScaledBoxExtent().X; 
        }
        
        float AttackRange = Attacker->GetAttackRangeForAI();
        float StandDistance = CityRadius + (AttackRange * 0.8f);
        
        OutPosition = CityLocation + (Direction * StandDistance);
        OutPosition.Z = AttackerLocation.Z;
        return true;
    }

    const FSGTargetCombatInfo* CombatInfo = TargetCombatInfoMap.Find(Target);
    if (!CombatInfo)
    {
        return false;
    }

    for (const FSGAttackSlot& Slot : CombatInfo->AttackSlots)
    {
        if (Slot.OccupyingUnit.Get() == Attacker)
        {
            OutPosition = Slot.GetWorldPosition(Target);
            return true;
        }
    }

    return false;
}

/**
 * @brief 为目标初始化攻击槽位
 */
void USG_CombatTargetManager::InitializeSlotsForTarget(AActor* Target)
{
    if (!Target)
    {
        return;
    }
    
    if (Target->IsA(ASG_MainCityBase::StaticClass())) return;
    
    FSGTargetCombatInfo& CombatInfo = TargetCombatInfoMap.FindOrAdd(Target);
    if (CombatInfo.AttackSlots.Num() > 0) return;

    int32 NumSlots = UnitSlotCount;
    float Distance = SlotDistance;

    CombatInfo.AttackSlots.SetNum(NumSlots);
    
    for (int32 i = 0; i < NumSlots; ++i)
    {
        float Angle = (360.0f / NumSlots) * i;
        float Radians = FMath::DegreesToRadians(Angle);
        
        FVector Offset;
        Offset.X = FMath::Cos(Radians) * Distance;
        Offset.Y = FMath::Sin(Radians) * Distance;
        Offset.Z = 0.0f;
        
        CombatInfo.AttackSlots[i].RelativePosition = Offset;
        CombatInfo.AttackSlots[i].OccupyingUnit = nullptr;
        // ✨ 新增 - 初始化状态为空闲
        CombatInfo.AttackSlots[i].Status = ESGSlotStatus::Free;
    }

    UE_LOG(LogSGGameplay, Log, TEXT("📍 为 %s 初始化了 %d 个攻击槽位（距离: %.0f）"),
        *Target->GetName(), NumSlots, Distance);
}

/**
 * @brief 获取或创建目标的战斗信息
 */
FSGTargetCombatInfo& USG_CombatTargetManager::GetOrCreateCombatInfo(AActor* Target)
{
    FSGTargetCombatInfo* Existing = TargetCombatInfoMap.Find(Target);
    if (Existing && Existing->AttackSlots.Num() > 0)
    {
        return *Existing;
    }

    InitializeSlotsForTarget(Target);
    return TargetCombatInfoMap.FindOrAdd(Target);
}

/**
 * @brief 查找最佳攻击槽位
 */
int32 USG_CombatTargetManager::FindNearestAvailableSlot(AActor* Target, const FVector& AttackerLocation)
{
    FSGTargetCombatInfo& CombatInfo = GetOrCreateCombatInfo(Target);
    UWorld* World = GetWorld();
    UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);

    int32 BestIndex = INDEX_NONE;
    float BestCost = FLT_MAX;

    for (int32 i = 0; i < CombatInfo.AttackSlots.Num(); ++i)
    {
        const FSGAttackSlot& Slot = CombatInfo.AttackSlots[i];
        
        if (Slot.IsOccupied()) continue;

        FVector SlotWorldPos = Slot.GetWorldPosition(Target);
        
        float DistSq = FVector::DistSquared(AttackerLocation, SlotWorldPos);
        float Cost = DistSq;

        if (NavSys)
        {
            FNavLocation ProjectedSlot;
            bool bProjected = NavSys->ProjectPointToNavigation(SlotWorldPos, ProjectedSlot, FVector(50,50,50));
            
            if (!bProjected)
            {
                continue; 
            }
        }

        if (Cost < BestCost)
        {
            BestCost = Cost;
            BestIndex = i;
        }
    }

    return BestIndex;
}

/**
 * @brief 使用场景查询获取范围内的敌方单位
 */
void USG_CombatTargetManager::QueryEnemiesInRange(ASG_UnitsBase* Querier, float Range, TArray<AActor*>& OutEnemies)
{
    OutEnemies.Empty();

    if (!Querier)
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    FVector Center = Querier->GetActorLocation();
    FGameplayTag QuerierFaction = Querier->FactionTag;

    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(Querier);

    FCollisionObjectQueryParams ObjectParams;
    ObjectParams.AddObjectTypesToQuery(ECC_Pawn);

    TArray<FOverlapResult> Overlaps;
    World->OverlapMultiByObjectType(
        Overlaps,
        Center,
        FQuat::Identity,
        ObjectParams,
        FCollisionShape::MakeSphere(Range),
        QueryParams
    );

    for (const FOverlapResult& Overlap : Overlaps)
    {
        AActor* Actor = Overlap.GetActor();
        if (!Actor)
        {
            continue;
        }

        ASG_UnitsBase* Unit = Cast<ASG_UnitsBase>(Actor);
        if (!Unit)
        {
            continue;
        }

        if (Unit->FactionTag == QuerierFaction)
        {
            continue;
        }

        if (Unit->bIsDead)
        {
            continue;
        }

        if (!Unit->CanBeTargeted())
        {
            continue;
        }

        OutEnemies.Add(Unit);
    }

    UE_LOG(LogSGGameplay, Verbose, TEXT("场景查询：找到 %d 个敌方单位"), OutEnemies.Num());
}

/**
 * @brief 定期清理无效数据
 */
void USG_CombatTargetManager::CleanupInvalidData()
{
    for (auto It = TargetCombatInfoMap.CreateIterator(); It; ++It)
    {
        if (!It.Key().IsValid())
        {
            It.RemoveCurrent();
            continue;
        }

        for (FSGAttackSlot& Slot : It.Value().AttackSlots)
        {
            if (Slot.OccupyingUnit.IsValid() && Slot.OccupyingUnit->bIsDead)
            {
                Slot.OccupyingUnit = nullptr;
                // ✨ 新增 - 重置状态
                Slot.Status = ESGSlotStatus::Free;
            }
        }
    }
}

// ========== ✨ 新增 - 调试可视化函数实现 ==========

/**
 * @brief 切换槽位可视化显示
 */
void USG_CombatTargetManager::ToggleSlotVisualization()
{
    bShowAttackSlots = !bShowAttackSlots;
    UE_LOG(LogSGGameplay, Log, TEXT("攻击槽位可视化：%s"), bShowAttackSlots ? TEXT("开启") : TEXT("关闭"));
}

/**
 * @brief 设置槽位可视化显示
 * @param bEnable 是否启用
 */
void USG_CombatTargetManager::SetSlotVisualization(bool bEnable)
{
    bShowAttackSlots = bEnable;
    UE_LOG(LogSGGameplay, Log, TEXT("攻击槽位可视化：%s"), bShowAttackSlots ? TEXT("开启") : TEXT("关闭"));
}

/**
 * @brief 绘制所有槽位的调试可视化
 * @details
 * 功能说明：
 * - 遍历所有目标的战斗信息
 * - 为每个目标绘制其槽位
 */
void USG_CombatTargetManager::DrawDebugSlots()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    for (auto& Pair : TargetCombatInfoMap)
    {
        AActor* Target = Pair.Key.Get();
        if (!Target)
        {
            continue;
        }

        // 跳过主城（主城没有槽位系统）
        if (Target->IsA(ASG_MainCityBase::StaticClass()))
        {
            continue;
        }

        DrawDebugSlotsForTarget(Target, Pair.Value);
    }
}

/**
 * @brief 绘制单个目标的槽位
 * @param Target 目标
 * @param CombatInfo 战斗信息
 * @details
 * 功能说明：
 * - 绘制目标周围的所有槽位
 * - 根据状态使用不同颜色：
 *   - 绿色：空闲
 *   - 蓝色：已预约（单位正在移动）
 *   - 红色：已占用（单位正在攻击）
 * - 可选显示连线和文字信息
 */
void USG_CombatTargetManager::DrawDebugSlotsForTarget(AActor* Target, const FSGTargetCombatInfo& CombatInfo)
{
    UWorld* World = GetWorld();
    if (!World || !Target)
    {
        return;
    }

    FVector TargetLocation = Target->GetActorLocation();
    
    // 绘制目标标记
    DrawDebugSphere(
        World,
        TargetLocation + FVector(0, 0, 100.0f),
        50.0f,
        12,
        FColor::White,
        false,
        -1.0f,
        0,
        2.0f
    );

    // 统计信息
    int32 FreeCount = 0;
    int32 ReservedCount = 0;
    int32 OccupiedCount = 0;

    // 绘制每个槽位
    for (int32 i = 0; i < CombatInfo.AttackSlots.Num(); ++i)
    {
        const FSGAttackSlot& Slot = CombatInfo.AttackSlots[i];
        FVector SlotWorldPos = Slot.GetWorldPosition(Target);
        
        // 确定槽位状态和颜色
        ESGSlotStatus Status = Slot.GetStatus();
        FColor SlotColor;
        FString StatusText;
        
        switch (Status)
        {
            case ESGSlotStatus::Free:
                SlotColor = SlotFreeColor;
                StatusText = TEXT("空闲");
                FreeCount++;
                break;
            case ESGSlotStatus::Reserved:
                SlotColor = SlotReservedColor;
                StatusText = TEXT("预约");
                ReservedCount++;
                break;
            case ESGSlotStatus::Occupied:
                SlotColor = SlotOccupiedColor;
                StatusText = TEXT("占用");
                OccupiedCount++;
                break;
            default:
                SlotColor = FColor::Gray;
                StatusText = TEXT("未知");
                break;
        }

        // 绘制槽位球体
        DrawDebugSphere(
            World,
            SlotWorldPos,
            SlotDebugRadius,
            8,
            SlotColor,
            false,
            -1.0f,
            0,
            2.0f
        );

        // 绘制槽位索引
        if (bShowSlotText)
        {
            FString SlotLabel = FString::Printf(TEXT("#%d"), i);
            DrawDebugString(
                World,
                SlotWorldPos + FVector(0, 0, 50.0f),
                SlotLabel,
                nullptr,
                SlotColor,
                0.0f,
                true
            );
        }

        // 绘制从目标到槽位的连线
        DrawDebugLine(
            World,
            TargetLocation,
            SlotWorldPos,
            FColor(128, 128, 128, 128),  // 灰色半透明
            false,
            -1.0f,
            0,
            1.0f
        );

        // 如果槽位被占用，绘制从单位到槽位的连线
        if (bShowSlotConnections && Slot.OccupyingUnit.IsValid())
        {
            ASG_UnitsBase* Unit = Slot.OccupyingUnit.Get();
            FVector UnitLocation = Unit->GetActorLocation();
            
            // 绘制从单位到槽位的连线
            DrawDebugLine(
                World,
                UnitLocation,
                SlotWorldPos,
                SlotColor,
                false,
                -1.0f,
                0,
                3.0f
            );

            // 绘制单位名称
            if (bShowSlotText)
            {
                FString UnitInfo = FString::Printf(TEXT("%s\n%s"), 
                    *Unit->GetName(), 
                    *StatusText);
                DrawDebugString(
                    World,
                    UnitLocation + FVector(0, 0, 120.0f),
                    UnitInfo,
                    nullptr,
                    SlotColor,
                    0.0f,
                    true
                );
            }
        }
    }

    // 绘制目标的统计信息
    if (bShowSlotText)
    {
        FString TargetInfo = FString::Printf(
            TEXT("%s\n槽位: %d/%d\n空闲:%d 预约:%d 占用:%d"),
            *Target->GetName(),
            CombatInfo.GetOccupiedSlotCount(),
            CombatInfo.AttackSlots.Num(),
            FreeCount,
            ReservedCount,
            OccupiedCount
        );
        
        // 根据是否满员使用不同颜色
        FColor InfoColor = (FreeCount == 0) ? FColor::Red : FColor::White;
        
        DrawDebugString(
            World,
            TargetLocation + FVector(0, 0, 180.0f),
            TargetInfo,
            nullptr,
            InfoColor,
            0.0f,
            true
        );
    }
}
