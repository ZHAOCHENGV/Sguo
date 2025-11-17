
// Copyright notice placeholder
/**
 * @file SG_FunctionLibrary.h
 * @brief 卡牌系统函数库头文件
 * @details
 * 功能说明：
 * - 提供卡牌信息获取、查询、类型判断等通用静态函数
 * - 支持从多种来源（DeckComponent、ViewModel）获取卡牌信息
 * - 提供格式化输出和调试功能
 * 
 * 详细流程：
 * - 通过 DeckComponent 或 ViewModel 获取卡牌数据
 * - 将数据封装为 FSGCardDetailInfo 结构体
 * - 提供各种查询和判断功能
 * 
 * 注意事项：
 * - 所有函数都是静态的，可以直接调用
 * - 函数支持蓝图调用
 * - 返回的详细信息结构体包含 bIsValid 字段，使用前应检查
 */
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Data/Type/SG_Types.h"
#include "CardsAndUnits/SG_CardDeckComponent.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SG_FunctionLibrary.generated.h"



// 前向声明卡组组件
class USG_CardDeckComponent;
// 前向声明卡牌 ViewModel
class USGCardViewModel;
// 前向声明手牌 ViewModel
class USGCardHandViewModel;
/**
 * @brief 卡牌详细信息结构体
 * @details
 * 功能说明：
 * - 包含卡牌的所有详细信息
 * - 便于在蓝图和 C++ 中传递和使用
 * - 包含基础信息、状态信息、放置信息等
 * 
 * 注意事项：
 * - 使用前应检查 bIsValid 字段
 * - 不同类型的卡牌，部分字段可能为空或默认值
 */
USTRUCT(BlueprintType)
struct FSGCardDetailInfo
{
	GENERATED_BODY()

	// === 基础信息 ===
	
	/**
	 * @brief 卡牌实例 ID
	 * @details 每张卡牌的唯一标识符，用于区分同一种卡牌的不同实例
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Card|Basic")
	FGuid InstanceId;

	/**
	 * @brief 卡牌名称
	 * @details 显示在 UI 上的卡牌名称，例如："曹操"、"步兵"、"强攻计"
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Card|Basic")
	FText CardName;

	/**
	 * @brief 卡牌描述
	 * @details 卡牌的详细说明文本，描述卡牌效果
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Card|Basic")
	FText CardDescription;

	/**
	 * @brief 卡牌图标
	 * @details 显示在 UI 上的卡牌图标纹理
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Card|Basic")
	UTexture2D* CardIcon = nullptr;

	/**
	 * @brief 卡牌类型标签
	 * @details 用于识别卡牌类型：Hero（英雄）、Troop（兵团）、Strategy（计谋）
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Card|Basic")
	FGameplayTag CardTypeTag;

	/**
	 * @brief 卡牌稀有度标签
	 * @details 用于区分卡牌稀有度：Common（普通）、Rare（稀有）、Epic（史诗）
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Card|Basic")
	FGameplayTag CardRarityTag;

	/**
	 * @brief 是否唯一卡
	 * @details 唯一卡在一局游戏中只能使用一次（如英雄卡）
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Card|Basic")
	bool bIsUnique = false;

	// === 状态信息 ===
	
	/**
	 * @brief 是否选中
	 * @details 当前卡牌是否被玩家选中
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Card|State")
	bool bIsSelected = false;

	/**
	 * @brief 是否可用
	 * @details 当前卡牌是否可以使用（不在冷却中）
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Card|State")
	bool bIsPlayable = true;

	// === 放置信息 ===
	
	/**
	 * @brief 放置类型
	 * @details 决定卡牌如何放置：单点、区域、全局
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Card|Placement")
	ESGPlacementType PlacementType = ESGPlacementType::Single;

	/**
	 * @brief 放置区域大小
	 * @details 只对 Area 类型有效，定义放置区域的宽度和高度（单位：厘米）
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Card|Placement")
	FVector2D PlacementAreaSize = FVector2D::ZeroVector;

	/**
	 * @brief 是否受前线限制
	 * @details True：只能在前线内放置；False：可以在任何地方放置
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Card|Placement")
	bool bRespectFrontLine = true;

	// === 原始数据引用 ===
	
	/**
	 * @brief 卡牌数据资产
	 * @details 指向原始卡牌数据资产的指针，可用于访问更多信息
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Card|Reference")
	USG_CardDataBase* CardData = nullptr;

	/**
	 * @brief 卡牌实例
	 * @details 完整的卡牌实例数据
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Card|Reference")
	FSGCardInstance CardInstance;

	/**
	 * @brief 是否有效
	 * @details 该详细信息结构体是否包含有效数据，使用前应检查此字段
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Card|Basic")
	bool bIsValid = false;
};

/**
 * @brief 卡牌系统函数库
 * @details
 * 功能说明：
 * - 提供卡牌相关的静态辅助函数
 * - 支持卡牌信息获取、查询、类型判断等功能
 * - 所有函数都可以在蓝图中调用
 * 
 * 注意事项：
 * - 所有函数都是静态的，不需要实例化
 * - 函数参数中的指针应确保有效性
 */
UCLASS()
class SGUO_API USG_FunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ==================== 卡牌信息获取 ====================

	/**
	 * @brief 根据实例 ID 获取卡牌详细信息
	 * @param DeckComponent 卡组组件指针
	 * @param CardInstanceId 卡牌实例 ID
	 * @param OutDetailInfo 输出的详细信息结构体
	 * @return 是否成功获取信息
	 * 
	 * @details
	 * 功能说明：
	 * - 从卡组组件中查找指定 ID 的卡牌
	 * - 提取卡牌的所有详细信息
	 * - 填充到输出结构体中
	 * 
	 * 详细流程：
	 * 1. 验证输入参数有效性
	 * 2. 在手牌中查找指定 ID 的卡牌实例
	 * 3. 从卡牌数据中提取信息
	 * 4. 获取卡牌的选中和可用状态
	 * 5. 填充输出结构体
	 * 
	 * 注意事项：
	 * - 如果卡牌不存在，返回 false 且 OutDetailInfo.bIsValid 为 false
	 * - 使用前应检查返回值和 bIsValid 字段
	 */
	UFUNCTION(BlueprintCallable, Category = "Card|Info", meta = (WorldContext = "WorldContextObject"))
	static bool GetCardDetailInfo(USG_CardDeckComponent* DeckComponent, const FGuid& CardInstanceId, FSGCardDetailInfo& OutDetailInfo);

	/**
	 * @brief 从 ViewModel 获取卡牌详细信息
	 * @param CardViewModel 卡牌 ViewModel 指针
	 * @param OutDetailInfo 输出的详细信息结构体
	 * @return 是否成功获取信息
	 * 
	 * @details
	 * 功能说明：
	 * - 从卡牌 ViewModel 中提取详细信息
	 * - 适用于 UI 层直接获取卡牌信息
	 * 
	 * 详细流程：
	 * 1. 验证 ViewModel 有效性
	 * 2. 从 ViewModel 属性中提取基础信息
	 * 3. 从关联的 CardData 中提取详细信息
	 * 4. 填充输出结构体
	 * 
	 * 注意事项：
	 * - ViewModel 必须已经正确初始化
	 * - 如果 ViewModel 为空，返回 false
	 */
	UFUNCTION(BlueprintCallable, Category = "Card|Info")
	static bool GetCardDetailInfoFromViewModel(USGCardViewModel* CardViewModel, FSGCardDetailInfo& OutDetailInfo);

	/**
	 * @brief 获取选中的卡牌详细信息
	 * @param DeckComponent 卡组组件指针
	 * @param OutDetailInfo 输出的详细信息结构体
	 * @return 是否有选中的卡牌
	 * 
	 * @details
	 * 功能说明：
	 * - 获取当前选中的卡牌的详细信息
	 * - 如果没有选中任何卡牌，返回 false
	 * 
	 * 详细流程：
	 * 1. 从 DeckComponent 获取选中的卡牌 ID
	 * 2. 调用 GetCardDetailInfo 获取详细信息
	 * 
	 * 注意事项：
	 * - 如果没有选中任何卡牌，返回 false
	 * - 适用于需要获取当前选中卡牌信息的场景
	 */
	UFUNCTION(BlueprintCallable, Category = "Card|Info")
	static bool GetSelectedCardDetailInfo(USG_CardDeckComponent* DeckComponent, FSGCardDetailInfo& OutDetailInfo);

	/**
	 * @brief 从手牌 ViewModel 获取选中的卡牌详细信息
	 * @param HandViewModel 手牌 ViewModel 指针
	 * @param OutDetailInfo 输出的详细信息结构体
	 * @return 是否有选中的卡牌
	 * 
	 * @details
	 * 功能说明：
	 * - 从手牌 ViewModel 中查找选中的卡牌
	 * - 提取其详细信息
	 * 
	 * 详细流程：
	 * 1. 遍历 HandViewModel 中的所有 CardViewModel
	 * 2. 查找 bIsSelected 为 true 的卡牌
	 * 3. 调用 GetCardDetailInfoFromViewModel 获取详细信息
	 * 
	 * 注意事项：
	 * - 如果没有选中任何卡牌，返回 false
	 * - 适用于 MVVM 架构的 UI 层
	 */
	UFUNCTION(BlueprintCallable, Category = "Card|Info")
	static bool GetSelectedCardDetailInfoFromHandViewModel(USGCardHandViewModel* HandViewModel, FSGCardDetailInfo& OutDetailInfo);

	// ==================== 卡牌查询 ====================

	/**
	 * @brief 查找卡牌实例
	 * @param DeckComponent 卡组组件指针
	 * @param CardInstanceId 卡牌实例 ID
	 * @param OutCardInstance 输出的卡牌实例
	 * @return 是否找到卡牌
	 * 
	 * @details
	 * 功能说明：
	 * - 在手牌中查找指定 ID 的卡牌实例
	 * 
	 * 详细流程：
	 * 1. 获取当前手牌数组
	 * 2. 遍历查找匹配的 InstanceId
	 * 3. 找到后返回卡牌实例
	 * 
	 * 注意事项：
	 * - 只在手牌中查找，不包括抽牌堆和弃牌堆
	 * - 如果未找到，返回 false
	 */
	UFUNCTION(BlueprintCallable, Category = "Card|Query")
	static bool FindCardInstance(USG_CardDeckComponent* DeckComponent, const FGuid& CardInstanceId, FSGCardInstance& OutCardInstance);

	/**
	 * @brief 获取所有指定类型标签的卡牌
	 * @param DeckComponent 卡组组件指针
	 * @param CardTypeTag 卡牌类型标签（如 Card.Type.Hero）
	 * @return 卡牌实例数组
	 * 
	 * @details
	 * 功能说明：
	 * - 筛选手牌中所有指定类型的卡牌
	 * - 使用 GameplayTag 进行匹配
	 * 
	 * 详细流程：
	 * 1. 遍历当前手牌
	 * 2. 检查每张卡牌的类型标签
	 * 3. 将匹配的卡牌添加到结果数组
	 * 
	 * 注意事项：
	 * - 返回的是卡牌实例数组，可能为空
	 * - 只包括手牌中的卡牌
	 * - 支持层级匹配（如 Card.Type 可以匹配 Card.Type.Hero）
	 */
	UFUNCTION(BlueprintCallable, Category = "Card|Query")
	static TArray<FSGCardInstance> GetCardsByTypeTag(USG_CardDeckComponent* DeckComponent, FGameplayTag CardTypeTag);

	/**
	 * @brief 获取所有唯一卡牌
	 * @param DeckComponent 卡组组件指针
	 * @return 唯一卡牌实例数组
	 * 
	 * @details
	 * 功能说明：
	 * - 筛选手牌中所有唯一卡牌
	 * 
	 * 详细流程：
	 * 1. 遍历当前手牌
	 * 2. 检查每张卡牌的 bIsUnique 属性
	 * 3. 将唯一卡牌添加到结果数组
	 * 
	 * 注意事项：
	 * - 唯一卡牌在一局游戏中只能使用一次
	 * - 返回的数组可能为空
	 */
	UFUNCTION(BlueprintCallable, Category = "Card|Query")
	static TArray<FSGCardInstance> GetUniqueCards(USG_CardDeckComponent* DeckComponent);

	/**
	 * @brief 检查是否有指定类型标签的卡牌
	 * @param DeckComponent 卡组组件指针
	 * @param CardTypeTag 卡牌类型标签
	 * @return 是否存在该类型的卡牌
	 * 
	 * @details
	 * 功能说明：
	 * - 快速检查手牌中是否有指定类型的卡牌
	 * 
	 * 详细流程：
	 * 1. 遍历当前手牌
	 * 2. 检查是否有匹配类型标签的卡牌
	 * 3. 找到第一个匹配的即返回 true
	 * 
	 * 注意事项：
	 * - 比 GetCardsByTypeTag 更高效，只需要判断存在性
	 * - 不返回具体的卡牌实例
	 */
	UFUNCTION(BlueprintCallable, Category = "Card|Query")
	static bool HasCardOfTypeTag(USG_CardDeckComponent* DeckComponent, FGameplayTag CardTypeTag);

	/**
	 * @brief 获取指定放置类型的卡牌
	 * @param DeckComponent 卡组组件指针
	 * @param PlacementType 放置类型
	 * @return 卡牌实例数组
	 * 
	 * @details
	 * 功能说明：
	 * - 筛选手牌中所有指定放置类型的卡牌
	 * 
	 * 详细流程：
	 * 1. 遍历当前手牌
	 * 2. 检查每张卡牌的放置类型
	 * 3. 将匹配的卡牌添加到结果数组
	 * 
	 * 注意事项：
	 * - 返回的数组可能为空
	 * - 只包括手牌中的卡牌
	 */
	UFUNCTION(BlueprintCallable, Category = "Card|Query")
	static TArray<FSGCardInstance> GetCardsByPlacementType(USG_CardDeckComponent* DeckComponent, ESGPlacementType PlacementType);

	// ==================== 卡牌类型判断 ====================

	/**
	 * @brief 检查卡牌是否匹配指定类型标签
	 * @param CardData 卡牌数据指针
	 * @param TypeTag 类型标签
	 * @param bExactMatch 是否精确匹配（false 则支持层级匹配）
	 * @return 是否匹配
	 * 
	 * @details
	 * 功能说明：
	 * - 检查卡牌的类型标签是否匹配指定标签
	 * - 支持精确匹配和层级匹配
	 * 
	 * 注意事项：
	 * - 纯函数，可以在任何地方调用
	 * - 如果 CardData 为空，返回 false
	 * - bExactMatch = false 时，Card.Type 可以匹配 Card.Type.Hero
	 */
	UFUNCTION(BlueprintPure, Category = "Card|Type")
	static bool IsCardOfType(const USG_CardDataBase* CardData, FGameplayTag TypeTag, bool bExactMatch = false);

	/**
	 * @brief 判断是否为英雄卡
	 * @param CardData 卡牌数据指针
	 * @return 是否为英雄卡
	 * 
	 * @details
	 * 功能说明：
	 * - 快速判断卡牌是否为英雄类型
	 * - 检查 CardTypeTag 是否匹配 Card.Type.Hero
	 * 
	 * 注意事项：
	 * - 纯函数，可以在任何地方调用
	 * - 如果 CardData 为空，返回 false
	 */
	UFUNCTION(BlueprintPure, Category = "Card|Type")
	static bool IsHeroCard(const USG_CardDataBase* CardData);

	/**
	 * @brief 判断是否为兵团卡
	 * @param CardData 卡牌数据指针
	 * @return 是否为兵团卡
	 * 
	 * @details
	 * 功能说明：
	 * - 快速判断卡牌是否为兵团类型
	 * - 检查 CardTypeTag 是否匹配 Card.Type.Troop
	 * 
	 * 注意事项：
	 * - 纯函数，可以在任何地方调用
	 * - 如果 CardData 为空，返回 false
	 */
	UFUNCTION(BlueprintPure, Category = "Card|Type")
	static bool IsTroopCard(const USG_CardDataBase* CardData);

	/**
	 * @brief 判断是否为计谋卡
	 * @param CardData 卡牌数据指针
	 * @return 是否为计谋卡
	 * 
	 * @details
	 * 功能说明：
	 * - 快速判断卡牌是否为计谋类型
	 * - 检查 CardTypeTag 是否匹配 Card.Type.Strategy
	 * 
	 * 注意事项：
	 * - 纯函数，可以在任何地方调用
	 * - 如果 CardData 为空，返回 false
	 */
	UFUNCTION(BlueprintPure, Category = "Card|Type")
	static bool IsStrategyCard(const USG_CardDataBase* CardData);

	// ==================== 卡牌状态 ====================

	/**
	 * @brief 检查卡牌是否可用
	 * @param DeckComponent 卡组组件指针
	 * @param CardInstanceId 卡牌实例 ID
	 * @return 是否可用
	 * 
	 * @details
	 * 功能说明：
	 * - 检查卡牌是否可以使用
	 * - 考虑冷却状态和卡牌存在性
	 * 
	 * 详细流程：
	 * 1. 检查卡组是否在冷却中
	 * 2. 检查卡牌是否存在于手牌中
	 * 
	 * 注意事项：
	 * - 只有不在冷却中且卡牌存在时才返回 true
	 */
	UFUNCTION(BlueprintCallable, Category = "Card|State")
	static bool IsCardPlayable(USG_CardDeckComponent* DeckComponent, const FGuid& CardInstanceId);

	/**
	 * @brief 检查卡牌是否被选中
	 * @param DeckComponent 卡组组件指针
	 * @param CardInstanceId 卡牌实例 ID
	 * @return 是否选中
	 * 
	 * @details
	 * 功能说明：
	 * - 检查指定卡牌是否为当前选中的卡牌
	 * 
	 * 详细流程：
	 * 1. 获取当前选中的卡牌 ID
	 * 2. 比较是否与指定 ID 相同
	 * 
	 * 注意事项：
	 * - 同一时间只能有一张卡牌被选中
	 */
	UFUNCTION(BlueprintCallable, Category = "Card|State")
	static bool IsCardSelected(USG_CardDeckComponent* DeckComponent, const FGuid& CardInstanceId);

	// ==================== 格式化输出 ====================

	/**
	 * @brief 获取放置类型的文本描述
	 * @param PlacementType 放置类型枚举
	 * @return 文本描述
	 * 
	 * @details
	 * 功能说明：
	 * - 将放置类型枚举转换为可读的文本
	 * 
	 * 注意事项：
	 * - 纯函数，可以在任何地方调用
	 * - 返回中文描述
	 */
	UFUNCTION(BlueprintPure, Category = "Card|Format")
	static FText GetPlacementTypeText(ESGPlacementType PlacementType);

	/**
	 * @brief 格式化卡牌信息为字符串
	 * @param DetailInfo 卡牌详细信息结构体
	 * @return 格式化的字符串
	 * 
	 * @details
	 * 功能说明：
	 * - 将卡牌详细信息格式化为易读的字符串
	 * - 包含所有相关信息
	 * 
	 * 详细流程：
	 * 1. 检查信息有效性
	 * 2. 格式化基础信息
	 * 3. 根据放置类型添加特定信息
	 * 
	 * 注意事项：
	 * - 适用于调试和日志输出
	 * - 返回多行格式化的字符串
	 */
	UFUNCTION(BlueprintPure, Category = "Card|Format")
	static FString FormatCardInfo(const FSGCardDetailInfo& DetailInfo);

	// ==================== 辅助函数 ====================

	/**
	 * @brief 打印卡牌详细信息到日志和屏幕
	 * @param WorldContextObject 世界上下文对象
	 * @param DetailInfo 卡牌详细信息结构体
	 * @param bPrintToScreen 是否打印到屏幕
	 * 
	 * @details
	 * 功能说明：
	 * - 将卡牌信息打印到日志
	 * - 可选打印到屏幕上（用于调试）
	 * 
	 * 详细流程：
	 * 1. 格式化卡牌信息
	 * 2. 打印到 UE_LOG
	 * 3. 如果启用，打印到屏幕
	 * 
	 * 注意事项：
	 * - 主要用于调试
	 * - 屏幕打印会显示 5 秒
	 */
	UFUNCTION(BlueprintCallable, Category = "Card|Debug", meta = (WorldContext = "WorldContextObject"))
	static void PrintCardDetailInfo(UObject* WorldContextObject, const FSGCardDetailInfo& DetailInfo, bool bPrintToScreen = true);

	/**
	 * @brief 比较两张卡牌是否相同
	 * @param CardA 卡牌 A
	 * @param CardB 卡牌 B
	 * @return 是否相同
	 * 
	 * @details
	 * 功能说明：
	 * - 通过实例 ID 比较两张卡牌是否为同一张
	 * 
	 * 注意事项：
	 * - 比较的是实例 ID，不是卡牌类型
	 * - 同一种卡牌的不同实例会被认为是不同的
	 */
	UFUNCTION(BlueprintPure, Category = "Card|Utility")
	static bool AreCardsEqual(const FSGCardInstance& CardA, const FSGCardInstance& CardB);

private:
	/**
	 * @brief 从 CardData 填充详细信息
	 * @param OutInfo 输出的详细信息结构体
	 * @param CardData 卡牌数据指针
	 * 
	 * @details
	 * 功能说明：
	 * - 内部辅助函数，从卡牌数据中提取信息
	 * - 填充到详细信息结构体中
	 * 
	 * 详细流程：
	 * 1. 提取基础信息（名称、描述等）
	 * 2. 提取类型标签和稀有度标签
	 * 3. 提取放置配置信息
	 * 
	 * 注意事项：
	 * - 私有函数，仅供内部使用
	 * - 不检查 CardData 有效性，调用前应确保有效
	 */
	static void FillDetailInfoFromCardData(FSGCardDetailInfo& OutInfo, const USG_CardDataBase* CardData);
};