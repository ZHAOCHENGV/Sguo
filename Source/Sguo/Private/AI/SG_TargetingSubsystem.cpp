// 📄 文件：Source/Sguo/Private/AI/SG_TargetingSubsystem.cpp
// ✨ 新增 - 目标管理子系统实现

#include "AI/SG_TargetingSubsystem.h"
#include "Units/SG_UnitsBase.h"
#include "Buildings/SG_MainCityBase.h"
#include "Debug/SG_LogCategories.h"
#include "Engine/OverlapResult.h"
#include "Components/BoxComponent.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"

// ========== 生命周期 ==========

void USG_TargetingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // 设置定期清理计时器（每5秒清理一次无效数据）
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            CleanupTimerHandle,
            this,
            &USG_TargetingSubsystem::CleanupInvalidData,
            5.0f,
            true
        );
    }

    UE_LOG(LogSGGameplay, Log, TEXT("✓ 目标管理子系统初始化完成"));
}

void USG_TargetingSubsystem::Deinitialize()
{
    // 清理计时器
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(CleanupTimerHandle);
    }

    TargetAttackerMap.Empty();

    Super::Deinitialize();
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

/**
 * @brief 使用场景查询查找最佳目标
 * @param Querier 查询者
 * @param SearchRadius 搜索半径
 * @param OutCandidates 输出：所有候选目标
 * @return 最佳目标
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

    // ========== 步骤1：场景查询获取范围内所有单位 ==========
    TArray<AActor*> NearbyActors;
    PerformSphereQuery(QuerierLocation, SearchRadius, NearbyActors);

    // ========== 步骤2：过滤并评估候选目标 ==========
    for (AActor* Actor : NearbyActors)
    {
        // 跳过自己
        if (Actor == Querier)
        {
            continue;
        }
        
        // ✨ 新增 - 检查是否在忽略列表中
        if (IgnoredActors.Contains(Actor))
        {
            continue;
        }

        // 检查是否是单位
        ASG_UnitsBase* Unit = Cast<ASG_UnitsBase>(Actor);
        if (Unit)
        {
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

            // 计算评分（这里会应用新的拥挤度惩罚）
            float Score = CalculateTargetScore(Querier, Unit, Distance, AttackerCount);

            // 添加到候选列表
            FSGTargetCandidate Candidate;
            Candidate.Target = Unit;
            Candidate.Distance = Distance;
            Candidate.AttackerCount = AttackerCount;
            Candidate.Score = Score;
            Candidate.bIsReachable = true;

            OutCandidates.Add(Candidate);
        }
    }

    // ========== 步骤3：如果没有找到敌方单位，查找敌方主城 ==========
    if (OutCandidates.Num() == 0)
    {
        // 主城通常在较远的地方，需要单独查询
        TArray<AActor*> AllMainCities;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASG_MainCityBase::StaticClass(), AllMainCities);

        for (AActor* Actor : AllMainCities)
        {
            ASG_MainCityBase* MainCity = Cast<ASG_MainCityBase>(Actor);
            if (!MainCity)
            {
                continue;
            }
            
            // ✨ 新增 - 主城也要检查忽略列表
            if (IgnoredActors.Contains(MainCity))
            {
                continue;
            }

            // 跳过同阵营
            if (MainCity->FactionTag == QuerierFaction)
            {
                continue;
            }

            // 跳过已摧毁的
            if (!MainCity->IsAlive())
            {
                continue;
            }

            // 计算距离（考虑主城体积）
            float Distance = FVector::Dist(QuerierLocation, MainCity->GetActorLocation());
            if (MainCity->GetAttackDetectionBox())
            {
                UBoxComponent* DetectionBox = MainCity->GetAttackDetectionBox();
                FVector BoxExtent = DetectionBox->GetScaledBoxExtent();
                float BoxRadius = FMath::Max3(BoxExtent.X, BoxExtent.Y, BoxExtent.Z);
                Distance = FMath::Max(0.0f, Distance - BoxRadius);
            }

            // 获取拥挤度
            int32 AttackerCount = GetAttackerCount(MainCity);

            // 计算评分（主城评分稍低，优先攻击敌方单位）
            float Score = CalculateTargetScore(Querier, MainCity, Distance, AttackerCount) * 0.8f;

            FSGTargetCandidate Candidate;
            Candidate.Target = MainCity;
            Candidate.Distance = Distance;
            Candidate.AttackerCount = AttackerCount;
            Candidate.Score = Score;
            Candidate.bIsReachable = true;

            OutCandidates.Add(Candidate);
        }
    }

    // ========== 步骤4：排序并返回最佳目标 ==========
    if (OutCandidates.Num() == 0)
    {
        UE_LOG(LogSGGameplay, Verbose, TEXT("%s 未找到任何目标"), *Querier->GetName());
        return nullptr;
    }

    // 按评分降序排序
    OutCandidates.Sort([](const FSGTargetCandidate& A, const FSGTargetCandidate& B)
    {
        return A.Score > B.Score;
    });

    // 返回评分最高的
    AActor* BestTarget = OutCandidates[0].Target.Get();

    UE_LOG(LogSGGameplay, Log, TEXT("🎯 %s 选择目标：%s (距离: %.0f, 攻击者: %d, 评分: %.2f)"),
        *Querier->GetName(),
        BestTarget ? *BestTarget->GetName() : TEXT("None"),
        OutCandidates[0].Distance,
        OutCandidates[0].AttackerCount,
        OutCandidates[0].Score);

    return BestTarget;
}




/**
 * @brief 计算目标评分
 * @param Querier 查询者
 * @param Target 目标
 * @param Distance 距离
 * @param AttackerCount 攻击者数量
 * @return 评分（越高越好）
 * @details
 * 评分公式：
 * Score = (MaxDistance - Distance) / MaxDistance * DistanceWeight
 *       / (1 + AttackerCount * CrowdingPenalty)
 */
float USG_TargetingSubsystem::CalculateTargetScore(
    ASG_UnitsBase* Querier,
    AActor* Target,
    float Distance,
    int32 AttackerCount) const
{
    // 基础分数：距离越近分数越高
    float MaxDistance = Querier->GetDetectionRange();
    if (MaxDistance <= 0.0f) MaxDistance = 1000.0f; // 安全检查

    float DistanceScore = FMath::Clamp((MaxDistance - Distance) / MaxDistance, 0.0f, 1.0f);

    // ✨ 修改 - 动态计算拥挤惩罚因子
    // 假设单位包围一个目标，超过 4-6 人就很难挤进去了
    float PenaltyFactor = 1.0f;
    
    if (AttackerCount > 4)
    {
        // 极度拥挤：如果有超过4个人在打，且你自己不是其中之一，评分大幅下降
        // 这会迫使后排单位去寻找其他目标
        PenaltyFactor = 5.0f + (AttackerCount - 4) * 2.0f;
    }
    else if (AttackerCount > 0)
    {
        // 轻微拥挤
        PenaltyFactor = 1.0f + (AttackerCount * 0.5f);
    }

    // 基础分扩大，便于观察
    float BaseScore = DistanceScore * 100.0f;

    // 最终分 = 基础分 / 拥挤惩罚
    // 如果拥挤度很高，FinalScore 会变得很小，单位就会选择稍远但没人打的目标
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
        // 计算有效攻击者数量（跳过无效引用）
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
}
