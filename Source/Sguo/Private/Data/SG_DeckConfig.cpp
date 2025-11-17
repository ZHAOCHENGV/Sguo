// Fill out your copyright notice in the Description page of Project Settings.


#include "Data/SG_DeckConfig.h"
// 引入 AssetManager 以访问 DeckAssetType 常量
#include "AssetManger/SG_AssetManager.h"
// ✨ NEW - 引入卡牌数据基类
#include "Data/SG_CardDataBase.h"
// ✨ NEW - 引入日志系统
#include "Debug/SG_LogCategories.h"
/**
 * @brief 获取卡组数据的主资产 ID
 * @return FPrimaryAssetId Deck 类型的主资产 ID
 */
FPrimaryAssetId USG_DeckConfig::GetPrimaryAssetId() const
{
	// 使用 AssetManager 中声明的卡组资产类型常量
	return FPrimaryAssetId(USG_AssetManager::DeckAssetType, GetFName());
}

// ✨ NEW - 获取所有卡牌数据（用于兼容旧代码）
TArray<USG_CardDataBase*> USG_DeckConfig::GetAllCardData() const
{
	// 创建结果数组
	TArray<USG_CardDataBase*> Result;
	Result.Reserve(AllowedCards.Num());
	
	// 遍历所有配置槽位
	for (const FSGCardConfigSlot& Slot : AllowedCards)
	{
		// 解析卡牌数据（支持软引用）
		USG_CardDataBase* CardData = Slot.CardData.IsValid() ? Slot.CardData.Get() : Slot.CardData.LoadSynchronous();
		
		// 添加到结果数组（可能为空）
		Result.Add(CardData);
	}
	
	return Result;
}

// ✨ NEW - 验证配置有效性
bool USG_DeckConfig::ValidateConfig(FString& OutErrorMessage) const
{
	// 检查是否有卡牌
	if (AllowedCards.Num() == 0)
	{
		OutErrorMessage = TEXT("错误：卡牌列表为空，至少需要添加一张卡牌！");
		return false;
	}
	
	// 统计有效卡牌数量
	int32 ValidCardCount = 0;
	// 统计保证初始手牌的卡牌数量
	int32 GuaranteedCardCount = 0;
	// 统计权重为 0 的卡牌数量
	int32 ZeroWeightCount = 0;
	
	// 遍历所有配置槽位
	for (int32 i = 0; i < AllowedCards.Num(); ++i)
	{
		const FSGCardConfigSlot& Slot = AllowedCards[i];
		
		// 检查卡牌引用是否有效
		if (Slot.CardData.IsNull())
		{
			OutErrorMessage = FString::Printf(TEXT("警告：第 %d 个槽位的卡牌引用为空，请删除或设置卡牌！"), i + 1);
			return false;
		}
		
		// 尝试加载卡牌数据
		USG_CardDataBase* CardData = Slot.CardData.IsValid() ? Slot.CardData.Get() : Slot.CardData.LoadSynchronous();
		if (!CardData)
		{
			OutErrorMessage = FString::Printf(TEXT("错误：第 %d 个槽位的卡牌数据加载失败！路径：%s"), 
				i + 1, *Slot.CardData.ToString());
			return false;
		}
		
		// 统计有效卡牌
		ValidCardCount++;
		
		// 检查权重配置
		if (Slot.DrawWeight < 0.0f)
		{
			OutErrorMessage = FString::Printf(TEXT("警告：卡牌 [%s] 的权重为负数（%.2f），将被视为 0！"), 
				*CardData->CardName.ToString(), Slot.DrawWeight);
			// 不返回 false，只是警告
		}
		
		if (Slot.DrawWeight == 0.0f)
		{
			ZeroWeightCount++;
		}
		
		// 检查保底配置
		if (Slot.PityMultiplier < 0.0f)
		{
			OutErrorMessage = FString::Printf(TEXT("警告：卡牌 [%s] 的保底系数为负数（%.2f），将被视为 0！"), 
				*CardData->CardName.ToString(), Slot.PityMultiplier);
		}
		
		if (Slot.PityMaxMultiplier < 1.0f)
		{
			OutErrorMessage = FString::Printf(TEXT("警告：卡牌 [%s] 的保底上限小于 1.0（%.2f），保底机制将不生效！"), 
				*CardData->CardName.ToString(), Slot.PityMaxMultiplier);
		}
		
		// 统计保证初始手牌的卡牌
		if (Slot.bGuaranteedInInitialHand)
		{
			GuaranteedCardCount++;
		}
	}
	
	// 检查是否所有卡牌权重都为 0
	if (ZeroWeightCount == ValidCardCount)
	{
		OutErrorMessage = TEXT("错误：所有卡牌的权重都为 0，无法抽卡！请至少设置一张卡牌的权重大于 0。");
		return false;
	}
	
	// 检查保证初始手牌的卡牌数量
	if (GuaranteedCardCount > InitialHand)
	{
		OutErrorMessage = FString::Printf(TEXT("警告：保证初始手牌的卡牌数量（%d）超过了初始手牌数（%d）！\n只有前 %d 张保证卡牌会生效。"), 
			GuaranteedCardCount, InitialHand, InitialHand);
		// 不返回 false，只是警告
	}
	
	// 配置有效
	OutErrorMessage = FString::Printf(TEXT("配置有效！共 %d 张卡牌，其中 %d 张保证初始手牌。"), 
		ValidCardCount, GuaranteedCardCount);
	return true;
}

// ✨ NEW - 获取实际使用的随机种子
int32 USG_DeckConfig::GetEffectiveRNGSeed() const
{
	// 如果使用固定种子，返回固定值
	if (bUseFixedSeed)
	{
		UE_LOG(LogSGCard, Log, TEXT("使用固定随机种子: %d"), FixedRNGSeed);
		return FixedRNGSeed;
	}
	
	// 否则生成随机种子
	int32 RandomSeed = GenerateRandomSeed();
	UE_LOG(LogSGCard, Log, TEXT("使用自动随机种子: %d"), RandomSeed);
	return RandomSeed;
}

// ✨ NEW - 生成随机种子（基于时间）
int32 USG_DeckConfig::GenerateRandomSeed()
{
	// 获取当前时间戳
	FDateTime Now = FDateTime::Now();
	
	// 方法 1：使用 Unix 时间戳（秒）
	int64 UnixTimestamp = Now.ToUnixTimestamp();
	
	// 方法 2：使用高精度时间戳（Ticks，100 纳秒为单位）
	int64 Ticks = Now.GetTicks();
	
	// 组合两者生成种子
	// 使用异或运算混合高位和低位，增加随机性
	int32 Seed = static_cast<int32>((UnixTimestamp * 1000) ^ (Ticks % 1000000));
	
	// 确保种子为正数（避免负数种子可能引起的问题）
	Seed = FMath::Abs(Seed);
	
	// 如果种子为 0，使用默认值 1（避免种子为 0）
	if (Seed == 0)
	{
		Seed = 1;
	}
	
	return Seed;
}





#if WITH_EDITOR
// ✨ NEW - 编辑器中属性修改后回调
void USG_DeckConfig::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// 调用父类实现
	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	// 获取修改的属性名称
	FName PropertyName = PropertyChangedEvent.GetPropertyName();
	// ✨ NEW - 当种子设置改变时，输出提示信息
	if (PropertyName == GET_MEMBER_NAME_CHECKED(USG_DeckConfig, bUseFixedSeed))
	{
		if (bUseFixedSeed)
		{
			UE_LOG(LogSGAsset, Log, TEXT("已启用固定随机种子模式，种子值: %d"), FixedRNGSeed);
			UE_LOG(LogSGAsset, Warning, TEXT("⚠️ 注意：固定种子会导致每次游戏的抽卡顺序完全相同！"));
		}
		else
		{
			UE_LOG(LogSGAsset, Log, TEXT("已启用自动随机种子模式，每次游戏抽卡顺序将不同"));
		}
	}
	if (PropertyName == GET_MEMBER_NAME_CHECKED(USG_DeckConfig, FixedRNGSeed))
	{
		if (bUseFixedSeed)
		{
			UE_LOG(LogSGAsset, Log, TEXT("固定随机种子已更新为: %d"), FixedRNGSeed);
		}
	}
	// 如果修改了卡牌列表或游戏规则，验证配置
	if (PropertyName == GET_MEMBER_NAME_CHECKED(USG_DeckConfig, AllowedCards) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(USG_DeckConfig, InitialHand))
	{
		// 验证配置
		FString ErrorMessage;
		if (ValidateConfig(ErrorMessage))
		{
			// 配置有效，输出日志
			UE_LOG(LogSGAsset, Log, TEXT("卡组配置验证通过：%s"), *ErrorMessage);
		}
		else
		{
			// 配置无效，输出警告
			UE_LOG(LogSGAsset, Warning, TEXT("卡组配置验证失败：%s"), *ErrorMessage);
		}
	}
}
#endif