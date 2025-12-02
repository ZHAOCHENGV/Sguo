// 📄 文件：Source/Sguo/Private/AI/SG_CombatTargetManager.cpp
// ✨ 新增 - 战斗目标管理器实现

#include "AI/SG_CombatTargetManager.h"
#include "Units/SG_UnitsBase.h"
#include "Buildings/SG_MainCityBase.h"
#include "Debug/SG_LogCategories.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "Engine/OverlapResult.h"
#include "TimerManager.h"

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

/**
 * @brief 为单位查找最佳目标（带槽位检查）
 * @details
 * 核心逻辑：
 * 1. 场景查询获取范围内敌人
 * 2. 过滤掉槽位已满的目标
 * 3. 选择距离最近且有空槽的目标
 */
AActor* USG_CombatTargetManager::FindBestTargetWithSlot(ASG_UnitsBase* Querier)
{
    if (!Querier)
    {
        return nullptr;
    }

    FVector QuerierLocation = Querier->GetActorLocation();
    FGameplayTag QuerierFaction = Querier->FactionTag;
    float SearchRadius = Querier->GetDetectionRange();

    // ========== 步骤1：场景查询获取范围内敌人 ==========
    TArray<AActor*> NearbyEnemies;
    QueryEnemiesInRange(Querier, SearchRadius, NearbyEnemies);

    // ========== 步骤2：评估每个候选目标 ==========
    AActor* BestTarget = nullptr;
    float BestScore = -FLT_MAX;

    for (AActor* Enemy : NearbyEnemies)
    {
        // 检查是否有可用槽位
        if (!HasAvailableSlot(Enemy))
        {
            UE_LOG(LogSGGameplay, Verbose, TEXT("  跳过 %s：槽位已满"), *Enemy->GetName());
            continue;
        }

        // 计算距离
        float Distance = FVector::Dist(QuerierLocation, Enemy->GetActorLocation());

        // 获取槽位占用情况
        int32 OccupiedSlots = GetOccupiedSlotCount(Enemy);

        // 评分：距离越近越好，占用越少越好
        // Score = 1000 / Distance - OccupiedSlots * 10
        float Score = 1000.0f / FMath::Max(Distance, 1.0f) - OccupiedSlots * 10.0f;

        UE_LOG(LogSGGameplay, Verbose, TEXT("  候选 %s：距离=%.0f, 占用槽位=%d, 评分=%.2f"),
            *Enemy->GetName(), Distance, OccupiedSlots, Score);

        if (Score > BestScore)
        {
            BestScore = Score;
            BestTarget = Enemy;
        }
    }

    // ========== 步骤3：如果没有敌方单位，查找主城 ==========
    if (!BestTarget)
    {
        TArray<AActor*> AllMainCities;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASG_MainCityBase::StaticClass(), AllMainCities);

        for (AActor* Actor : AllMainCities)
        {
            ASG_MainCityBase* MainCity = Cast<ASG_MainCityBase>(Actor);
            if (!MainCity || !MainCity->IsAlive())
            {
                continue;
            }

            if (MainCity->FactionTag == QuerierFaction)
            {
                continue;
            }

            if (!HasAvailableSlot(MainCity))
            {
                continue;
            }

            float Distance = FVector::Dist(QuerierLocation, MainCity->GetActorLocation());
            float Score = 1000.0f / FMath::Max(Distance, 1.0f);

            if (Score > BestScore)
            {
                BestScore = Score;
                BestTarget = MainCity;
            }
        }
    }

    if (BestTarget)
    {
        UE_LOG(LogSGGameplay, Log, TEXT("🎯 %s 选择目标：%s (评分: %.2f)"),
            *Querier->GetName(), *BestTarget->GetName(), BestScore);
    }
    else
    {
        UE_LOG(LogSGGameplay, Warning, TEXT("⚠️ %s 未找到可用目标（所有目标槽位已满或无敌人）"),
            *Querier->GetName());
    }

    return BestTarget;
}

/**
 * @brief 尝试预约目标的攻击槽位
 */
bool USG_CombatTargetManager::TryReserveAttackSlot(ASG_UnitsBase* Attacker, AActor* Target, FVector& OutSlotPosition)
{
    if (!Attacker || !Target)
    {
        return false;
    }

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
    OutSlotPosition = CombatInfo.AttackSlots[SlotIndex].GetWorldPosition(Target);

    UE_LOG(LogSGGameplay, Log, TEXT("✅ %s 预约了 %s 的槽位 #%d (位置: %s)"),
        *Attacker->GetName(), *Target->GetName(), SlotIndex, *OutSlotPosition.ToString());

    return true;
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
            UE_LOG(LogSGGameplay, Log, TEXT("🔓 %s 释放了 %s 的槽位"),
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

    for (auto& Pair : TargetCombatInfoMap)
    {
        for (FSGAttackSlot& Slot : Pair.Value.AttackSlots)
        {
            if (Slot.OccupyingUnit.Get() == Attacker)
            {
                Slot.OccupyingUnit = nullptr;
            }
        }
    }

    UE_LOG(LogSGGameplay, Verbose, TEXT("🔓 %s 释放了所有槽位"), *Attacker->GetName());
}

/**
 * @brief 检查目标是否有可用槽位
 */
bool USG_CombatTargetManager::HasAvailableSlot(AActor* Target) const
{
    const FSGTargetCombatInfo* CombatInfo = TargetCombatInfoMap.Find(Target);
    if (!CombatInfo)
    {
        return true;  // 尚未初始化，肯定有槽位
    }
    return CombatInfo->GetAvailableSlotCount() > 0;
}

/**
 * @brief 获取目标的已占用槽位数量
 */
int32 USG_CombatTargetManager::GetOccupiedSlotCount(AActor* Target) const
{
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
 * @details 在目标周围均匀分布槽位
 */
void USG_CombatTargetManager::InitializeSlotsForTarget(AActor* Target)
{
    if (!Target)
    {
        return;
    }

    FSGTargetCombatInfo& CombatInfo = TargetCombatInfoMap.FindOrAdd(Target);
    
    // 如果已经初始化，跳过
    if (CombatInfo.AttackSlots.Num() > 0)
    {
        return;
    }

    // 确定槽位数量
    int32 NumSlots = UnitSlotCount;
    float Distance = SlotDistance;

    // 主城使用更多槽位和更大距离
    if (Target->IsA(ASG_MainCityBase::StaticClass()))
    {
        NumSlots = MainCitySlotCount;
        Distance = SlotDistance * 2.0f;
    }

    // 获取目标的攻击范围（如果是单位）
    if (ASG_UnitsBase* TargetUnit = Cast<ASG_UnitsBase>(Target))
    {
        // 槽位距离应该在攻击者的攻击范围内
        // 这里使用固定值，实际攻击时会根据攻击者调整
    }

    // 在目标周围均匀分布槽位
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
 * @brief 查找最近的可用槽位
 */
int32 USG_CombatTargetManager::FindNearestAvailableSlot(AActor* Target, const FVector& AttackerLocation)
{
    FSGTargetCombatInfo& CombatInfo = GetOrCreateCombatInfo(Target);

    int32 BestIndex = INDEX_NONE;
    float BestDistance = FLT_MAX;

    for (int32 i = 0; i < CombatInfo.AttackSlots.Num(); ++i)
    {
        const FSGAttackSlot& Slot = CombatInfo.AttackSlots[i];
        
        if (Slot.IsOccupied())
        {
            continue;
        }

        FVector SlotWorldPos = Slot.GetWorldPosition(Target);
        float Distance = FVector::Dist(AttackerLocation, SlotWorldPos);

        if (Distance < BestDistance)
        {
            BestDistance = Distance;
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

    // 场景查询
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

        // 跳过同阵营
        if (Unit->FactionTag == QuerierFaction)
        {
            continue;
        }

        // 跳过死亡
        if (Unit->bIsDead)
        {
            continue;
        }

        // 跳过不可被选中
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
        // 目标无效，移除
        if (!It.Key().IsValid())
        {
            It.RemoveCurrent();
            continue;
        }

        // 清理无效的占用者
        for (FSGAttackSlot& Slot : It.Value().AttackSlots)
        {
            if (Slot.OccupyingUnit.IsValid() && Slot.OccupyingUnit->bIsDead)
            {
                Slot.OccupyingUnit = nullptr;
            }
        }
    }
}
