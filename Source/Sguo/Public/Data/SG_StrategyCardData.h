// Fill out your copyright notice in the Description page of Project Settings.
/**
 * @brief 计谋卡数据资产
 * 
 * 用于施放计谋效果的卡牌
 * 
 * 计谋卡的两种实现方式：
 * 1. 生成效果Actor（如火堆、滚石）：设置EffectActorClass
 * 2. 直接应用GameplayEffect（如Buff）：设置GameplayEffectClass
 * 3. 两者都设置：同时生成Actor和应用Effect
 */
#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "SG_CardDataBase.h"
#include "SG_StrategyCardData.generated.h"

/**
 * 
 */
UCLASS()
class SGUO_API USG_StrategyCardData : public USG_CardDataBase
{
	GENERATED_BODY()

public:
	// 计谋效果标签
	// 标识计谋的类型：Fire（业火）、RollingStone（滚石）、DamageBoost（强攻）等
	// 用途：
	// - 在代码中识别计谋类型
	// - 可以用于条件判断和效果叠加规则
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Strategy", 
		meta = (Categories = "Strategy.Effect"))
	FGameplayTag StrategyEffectTag;
	
	// 目标类型
	// 决定计谋作用于哪一方：友方、敌方、区域、全局
	// 影响目标筛选和效果应用逻辑
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Strategy")
	ESGStrategyTargetType TargetType;
	
	// 效果持续时间（秒）
	// 0表示瞬时效果（如康复计）
	// >0表示持续效果（如强攻计持续6秒）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Strategy")
	float Duration;
	
	// 效果Actor类
	// 如果需要在场景中生成可见的效果（如火堆、滚石），设置此项
	// 如果为None，则不生成Actor（如纯Buff效果）
	// 为什么用TSubclassOf<AActor>而不是具体类型：
	// - 提供最大的灵活性，可以使用任何Actor
	// - 效果Actor可能有不同的基类
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Strategy")
	TSubclassOf<AActor> EffectActorClass;
	
	// 要应用的GameplayEffect类
	// 用于应用属性修改或状态效果（如增加伤害、减速等）
	// 如果为None，则不应用GE（如纯视觉效果）
	// 为什么使用GameplayEffect：
	// - GAS系统的标准做法
	// - 支持复杂的属性计算和修改
	// - 自动处理网络同步和持续时间
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Strategy")
	TSubclassOf<UGameplayEffect> GameplayEffectClass;
};
