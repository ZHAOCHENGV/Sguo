// 📄 文件：Source/Sguo/Private/Strategies/SG_StrategyEffect_RollingLog.cpp
// 🔧 修改 - 简化版实现

#include "Strategies/SG_StrategyEffect_RollingLog.h"
#include "Actors/SG_RollingLogSpawner.h"
#include "Data/SG_RollingLogCardData.h"
#include "Data/SG_StrategyCardData.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Debug/SG_LogCategories.h"
#include "Kismet/GameplayStatics.h"

ASG_StrategyEffect_RollingLog::ASG_StrategyEffect_RollingLog()
{
    // 不需要 Tick
    PrimaryActorTick.bCanEverTick = false;
}

bool ASG_StrategyEffect_RollingLog::RequiresTargetSelection_Implementation() const
{
    // 不需要玩家选择目标，直接激活场景中的生成器
    return false;
}

bool ASG_StrategyEffect_RollingLog::CanExecute_Implementation() const
{
    // 检查是否有可用的生成器
    TArray<ASG_RollingLogSpawner*> AvailableSpawners;
    int32 Count = FindAvailableSpawners(AvailableSpawners);
    
    return Count > 0;
}

FText ASG_StrategyEffect_RollingLog::GetCannotExecuteReason_Implementation() const
{
    TArray<ASG_RollingLogSpawner*> AvailableSpawners;
    int32 Count = FindAvailableSpawners(AvailableSpawners);
    
    if (Count == 0)
    {
        return FText::FromString(TEXT("场景中没有可用的流木计生成器"));
    }
    
    return FText::GetEmpty();
}

void ASG_StrategyEffect_RollingLog::ExecuteEffect_Implementation()
{
    UE_LOG(LogSGGameplay, Log, TEXT("========== 执行流木计效果 =========="));

    // 设置状态
    SetState(ESGStrategyEffectState::Executing);

    // 查找可用的生成器
    TArray<ASG_RollingLogSpawner*> AvailableSpawners;
    int32 TotalCount = FindAvailableSpawners(AvailableSpawners);

    UE_LOG(LogSGGameplay, Log, TEXT("  找到 %d 个可用生成器"), TotalCount);

    if (TotalCount == 0)
    {
        UE_LOG(LogSGGameplay, Warning, TEXT("  ⚠️ 没有可用的生成器"));
        EndEffect();
        return;
    }

    // 激活所有生成器
    int32 ActivatedCount = 0;
    for (ASG_RollingLogSpawner* Spawner : AvailableSpawners)
    {
        if (ActivateSpawner(Spawner))
        {
            ActivatedCount++;
            ActivatedSpawners.Add(Spawner);
            K2_OnSpawnerActivated(Spawner);
        }
    }

    UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 成功激活 %d/%d 个生成器"), ActivatedCount, TotalCount);

    // 广播所有生成器激活完成
    K2_OnAllSpawnersActivated(ActivatedCount);

    // 效果执行完成（生成器自己管理持续时间）
    EndEffect();

    UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
}

int32 ASG_StrategyEffect_RollingLog::FindAvailableSpawners(TArray<ASG_RollingLogSpawner*>& OutSpawners) const
{
    OutSpawners.Empty();

    // 获取所有生成器
    TArray<AActor*> AllSpawners;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASG_RollingLogSpawner::StaticClass(), AllSpawners);

    for (AActor* Actor : AllSpawners)
    {
        ASG_RollingLogSpawner* Spawner = Cast<ASG_RollingLogSpawner>(Actor);
        if (!Spawner)
        {
            continue;
        }

        // 检查是否可激活
        if (!Spawner->CanActivate())
        {
            continue;
        }

        // 检查阵营（如果需要）
        if (bOnlyActivateSameFaction)
        {
            if (Spawner->FactionTag != InstigatorFactionTag)
            {
                continue;
            }
        }

        OutSpawners.Add(Spawner);
    }

    return OutSpawners.Num();
}

bool ASG_StrategyEffect_RollingLog::ActivateSpawner(ASG_RollingLogSpawner* Spawner)
{
    if (!Spawner)
    {
        return false;
    }

    // 获取卡牌数据（转换为专用类型）
    USG_RollingLogCardData* RollingLogCardData = Cast<USG_RollingLogCardData>(CardData);
    
    if (!RollingLogCardData)
    {
        UE_LOG(LogSGGameplay, Error, TEXT("  ❌ 卡牌数据不是 USG_RollingLogCardData 类型"));
        return false;
    }

    // 获取施放者 ASC
    UAbilitySystemComponent* SpawnerASC = nullptr;
    if (EffectInstigator)
    {
        SpawnerASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(EffectInstigator);
    }

    // 激活生成器
    return Spawner->Activate(RollingLogCardData, SpawnerASC);
}
