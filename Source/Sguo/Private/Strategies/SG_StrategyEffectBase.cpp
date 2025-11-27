// SG_StrategyEffectBase.cpp
// Fill out your copyright notice in the Description page of Project Settings.

#include "Strategies/SG_StrategyEffectBase.h"
#include "Data/SG_StrategyCardData.h"
#include "Units/SG_UnitsBase.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Kismet/GameplayStatics.h"
#include "Debug/SG_LogCategories.h"

ASG_StrategyEffectBase::ASG_StrategyEffectBase()
{
    // 禁用 Tick（子类按需开启）
    PrimaryActorTick.bCanEverTick = false;
}

void ASG_StrategyEffectBase::BeginPlay()
{
    Super::BeginPlay();
}

// 🔧 修改 - 更新参数名和变量名
void ASG_StrategyEffectBase::InitializeEffect(
    USG_StrategyCardData* InCardData,
    AActor* InEffectInstigator,
    const FVector& InTargetLocation)
{
    // 输出日志
    UE_LOG(LogSGGameplay, Log, TEXT("========== 初始化计谋效果 =========="));
    
    // 缓存卡牌数据
    CardData = InCardData;
    
    // 🔧 修改 - 使用新的变量名
    // 缓存施放者
    EffectInstigator = InEffectInstigator;
    
    // 设置目标位置
    TargetLocation = InTargetLocation;
    
    // 从卡牌数据读取持续时间
    if (CardData)
    {
        EffectDuration = CardData->Duration;
        UE_LOG(LogSGGameplay, Log, TEXT("  卡牌：%s"), *CardData->CardName.ToString());
        UE_LOG(LogSGGameplay, Log, TEXT("  持续时间：%.1f 秒"), EffectDuration);
    }
    
    // 确定施放者阵营
    // 默认为玩家阵营
    InstigatorFactionTag = FGameplayTag::RequestGameplayTag(FName("Unit.Faction.Player"), false);
    
    // 🔧 修改 - 使用新的变量名
    // 如果施放者是单位，使用其阵营
    if (ASG_UnitsBase* InstigatorUnit = Cast<ASG_UnitsBase>(InEffectInstigator))
    {
        InstigatorFactionTag = InstigatorUnit->FactionTag;
    }
    
    UE_LOG(LogSGGameplay, Log, TEXT("  施放者阵营：%s"), *InstigatorFactionTag.ToString());
    UE_LOG(LogSGGameplay, Log, TEXT("  目标位置：%s"), *TargetLocation.ToString());
    UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
}

void ASG_StrategyEffectBase::ExecuteEffect_Implementation()
{
    // 标记已执行
    bHasExecuted = true;
    
    // 基类不执行任何效果
    // 子类需要重写此函数
    UE_LOG(LogSGGameplay, Warning, TEXT("⚠️ ASG_StrategyEffectBase::ExecuteEffect_Implementation 被调用"));
    UE_LOG(LogSGGameplay, Warning, TEXT("   子类应该重写此函数！"));
}

void ASG_StrategyEffectBase::EndEffect()
{
    UE_LOG(LogSGGameplay, Log, TEXT("========== 结束计谋效果 =========="));
    
    if (CardData)
    {
        UE_LOG(LogSGGameplay, Log, TEXT("  卡牌：%s"), *CardData->CardName.ToString());
    }
    
    // 销毁 Actor
    Destroy();
    
    UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
}

void ASG_StrategyEffectBase::GetAllUnitsOfFaction(FGameplayTag FactionTag, TArray<AActor*>& OutUnits)
{
    // 清空输出数组
    OutUnits.Empty();
    
    // 获取场景中所有单位
    TArray<AActor*> AllUnits;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASG_UnitsBase::StaticClass(), AllUnits);
    
    // 筛选指定阵营的存活单位
    for (AActor* Actor : AllUnits)
    {
        ASG_UnitsBase* Unit = Cast<ASG_UnitsBase>(Actor);
        if (Unit && !Unit->bIsDead && Unit->FactionTag.MatchesTag(FactionTag))
        {
            OutUnits.Add(Unit);
        }
    }
    
    UE_LOG(LogSGGameplay, Verbose, TEXT("  找到 %d 个 %s 阵营的单位"), 
        OutUnits.Num(), *FactionTag.ToString());
}

void ASG_StrategyEffectBase::GetUnitsInRadius(
    const FVector& Center,
    float Radius,
    FGameplayTag FactionTag,
    TArray<AActor*>& OutUnits)
{
    // 清空输出数组
    OutUnits.Empty();
    
    // 获取场景中所有单位
    TArray<AActor*> AllUnits;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASG_UnitsBase::StaticClass(), AllUnits);
    
    // 筛选范围内的单位
    for (AActor* Actor : AllUnits)
    {
        ASG_UnitsBase* Unit = Cast<ASG_UnitsBase>(Actor);
        if (!Unit || Unit->bIsDead)
        {
            continue;
        }
        
        // 检查阵营（如果指定了阵营）
        if (FactionTag.IsValid() && !Unit->FactionTag.MatchesTag(FactionTag))
        {
            continue;
        }
        
        // 检查距离
        float Distance = FVector::Dist(Center, Unit->GetActorLocation());
        if (Distance <= Radius)
        {
            OutUnits.Add(Unit);
        }
    }
    
    UE_LOG(LogSGGameplay, Verbose, TEXT("  在半径 %.0f 内找到 %d 个单位"), 
        Radius, OutUnits.Num());
}

// 🔧 修改 - 使用新的变量名
bool ASG_StrategyEffectBase::ApplyGameplayEffectToTarget(
    AActor* TargetActor,
    TSubclassOf<UGameplayEffect> EffectClass,
    float Level)
{
    // 检查参数有效性
    if (!TargetActor || !EffectClass)
    {
        UE_LOG(LogSGGameplay, Warning, TEXT("  ⚠️ ApplyGameplayEffectToTarget：参数无效"));
        return false;
    }
    
    // 获取目标的 ASC
    UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor);
    if (!TargetASC)
    {
        UE_LOG(LogSGGameplay, Warning, TEXT("  ⚠️ 目标 %s 没有 ASC"), *TargetActor->GetName());
        return false;
    }
    
    // 创建效果上下文
    FGameplayEffectContextHandle ContextHandle = TargetASC->MakeEffectContext();
    // 🔧 修改 - 使用新的变量名
    ContextHandle.AddInstigator(EffectInstigator, EffectInstigator);
    
    // 创建效果规格
    FGameplayEffectSpecHandle SpecHandle = TargetASC->MakeOutgoingSpec(EffectClass, Level, ContextHandle);
    if (!SpecHandle.IsValid())
    {
        UE_LOG(LogSGGameplay, Warning, TEXT("  ⚠️ 无法创建 GE 规格"));
        return false;
    }
    
    // 应用效果
    FActiveGameplayEffectHandle ActiveHandle = TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
    
    if (ActiveHandle.IsValid())
    {
        UE_LOG(LogSGGameplay, Verbose, TEXT("  ✓ 对 %s 应用 %s 成功"), 
            *TargetActor->GetName(), *EffectClass->GetName());
        return true;
    }
    
    return false;
}

FGameplayTag ASG_StrategyEffectBase::GetInstigatorFactionTag() const
{
    return InstigatorFactionTag;
}
