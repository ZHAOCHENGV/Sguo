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
#include "Components/BoxComponent.h"

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
 * @brief 为单位查找最佳目标（带槽位检查 + 路径可达性检查）
 * @details
 * 优化逻辑：
 * 1. 获取所有候选目标。
 * 2. 筛选出有槽位的。
 * 3. 按直线距离排序。
 * 4. 关键：按顺序对候选目标进行 NavMesh 路径测试，返回第一个“路通”的目标。
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

    // 3. 初步筛选单位：只看阵营和槽位（性能消耗低）
    for (AActor* Enemy : NearbyEnemies)
    {
        // 🔧 修改 - 只有非主城单位才检查槽位
        if (!Enemy->IsA(ASG_MainCityBase::StaticClass()))
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
            
            // 🔧 修改 - 主城不再检查 HasAvailableSlot，总是可以攻击
            // if (!HasAvailableSlot(City)) continue; 

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

        // 🔧 修改 - 如果是主城，只要直线距离够近或者能通过 NavMesh，基本都可达
        // 主城体积大，NavMesh 测试可能会因为检测点在建筑中心而失败，所以对主城放宽检测
        bool bIsReachable = true;
        
        if (Candidate.Actor->IsA(ASG_MainCityBase::StaticClass()))
        {
            // 对于主城，我们假设总是可达的（或者只做简单的射线检测）
            // 因为主城通常是静态的大型目标，寻路系统会自动处理边缘
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
            UE_LOG(LogSGGameplay, Log, TEXT("🎯 %s 选中最佳目标：%s (距离: %.0f)"),
                *Querier->GetName(), *Candidate.Actor->GetName(), FMath::Sqrt(Candidate.DistSq));
            return Candidate.Actor;
        }
    }

    return nullptr;
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

    // 🔧 MODIFIED - 特殊处理主城逻辑
    if (ASG_MainCityBase* MainCity = Cast<ASG_MainCityBase>(Target))
    {
        // 计算攻击方向（从主城指向攻击者）
        FVector CityLocation = MainCity->GetActorLocation();
        FVector AttackerLocation = Attacker->GetActorLocation();
        FVector Direction = (AttackerLocation - CityLocation).GetSafeNormal();

        // 获取主城的碰撞盒大小（如果没有碰撞盒，给一个默认半径）
        float CityRadius = 800.0f; 
        if (MainCity->GetAttackDetectionBox())
        {
            CityRadius = MainCity->GetAttackDetectionBox()->GetScaledBoxExtent().X; 
        }

        // 攻击者的攻击范围
        float AttackRange = Attacker->GetAttackRangeForAI();
        
        // 计算理想站位：主城边缘 + 攻击范围的 80% (留点余量)
        // 这样攻击者会围成一个圈
        float StandDistance = CityRadius + (AttackRange * 0.8f);
        
        OutSlotPosition = CityLocation + (Direction * StandDistance);
        
        // 确保 Z 轴高度正确 (与攻击者一致)
        OutSlotPosition.Z = AttackerLocation.Z;

        // 主城不需要记录在 TargetCombatInfoMap 中，因为它没有槽位上限
        return true; 
    }

    // ========== 以下是普通单位的槽位逻辑（保持不变） ==========

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
        // 只有非主城单位才会因为槽位已满而失败
        UE_LOG(LogSGGameplay, Warning, TEXT("❌ %s 无法预约 %s 的槽位：已满"),
            *Attacker->GetName(), *Target->GetName());
        return false;
    }

    // 预约槽位
    CombatInfo.AttackSlots[SlotIndex].OccupyingUnit = Attacker;
    OutSlotPosition = CombatInfo.AttackSlots[SlotIndex].GetWorldPosition(Target);

    UE_LOG(LogSGGameplay, Verbose, TEXT("✅ %s 预约了 %s 的槽位 #%d"),
        *Attacker->GetName(), *Target->GetName(), SlotIndex);

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

    // 🔧 MODIFIED - 主城不需要释放槽位
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
            UE_LOG(LogSGGameplay, Verbose, TEXT("🔓 %s 释放了 %s 的槽位"),
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

    // 遍历所有被攻击目标的槽位信息
    for (auto& Pair : TargetCombatInfoMap)
    {
        // 🔧 MODIFIED - 如果 Key 是主城（虽然我们现在不往 Map 里加主城了，但为了健壮性检查一下）
        if (AActor* TargetActor = Pair.Key.Get())
        {
            if (TargetActor->IsA(ASG_MainCityBase::StaticClass())) continue;
        }

        for (FSGAttackSlot& Slot : Pair.Value.AttackSlots)
        {
            if (Slot.OccupyingUnit.Get() == Attacker)
            {
                Slot.OccupyingUnit = nullptr;
            }
        }
    }
}

/**
 * @brief 检查目标是否有可用槽位
 */
bool USG_CombatTargetManager::HasAvailableSlot(AActor* Target) const
{
    // 🔧 MODIFIED - 主城永远有空位
    if (Target && Target->IsA(ASG_MainCityBase::StaticClass()))
    {
        return true;
    }

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
    // 🔧 MODIFIED - 主城不统计占用数（或者返回0避免排序逻辑出错）
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

    // 🔧 MODIFIED - 主城逻辑：实时计算位置，而不是从 Map 获取
    if (ASG_MainCityBase* MainCity = Cast<ASG_MainCityBase>(Target))
    {
        // 复用 TryReserveAttackSlot 中的计算逻辑，确保一致性
        // 计算攻击方向
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
 * @details 在目标周围均匀分布槽位
 */
void USG_CombatTargetManager::InitializeSlotsForTarget(AActor* Target)
{
    if (!Target)
    {
        return;
    }
    // 🔧 MODIFIED - 主城不需要初始化槽位
    if (Target->IsA(ASG_MainCityBase::StaticClass())) return;
    
    FSGTargetCombatInfo& CombatInfo = TargetCombatInfoMap.FindOrAdd(Target);
    if (CombatInfo.AttackSlots.Num() > 0) return;

    int32 NumSlots = UnitSlotCount;
    float Distance = SlotDistance;

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
 * @brief 查找最佳攻击槽位
 * @details
 * 改进算法：
 * 1. 不只看直线距离，而是看“可达性”。
 * 2. 如果正面的槽位虽然近，但是被堵住了，就选侧面的。
 */
int32 USG_CombatTargetManager::FindNearestAvailableSlot(AActor* Target, const FVector& AttackerLocation)
{
    FSGTargetCombatInfo& CombatInfo = GetOrCreateCombatInfo(Target);
    UWorld* World = GetWorld();
    UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);

    int32 BestIndex = INDEX_NONE;
    float BestCost = FLT_MAX;

    // 获取目标朝向，用于判断正面/侧面
    FVector TargetForward = Target->GetActorForwardVector();

    for (int32 i = 0; i < CombatInfo.AttackSlots.Num(); ++i)
    {
        const FSGAttackSlot& Slot = CombatInfo.AttackSlots[i];
        
        // 跳过已占用的
        if (Slot.IsOccupied()) continue;

        FVector SlotWorldPos = Slot.GetWorldPosition(Target);
        
        // 1. 基础距离分 (A* Heuristic)
        float DistSq = FVector::DistSquared(AttackerLocation, SlotWorldPos);
        float Cost = DistSq;

        // 2. 角度惩罚 (可选)
        // 这一步是为了让单位倾向于去它“顺路”的那一侧，而不是穿过目标去另一侧
        // FVector DirToSlot = (SlotWorldPos - Target->GetActorLocation()).GetSafeNormal();
        // FVector DirToAttacker = (AttackerLocation - Target->GetActorLocation()).GetSafeNormal();
        // float Dot = FVector::DotProduct(DirToSlot, DirToAttacker);
        // if (Dot < 0) Cost *= 1.5f; // 如果槽位在对面，增加代价

        // 3. 关键：路径可达性测试 (A* Pathfinding Check)
        // 使用 ProjectPointToNavigation 快速检查槽位是否在 NavMesh 上
        // 如果槽位在“障碍物”里（比如被其他人堵死），导航系统会投影失败或投射到很远
        if (NavSys)
        {
            FNavLocation ProjectedSlot;
            // 搜索半径设小一点，检测是否真的“有落脚点”
            bool bProjected = NavSys->ProjectPointToNavigation(SlotWorldPos, ProjectedSlot, FVector(50,50,50));
            
            if (!bProjected)
            {
                // 槽位无效（可能在墙里或悬崖外），跳过
                continue; 
            }
            
            // 可选：进行真正的路径开销计算 (Raycast 或 FindPath)
            // 这是一个性能权衡。如果单位少，可以用 NavSys->GetPathCost()
            // 这里为了性能，我们假设直线距离 + RVO 足够
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
