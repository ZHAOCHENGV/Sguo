// Fill out your copyright notice in the Description page of Project Settings.


#include "AssetManger/SG_AssetManager.h"
// 引入 GameplayTags 定义头文件，便于初始化原生标签
#include "Public/AbilitySystem/SG_GameplayTags.h"

// 定义卡牌主资产类型常量
// 定义卡牌主资产类型的实际值
const FPrimaryAssetType USG_AssetManager::CardAssetType = FPrimaryAssetType(TEXT("Card"));
// 定义卡组主资产类型常量
const FPrimaryAssetType USG_AssetManager::DeckAssetType = FPrimaryAssetType(TEXT("Deck"));
// 蓝图版本：异步加载卡组配置
void USG_AssetManager::LoadDeckConfigAsync(const FString& DeckAssetId)
{
	// 将字符串解析为主资产 ID
	FPrimaryAssetId AssetId(DeckAssetId);

	// 检查 ID 是否有效且类型匹配
	if (!AssetId.IsValid() || AssetId.PrimaryAssetType != DeckAssetType)
	{
		// 输出警告，提示 ID 无效
		UE_LOG(LogTemp, Warning, TEXT("异步加载卡组失败：无效的卡组ID '%s'"), *DeckAssetId);
		// 告知调用方正确的 ID 写法
		UE_LOG(LogTemp, Warning, TEXT("  正确格式：'Deck:卡组资产名称'"));
		// 终止函数，避免继续执行加载逻辑
		return;
	}

	// 输出日志便于调试
	UE_LOG(LogTemp, Log, TEXT("开始异步加载卡组：%s"), *DeckAssetId);

	// 调用 C++ 实现
	LoadDeckConfig(AssetId, FStreamableDelegate());
}

// 蓝图版本：获取已加载的卡组配置
UObject* USG_AssetManager::GetLoadedDeckConfig(const FString& DeckAssetId) const
{
	// 构造主资产 ID
	FPrimaryAssetId AssetId(DeckAssetId);

	// 检查 ID 是否有效
	if (!AssetId.IsValid() || AssetId.PrimaryAssetType != DeckAssetType)
	{
		// 输出警告，提示 ID 无效
		UE_LOG(LogTemp, Warning, TEXT("获取卡组配置失败：无效的卡组ID '%s'"), *DeckAssetId);
		// 返回空指针告知调用方失败
		return nullptr;
	}

	// 返回已加载的对象
	UObject* LoadedAsset = GetPrimaryAssetObject(AssetId);

	// 输出日志帮助定位问题
	if (LoadedAsset)
	{
		// 记录成功信息
		UE_LOG(LogTemp, Verbose, TEXT("✓ 成功获取已加载的卡组：%s"), *DeckAssetId);
	}
	else
	{
		// 提醒调用方资产尚未准备好
		UE_LOG(LogTemp, Warning, TEXT("卡组尚未加载或加载失败：%s"), *DeckAssetId);
	}

	// 返回查询结果
	return LoadedAsset;
}

// 蓝图版本：同步加载卡组配置
UObject* USG_AssetManager::LoadDeckConfigSyncBP(const FString& DeckAssetId)
{
	// 构造主资产 ID
	FPrimaryAssetId AssetId(DeckAssetId);

	// 检查 ID 与类型
	if (!AssetId.IsValid() || AssetId.PrimaryAssetType != DeckAssetType)
	{
		// 输出错误日志，阻止非法 ID 的同步加载
		UE_LOG(LogTemp, Error, TEXT("同步加载卡组失败：无效的卡组ID '%s'"), *DeckAssetId);
		// 返回空指针提示调用方加载失败
		return nullptr;
	}

	// 提醒开发者同步加载可能带来卡顿
	UE_LOG(LogTemp, Warning, TEXT("⚠️ 正在同步加载卡组 '%s'，可能造成卡顿"), *DeckAssetId);

	// 调用 C++ 版本
	return LoadDeckConfigSync(AssetId);
}



// 获取 AssetManager 单例
USG_AssetManager* USG_AssetManager::Get()
{
	// 检查引擎是否已初始化
	// 为什么要检查：在某些情况下（如编辑器启动早期）GEngine 可能为空
	if (!GEngine)
	{
		UE_LOG(LogTemp, Error, TEXT("获取资产管理器失败：引擎尚未初始化"));
		return nullptr;
	}

	// 从引擎获取 AssetManager 实例
	USG_AssetManager* AssetManager = Cast<USG_AssetManager>(GEngine->AssetManager);

	// 检查转换是否成功
	if (AssetManager)
	{
		return AssetManager;
	}
	else
	{
		// 如果转换失败，说明配置有问题
		UE_LOG(LogTemp, Error, TEXT("✗ 资产管理器配置错误！请在 DefaultEngine.ini 中设置 AssetManagerClassName=SG_AssetManager"));
		return nullptr;
	}
}

// 蓝图版本：异步加载卡牌数据
void USG_AssetManager::LoadCardDataAsync(const FString& CardAssetId)
{
	// 直接从字符串构造 FPrimaryAssetId
	// 这比手动分割字符串更简单且不容易出错
	FPrimaryAssetId AssetId(CardAssetId);
	
	// 验证 AssetId 是否有效
	if (!AssetId.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("异步加载卡牌失败：无效的卡牌ID格式 '%s'"), *CardAssetId);
		UE_LOG(LogTemp, Warning, TEXT("  正确格式：'Card:卡牌资产名称'，例如 'Card:DA_Card_Troop_Infantry'"));
		return;
	}
	
	UE_LOG(LogTemp, Log, TEXT("开始异步加载卡牌：%s"), *CardAssetId);
	
	// 调用 C++ 版本的加载函数
	LoadCardData(AssetId, FStreamableDelegate());
}

// 蓝图版本：获取已加载的卡牌数据
UObject* USG_AssetManager::GetLoadedCardData(const FString& CardAssetId) const
{
	// 直接从字符串构造
	FPrimaryAssetId AssetId(CardAssetId);
	
	// 验证 AssetId
	if (!AssetId.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("获取卡牌数据失败：无效的卡牌ID '%s'"), *CardAssetId);
		return nullptr;
	}
	
	// 获取已加载的资产
	UObject* LoadedAsset = GetPrimaryAssetObject(AssetId);
	
	if (LoadedAsset)
	{
		UE_LOG(LogTemp, Verbose, TEXT("✓ 成功获取已加载的卡牌：%s"), *CardAssetId);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("卡牌尚未加载或加载失败：%s"), *CardAssetId);
		UE_LOG(LogTemp, Warning, TEXT("  提示：请先调用 LoadCardDataAsync 加载卡牌"));
	}
	
	return LoadedAsset;
}

// 蓝图版本：同步加载卡牌数据
UObject* USG_AssetManager::LoadCardDataSyncBP(const FString& CardAssetId)
{
	// 直接从字符串构造
	FPrimaryAssetId AssetId(CardAssetId);
	
	// 验证 AssetId
	if (!AssetId.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("同步加载失败：无效的卡牌ID '%s'"), *CardAssetId);
		return nullptr;
	}
	
	UE_LOG(LogTemp, Warning, TEXT("⚠️ 正在同步加载卡牌 '%s'，这可能导致游戏卡顿！"), *CardAssetId);
	
	// 调用 C++ 版本的同步加载函数
	return LoadCardDataSync(AssetId);
}

// ========== 以下是原有的 C++ 函数实现 ==========

// C++ 版本：异步加载单个卡牌数据
void USG_AssetManager::LoadCardData(const FPrimaryAssetId& CardId, FStreamableDelegate Delegate)
{
	// 检查 CardId 是否有效
	if (!CardId.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("加载卡牌失败：无效的 CardId"));
		
		// 如果 Delegate 绑定了函数，立即执行（表示加载"完成"，虽然失败了）
		if (Delegate.IsBound())
		{
			Delegate.Execute();
		}
		return;
	}

	// 创建加载请求
	// TArray<FName> 是要加载的资产包列表
	// 为什么使用空数组：LoadPrimaryAsset 会自动处理依赖
	TArray<FName> Bundles;
	
	// 调用引擎的异步加载函数
	CurrentLoadHandle = LoadPrimaryAsset(CardId, Bundles, Delegate);

	// 检查加载是否成功启动
	if (!CurrentLoadHandle.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("启动异步加载失败：%s"), *CardId.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("✓ 开始异步加载：%s"), *CardId.ToString());
	}
}

// 批量异步加载多个卡牌数据
TSharedPtr<FStreamableHandle> USG_AssetManager::LoadCardDataBatch(const TArray<FPrimaryAssetId>& CardIds, FStreamableDelegate Delegate)
{
	// 检查输入是否有效
	if (CardIds.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("批量加载失败：卡牌ID列表为空"));
		
		// 立即执行回调
		if (Delegate.IsBound())
		{
			Delegate.Execute();
		}
		return nullptr;
	}

	// 创建空的资产包列表
	TArray<FName> Bundles;
	
	// 批量加载所有卡牌
	TSharedPtr<FStreamableHandle> LoadHandle = LoadPrimaryAssets(CardIds, Bundles, Delegate);

	// 检查加载是否成功启动
	if (!LoadHandle.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("批量加载启动失败：共 %d 张卡牌"), CardIds.Num());
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("✓ 开始批量加载 %d 张卡牌"), CardIds.Num());
	}
	
	return LoadHandle;
}

// C++ 版本：异步加载卡组配置
void USG_AssetManager::LoadDeckConfig(const FPrimaryAssetId& DeckId, FStreamableDelegate Delegate)
{
	// 校验 ID 是否有效
	if (!DeckId.IsValid() || DeckId.PrimaryAssetType != DeckAssetType)
	{
		// 输出警告，提示 DeckId 异常
		UE_LOG(LogTemp, Warning, TEXT("加载卡组失败：无效的 DeckId %s"), *DeckId.ToString());
		if (Delegate.IsBound())
		{
			// 回调执行，通知调用方加载已结束（失败）
			Delegate.Execute();
		}
		// 终止函数
		return;
	}

	// 准备资产包列表（当前为空）
	TArray<FName> Bundles;

	// 调用引擎异步加载
	CurrentLoadHandle = LoadPrimaryAsset(DeckId, Bundles, Delegate);

	// 根据句柄有效性记录日志
	if (!CurrentLoadHandle.IsValid())
	{
		// 提示加载启动失败
		UE_LOG(LogTemp, Warning, TEXT("启动卡组异步加载失败：%s"), *DeckId.ToString());
	}
	else
	{
		// 输出成功开始加载的日志
		UE_LOG(LogTemp, Log, TEXT("✓ 开始异步加载卡组：%s"), *DeckId.ToString());
	}
}

// 预加载常用资产
void USG_AssetManager::PreloadEssentialAssets()
{
	UE_LOG(LogTemp, Log, TEXT("========== 开始预加载常用资产 =========="));

	// TODO: 在这里添加需要预加载的资产
	// 例如：
	// - UI 资产
	// - 常用音效
	// - 粒子特效
	
	// 示例：预加载所有卡牌（如果卡牌不多的话）
	// TArray<FPrimaryAssetId> AllCardIds;
	// GetPrimaryAssetIdList(TEXT("Card"), AllCardIds);
	// if (AllCardIds.Num() > 0)
	// {
	//     UE_LOG(LogTemp, Log, TEXT("预加载 %d 张卡牌..."), AllCardIds.Num());
	//     LoadCardDataBatch(AllCardIds, FStreamableDelegate());
	// }

	UE_LOG(LogTemp, Log, TEXT("========== 预加载完成 =========="));
}

// C++ 版本：同步加载卡牌数据（阻塞）
UObject* USG_AssetManager::LoadCardDataSync(const FPrimaryAssetId& CardId)
{
	// 警告：这个函数会阻塞游戏线程
	UE_LOG(LogTemp, Warning, TEXT("⚠️ 同步加载（阻塞）：%s，可能导致游戏卡顿"), *CardId.ToString());

	// 检查 CardId 是否有效
	if (!CardId.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("同步加载失败：无效的 CardId"));
		// 返回空指针，外部需判空
		return nullptr;
	}

	// 同步加载资产
	TArray<FName> Bundles;
	LoadPrimaryAsset(CardId, Bundles);

	// 获取加载的资产
	UObject* LoadedAsset = GetPrimaryAssetObject(CardId);

	// 检查加载结果
	if (LoadedAsset)
	{
		UE_LOG(LogTemp, Log, TEXT("✓ 同步加载成功：%s"), *CardId.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("✗ 同步加载失败：%s"), *CardId.ToString());
		UE_LOG(LogTemp, Error, TEXT("  可能原因："));
		UE_LOG(LogTemp, Error, TEXT("  1. 资产不存在或路径错误"));
		UE_LOG(LogTemp, Error, TEXT("  2. DefaultGame.ini 中未正确配置 PrimaryAssetTypesToScan"));
		UE_LOG(LogTemp, Error, TEXT("  3. 资产类型不匹配"));
	}

	return LoadedAsset;
}

// C++ 版本：同步加载卡组配置
UObject* USG_AssetManager::LoadDeckConfigSync(const FPrimaryAssetId& DeckId)
{
	// 输出警告，提示同步加载的风险
	UE_LOG(LogTemp, Warning, TEXT("⚠️ 同步加载（阻塞）卡组：%s"), *DeckId.ToString());

	// 校验 ID 是否有效
	if (!DeckId.IsValid() || DeckId.PrimaryAssetType != DeckAssetType)
	{
		// 输出错误日志，阻止非法 DeckId 的加载
		UE_LOG(LogTemp, Error, TEXT("同步加载卡组失败：无效的 DeckId"));
		// 返回空指针，通知调用方加载没有成功
		return nullptr;
	}

	// 同步加载资产
	TArray<FName> Bundles;
	LoadPrimaryAsset(DeckId, Bundles);

	// 获取加载结果
	UObject* LoadedAsset = GetPrimaryAssetObject(DeckId);

	// 输出结果日志
	if (LoadedAsset)
	{
		// 记录成功信息
		UE_LOG(LogTemp, Log, TEXT("✓ 同步加载卡组成功：%s"), *DeckId.ToString());
	}
	else
	{
		// 输出错误信息
		UE_LOG(LogTemp, Error, TEXT("✗ 同步加载卡组失败：%s"), *DeckId.ToString());
	}

	// 返回对象
	return LoadedAsset;
}

// 构造卡牌资产 ID
FPrimaryAssetId USG_AssetManager::MakeCardAssetId(const FName& AssetName)
{
	// 使用统一的资产类型与名称构建 ID
	return FPrimaryAssetId(CardAssetType, AssetName);
}

// 构造卡组资产 ID
FPrimaryAssetId USG_AssetManager::MakeDeckAssetId(const FName& AssetName)
{
	// 使用卡组类型构建 ID
	return FPrimaryAssetId(DeckAssetType, AssetName);
}

// 引擎启动时调用
void USG_AssetManager::StartInitialLoading()
{
	// 调用父类实现
	Super::StartInitialLoading();

	// 注册原生 GameplayTags
	FSG_GameplayTags::InitializeNativeTags();

	UE_LOG(LogTemp, Log, TEXT("========================================"));
	UE_LOG(LogTemp, Log, TEXT("  SG 资产管理器已启动"));
	UE_LOG(LogTemp, Log, TEXT("========================================"));

	// 预加载常用资产（可选）
	// 注意：如果资产很多，可能会增加启动时间
	// PreloadEssentialAssets();
}