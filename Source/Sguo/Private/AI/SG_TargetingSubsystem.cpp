// 📄 文件：Source/Sguo/Private/AI/SG_TargetingSubsystem.cpp
// 🔧 修改 - 添加蓝图接口实现
// ✅ 这是完整文件

#include "AI/SG_TargetingSubsystem.h"
#include "Units/SG_UnitsBase.h"
#include "Buildings/SG_MainCityBase.h"
#include "Debug/SG_LogCategories.h"
#include "Engine/OverlapResult.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"

// ========== 生命周期 ==========

/**
 * @brief 子系统初始化
 * @param Collection 子系统集合
 * @details
 * 功能说明：
 * - 设置定期清理计时器
 * - 设置主城缓存刷新计时器
 */
void USG_TargetingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    if (UWorld* World = GetWorld())
    {
        // 设置定期清理计时器（每5秒清理一次无效数据）
        World->GetTimerManager().SetTimer(
            CleanupTimerHandle,
            this,
            &USG_TargetingSubsystem::CleanupInvalidData,
            5.0f,
            true
        );

        // 设置主城缓存刷新计时器
        World->GetTimerManager().SetTimer(
            MainCityCacheTimerHandle,
            this,
            &USG_TargetingSubsystem::RefreshMainCityCache,
            MainCityCacheRefreshInterval,
            true,
            0.1f  // 初始延迟，确保游戏开始后主城已生成
        );
    }

    UE_LOG(LogSGGameplay, Log, TEXT("✓ 目标管理子系统初始化完成"));
}

/**
 * @brief 子系统销毁
 */
void USG_TargetingSubsystem::Deinitialize()
{
    if (UWorld* World = GetWorld())
    {
        // 清理计时器
        World->GetTimerManager().ClearTimer(CleanupTimerHandle);
        World->GetTimerManager().ClearTimer(MainCityCacheTimerHandle);
    }

    TargetAttackerMap.Empty();
    CachedMainCities.Empty();
    bMainCityCacheValid = false;

    Super::Deinitialize();
}

// ========== 主城缓存管理 ==========

/**
 * @brief 刷新主城缓存
 * @details
 * 功能说明：
 * - 定期更新主城列表
 * - 避免每次查询都遍历所有 Actor
 * - 只保留存活的主城
 */
void USG_TargetingSubsystem::RefreshMainCityCache()
{
    CachedMainCities.Empty();

    UWorld* World = GetWorld();
    if (!World)
    {
        bMainCityCacheValid = false;
        return;
    }

    // 获取所有主城
    TArray<AActor*> AllMainCities;
    UGameplayStatics::GetAllActorsOfClass(World, ASG_MainCityBase::StaticClass(), AllMainCities);

    // 过滤并缓存存活的主城
    for (AActor* Actor : AllMainCities)
    {
        ASG_MainCityBase* MainCity = Cast<ASG_MainCityBase>(Actor);
        if (MainCity && MainCity->IsAlive())
        {
            CachedMainCities.Add(MainCity);
        }
    }

    bMainCityCacheValid = true;

    UE_LOG(LogSGGameplay, Verbose, TEXT("🏰 主城缓存刷新：找到 %d 个存活主城"), CachedMainCities.Num());
}

/**
 * @brief 获取目标的碰撞半径
 * @param Target 目标 Actor
 * @return 碰撞半径
 */
float USG_TargetingSubsystem::GetTargetCollisionRadius(AActor* Target) const
{
    if (!Target)
    {
        return 50.0f;
    }

    // 主城特殊处理
    if (ASG_MainCityBase* MainCity = Cast<ASG_MainCityBase>(Target))
    {
        if (MainCity->GetAttackDetectionBox())
        {
            FVector BoxExtent = MainCity->GetAttackDetectionBox()->GetScaledBoxExtent();
            return FMath::Max(BoxExtent.X, BoxExtent.Y);
        }
        return 800.0f;
    }

    // 单位使用胶囊体半径
    if (ASG_UnitsBase* Unit = Cast<ASG_UnitsBase>(Target))
    {
        if (UCapsuleComponent* Capsule = Unit->GetCapsuleComponent())
        {
            return Capsule->GetScaledCapsuleRadius();
        }
    }

    // 通用：尝试查找胶囊体组件
    if (UCapsuleComponent* Capsule = Target->FindComponentByClass<UCapsuleComponent>())
    {
        return Capsule->GetScaledCapsuleRadius();
    }

    return 50.0f;
}

// ========== 场景查询 ==========

/**
 * @brief 使用 OverlapSphere 进行场景查询
 * @param Center 查询中心点
 * @param Radius 查询半径
 * @param OutActors 输出：查询到的 Actor 列表
 * @details
 * 功能说明：
 * - 使用 UE 内置的场景查询系统
 * - 比 GetAllActorsOfClass 高效得多
 * - 只查询指定范围内的 Actor
 */
void USG_TargetingSubsystem::PerformSphereQuery(const FVector& Center, float Radius, TArray<AActor*>& OutActors)
{
    OutActors.Empty();

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    // 设置查询参数
    FCollisionQueryParams QueryParams;
    QueryParams.bTraceComplex = false;
    QueryParams.bReturnPhysicalMaterial = false;

    // 设置碰撞形状
    FCollisionShape SphereShape = FCollisionShape::MakeSphere(Radius);

    // 设置碰撞对象类型（查询 Pawn）
    FCollisionObjectQueryParams ObjectQueryParams;
    ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

    // 执行重叠查询
    TArray<FOverlapResult> OverlapResults;
    bool bHasOverlap = World->OverlapMultiByObjectType(
        OverlapResults,
        Center,
        FQuat::Identity,
        ObjectQueryParams,
        SphereShape,
        QueryParams
    );

    if (bHasOverlap)
    {
        for (const FOverlapResult& Result : OverlapResults)
        {
            if (AActor* Actor = Result.GetActor())
            {
                OutActors.AddUnique(Actor);
            }
        }
    }

    UE_LOG(LogSGGameplay, Verbose, TEXT("场景查询：中心 %s，半径 %.0f，找到 %d 个 Actor"),
        *Center.ToString(), Radius, OutActors.Num());
}

// ========== 核心目标查找逻辑 ==========

/**
 * @brief 查找最佳目标（C++ 核心接口）
 * @param Querier 查询者单位
 * @param SearchRadius 搜索半径
 * @param OutCandidates 输出：候选目标列表
 * @param IgnoredActors 需要忽略的 Actor 列表
 * @return 最佳目标 Actor
 * @details
 * 核心逻辑：
 * 1. 优先在视野范围内查找敌方单位
 * 2. 如果没有敌方单位，自动回退到敌方主城
 * 3. 使用缓存的主城列表提高性能
 */
AActor* USG_TargetingSubsystem::FindBestTarget(
    ASG_UnitsBase* Querier,
    float SearchRadius,
    TArray<FSGTargetCandidate>& OutCandidates,
    const TSet<TWeakObjectPtr<AActor>>& IgnoredActors)
{
    OutCandidates.Empty();

    if (!Querier)
    {
        return nullptr;
    }

    FVector QuerierLocation = Querier->GetActorLocation();
    FGameplayTag QuerierFaction = Querier->FactionTag;

    // ========== 步骤1：使用球形查询获取范围内的敌方单位 ==========
    TArray<AActor*> NearbyActors;
    PerformSphereQuery(QuerierLocation, SearchRadius, NearbyActors);

    // ========== 步骤2：过滤并评估敌方单位 ==========
    for (AActor* Actor : NearbyActors)
    {
        // 跳过自己
        if (Actor == Querier)
        {
            continue;
        }

        // 检查是否在忽略列表中
        if (IgnoredActors.Contains(Actor))
        {
            continue;
        }

        // 检查是否是单位
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

        // 跳过死亡单位
        if (Unit->bIsDead)
        {
            continue;
        }

        // 跳过不可被选为目标的单位
        if (!Unit->CanBeTargeted())
        {
            continue;
        }

        // 计算距离
        float Distance = FVector::Dist(QuerierLocation, Unit->GetActorLocation());

        // 获取拥挤度
        int32 AttackerCount = GetAttackerCount(Unit);

        // 计算评分
        float Score = CalculateTargetScore(Querier, Unit, Distance, AttackerCount);

        // 添加到候选列表
        FSGTargetCandidate Candidate;
        Candidate.Target = Unit;
        Candidate.Distance = Distance;
        Candidate.AttackerCount = AttackerCount;
        Candidate.Score = Score;
        Candidate.bIsReachable = true;
        Candidate.bIsMainCity = false;

        OutCandidates.Add(Candidate);
    }

    // ========== 步骤3：如果有敌方单位，按评分排序并返回最佳目标 ==========
    if (OutCandidates.Num() > 0)
    {
        // 按评分降序排序
        OutCandidates.Sort([](const FSGTargetCandidate& A, const FSGTargetCandidate& B)
        {
            return A.Score > B.Score;
        });

        AActor* BestTarget = OutCandidates[0].Target.Get();

        UE_LOG(LogSGGameplay, Log, TEXT("🎯 %s 选择敌方单位：%s (距离: %.0f, 攻击者: %d, 评分: %.2f)"),
            *Querier->GetName(),
            BestTarget ? *BestTarget->GetName() : TEXT("None"),
            OutCandidates[0].Distance,
            OutCandidates[0].AttackerCount,
            OutCandidates[0].Score);

        return BestTarget;
    }

    // ========== 步骤4：没有敌方单位，回退到敌方主城 ==========
    UE_LOG(LogSGGameplay, Log, TEXT("📍 %s 视野内无敌方单位，查找敌方主城..."), *Querier->GetName());

    // 确保主城缓存有效
    if (!bMainCityCacheValid)
    {
        RefreshMainCityCache();
    }

    // 查找最近的敌方主城
    ASG_MainCityBase* NearestEnemyCity = nullptr;
    float NearestCityDistance = FLT_MAX;

    for (const TWeakObjectPtr<ASG_MainCityBase>& CityPtr : CachedMainCities)
    {
        ASG_MainCityBase* City = CityPtr.Get();
        if (!City)
        {
            continue;
        }

        // 跳过同阵营主城
        if (City->FactionTag == QuerierFaction)
        {
            continue;
        }

        // 跳过已摧毁的主城
        if (!City->IsAlive())
        {
            continue;
        }

        // 检查是否在忽略列表中
        if (IgnoredActors.Contains(City))
        {
            continue;
        }

        // 计算到主城的距离（考虑主城体积）
        float Distance = FVector::Dist(QuerierLocation, City->GetActorLocation());
        float CityRadius = GetTargetCollisionRadius(City);
        float EffectiveDistance = FMath::Max(0.0f, Distance - CityRadius);

        if (EffectiveDistance < NearestCityDistance)
        {
            NearestCityDistance = EffectiveDistance;
            NearestEnemyCity = City;
        }
    }

    // 如果找到敌方主城，添加到候选列表并返回
    if (NearestEnemyCity)
    {
        // 获取拥挤度
        int32 AttackerCount = GetAttackerCount(NearestEnemyCity);

        // 计算评分
        float Score = CalculateTargetScore(Querier, NearestEnemyCity, NearestCityDistance, AttackerCount);

        // 添加到候选列表
        FSGTargetCandidate Candidate;
        Candidate.Target = NearestEnemyCity;
        Candidate.Distance = NearestCityDistance;
        Candidate.AttackerCount = AttackerCount;
        Candidate.Score = Score;
        Candidate.bIsReachable = true;
        Candidate.bIsMainCity = true;

        OutCandidates.Add(Candidate);

        UE_LOG(LogSGGameplay, Log, TEXT("🏰 %s 回退到敌方主城：%s (距离: %.0f, 攻击者: %d)"),
            *Querier->GetName(),
            *NearestEnemyCity->GetName(),
            NearestCityDistance,
            AttackerCount);

        return NearestEnemyCity;
    }

    // 完全没有目标
    UE_LOG(LogSGGameplay, Warning, TEXT("⚠️ %s 未找到任何敌方目标（单位和主城都没有）"), *Querier->GetName());
    return nullptr;
}

/**
 * @brief 查找最佳目标（带结果类型，C++ 接口）
 */
AActor* USG_TargetingSubsystem::FindBestTargetWithType(
    ASG_UnitsBase* Querier,
    float SearchRadius,
    ESGTargetFindResult& OutResultType,
    const TSet<TWeakObjectPtr<AActor>>& IgnoredActors)
{
    TArray<FSGTargetCandidate> Candidates;
    AActor* Result = FindBestTarget(Querier, SearchRadius, Candidates, IgnoredActors);

    if (!Result)
    {
        OutResultType = ESGTargetFindResult::None;
        return nullptr;
    }

    // 检查结果类型
    if (Candidates.Num() > 0 && Candidates[0].bIsMainCity)
    {
        OutResultType = ESGTargetFindResult::EnemyCity;
    }
    else
    {
        OutResultType = ESGTargetFindResult::EnemyUnit;
    }

    return Result;
}

/**
 * @brief 仅查找敌方单位（C++ 接口）
 */
AActor* USG_TargetingSubsystem::FindEnemyUnitsOnly(
    ASG_UnitsBase* Querier,
    float SearchRadius,
    TArray<FSGTargetCandidate>& OutCandidates,
    const TSet<TWeakObjectPtr<AActor>>& IgnoredActors)
{
    OutCandidates.Empty();

    if (!Querier)
    {
        return nullptr;
    }

    FVector QuerierLocation = Querier->GetActorLocation();
    FGameplayTag QuerierFaction = Querier->FactionTag;

    // 使用球形查询获取范围内的单位
    TArray<AActor*> NearbyActors;
    PerformSphereQuery(QuerierLocation, SearchRadius, NearbyActors);

    // 过滤并评估敌方单位
    for (AActor* Actor : NearbyActors)
    {
        if (Actor == Querier)
        {
            continue;
        }

        if (IgnoredActors.Contains(Actor))
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

        float Distance = FVector::Dist(QuerierLocation, Unit->GetActorLocation());
        int32 AttackerCount = GetAttackerCount(Unit);
        float Score = CalculateTargetScore(Querier, Unit, Distance, AttackerCount);

        FSGTargetCandidate Candidate;
        Candidate.Target = Unit;
        Candidate.Distance = Distance;
        Candidate.AttackerCount = AttackerCount;
        Candidate.Score = Score;
        Candidate.bIsReachable = true;
        Candidate.bIsMainCity = false;

        OutCandidates.Add(Candidate);
    }

    if (OutCandidates.Num() == 0)
    {
        return nullptr;
    }

    // 按评分排序
    OutCandidates.Sort([](const FSGTargetCandidate& A, const FSGTargetCandidate& B)
    {
        return A.Score > B.Score;
    });

    return OutCandidates[0].Target.Get();
}

// ========== ✨ 新增 - 蓝图接口实现 ==========

/**
 * @brief 查找最佳目标（蓝图接口）
 * @param Querier 查询者单位
 * @param SearchRadius 搜索半径
 * @return 最佳目标 Actor
 * @details
 * 功能说明：
 * - 蓝图友好版本，不需要传入忽略列表
 * - 内部创建空的忽略列表调用 C++ 版本
 */
AActor* USG_TargetingSubsystem::FindBestTargetBP(ASG_UnitsBase* Querier, float SearchRadius)
{
    TArray<FSGTargetCandidate> Candidates;
    TSet<TWeakObjectPtr<AActor>> EmptyIgnoreList;
    return FindBestTarget(Querier, SearchRadius, Candidates, EmptyIgnoreList);
}

/**
 * @brief 查找最佳目标带类型（蓝图接口）
 * @param Querier 查询者单位
 * @param SearchRadius 搜索半径
 * @param OutResultType 输出：结果类型
 * @return 最佳目标 Actor
 */
AActor* USG_TargetingSubsystem::FindBestTargetWithTypeBP(
    ASG_UnitsBase* Querier,
    float SearchRadius,
    ESGTargetFindResult& OutResultType)
{
    TSet<TWeakObjectPtr<AActor>> EmptyIgnoreList;
    return FindBestTargetWithType(Querier, SearchRadius, OutResultType, EmptyIgnoreList);
}

/**
 * @brief 仅查找敌方单位（蓝图接口）
 * @param Querier 查询者单位
 * @param SearchRadius 搜索半径
 * @return 最佳敌方单位
 */
AActor* USG_TargetingSubsystem::FindEnemyUnitsOnlyBP(ASG_UnitsBase* Querier, float SearchRadius)
{
    TArray<FSGTargetCandidate> Candidates;
    TSet<TWeakObjectPtr<AActor>> EmptyIgnoreList;
    return FindEnemyUnitsOnly(Querier, SearchRadius, Candidates, EmptyIgnoreList);
}

/**
 * @brief 查找敌方主城
 * @param Querier 查询者单位
 * @return 最近的敌方主城
 * @details
 * 功能说明：
 * - 从缓存中查找敌方主城
 * - 返回最近的存活主城
 */
ASG_MainCityBase* USG_TargetingSubsystem::FindEnemyMainCity(ASG_UnitsBase* Querier)
{
    if (!Querier)
    {
        return nullptr;
    }

    // 确保主城缓存有效
    if (!bMainCityCacheValid)
    {
        RefreshMainCityCache();
    }

    FVector QuerierLocation = Querier->GetActorLocation();
    FGameplayTag QuerierFaction = Querier->FactionTag;

    ASG_MainCityBase* NearestEnemyCity = nullptr;
    float NearestDistance = FLT_MAX;

    for (const TWeakObjectPtr<ASG_MainCityBase>& CityPtr : CachedMainCities)
    {
        ASG_MainCityBase* City = CityPtr.Get();
        if (!City)
        {
            continue;
        }

        // 跳过同阵营
        if (City->FactionTag == QuerierFaction)
        {
            continue;
        }

        // 跳过已摧毁的
        if (!City->IsAlive())
        {
            continue;
        }

        float Distance = FVector::Dist(QuerierLocation, City->GetActorLocation());
        if (Distance < NearestDistance)
        {
            NearestDistance = Distance;
            NearestEnemyCity = City;
        }
    }

    return NearestEnemyCity;
}

/**
 * @brief 计算目标评分
 * @param Querier 查询者
 * @param Target 目标
 * @param Distance 距离
 * @param AttackerCount 攻击者数量
 * @return 评分（越高越好）
 */
float USG_TargetingSubsystem::CalculateTargetScore(
    ASG_UnitsBase* Querier,
    AActor* Target,
    float Distance,
    int32 AttackerCount) const
{
    // 基础分数：距离越近分数越高
    float MaxDistance = Querier->GetDetectionRange();
    if (MaxDistance <= 0.0f) MaxDistance = 1000.0f;

    float DistanceScore = FMath::Clamp((MaxDistance - Distance) / MaxDistance, 0.0f, 1.0f);

    // 动态计算拥挤惩罚因子
    float PenaltyFactor = 1.0f;

    if (AttackerCount > 4)
    {
        // 极度拥挤
        PenaltyFactor = 5.0f + (AttackerCount - 4) * 2.0f;
    }
    else if (AttackerCount > 0)
    {
        // 轻微拥挤
        PenaltyFactor = 1.0f + (AttackerCount * 0.5f);
    }

    // 基础分扩大
    float BaseScore = DistanceScore * 100.0f;

    // 最终分
    float FinalScore = BaseScore / PenaltyFactor;

    UE_LOG(LogSGGameplay, Verbose, TEXT("  评分计算 [%s]: 距离分=%.2f, 攻击者=%d, 惩罚因子=%.2f, 最终=%.2f"),
        *Target->GetName(), DistanceScore, AttackerCount, PenaltyFactor, FinalScore);

    return FinalScore;
}

// ========== 拥挤度管理 ==========

/**
 * @brief 注册攻击者
 */
void USG_TargetingSubsystem::RegisterAttacker(ASG_UnitsBase* Attacker, AActor* Target)
{
    if (!Attacker || !Target)
    {
        return;
    }

    FSGTargetAttackerInfo& Info = TargetAttackerMap.FindOrAdd(Target);

    // 避免重复注册
    for (const auto& ExistingAttacker : Info.Attackers)
    {
        if (ExistingAttacker.Get() == Attacker)
        {
            return;
        }
    }

    Info.Attackers.Add(Attacker);

    UE_LOG(LogSGGameplay, Verbose, TEXT("📝 注册攻击者：%s → %s (当前攻击者数: %d)"),
        *Attacker->GetName(), *Target->GetName(), Info.Attackers.Num());
}

/**
 * @brief 注销攻击者
 */
void USG_TargetingSubsystem::UnregisterAttacker(ASG_UnitsBase* Attacker, AActor* Target)
{
    if (!Attacker || !Target)
    {
        return;
    }

    FSGTargetAttackerInfo* Info = TargetAttackerMap.Find(Target);
    if (Info)
    {
        Info->Attackers.RemoveAll([Attacker](const TWeakObjectPtr<ASG_UnitsBase>& Ptr)
        {
            return Ptr.Get() == Attacker;
        });

        UE_LOG(LogSGGameplay, Verbose, TEXT("📝 注销攻击者：%s → %s (剩余攻击者数: %d)"),
            *Attacker->GetName(), *Target->GetName(), Info->Attackers.Num());

        // 如果没有攻击者了，移除记录
        if (Info->Attackers.Num() == 0)
        {
            TargetAttackerMap.Remove(Target);
        }
    }
}

/**
 * @brief 获取目标的攻击者数量
 */
int32 USG_TargetingSubsystem::GetAttackerCount(AActor* Target) const
{
    if (!Target)
    {
        return 0;
    }

    const FSGTargetAttackerInfo* Info = TargetAttackerMap.Find(Target);
    if (Info)
    {
        int32 Count = 0;
        for (const auto& Attacker : Info->Attackers)
        {
            if (Attacker.IsValid() && !Attacker->bIsDead)
            {
                Count++;
            }
        }
        return Count;
    }

    return 0;
}

/**
 * @brief 检查目标是否已满
 */
bool USG_TargetingSubsystem::IsTargetFull(AActor* Target, int32 MaxAttackers) const
{
    return GetAttackerCount(Target) >= MaxAttackers;
}

/**
 * @brief 清理无效数据
 */
void USG_TargetingSubsystem::CleanupInvalidData()
{
    // 清理无效的目标
    for (auto It = TargetAttackerMap.CreateIterator(); It; ++It)
    {
        // 目标无效，移除整个记录
        if (!It.Key().IsValid())
        {
            It.RemoveCurrent();
            continue;
        }

        // 清理无效的攻击者
        It.Value().GetValidAttackerCount();

        // 如果没有攻击者了，移除记录
        if (It.Value().Attackers.Num() == 0)
        {
            It.RemoveCurrent();
        }
    }

    // 清理主城缓存中的无效引用
    CachedMainCities.RemoveAll([](const TWeakObjectPtr<ASG_MainCityBase>& Ptr)
    {
        return !Ptr.IsValid() || !Ptr->IsAlive();
    });
}
