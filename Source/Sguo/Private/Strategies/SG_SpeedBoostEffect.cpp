// SG_SpeedBoostEffect.cpp
// Fill out your copyright notice in the Description page of Project Settings.

#include "Strategies/SG_SpeedBoostEffect.h"
#include "Units/SG_UnitsBase.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Kismet/GameplayStatics.h"
#include "Debug/SG_LogCategories.h"

ASG_SpeedBoostEffect::ASG_SpeedBoostEffect()
{
    // 默认速度倍率
    SpeedMultiplier = 1.5f;
}

void ASG_SpeedBoostEffect::ExecuteEffect_Implementation()
{
    // 调用父类实现
    Super::ExecuteEffect_Implementation();
    
    UE_LOG(LogSGGameplay, Log, TEXT("========== 执行神速计 =========="));
    
    // ========== 步骤1：播放施放音效 ==========
    if (CastSound)
    {
        UGameplayStatics::PlaySound2D(this, CastSound);
        UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 播放施放音效"));
    }
    
    // ========== 步骤2：获取所有友方单位 ==========
    TArray<AActor*> FriendlyUnits;
    GetAllUnitsOfFaction(InstigatorFactionTag, FriendlyUnits);
    
    UE_LOG(LogSGGameplay, Log, TEXT("  友方单位数量：%d"), FriendlyUnits.Num());
    
    // ========== 步骤3：检查 GE 类是否设置 ==========
    if (!SpeedBoostEffectClass)
    {
        UE_LOG(LogSGGameplay, Error, TEXT("  ❌ SpeedBoostEffectClass 未设置！"));
        UE_LOG(LogSGGameplay, Error, TEXT("     请在蓝图中设置速度增益 GE"));
        EndEffect();
        return;
    }
    
    // ========== 步骤4：对每个单位应用效果 ==========
    int32 SuccessCount = 0;
    
    for (AActor* Actor : FriendlyUnits)
    {
        ASG_UnitsBase* Unit = Cast<ASG_UnitsBase>(Actor);
        if (!Unit)
        {
            continue;
        }
        
        // 获取单位的 ASC
        UAbilitySystemComponent* UnitASC = Unit->GetAbilitySystemComponent();
        if (!UnitASC)
        {
            UE_LOG(LogSGGameplay, Warning, TEXT("  ⚠️ 单位 %s 没有 ASC"), *Unit->GetName());
            continue;
        }
        
        // 创建效果上下文
        FGameplayEffectContextHandle ContextHandle = UnitASC->MakeEffectContext();
        ContextHandle.AddInstigator(EffectInstigator, EffectInstigator);
        
        // 创建效果规格
        FGameplayEffectSpecHandle SpecHandle = UnitASC->MakeOutgoingSpec(
            SpeedBoostEffectClass, 
            1.0f, 
            ContextHandle
        );
        
        if (!SpecHandle.IsValid())
        {
            UE_LOG(LogSGGameplay, Warning, TEXT("  ⚠️ 无法为 %s 创建 GE 规格"), *Unit->GetName());
            continue;
        }
        
        // ✨ 通过 SetByCaller 传递速度倍率
        // 需要在 GE 蓝图中配置对应的 Tag
        FGameplayTag SpeedMultiplierTag = FGameplayTag::RequestGameplayTag(
            FName("Data.SpeedMultiplier"), 
            false
        );
        
        if (SpeedMultiplierTag.IsValid())
        {
            SpecHandle.Data->SetSetByCallerMagnitude(SpeedMultiplierTag, SpeedMultiplier);
        }
        
        // 应用效果
        FActiveGameplayEffectHandle ActiveHandle = UnitASC->ApplyGameplayEffectSpecToSelf(
            *SpecHandle.Data.Get()
        );
        
        if (ActiveHandle.IsValid())
        {
            SuccessCount++;
            UE_LOG(LogSGGameplay, Verbose, TEXT("  ✓ 对 %s 应用速度增益"), *Unit->GetName());
            
            // 播放增益特效
            if (BuffVFX)
            {
                UGameplayStatics::SpawnEmitterAttached(
                    BuffVFX,
                    Unit->GetRootComponent(),
                    NAME_None,
                    FVector::ZeroVector,
                    FRotator::ZeroRotator,
                    EAttachLocation::KeepRelativeOffset,
                    true
                );
            }
        }
    }
    
    UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 成功对 %d/%d 个单位应用速度增益"), 
        SuccessCount, FriendlyUnits.Num());
    UE_LOG(LogSGGameplay, Log, TEXT("  速度倍率：%.1fx"), SpeedMultiplier);
    UE_LOG(LogSGGameplay, Log, TEXT("  持续时间：%.1f 秒"), EffectDuration);
    UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
    
    // ========== 步骤5：效果立即结束（GE 会自动管理持续时间）==========
    EndEffect();
}
