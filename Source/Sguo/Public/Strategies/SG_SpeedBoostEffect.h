// SG_SpeedBoostEffect.h
// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Strategies/SG_StrategyEffectBase.h"
#include "SG_SpeedBoostEffect.generated.h"

/**
 * @brief 神速计效果
 * @details
 * 功能说明：
 * - 6秒内提高我方全员速度（包括移速和攻速）
 * - 全局效果，选中后点击任意位置即可生效
 * 详细流程：
 * 1. 获取所有友方单位
 * 2. 对每个单位应用速度增益 GE
 * 3. GE 自动在持续时间结束后移除
 * 注意事项：
 * - 需要创建对应的 GameplayEffect 蓝图
 * - GE 应该修改 MoveSpeed 和 AttackSpeed 属性
 */
UCLASS(BlueprintType, Blueprintable)
class SGUO_API ASG_SpeedBoostEffect : public ASG_StrategyEffectBase
{
    GENERATED_BODY()

public:
    ASG_SpeedBoostEffect();

protected:
    /**
     * @brief 执行神速计效果
     * @details
     * 功能说明：
     * - 获取所有友方单位
     * - 对每个单位应用速度增益 GE
     * - 播放全局特效和音效
     */
    virtual void ExecuteEffect_Implementation() override;

protected:
    // ========== 配置属性 ==========
    
    /**
     * @brief 速度增益 GameplayEffect 类
     * @details
     * 功能说明：
     * - 在蓝图中设置
     * - 应该是一个持续时间型 GE
     * - 修改 MoveSpeed 和 AttackSpeed 属性
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Speed Boost", 
        meta = (DisplayName = "速度增益GE"))
    TSubclassOf<UGameplayEffect> SpeedBoostEffectClass;

    /**
     * @brief 速度增益倍率
     * @details
     * 功能说明：
     * - 速度提升的百分比
     * - 1.5 表示提升 50%
     * - 通过 SetByCaller 传递给 GE
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Speed Boost", 
        meta = (DisplayName = "速度倍率", ClampMin = "1.0", UIMin = "1.0", UIMax = "3.0"))
    float SpeedMultiplier = 1.5f;

    /**
     * @brief 增益特效（施加到每个单位上）
     * @details
     * 功能说明：
     * - 速度增益期间显示的粒子特效
     * - 附着在单位身上
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Speed Boost|VFX", 
        meta = (DisplayName = "增益特效"))
    TObjectPtr<UParticleSystem> BuffVFX;

    /**
     * @brief 施放音效
     * @details
     * 功能说明：
     * - 施放神速计时播放的音效
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Speed Boost|SFX", 
        meta = (DisplayName = "施放音效"))
    TObjectPtr<USoundBase> CastSound;
};
