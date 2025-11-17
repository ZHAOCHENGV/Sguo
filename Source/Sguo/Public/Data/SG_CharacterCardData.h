// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SG_CardDataBase.h"
#include "SG_CharacterCardData.generated.h"

/**
 * 
 */
UCLASS()
class SGUO_API USG_CharacterCardData : public USG_CardDataBase
{
	GENERATED_BODY()
	public:
	// 要生成的角色蓝图类
	// 使用此卡牌时会生成此类的实例
	// 应该是继承自Character或我们的BP_SG_CharacterBase的蓝图类
	// 为什么用TSubclassOf而不是直接引用：
	// - TSubclassOf提供类型安全，只能选择AActor的子类
	// - 在编辑器中有更好的选择器UI
	// - 支持蓝图类和C++类
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character")
	TSubclassOf<AActor> CharacterClass;
	
	// 是否是兵团卡
	// True：生成多个单位（兵团）
	// False：生成单个单位（英雄）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Troop")
	bool bIsTroopCard;
	
	// 兵团阵型
	// 定义兵团的行列数
	// 例如：(5, 5) 表示5行5列，共25个单位
	// 只有当bIsTroopCard为True时才显示
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Troop", 
		meta = (EditCondition = "bIsTroopCard", EditConditionHides))
	FIntPoint TroopFormation;
	
	// 兵团单位间距
	// 单位之间的距离（单位：厘米）
	// 例如：100.0 表示单位间隔1米
	// 为什么需要配置：
	// - 不同单位体型不同，需要不同的间距
	// - 影响兵团的整体占地面积
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Troop", 
		meta = (EditCondition = "bIsTroopCard", EditConditionHides))
	float TroopSpacing;
};


