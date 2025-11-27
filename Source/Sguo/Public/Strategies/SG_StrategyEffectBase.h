// SG_StrategyEffectBase.h
// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "SG_StrategyEffectBase.generated.h"

class USG_StrategyCardData;
class UAbilitySystemComponent;

/**
 * @brief 计谋效果基类
 * @details
 * 功能说明：
 * - 所有计谋效果 Actor 的基类
 * - 提供通用的初始化、执行、清理接口
 * - 支持全局效果和区域效果两种模式
 * 详细流程：
 * 1. PlayerController 生成效果 Actor
 * 2. 调用 InitializeEffect 传入卡牌数据和施放者
 * 3. 调用 ExecuteEffect 开始执行效果
 * 4. 效果结束后自动销毁
 * 注意事项：
 * - 子类需要重写 ExecuteEffect_Implementation
 * - 区域效果需要设置 TargetLocation
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class SGUO_API ASG_StrategyEffectBase : public AActor
{
    GENERATED_BODY()

public:
    ASG_StrategyEffectBase();

protected:
    virtual void BeginPlay() override;

public:
    // ========== 初始化接口 ==========
    
    /**
     * @brief 初始化计谋效果
     * @param InCardData 计谋卡数据
     * @param InEffectInstigator 施放者（通常是 PlayerController 的 Pawn）
     * @param InTargetLocation 目标位置（区域效果使用）
     * @details
     * 功能说明：
     * - 缓存卡牌数据和施放者信息
     * - 设置目标位置
     * - 确定施放者阵营
     */
    UFUNCTION(BlueprintCallable, Category = "Strategy Effect")
    virtual void InitializeEffect(
        USG_StrategyCardData* InCardData,
        AActor* InEffectInstigator,  // 🔧 修改 - 重命名参数避免与基类冲突
        const FVector& InTargetLocation
    );

    /**
     * @brief 执行计谋效果
     * @details
     * 功能说明：
     * - 开始执行计谋效果
     * - 子类需要重写 ExecuteEffect_Implementation
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Strategy Effect")
    void ExecuteEffect();
    virtual void ExecuteEffect_Implementation();

    /**
     * @brief 结束计谋效果
     * @details
     * 功能说明：
     * - 清理效果
     * - 销毁 Actor
     */
    UFUNCTION(BlueprintCallable, Category = "Strategy Effect")
    virtual void EndEffect();

protected:
    // ========== 辅助函数 ==========
    
    /**
     * @brief 获取指定阵营的所有单位
     * @param FactionTag 阵营标签
     * @param OutUnits 输出的单位数组
     * @details
     * 功能说明：
     * - 查找场景中指定阵营的所有存活单位
     * - 用于全局效果（如神速计）
     */
    UFUNCTION(BlueprintCallable, Category = "Strategy Effect")
    void GetAllUnitsOfFaction(FGameplayTag FactionTag, TArray<AActor*>& OutUnits);

    /**
     * @brief 获取区域内的所有单位
     * @param Center 区域中心
     * @param Radius 区域半径
     * @param FactionTag 阵营标签（可选，为空则获取所有阵营）
     * @param OutUnits 输出的单位数组
     * @details
     * 功能说明：
     * - 查找指定区域内的所有单位
     * - 用于区域效果（如火矢计）
     */
    UFUNCTION(BlueprintCallable, Category = "Strategy Effect")
    void GetUnitsInRadius(
        const FVector& Center,
        float Radius,
        FGameplayTag FactionTag,
        TArray<AActor*>& OutUnits
    );

    /**
     * @brief 对单位应用 GameplayEffect
     * @param TargetActor 目标单位
     * @param EffectClass 要应用的 GE 类
     * @param Level 效果等级
     * @return 是否成功应用
     */
    UFUNCTION(BlueprintCallable, Category = "Strategy Effect")
    bool ApplyGameplayEffectToTarget(
        AActor* TargetActor,
        TSubclassOf<UGameplayEffect> EffectClass,
        float Level = 1.0f
    );

    /**
     * @brief 获取施放者的阵营标签
     * @return 阵营标签
     */
    UFUNCTION(BlueprintPure, Category = "Strategy Effect")
    FGameplayTag GetInstigatorFactionTag() const;

    // ✨ 新增 - 获取效果施放者
    /**
     * @brief 获取效果施放者
     * @return 施放者 Actor
     */
    UFUNCTION(BlueprintPure, Category = "Strategy Effect")
    AActor* GetEffectInstigator() const { return EffectInstigator; }

protected:
    // ========== 配置属性 ==========
    
    // 计谋卡数据引用
    UPROPERTY(BlueprintReadOnly, Category = "Strategy Effect", meta = (DisplayName = "卡牌数据"))
    TObjectPtr<USG_StrategyCardData> CardData;

    // 🔧 修改 - 重命名变量避免与 AActor::Instigator 冲突
    // 施放者（通常是 PlayerController 的 Pawn）
    UPROPERTY(BlueprintReadOnly, Category = "Strategy Effect", meta = (DisplayName = "效果施放者"))
    TObjectPtr<AActor> EffectInstigator;

    // 目标位置（区域效果使用）
    UPROPERTY(BlueprintReadOnly, Category = "Strategy Effect", meta = (DisplayName = "目标位置"))
    FVector TargetLocation;

    // 施放者阵营标签
    UPROPERTY(BlueprintReadOnly, Category = "Strategy Effect", meta = (DisplayName = "施放者阵营"))
    FGameplayTag InstigatorFactionTag;

    // 效果持续时间（从卡牌数据读取）
    UPROPERTY(BlueprintReadOnly, Category = "Strategy Effect", meta = (DisplayName = "持续时间"))
    float EffectDuration;

    // 是否已执行
    UPROPERTY(BlueprintReadOnly, Category = "Strategy Effect", meta = (DisplayName = "已执行"))
    bool bHasExecuted = false;
};
