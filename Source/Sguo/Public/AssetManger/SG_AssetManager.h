// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
/**
 * @file SG_AssetManager.h
 * @brief 自定义资产管理器声明
 * @details
 * 功能说明：
 * - 统一管理项目中的 Primary Assets（卡牌、卡组等）。
 * - 提供 Blueprint/C++ 两套加载接口，支持异步、批量与阻塞加载。
 * - 在引擎初始化阶段完成 GameplayTags 注册与必要的预加载。
 * 详细流程：
 * 1. 引擎启动 → `StartInitialLoading()` → 注册标签、准备常用资产。
 * 2. 运行时通过 `LoadCardData`/`LoadDeckConfig` 等函数加载所需数据资产。
 * 3. 加载完成后通过 `GetPrimaryAssetObject` 或本类包装函数获取实例。
 * 注意事项：
 * - 需在 `DefaultEngine.ini` 中配置 `AssetManagerClassName`。
 * - 新增 Primary Asset Type 时务必更新 `DefaultGame.ini` 的扫描设置。
 * - Blueprint 接口在资产未完成加载时会返回 `nullptr`，调用方需判空。
 */

#include "CoreMinimal.h"
#include "Engine/AssetManager.h"
#include "SG_AssetManager.generated.h"
/**
 * @brief 自定义资产管理器类
 * 
 * 继承自 UAssetManager，提供游戏特定的资产管理功能
 * 
 * 主要功能：
 * - 异步加载卡牌数据
 * - 批量加载资产
 * - 预加载常用资产
 * - 提供单例访问接口
 */

UCLASS()
class SGUO_API USG_AssetManager : public UAssetManager
{
	GENERATED_BODY()

public:
	/**
	 * @brief 卡牌主资产类型常量
	 */
	// 声明卡牌主资产类型，避免使用硬编码字符串
	static const FPrimaryAssetType CardAssetType;

	/**
	 * @brief 卡组主资产类型常量
	 */
	// 声明卡组主资产类型，统一 Deck 资产的类型引用
	static const FPrimaryAssetType DeckAssetType;

	/**
	 * @brief 获取 AssetManager 单例
	 * @return USGAssetManager* 单例指针
	 * 
	 * 使用方式（C++）：
	 * USGAssetManager& AssetManager = USGAssetManager::Get();
	 * 
	 * 使用方式（蓝图）：
	 * Get SG Asset Manager 节点
	 * 
	 * 为什么使用单例：
	 * - AssetManager 在整个游戏中只有一个实例
	 * - 方便全局访问
	 * - 符合 UE 的设计模式
	 * 
	 * 注意：
	 * - 蓝图版本返回指针而不是引用
	 * - 如果配置错误可能返回 nullptr
	 */
	UFUNCTION(BlueprintPure, Category = "Asset Manager", meta = (DisplayName = "Get SG Asset Manager", CompactNodeTitle = "SG Asset Manager"))
	static USG_AssetManager* Get();

	/**
	 * @brief 异步加载单个卡牌数据（蓝图版本）
	 * @param CardAssetId 卡牌的主资产ID字符串（格式："Card:卡牌名称"）
	 * 
	 * 使用示例（蓝图）：
	 * Load Card Data Async
	 *   Card Asset Id = "Card:DA_Card_Troop_Infantry"
	 *   
	 * 注意：
	 * - 这是异步加载，不会立即返回结果
	 * - 加载完成后需要通过事件或轮询检查
	 * - 实际使用中建议在 C++ 中处理加载逻辑
	 */
	UFUNCTION(BlueprintCallable, Category = "Asset Manager")
	void LoadCardDataAsync(const FString& CardAssetId);

	/**
	 * @brief 获取已加载的卡牌数据（蓝图版本）
	 * @param CardAssetId 卡牌的主资产ID字符串
	 * @return UObject* 卡牌数据对象，如果未加载返回 nullptr
	 * 
	 * 使用示例（蓝图）：
	 * Get Loaded Card Data
	 *   Card Asset Id = "Card:DA_Card_Troop_Infantry"
	 *   返回 → Cast To SG Character Card Data
	 * 
	 * 注意：
	 * - 必须先调用 LoadCardDataAsync 加载
	 * - 如果资产未加载完成会返回 nullptr
	 */
	UFUNCTION(BlueprintPure, Category = "Asset Manager")
	UObject* GetLoadedCardData(const FString& CardAssetId) const;

	/**
	 * @brief 同步加载卡牌数据（蓝图版本）
	 * @param CardAssetId 卡牌的主资产ID字符串
	 * @return UObject* 加载的卡牌数据，失败返回 nullptr
	 * 
	 * 警告：
	 * - 此函数会阻塞游戏线程
	 * - 仅用于编辑器工具或特殊情况
	 * - 不推荐在游戏运行时使用
	 */
	UFUNCTION(BlueprintCallable, Category = "Asset Manager", meta = (DisplayName = "Load Card Data Sync (Blocking)"))
	UObject* LoadCardDataSyncBP(const FString& CardAssetId);

	/**
	 * @brief 异步加载卡组配置数据（蓝图版本）
	 * @param DeckAssetId 格式为 "Deck:资产名称" 的主资产 ID 字符串
	 */
	// 蓝图辅助函数：触发卡组配置的异步加载
	UFUNCTION(BlueprintCallable, Category = "Asset Manager")
	void LoadDeckConfigAsync(const FString& DeckAssetId);

	/**
	 * @brief 获取已加载的卡组配置（蓝图版本）
	 * @param DeckAssetId 卡组主资产 ID 字符串
	 * @return UObject* 对应的卡组数据资产
	 */
	// 蓝图辅助函数：查询已经缓存的卡组配置
	UFUNCTION(BlueprintPure, Category = "Asset Manager")
	UObject* GetLoadedDeckConfig(const FString& DeckAssetId) const;

	/**
	 * @brief 同步加载卡组配置（蓝图版本）
	 * @param DeckAssetId 卡组主资产 ID 字符串
	 * @return UObject* 加载完成的卡组数据
	 */
	// 蓝图辅助函数：阻塞式加载卡组配置，仅限编辑器或初始化阶段
	UFUNCTION(BlueprintCallable, Category = "Asset Manager", meta = (DisplayName = "Load Deck Config Sync (Blocking)"))
	UObject* LoadDeckConfigSyncBP(const FString& DeckAssetId);

	// ========== C++ 接口（不暴露给蓝图） ==========
	
	/**
	 * @brief 异步加载单个卡牌数据（C++ 版本）
	 */
	void LoadCardData(const FPrimaryAssetId& CardId, FStreamableDelegate Delegate);

	/**
	 * @brief 批量异步加载多个卡牌数据
	 * @return TSharedPtr<FStreamableHandle> 加载句柄
	 */
	TSharedPtr<FStreamableHandle> LoadCardDataBatch(const TArray<FPrimaryAssetId>& CardIds, FStreamableDelegate Delegate);

	/**
	 * @brief 异步加载卡组配置（C++ 版本）
	 * @param DeckId 目标卡组 ID
	 * @param Delegate 加载完成后的回调
	 */
	// 提供 C++ 版卡组异步加载接口，便于 Deck 系统调用
	void LoadDeckConfig(const FPrimaryAssetId& DeckId, FStreamableDelegate Delegate);

	/**
	 * @brief 预加载常用资产
	 */
	void PreloadEssentialAssets();

	/**
	 * @brief 同步加载卡牌数据（C++ 版本）
	 */
	UObject* LoadCardDataSync(const FPrimaryAssetId& CardId);

	/**
	 * @brief 同步加载卡组配置（C++ 版本）
	 * @param DeckId 目标卡组 ID
	 * @return UObject* 对应的卡组数据资产
	 */
	// 提供 C++ 版卡组同步加载接口，避免外部重复实现
	UObject* LoadDeckConfigSync(const FPrimaryAssetId& DeckId);

	/**
	 * @brief 构造卡牌主资产 ID
	 * @param AssetName 资产名称
	 * @return FPrimaryAssetId 生成的 ID
	 */
	// 帮助函数：根据资产名构造卡牌资产 ID
	static FPrimaryAssetId MakeCardAssetId(const FName& AssetName);

	/**
	 * @brief 构造卡组主资产 ID
	 * @param AssetName 资产名称
	 * @return FPrimaryAssetId 生成的 ID
	 */
	// 帮助函数：根据资产名构造卡组资产 ID
	static FPrimaryAssetId MakeDeckAssetId(const FName& AssetName);

protected:
	virtual void StartInitialLoading() override;

private:
	// 当前的加载句柄
	TSharedPtr<FStreamableHandle> CurrentLoadHandle;
};
