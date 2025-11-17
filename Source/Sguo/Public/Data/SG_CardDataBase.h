
/**
 * @file SG_CardData.h
 * @brief 卡牌数据资产基类
 * 
 * 功能说明：
 * - 定义所有卡牌的基础数据结构
 * - 支持AssetManager异步加载
 * - 子类在蓝图中创建
 * 
 * 为什么用C++：
 * - DataAsset需要C++基类以支持序列化
 * - 需要重写GetPrimaryAssetId以支持AssetManager
 * - C++提供更好的类型检查和性能
 * 
 * 使用方式：
 * - 在蓝图中继承此类创建具体的卡牌数据资产
 * - 配置各种参数
 * - 在卡牌池中引用这些资产
 * 
 * 注意事项：
 * - 不要直接实例化此类，应该创建子类
 * - 卡牌ID（资产名称）必须唯一
 */

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "Type/SG_Types.h"
#include "SG_CardDataBase.generated.h"



/**
 * @brief 卡牌数据资产基类
 * 
 * 所有卡牌的基础数据，包含：
 * - 卡牌基本信息（名称、图标、描述）
 * - 卡牌类型标签
 * - 放置配置
 * 
 * 继承关系：
 * - UPrimaryDataAsset：支持AssetManager管理
 * - 子类：USGCharacterCardData（角色卡）、USGStrategyCardData（计谋卡）
 */
UCLASS(BlueprintType,Blueprintable)
class SGUO_API USG_CardDataBase : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	// ========== 卡牌基本信息 ==========
	
	// 卡牌名称
	// 显示在UI上的卡牌名称
	// 例如："曹操"、"步兵"、"强攻计"
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Card Info")
	FText CardName;
	
	// 卡牌描述
	// 显示卡牌的效果说明
	// 例如："魏国领袖，技能：剑雨"
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Card Info")
	FText CardDescription;
	
	// 卡牌图标
	// 在手牌UI中显示的图片
	// 建议尺寸：256x256或512x512
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Card Info")
	UTexture2D* CardIcon;
	
	// 卡牌类型标签
	// 用于识别卡牌类型：Hero（英雄）、Troop（兵团）、Strategy（计谋）
	// 为什么用GameplayTag而不是枚举：
	// - GameplayTag支持层级结构，更灵活
	// - 可以在运行时动态添加新类型
	// - GAS系统原生支持，便于集成
	// meta = (Categories = "Card.Type")：限制只能选择Card.Type下的标签
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Card Info", meta = (Categories = "Card.Type"))
	FGameplayTag CardTypeTag;
	
	// 卡牌稀有度标签
	// 用于区分卡牌稀有度：Common（普通）、Rare（稀有）、Epic（史诗）
	// 可以影响卡牌的抽取概率、UI颜色等
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Card Info", meta = (Categories = "Card.Rarity"))
	FGameplayTag CardRarityTag;


	// ========== 放置配置 ==========
	
	// 放置类型
	// 决定卡牌如何放置：单点、区域、全局
	// 影响玩家的操作流程和UI显示
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placement")
	ESGPlacementType PlacementType;
	
	// 放置区域大小
	// 只对Area类型有效，定义放置区域的宽度和高度（单位：厘米）
	// 例如：(500, 500) 表示5米x5米的区域
	// EditCondition：只有当PlacementType为Area时才显示此属性
	// EditConditionHides：不满足条件时隐藏而不是禁用
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placement", 
		meta = (EditCondition = "PlacementType == ESGPlacementType::Area", EditConditionHides))
	FVector2D PlacementAreaSize;
	
	// 是否受前线限制
	// True：只能在前线内放置（玩家单位在蓝线左侧）
	// False：可以在任何地方放置（通常是计谋卡）
	// 为什么需要这个：
	// - 防止玩家直接在敌人后方放置单位
	// - 计谋卡需要能够作用于敌方区域
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placement")
	bool bRespectFrontLine;
	
	// 是否唯一
	// True：此卡牌在整局游戏中只能使用一次（如英雄卡）
	// False：可以重复抽到和使用
	// 为什么需要这个：
	// - 英雄卡的平衡性考虑，避免多个同名英雄
	// - 符合三国题材的设定（每个武将只有一个）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Card Info")
	bool bIsUnique;

	// ========== AssetManager支持 ==========
	
	/**
	 * @brief 获取主资产ID
	 * @return FPrimaryAssetId 资产ID，格式为 "Card:卡牌名称"
	 * 
	 * 用途：
	 * - AssetManager使用此ID管理资产
	 * - 支持异步加载
	 * - 用于资产引用和查找
	 * 
	 * 为什么需要重写：
	 * - 默认实现返回空ID
	 * - 我们需要自定义ID格式以便管理
	 * 
	 * 注意：
	 * - ID必须全局唯一
	 * - 使用资产的FName作为ID，确保唯一性
	 */
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
