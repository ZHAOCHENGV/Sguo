
// Copyright notice placeholder
/**
 * @file SG_FunctionLibrary.cpp
 * @brief 卡牌系统函数库实现文件
 * @details
 * 功能说明：
 * - 实现卡牌信息获取、查询、类型判断等功能
 * - 提供格式化输出和调试功能
 * 
 * 注意事项：
 * - 所有函数都进行了空指针检查
 * - 返回值应该被调用者检查
 */
#include "Library/SG_FunctionLibrary.h"
#include "CardsAndUnits/SG_CardDeckComponent.h"
#include "Data/SG_CardDataBase.h"
#include "UIHud/SG_CardViewModel.h"
#include "UIHud/SG_CardHandViewModel.h"
#include "Engine/Engine.h"

// ==================== 卡牌信息获取 ====================

/**
 * @brief 根据实例 ID 获取卡牌详细信息
 * @param DeckComponent 卡组组件指针
 * @param CardInstanceId 卡牌实例 ID
 * @param OutDetailInfo 输出的详细信息结构体
 * @return 是否成功获取信息
 */
bool USG_FunctionLibrary::GetCardDetailInfo(USG_CardDeckComponent* DeckComponent, const FGuid& CardInstanceId, FSGCardDetailInfo& OutDetailInfo)
{
	// 检查卡组组件是否有效
	if (!DeckComponent || !CardInstanceId.IsValid())
	{
		// 标记输出信息为无效
		OutDetailInfo.bIsValid = false;
		// 返回失败
		return false;
	}

	// 声明卡牌实例变量
	FSGCardInstance CardInstance;
	// 查找卡牌实例
	if (!FindCardInstance(DeckComponent, CardInstanceId, CardInstance))
	{
		// 未找到卡牌，标记为无效
		OutDetailInfo.bIsValid = false;
		// 返回失败
		return false;
	}

	// 填充基础信息：实例 ID
	OutDetailInfo.InstanceId = CardInstance.InstanceId;
	// 填充基础信息：是否唯一
	OutDetailInfo.bIsUnique = CardInstance.bIsUnique;
	// 保存完整的卡牌实例
	OutDetailInfo.CardInstance = CardInstance;
	// 保存卡牌数据引用
	OutDetailInfo.CardData = CardInstance.CardData;

	// 从 CardData 填充详细信息
	if (CardInstance.CardData)
	{
		// 调用辅助函数填充详细信息
		FillDetailInfoFromCardData(OutDetailInfo, CardInstance.CardData);
	}

	// 填充状态信息：是否选中（通过比较当前选中的卡牌 ID）
	OutDetailInfo.bIsSelected = (DeckComponent->GetSelectedCardId() == CardInstanceId);
	// 填充状态信息：是否可用（检查是否在冷却中）
	OutDetailInfo.bIsPlayable = DeckComponent->CanAct();

	// 标记输出信息为有效
	OutDetailInfo.bIsValid = true;
	// 返回成功
	return true;
}

/**
 * @brief 从 ViewModel 获取卡牌详细信息
 * @param CardViewModel 卡牌 ViewModel 指针
 * @param OutDetailInfo 输出的详细信息结构体
 * @return 是否成功获取信息
 */
bool USG_FunctionLibrary::GetCardDetailInfoFromViewModel(USGCardViewModel* CardViewModel, FSGCardDetailInfo& OutDetailInfo)
{
	// 检查 ViewModel 是否有效
	if (!CardViewModel)
	{
		// 标记输出信息为无效
		OutDetailInfo.bIsValid = false;
		// 返回失败
		return false;
	}

	// 从 ViewModel 填充基础信息：实例 ID
	OutDetailInfo.InstanceId = CardViewModel->InstanceId;
	// 从 ViewModel 填充基础信息：卡牌名称
	OutDetailInfo.CardName = CardViewModel->CardName;
	// 从 ViewModel 填充基础信息：卡牌描述
	OutDetailInfo.CardDescription = CardViewModel->CardDescription;
	// 从 ViewModel 填充基础信息：卡牌图标
	OutDetailInfo.CardIcon = CardViewModel->CardIcon;
	// 从 ViewModel 填充基础信息：是否唯一
	OutDetailInfo.bIsUnique = CardViewModel->bIsUnique;
	// 从 ViewModel 填充状态信息：是否选中
	OutDetailInfo.bIsSelected = CardViewModel->bIsSelected;
	// 从 ViewModel 填充状态信息：是否可用
	OutDetailInfo.bIsPlayable = CardViewModel->bIsPlayable;

	// 从 CardData 填充详细信息
	if (CardViewModel->GetCardData())
	{
		// 保存 CardData 引用
		OutDetailInfo.CardData = CardViewModel->GetCardData();
		// 调用辅助函数填充详细信息
		FillDetailInfoFromCardData(OutDetailInfo, CardViewModel->GetCardData());
	}

	// 标记输出信息为有效
	OutDetailInfo.bIsValid = true;
	// 返回成功
	return true;
}

/**
 * @brief 获取选中的卡牌详细信息
 * @param DeckComponent 卡组组件指针
 * @param OutDetailInfo 输出的详细信息结构体
 * @return 是否有选中的卡牌
 */
bool USG_FunctionLibrary::GetSelectedCardDetailInfo(USG_CardDeckComponent* DeckComponent, FSGCardDetailInfo& OutDetailInfo)
{
	// 检查卡组组件是否有效
	if (!DeckComponent)
	{
		// 标记输出信息为无效
		OutDetailInfo.bIsValid = false;
		// 返回失败
		return false;
	}

	// 获取当前选中的卡牌 ID
	FGuid SelectedId = DeckComponent->GetSelectedCardId();
	// 检查选中的 ID 是否有效
	if (!SelectedId.IsValid())
	{
		// 没有选中任何卡牌，标记为无效
		OutDetailInfo.bIsValid = false;
		// 返回失败
		return false;
	}

	// 调用 GetCardDetailInfo 获取选中卡牌的详细信息
	return GetCardDetailInfo(DeckComponent, SelectedId, OutDetailInfo);
}

/**
 * @brief 从手牌 ViewModel 获取选中的卡牌详细信息
 * @param HandViewModel 手牌 ViewModel 指针
 * @param OutDetailInfo 输出的详细信息结构体
 * @return 是否有选中的卡牌
 */
bool USG_FunctionLibrary::GetSelectedCardDetailInfoFromHandViewModel(USGCardHandViewModel* HandViewModel, FSGCardDetailInfo& OutDetailInfo)
{
	// 检查手牌 ViewModel 是否有效
	if (!HandViewModel)
	{
		// 标记输出信息为无效
		OutDetailInfo.bIsValid = false;
		// 返回失败
		return false;
	}

	// 获取所有卡牌 ViewModel（现在返回的是 TArray<USGCardViewModel*>）
	TArray<USGCardViewModel*> CardViewModels = HandViewModel->GetCardViewModels();
	// 遍历所有卡牌 ViewModel
	for (USGCardViewModel* CardVM : CardViewModels)
	{
		// 检查 ViewModel 是否有效且是否被选中
		if (CardVM && CardVM->bIsSelected)
		{
			// 找到选中的卡牌，获取其详细信息
			return GetCardDetailInfoFromViewModel(CardVM, OutDetailInfo);
		}
	}

	// 没有找到选中的卡牌，标记为无效
	OutDetailInfo.bIsValid = false;
	// 返回失败
	return false;
}

// ==================== 卡牌查询 ====================

/**
 * @brief 查找卡牌实例
 * @param DeckComponent 卡组组件指针
 * @param CardInstanceId 卡牌实例 ID
 * @param OutCardInstance 输出的卡牌实例
 * @return 是否找到卡牌
 */
bool USG_FunctionLibrary::FindCardInstance(USG_CardDeckComponent* DeckComponent, const FGuid& CardInstanceId, FSGCardInstance& OutCardInstance)
{
	// 检查卡组组件和实例 ID 是否有效
	if (!DeckComponent || !CardInstanceId.IsValid())
	{
		// 返回失败
		return false;
	}

	// 获取当前手牌数组
	const TArray<FSGCardInstance>& Hand = DeckComponent->GetHand();
	// 使用 Lambda 表达式查找匹配的卡牌实例
	const FSGCardInstance* Found = Hand.FindByPredicate([&](const FSGCardInstance& Instance)
	{
		// 比较实例 ID 是否匹配
		return Instance.InstanceId == CardInstanceId;
	});

	// 检查是否找到
	if (Found)
	{
		// 将找到的实例复制到输出参数
		OutCardInstance = *Found;
		// 返回成功
		return true;
	}

	// 未找到，返回失败
	return false;
}

/**
 * @brief 获取所有指定类型标签的卡牌
 * @param DeckComponent 卡组组件指针
 * @param CardTypeTag 卡牌类型标签
 * @return 卡牌实例数组
 */
TArray<FSGCardInstance> USG_FunctionLibrary::GetCardsByTypeTag(USG_CardDeckComponent* DeckComponent, FGameplayTag CardTypeTag)
{
	// 声明结果数组
	TArray<FSGCardInstance> Result;

	// 检查卡组组件是否有效
	if (!DeckComponent)
	{
		// 返回空数组
		return Result;
	}

	// 检查类型标签是否有效
	if (!CardTypeTag.IsValid())
	{
		// 返回空数组
		return Result;
	}

	// 获取当前手牌数组
	const TArray<FSGCardInstance>& Hand = DeckComponent->GetHand();
	// 遍历手牌
	for (const FSGCardInstance& Card : Hand)
	{
		// 检查卡牌数据是否有效
		if (Card.CardData)
		{
			// 检查类型标签是否匹配（支持层级匹配）
			// 例如：Card.Type 可以匹配 Card.Type.Hero
			if (Card.CardData->CardTypeTag.MatchesTag(CardTypeTag))
			{
				// 添加到结果数组
				Result.Add(Card);
			}
		}
	}

	// 返回结果数组
	return Result;
}

/**
 * @brief 获取所有唯一卡牌
 * @param DeckComponent 卡组组件指针
 * @return 唯一卡牌实例数组
 */
TArray<FSGCardInstance> USG_FunctionLibrary::GetUniqueCards(USG_CardDeckComponent* DeckComponent)
{
	// 声明结果数组
	TArray<FSGCardInstance> Result;

	// 检查卡组组件是否有效
	if (!DeckComponent)
	{
		// 返回空数组
		return Result;
	}

	// 获取当前手牌数组
	const TArray<FSGCardInstance>& Hand = DeckComponent->GetHand();
	// 遍历手牌
	for (const FSGCardInstance& Card : Hand)
	{
		// 检查是否为唯一卡
		if (Card.bIsUnique)
		{
			// 添加到结果数组
			Result.Add(Card);
		}
	}

	// 返回结果数组
	return Result;
}

/**
 * @brief 检查是否有指定类型标签的卡牌
 * @param DeckComponent 卡组组件指针
 * @param CardTypeTag 卡牌类型标签
 * @return 是否存在该类型的卡牌
 */
bool USG_FunctionLibrary::HasCardOfTypeTag(USG_CardDeckComponent* DeckComponent, FGameplayTag CardTypeTag)
{
	// 检查卡组组件是否有效
	if (!DeckComponent)
	{
		// 返回 false
		return false;
	}

	// 检查类型标签是否有效
	if (!CardTypeTag.IsValid())
	{
		// 返回 false
		return false;
	}

	// 获取当前手牌数组
	const TArray<FSGCardInstance>& Hand = DeckComponent->GetHand();
	// 遍历手牌
	for (const FSGCardInstance& Card : Hand)
	{
		// 检查卡牌数据是否有效
		if (Card.CardData)
		{
			// 检查类型标签是否匹配
			if (Card.CardData->CardTypeTag.MatchesTag(CardTypeTag))
			{
				// 找到匹配的卡牌，返回 true
				return true;
			}
		}
	}

	// 未找到匹配的卡牌，返回 false
	return false;
}

/**
 * @brief 获取指定放置类型的卡牌
 * @param DeckComponent 卡组组件指针
 * @param PlacementType 放置类型
 * @return 卡牌实例数组
 */
TArray<FSGCardInstance> USG_FunctionLibrary::GetCardsByPlacementType(USG_CardDeckComponent* DeckComponent, ESGPlacementType PlacementType)
{
	// 声明结果数组
	TArray<FSGCardInstance> Result;

	// 检查卡组组件是否有效
	if (!DeckComponent)
	{
		// 返回空数组
		return Result;
	}

	// 获取当前手牌数组
	const TArray<FSGCardInstance>& Hand = DeckComponent->GetHand();
	// 遍历手牌
	for (const FSGCardInstance& Card : Hand)
	{
		// 检查卡牌数据是否有效，且放置类型是否匹配
		if (Card.CardData && Card.CardData->PlacementType == PlacementType)
		{
			// 添加到结果数组
			Result.Add(Card);
		}
	}

	// 返回结果数组
	return Result;
}

// ==================== 卡牌类型判断 ====================

/**
 * @brief 检查卡牌是否匹配指定类型标签
 * @param CardData 卡牌数据指针
 * @param TypeTag 类型标签
 * @param bExactMatch 是否精确匹配
 * @return 是否匹配
 */
bool USG_FunctionLibrary::IsCardOfType(const USG_CardDataBase* CardData, FGameplayTag TypeTag, bool bExactMatch)
{
	// 检查卡牌数据是否有效
	if (!CardData)
	{
		// 返回 false
		return false;
	}

	// 检查类型标签是否有效
	if (!TypeTag.IsValid())
	{
		// 返回 false
		return false;
	}

	// 根据匹配模式进行比较
	if (bExactMatch)
	{
		// 精确匹配：标签必须完全相同
		return CardData->CardTypeTag.MatchesTagExact(TypeTag);
	}
	else
	{
		// 层级匹配：支持父标签匹配子标签
		// 例如：Card.Type 可以匹配 Card.Type.Hero
		return CardData->CardTypeTag.MatchesTag(TypeTag);
	}
}

/**
 * @brief 判断是否为英雄卡
 * @param CardData 卡牌数据指针
 * @return 是否为英雄卡
 */
bool USG_FunctionLibrary::IsHeroCard(const USG_CardDataBase* CardData)
{
	// 检查卡牌数据是否有效
	if (!CardData)
	{
		// 返回 false
		return false;
	}

	// 创建英雄类型标签（Card.Type.Hero）
	// 使用 FName 构造，确保标签名称正确
	FGameplayTag HeroTag = FGameplayTag::RequestGameplayTag(FName("Card.Type.Hero"));
	
	// 检查卡牌类型标签是否匹配英雄标签
	return CardData->CardTypeTag.MatchesTag(HeroTag);
}

/**
 * @brief 判断是否为兵团卡
 * @param CardData 卡牌数据指针
 * @return 是否为兵团卡
 */
bool USG_FunctionLibrary::IsTroopCard(const USG_CardDataBase* CardData)
{
	// 检查卡牌数据是否有效
	if (!CardData)
	{
		// 返回 false
		return false;
	}

	// 创建兵团类型标签（Card.Type.Troop）
	FGameplayTag TroopTag = FGameplayTag::RequestGameplayTag(FName("Card.Type.Troop"));
	
	// 检查卡牌类型标签是否匹配兵团标签
	return CardData->CardTypeTag.MatchesTag(TroopTag);
}

/**
 * @brief 判断是否为计谋卡
 * @param CardData 卡牌数据指针
 * @return 是否为计谋卡
 */
bool USG_FunctionLibrary::IsStrategyCard(const USG_CardDataBase* CardData)
{
	// 检查卡牌数据是否有效
	if (!CardData)
	{
		// 返回 false
		return false;
	}

	// 创建计谋类型标签（Card.Type.Strategy）
	FGameplayTag StrategyTag = FGameplayTag::RequestGameplayTag(FName("Card.Type.Strategy"));
	
	// 检查卡牌类型标签是否匹配计谋标签
	return CardData->CardTypeTag.MatchesTag(StrategyTag);
}

// ==================== 卡牌状态 ====================

/**
 * @brief 检查卡牌是否可用
 * @param DeckComponent 卡组组件指针
 * @param CardInstanceId 卡牌实例 ID
 * @return 是否可用
 */
bool USG_FunctionLibrary::IsCardPlayable(USG_CardDeckComponent* DeckComponent, const FGuid& CardInstanceId)
{
	// 检查卡组组件和实例 ID 是否有效
	if (!DeckComponent || !CardInstanceId.IsValid())
	{
		// 返回 false
		return false;
	}

	// 检查是否在冷却中（不可行动）
	if (!DeckComponent->CanAct())
	{
		// 在冷却中，返回 false
		return false;
	}

	// 检查卡牌是否存在于手牌中
	FSGCardInstance CardInstance;
	// 返回查找结果（存在且不在冷却中才返回 true）
	return FindCardInstance(DeckComponent, CardInstanceId, CardInstance);
}

/**
 * @brief 检查卡牌是否被选中
 * @param DeckComponent 卡组组件指针
 * @param CardInstanceId 卡牌实例 ID
 * @return 是否选中
 */
bool USG_FunctionLibrary::IsCardSelected(USG_CardDeckComponent* DeckComponent, const FGuid& CardInstanceId)
{
	// 检查卡组组件和实例 ID 是否有效
	if (!DeckComponent || !CardInstanceId.IsValid())
	{
		// 返回 false
		return false;
	}

	// 比较当前选中的卡牌 ID 是否与指定 ID 相同
	return DeckComponent->GetSelectedCardId() == CardInstanceId;
}

// ==================== 格式化输出 ====================

/**
 * @brief 获取放置类型的文本描述
 * @param PlacementType 放置类型枚举
 * @return 文本描述
 */
FText USG_FunctionLibrary::GetPlacementTypeText(ESGPlacementType PlacementType)
{
	// 根据放置类型返回对应的中文描述
	switch (PlacementType)
	{
	case ESGPlacementType::Single:
		// 单点放置
		return FText::FromString(TEXT("单点放置"));
	case ESGPlacementType::Area:
		// 区域放置
		return FText::FromString(TEXT("区域放置"));
	case ESGPlacementType::Global:
		// 全局效果
		return FText::FromString(TEXT("全局效果"));
	default:
		// 未知类型
		return FText::FromString(TEXT("未知"));
	}
}

/**
 * @brief 格式化卡牌信息为字符串
 * @param DetailInfo 卡牌详细信息结构体
 * @return 格式化的字符串
 */
FString USG_FunctionLibrary::FormatCardInfo(const FSGCardDetailInfo& DetailInfo)
{
	// 检查信息是否有效
	if (!DetailInfo.bIsValid)
	{
		// 返回无效提示
		return TEXT("无效卡牌");
	}

	// 开始构建格式化字符串：标题
	FString Result = FString::Printf(TEXT("=== 卡牌信息 ===\n"));
	// 添加卡牌名称
	Result += FString::Printf(TEXT("名称: %s\n"), *DetailInfo.CardName.ToString());
	// 添加卡牌描述
	Result += FString::Printf(TEXT("描述: %s\n"), *DetailInfo.CardDescription.ToString());
	// 添加卡牌类型标签
	Result += FString::Printf(TEXT("类型: %s\n"), *DetailInfo.CardTypeTag.ToString());
	// 添加卡牌稀有度标签
	Result += FString::Printf(TEXT("稀有度: %s\n"), *DetailInfo.CardRarityTag.ToString());
	// 添加是否唯一
	Result += FString::Printf(TEXT("唯一: %s\n"), DetailInfo.bIsUnique ? TEXT("是") : TEXT("否"));
	// 添加是否选中
	Result += FString::Printf(TEXT("选中: %s\n"), DetailInfo.bIsSelected ? TEXT("是") : TEXT("否"));
	// 添加是否可用
	Result += FString::Printf(TEXT("可用: %s\n"), DetailInfo.bIsPlayable ? TEXT("是") : TEXT("否"));

	// 添加放置信息标题
	Result += FString::Printf(TEXT("\n--- 放置信息 ---\n"));
	// 添加放置类型
	Result += FString::Printf(TEXT("放置类型: %s\n"), *GetPlacementTypeText(DetailInfo.PlacementType).ToString());
	// 添加是否受前线限制
	Result += FString::Printf(TEXT("受前线限制: %s\n"), DetailInfo.bRespectFrontLine ? TEXT("是") : TEXT("否"));
	
	// 如果是区域放置，添加区域大小信息
	if (DetailInfo.PlacementType == ESGPlacementType::Area)
	{
		// 添加区域大小
		Result += FString::Printf(TEXT("区域大小: %.2f x %.2f\n"), 
			DetailInfo.PlacementAreaSize.X, 
			DetailInfo.PlacementAreaSize.Y);
	}

	// 返回完整的格式化字符串
	return Result;
}

// ==================== 辅助函数 ====================

/**
 * @brief 打印卡牌详细信息到日志和屏幕
 * @param WorldContextObject 世界上下文对象
 * @param DetailInfo 卡牌详细信息结构体
 * @param bPrintToScreen 是否打印到屏幕
 */
void USG_FunctionLibrary::PrintCardDetailInfo(UObject* WorldContextObject, const FSGCardDetailInfo& DetailInfo, bool bPrintToScreen)
{
	// 格式化卡牌信息为字符串
	FString InfoString = FormatCardInfo(DetailInfo);

	// 打印到日志（LogTemp 类别，Log 级别）
	UE_LOG(LogTemp, Log, TEXT("%s"), *InfoString);

	// 如果启用屏幕打印且引擎实例有效
	if (bPrintToScreen && GEngine)
	{
		// 添加到屏幕调试消息
		// 参数：-1（自动生成 Key），5.0f（显示 5 秒），青色，消息内容
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan, InfoString);
	}
}

/**
 * @brief 比较两张卡牌是否相同
 * @param CardA 卡牌 A
 * @param CardB 卡牌 B
 * @return 是否相同
 */
bool USG_FunctionLibrary::AreCardsEqual(const FSGCardInstance& CardA, const FSGCardInstance& CardB)
{
	// 通过比较实例 ID 判断是否为同一张卡牌
	return CardA.InstanceId == CardB.InstanceId;
}

// ==================== 私有辅助函数 ====================

/**
 * @brief 从 CardData 填充详细信息
 * @param OutInfo 输出的详细信息结构体
 * @param CardData 卡牌数据指针
 * 
 * @details
 * 功能说明：
 * - 内部辅助函数，从卡牌数据中提取信息
 * - 根据卡牌类型填充不同的信息
 * 
 * 注意事项：
 * - 调用前应确保 CardData 有效
 * - 不同类型的卡牌会填充不同的字段
 */
void USG_FunctionLibrary::FillDetailInfoFromCardData(FSGCardDetailInfo& OutInfo, const USG_CardDataBase* CardData)
{
	// 检查卡牌数据是否有效
	if (!CardData)
	{
		// 无效则直接返回
		return;
	}

	// 填充基础信息：卡牌名称
	OutInfo.CardName = CardData->CardName;
	// 填充基础信息：卡牌描述
	OutInfo.CardDescription = CardData->CardDescription;
	// 填充基础信息：卡牌图标
	OutInfo.CardIcon = CardData->CardIcon;
	// 填充基础信息：卡牌类型标签
	OutInfo.CardTypeTag = CardData->CardTypeTag;
	// 填充基础信息：卡牌稀有度标签
	OutInfo.CardRarityTag = CardData->CardRarityTag;

	// 填充放置信息：放置类型
	OutInfo.PlacementType = CardData->PlacementType;
	// 填充放置信息：放置区域大小
	OutInfo.PlacementAreaSize = CardData->PlacementAreaSize;
	// 填充放置信息：是否受前线限制
	OutInfo.bRespectFrontLine = CardData->bRespectFrontLine;
}