// Copyright notice placeholder
/**
 * @file SG_CardDeckComponent.cpp
 * @brief 卡组运行时组件实现
 * @details
 * 功能说明：实现抽牌、弃牌、冷却与事件广播逻辑。
 */
#include "CardsAndUnits/SG_CardDeckComponent.h"
// 引入卡组配置
#include "Data/SG_DeckConfig.h"
// 引入卡牌数据基础类
#include "Data/SG_CardDataBase.h"
// 引入资产管理器
#include "AssetManger/SG_AssetManager.h"
// 引入世界与计时器
#include "Engine/World.h"
// 引入计时管理器
#include "TimerManager.h"
// ✨ NEW - 引入日志系统
#include "Debug/SG_LogCategories.h"

// 构造函数
USG_CardDeckComponent::USG_CardDeckComponent()
{
	// 启用 Tick 以便更新冷却时间
	PrimaryComponentTick.bCanEverTick = true;
}

// 生命周期开始
void USG_CardDeckComponent::BeginPlay()
{
	// 调用父类 BeginPlay
	Super::BeginPlay();
	// 若设置自动初始化且所属 Owner 不是 PlayerController 则执行
	// PlayerController 会手动控制初始化时机
	if (bAutoInitialize && !Cast<APlayerController>(GetOwner()))
	{
		// 初始化卡组
		InitializeDeck();
	}
}

/**
 * @brief 每帧更新
 * @param DeltaTime 帧间隔时间
 * @param TickType Tick 类型
 * @param ThisTickFunction Tick 函数指针
 * @details
 * 功能说明：
 * - 更新冷却剩余时间
 * - 🔧 MODIFIED - 检测冷却卡死问题
 * 详细流程：
 * 1. 检查是否在冷却中
 * 2. 更新冷却剩余时间
 * 3. 🔧 MODIFIED - 如果剩余时间接近 0，强制完成冷却
 * 4. 广播行动状态
 * 注意事项：
 * - 防止计时器精度问题导致冷却卡死
 */
void USG_CardDeckComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	// 调用父类 Tick
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    
	// 若冷却进行中则更新剩余时间
	if (!bActionAvailable)
	{
		// 读取剩余时间
		CooldownRemaining = GetWorld()->GetTimerManager().GetTimerRemaining(CooldownTimerHandle);
        
		// 🔧 MODIFIED - 检测冷却卡死问题
		// 如果剩余时间小于 0.01 秒，强制完成冷却
		if (CooldownRemaining > 0.0f && CooldownRemaining < 0.01f)
		{
			// 输出警告日志
			UE_LOG(LogSGCard, Warning, TEXT("⚠️ 检测到冷却卡死（剩余 %.4f 秒），强制完成冷却"), CooldownRemaining);
            
			// 清除计时器
			GetWorld()->GetTimerManager().ClearTimer(CooldownTimerHandle);
            
			// 强制完成冷却
			CompleteCooldown();
            
			// 直接返回，避免后续逻辑
			return;
		}
        
		// 🔧 MODIFIED - 如果计时器无效但还在冷却状态，强制完成
		if (!GetWorld()->GetTimerManager().IsTimerActive(CooldownTimerHandle) && CooldownRemaining <= 0.0f)
		{
			// 输出警告日志
			UE_LOG(LogSGCard, Warning, TEXT("⚠️ 检测到计时器失效，强制完成冷却"));
            
			// 强制完成冷却
			CompleteCooldown();
            
			// 直接返回
			return;
		}
        
		// 广播当前冷却状态
		BroadcastActionState();
	}
}

// 初始化卡组
void USG_CardDeckComponent::InitializeDeck()
{
	// 记录初始化开始
	UE_LOG(LogSGCard, Log, TEXT("InitializeDeck 开始 - Owner: %s"), *GetNameSafe(GetOwner()));
	
	// 避免重复初始化或重复加载
	if (bInitialized)
	{
		UE_LOG(LogSGCard, Warning, TEXT("卡组已初始化，跳过"));
		return;
	}
	if (bAssetsLoading)
	{
		UE_LOG(LogSGCard, Warning, TEXT("卡组正在加载中，跳过"));
		return;
	}
	
	// 检查 DeckConfigAsset 是否配置
	if (DeckConfigAsset.IsNull())
	{
		UE_LOG(LogSGCard, Error, TEXT("❌ 卡组配置未设置！请在蓝图 BP_SGPlayerController 的 CardDeckComponent 中设置 DeckConfigAsset"));
		UE_LOG(LogSGCard, Error, TEXT("   路径：选中 CardDeckComponent -> Details -> Card Deck -> Deck Config Asset"));
		return;
	}
	
	// 解析卡组配置
	ResolvedDeckConfig = DeckConfigAsset.IsValid() ? DeckConfigAsset.Get() : DeckConfigAsset.LoadSynchronous();
	
	// 若配置为空则退出
	if (!ResolvedDeckConfig)
	{
		UE_LOG(LogSGCard, Error, TEXT("❌ 卡组配置加载失败！DeckConfigAsset: %s"), *DeckConfigAsset.ToString());
		return;
	}
	
	UE_LOG(LogSGCard, Log, TEXT("✓ 卡组配置已加载: %s"), *ResolvedDeckConfig->GetName());
	
	// 🔧 MODIFIED - 使用 GetEffectiveRNGSeed() 获取种子
	// 根据配置自动选择固定种子或随机种子
	int32 EffectiveSeed = ResolvedDeckConfig->GetEffectiveRNGSeed();
	
	// ✨ NEW - 记录种子信息到日志（重要：用于问题复现）
	if (ResolvedDeckConfig->bUseFixedSeed)
	{
		UE_LOG(LogSGCard, Warning, TEXT("⚠️ 使用固定随机种子: %d（抽卡顺序将可重现）"), EffectiveSeed);
	}
	else
	{
		UE_LOG(LogSGCard, Log, TEXT("使用自动随机种子: %d（抽卡顺序将随机）"), EffectiveSeed);
		UE_LOG(LogSGCard, Log, TEXT("💡 提示：如需复现此次抽卡顺序，请在配置中启用固定种子并设置为: %d"), EffectiveSeed);
	}
	
	// 初始化随机数生成器
	RandomStream.Initialize(EffectiveSeed);
	
	// 收集需要加载的卡牌资产
	TArray<FPrimaryAssetId> CardIds = GatherCardAssetIds();
	UE_LOG(LogSGCard, Log, TEXT("收集到 %d 张卡牌需要加载"), CardIds.Num());
	
	if (USG_AssetManager* AssetManager = USG_AssetManager::Get())
	{
		if (CardIds.Num() > 0)
		{
			UE_LOG(LogSGAsset, Log, TEXT("开始异步批量加载卡牌..."));
			bAssetsLoading = true;
			CurrentLoadHandle = AssetManager->LoadCardDataBatch(CardIds, FStreamableDelegate::CreateUObject(this, &USG_CardDeckComponent::HandleCardAssetsLoaded));
			if (!CurrentLoadHandle.IsValid())
			{
				UE_LOG(LogSGAsset, Warning, TEXT("异步加载句柄无效，立即执行回调"));
				bAssetsLoading = false;
				HandleCardAssetsLoaded();
			}
		}
		else
		{
			UE_LOG(LogSGCard, Log, TEXT("无卡牌需要加载，直接完成初始化"));
			HandleCardAssetsLoaded();
		}
	}
	else
	{
		UE_LOG(LogSGAsset, Error, TEXT("❌ AssetManager 未找到！"));
		HandleCardAssetsLoaded();
	}
}

// 获取手牌
const TArray<FSGCardInstance>& USG_CardDeckComponent::GetHand() const
{
	// 返回当前手牌引用
	return HandCards;
}

// 选择卡牌
void USG_CardDeckComponent::SelectCard(const FGuid& InstanceId)
{
	// 更新当前选中 ID
	SelectedCardId = InstanceId;
	// 广播选中变化
	OnSelectionChanged.Broadcast(SelectedCardId);
}

// 获取选中卡牌
FGuid USG_CardDeckComponent::GetSelectedCardId() const
{
	// 返回选中卡牌 ID
	return SelectedCardId;
}


/**
 * @brief 使用卡牌
 * @param InstanceId 卡牌实例 ID
 * @return 是否成功使用
 * @details
 * 功能说明：
 * - 使用指定卡牌并触发冷却
 * - 🔧 MODIFIED - 确保冷却后自动抽卡
 * 详细流程：
 * 1. 检查是否在冷却中
 * 2. 查找目标卡牌
 * 3. 从手牌移除
 * 4. 处理弃牌/消耗
 * 5. 广播事件
 * 6. 🔧 MODIFIED - 启动冷却（冷却结束后自动抽卡）
 * 注意事项：
 * - 冷却中无法使用卡牌
 * - 唯一卡牌使用后不会再次出现
 */
bool USG_CardDeckComponent::UseCard(const FGuid& InstanceId)
{
    // 输出日志
    UE_LOG(LogSGCard, Log, TEXT("========== 尝试使用卡牌 =========="));
    
    // 冷却中无法使用
    if (!bActionAvailable)
    {
        // 输出警告
        UE_LOG(LogSGCard, Warning, TEXT("UseCard 失败：处于冷却中（剩余 %.2f 秒）"), CooldownRemaining);
        // 返回失败
        return false;
    }
    
    // 在手牌中查找目标卡牌
    int32 FoundIndex = HandCards.IndexOfByPredicate([&InstanceId](const FSGCardInstance& Card)
    {
        return Card.InstanceId == InstanceId;
    });
    
    // 未找到卡牌
    if (FoundIndex == INDEX_NONE)
    {
        // 输出错误
        UE_LOG(LogSGCard, Error, TEXT("UseCard 失败：未找到卡牌 ID: %s"), *InstanceId.ToString());
        // 返回失败
        return false;
    }
    
    // 缓存使用的卡牌
    FSGCardInstance UsedCard = HandCards[FoundIndex];
    
    // 检查卡牌数据有效性
    if (!UsedCard.CardData)
    {
        // 输出错误
        UE_LOG(LogSGCard, Error, TEXT("UseCard 失败：卡牌数据为空"));
        // 返回失败
        return false;
    }
    
    // 输出使用的卡牌信息
    UE_LOG(LogSGCard, Log, TEXT("使用卡牌：%s（实例 ID: %s）"), 
        *UsedCard.CardData->CardName.ToString(), 
        *UsedCard.InstanceId.ToString());
    
    // 从手牌中移除
    HandCards.RemoveAt(FoundIndex);
    UE_LOG(LogSGCard, Log, TEXT("  ✓ 已从手牌移除，当前手牌数：%d"), HandCards.Num());
    
    // 非唯一卡加入弃牌堆
    if (!UsedCard.bIsUnique)
    {
        // 构建弃牌槽位
        FSGCardDrawSlot Slot;
        // 记录卡牌 ID
        Slot.CardId = UsedCard.CardId;
        // 推入弃牌堆
        DiscardPile.Add(Slot);
        
        // 输出日志
        UE_LOG(LogSGCard, Log, TEXT("  ✓ 非唯一卡牌已加入弃牌堆"));
    }
    else
    {
        // 唯一卡记录为已使用
        ConsumedUniqueCards.Add(UsedCard.CardId);
        
        // 输出日志
        UE_LOG(LogSGCard, Log, TEXT("  ✓ 唯一卡牌已加入消耗列表，不会再次出现"));
    }
    
    // 清空选中 ID
    SelectedCardId.Invalidate();
    
    // 广播手牌变化
    OnHandChanged.Broadcast(HandCards);
    // 广播选中变化
    OnSelectionChanged.Broadcast(SelectedCardId);
    // 广播卡牌使用
    OnCardUsed.Broadcast(UsedCard);
    
    // 输出日志
    UE_LOG(LogSGCard, Log, TEXT("✓ 卡牌使用成功"));
    UE_LOG(LogSGCard, Log, TEXT("========================================"));
    
    // 🔧 MODIFIED - 启动冷却（冷却结束后会自动抽卡）
    UE_LOG(LogSGCard, Log, TEXT("启动冷却计时器..."));
    StartCooldown();
    
    // 返回成功
    return true;
}
/**
 * @brief 跳过行动
 * @return 是否成功跳过
 * @details
 * 功能说明：
 * - 放弃本次行动并进入冷却
 * - 🔧 MODIFIED - 确保冷却后自动抽卡
 * 详细流程：
 * 1. 检查是否在冷却中
 * 2. 取消选中的卡牌
 * 3. 🔧 MODIFIED - 启动冷却（冷却结束后自动抽卡）
 * 注意事项：
 * - 冷却中无法跳过
 * - 跳过后会抽取新卡
 */
bool USG_CardDeckComponent::SkipAction()
{
	// 输出日志
	UE_LOG(LogSGCard, Log, TEXT("========== 尝试跳过行动 =========="));
    
	// 冷却中无法跳过
	if (!bActionAvailable)
	{
		// 输出警告
		UE_LOG(LogSGCard, Warning, TEXT("SkipAction 失败：处于冷却中（剩余 %.2f 秒）"), CooldownRemaining);
		// 返回失败
		return false;
	}
    
	// 输出日志
	UE_LOG(LogSGCard, Log, TEXT("玩家选择跳过行动"));

	// 检查是否有选中的卡牌
	if (SelectedCardId.IsValid())
	{
		// 输出日志
		UE_LOG(LogSGCard, Log, TEXT("  取消选中的卡牌（ID: %s）"), *SelectedCardId.ToString());
        
		// 清空选中 ID
		SelectedCardId.Invalidate();
        
		// 广播选中变化（通知 UI 取消高亮）
		OnSelectionChanged.Broadcast(SelectedCardId);
	}

	// 输出日志
	UE_LOG(LogSGCard, Log, TEXT("✓ 跳过行动成功"));
	UE_LOG(LogSGCard, Log, TEXT("========================================"));
    
	// 🔧 MODIFIED - 启动冷却（冷却结束后会自动抽卡）
	UE_LOG(LogSGCard, Log, TEXT("启动冷却计时器..."));
	StartCooldown();
    
	// 返回成功
	return true;
}

// 行动是否可用
bool USG_CardDeckComponent::CanAct() const
{
	// 返回行动可用状态
	return bActionAvailable;
}

// 获取冷却剩余时间
float USG_CardDeckComponent::GetCooldownRemaining() const
{
	// 返回冷却剩余
	return CooldownRemaining;
}

// 获取卡组配置
USG_DeckConfig* USG_CardDeckComponent::GetDeckConfig() const
{
	// 返回卡组配置
	return ResolvedDeckConfig;
}

// 构建抽牌池
void USG_CardDeckComponent::BuildDrawPile()
{
	// 记录开始构建
	UE_LOG(LogSGCard, Log, TEXT("开始构建抽牌池..."));
	
	// 清空抽牌堆
	DrawPile.Reset();
	// 清空已使用的唯一卡集合
	ConsumedUniqueCards.Reset();
	
	// 检测配置有效性
	if (!ResolvedDeckConfig)
	{
		UE_LOG(LogSGCard, Error, TEXT("BuildDrawPile 失败：卡组配置为空"));
		return;
	}
	
	// 🔧 MODIFIED - 遍历配置槽位（而不是简单的卡牌数组）
	for (const FSGCardConfigSlot& ConfigSlot : ResolvedDeckConfig->AllowedCards)
	{
		// 解析卡牌数据（处理软引用）
		USG_CardDataBase* CardAsset = ConfigSlot.CardData.IsValid() ? 
			ConfigSlot.CardData.Get() : ConfigSlot.CardData.LoadSynchronous();
		
		// 跳过无效引用
		if (!CardAsset)
		{
			UE_LOG(LogSGCard, Warning, TEXT("  ⚠️ 配置槽位的卡牌数据无效，跳过"));
			continue;
		}
		
		// 构建抽牌槽位
		FSGCardDrawSlot Slot;
		
		// 记录资产 ID
		Slot.CardId = CardAsset->GetPrimaryAssetId();
		
		// 从配置中复制权重和保底参数
		Slot.DrawWeight = FMath::Max(0.0f, ConfigSlot.DrawWeight); // 确保权重非负
		Slot.PityMultiplier = FMath::Max(0.0f, ConfigSlot.PityMultiplier);
		Slot.PityMaxMultiplier = FMath::Max(1.0f, ConfigSlot.PityMaxMultiplier);
		Slot.MaxOccurrences = FMath::Max(0, ConfigSlot.MaxOccurrences);
		
		// 初始化运行时数据
		Slot.MissCount = 0;
		Slot.OccurrenceCount = 0;
		
		// 加入抽牌堆
		DrawPile.Add(Slot);
		
		// 记录槽位信息
		UE_LOG(LogSGCard, Verbose, TEXT("  ✓ 添加槽位 - 卡牌: %s, 权重: %.2f, 保底系数: %.2f, 保底上限: %.2f, 最大出现: %d"), 
			*CardAsset->CardName.ToString(),
			Slot.DrawWeight,
			Slot.PityMultiplier,
			Slot.PityMaxMultiplier,
			Slot.MaxOccurrences);
	}
	
	// 进行洗牌（Fisher-Yates 洗牌算法）
	for (int32 i = DrawPile.Num() - 1; i > 0; --i)
	{
		// 生成交换索引
		int32 SwapIndex = RandomStream.RandRange(0, i);
		// 交换元素
		DrawPile.Swap(i, SwapIndex);
	}
	
	// 记录构建完成
	UE_LOG(LogSGCard, Log, TEXT("✓ 抽牌池构建完成，共 %d 个槽位"), DrawPile.Num());
}

/**
 * @brief 抽取多张卡牌
 * @param Count 要抽取的卡牌数量
 * @details
 * 功能说明：
 * - 连续抽取指定数量的卡牌
 * - 🔧 MODIFIED - 增强日志，便于调试
 * 详细流程：
 * 1. 输出开始日志
 * 2. 循环抽取卡牌
 * 3. 如果抽取失败，输出详细原因
 * 4. 广播手牌变化
 * 注意事项：
 * - 如果抽牌池为空，会自动重新填充
 * - 如果无法抽取，会提前终止
 */
void USG_CardDeckComponent::DrawCards(int32 Count)
{
    // 输出日志
    UE_LOG(LogSGCard, Log, TEXT("========== 开始抽取 %d 张卡牌 =========="), Count);
    
    // 🔧 MODIFIED - 输出当前状态
    UE_LOG(LogSGCard, Log, TEXT("  当前手牌数：%d"), HandCards.Num());
    UE_LOG(LogSGCard, Log, TEXT("  抽牌池：%d 张"), DrawPile.Num());
    UE_LOG(LogSGCard, Log, TEXT("  弃牌池：%d 张"), DiscardPile.Num());
    
    // 记录成功抽取的数量
    int32 DrawnCount  = 0;
    
	for (int32 i = 0; i < Count; ++i)
	{
		FSGCardInstance NewCard;
		if (DrawSingleCard(NewCard))
		{
			HandCards.Add(NewCard);
			DrawnCount++;
            
			UE_LOG(LogSGCard, Log, TEXT("  [%d] %s"), 
				i + 1, 
				NewCard.CardData ? *NewCard.CardData->CardName.ToString() : TEXT("未知"));
		}
		else
		{
			UE_LOG(LogSGCard, Warning, TEXT("  [%d] 抽卡失败"), i + 1);
		}
	}
    
	UE_LOG(LogSGCard, Log, TEXT("成功抽取 %d/%d 张卡牌，当前手牌数：%d"), 
		DrawnCount, Count, HandCards.Num());
}

/**
 * @brief 抽取单张卡牌（权重随机系统）
 * @param OutInstance 输出参数，抽到的卡牌实例
 * @return 是否成功抽取
 */
bool USG_CardDeckComponent::DrawSingleCard(FSGCardInstance& OutInstance)
{
	// 收集所有可抽取的槽位
	TArray<FSGCardDrawSlot*> ValidSlots;
	float TotalWeight = 0.0f;
	
	// 遍历抽牌池，收集有效槽位并计算总权重
	for (FSGCardDrawSlot& Slot : DrawPile)
	{
		// 检查是否为已消耗的唯一卡牌
		if (ConsumedUniqueCards.Contains(Slot.CardId))
		{
			continue;
		}
		
		// 检查槽位是否可以抽取（权重、出现次数等）
		if (!Slot.CanDraw())
		{
			continue;
		}
		
		// 获取实际权重 (DrawWeight * 保底系数)
		float EffectiveWeight = Slot.GetEffectiveWeight();
		
		// 累加总权重
		TotalWeight += EffectiveWeight;
		
		// 添加到有效槽位列表
		ValidSlots.Add(&Slot);
	}
	
	// 如果没有有效槽位，尝试重新填充抽牌池
	if (ValidSlots.Num() == 0)
	{
		UE_LOG(LogSGCard, Warning, TEXT("抽牌池为空，尝试重新填充..."));
		
		// 重新填充抽牌池
		RefillDrawPile();
		
		// 再次收集有效槽位
		ValidSlots.Reset();
		TotalWeight = 0.0f;
		
		for (FSGCardDrawSlot& Slot : DrawPile)
		{
			if (ConsumedUniqueCards.Contains(Slot.CardId))
			{
				continue;
			}
			
			if (!Slot.CanDraw())
			{
				continue;
			}
			
			float EffectiveWeight = Slot.GetEffectiveWeight();
			TotalWeight += EffectiveWeight;
			ValidSlots.Add(&Slot);
		}
		
		// 如果仍然没有有效槽位，返回失败
		if (ValidSlots.Num() == 0)
		{
			UE_LOG(LogSGCard, Error, TEXT("❌ 抽牌失败：抽牌池为空且无法重新填充"));
			return false;
		}
	}
	
	// 使用轮盘赌算法随机选择一个槽位
	float RandomValue = RandomStream.FRandRange(0.0f, TotalWeight);
	float CurrentWeight = 0.0f;
	
	FSGCardDrawSlot* SelectedSlot = nullptr;
	for (FSGCardDrawSlot* Slot : ValidSlots)
	{
		float EffectiveWeight = Slot->GetEffectiveWeight();
		CurrentWeight += EffectiveWeight;
		
		if (RandomValue <= CurrentWeight)
		{
			SelectedSlot = Slot;
			break;
		}
	}
	
	// 如果未选中任何槽位，选择最后一个（兜底防止浮点误差）
	if (!SelectedSlot && ValidSlots.Num() > 0)
	{
		SelectedSlot = ValidSlots.Last();
		UE_LOG(LogSGCard, Warning, TEXT("⚠️ 轮盘赌算法未选中槽位，使用最后一个槽位"));
	}
	
	if (!SelectedSlot)
	{
		UE_LOG(LogSGCard, Error, TEXT("❌ 抽牌失败：未能选中任何槽位"));
		return false;
	}
	
	// 更新所有槽位的 MissCount 和 OccurrenceCount
	for (FSGCardDrawSlot* Slot : ValidSlots)
	{
		if (Slot == SelectedSlot)
		{
			// 抽到的槽位重置 MissCount
			Slot->MissCount = 0;
			// 增加出现次数
			Slot->OccurrenceCount++;
		}
		else
		{
			// 未抽到的槽位增加 MissCount (增加下次抽中的概率)
			Slot->MissCount++;
		}
	}
	
	// 解析卡牌数据
	USG_CardDataBase* CardData = ResolveCardData(SelectedSlot->CardId);
	if (!CardData)
	{
		UE_LOG(LogSGCard, Error, TEXT("❌ 抽牌失败：卡牌数据解析失败，CardId: %s"), *SelectedSlot->CardId.ToString());
		return false;
	}
	
	// 生成实例数据
	OutInstance.InstanceId = FGuid::NewGuid();
	OutInstance.CardData = CardData;
	OutInstance.CardId = SelectedSlot->CardId;
	OutInstance.bIsUnique = CardData->bIsUnique;
	
	// 记录详细的抽卡日志 (包含概率)
	UE_LOG(LogSGCard, Log, TEXT("    🎲 抽中: %s (权重: %.1f/%.1f, 概率: %.1f%%, Miss: %d, Count: %d)"), 
		*CardData->CardName.ToString(), 
		SelectedSlot->GetEffectiveWeight(),
		TotalWeight,
		(TotalWeight > 0.0f) ? (SelectedSlot->GetEffectiveWeight() / TotalWeight * 100.0f) : 0.0f,
		SelectedSlot->MissCount,
		SelectedSlot->OccurrenceCount);
	
	// 如果是唯一卡牌，加入消耗列表
	if (OutInstance.bIsUnique)
	{
		ConsumedUniqueCards.Add(SelectedSlot->CardId);
		UE_LOG(LogSGCard, Log, TEXT("    唯一卡牌 [%s] 已加入消耗列表"), *CardData->CardName.ToString());
	}
	
	return true;
}

// 重新填充抽牌堆
void USG_CardDeckComponent::RefillDrawPile()
{
	// 🔧 MODIFIED - 使用新的日志类别
	UE_LOG(LogSGCard, Log, TEXT("开始重新填充抽牌池..."));
	
	// 将弃牌堆的所有槽位加入抽牌池
	for (const FSGCardDrawSlot& Slot : DiscardPile)
	{
		DrawPile.Add(Slot);
	}
	
	// 清空弃牌堆
	DiscardPile.Reset();
	
	// 🔧 MODIFIED - 使用新的日志类别
	UE_LOG(LogSGCard, Log, TEXT("  从弃牌堆恢复 %d 个槽位"), DrawPile.Num());
	
	// ✨ NEW - 移除已消耗的唯一卡牌槽位
	int32 RemovedCount = DrawPile.RemoveAll([this](const FSGCardDrawSlot& Slot)
	{
		return ConsumedUniqueCards.Contains(Slot.CardId);
	});
	
	// 🔧 MODIFIED - 使用新的日志类别
	if (RemovedCount > 0)
	{
		UE_LOG(LogSGCard, Log, TEXT("  移除 %d 个已消耗的唯一卡牌槽位"), RemovedCount);
	}
	
	// 重新洗牌（Fisher-Yates 洗牌算法）
	for (int32 i = DrawPile.Num() - 1; i > 0; --i)
	{
		// 生成随机索引
		int32 SwapIndex = RandomStream.RandRange(0, i);
		// 交换位置
		DrawPile.Swap(i, SwapIndex);
	}
	
	// 🔧 MODIFIED - 使用新的日志类别
	UE_LOG(LogSGCard, Log, TEXT("✓ 抽牌池重新填充完成，当前槽位数：%d"), DrawPile.Num());
}

/**
 * @brief 开始冷却
 * @details
 * 功能说明：
 * - 启动冷却计时器
 * - 🔧 MODIFIED - 确保冷却结束后正确抽卡
 * 详细流程：
 * 1. 标记为不可行动
 * 2. 读取冷却时长
 * 3. 🔧 MODIFIED - 如果冷却为 0，也要抽卡
 * 4. 启动计时器或立即完成
 * 5. 广播状态变化
 * 注意事项：
 * - 即使冷却为 0，也要执行抽卡逻辑
 */
void USG_CardDeckComponent::StartCooldown()
{
// 标记不可行动
    bActionAvailable = false;
    
    // 读取冷却时长
    CooldownRemaining = ResolvedDeckConfig ? ResolvedDeckConfig->DrawCDSeconds : 0.0f;
    
    // 输出日志
    UE_LOG(LogSGCard, Log, TEXT("========== 开始冷却 =========="));
    UE_LOG(LogSGCard, Log, TEXT("  冷却时长：%.2f 秒"), CooldownRemaining);
    
    // 🔧 MODIFIED - 如果冷却时长小于 0.01 秒，视为 0
    if (CooldownRemaining < 0.01f)
    {
        // 输出日志
        UE_LOG(LogSGCard, Log, TEXT("  冷却时长接近 0，立即完成并抽卡"));
        // 直接完成冷却（会抽卡）
        CompleteCooldown();
        // 提前返回
        return;
    }
    
    // 🔧 MODIFIED - 清除旧的计时器（如果存在）
    UWorld* World = GetWorld();
    if (World)
    {
        // 检查是否有活动的计时器
        if (World->GetTimerManager().IsTimerActive(CooldownTimerHandle))
        {
            // 输出警告
            UE_LOG(LogSGCard, Warning, TEXT("  ⚠️ 检测到旧的计时器，先清除"));
            // 清除旧计时器
            World->GetTimerManager().ClearTimer(CooldownTimerHandle);
        }
        
        // 设置冷却计时器
        World->GetTimerManager().SetTimer(
            CooldownTimerHandle, 
            this, 
            &USG_CardDeckComponent::CompleteCooldown, 
            CooldownRemaining, 
            false  // 不循环
        );
        
        // 🔧 MODIFIED - 验证计时器是否成功设置
        if (World->GetTimerManager().IsTimerActive(CooldownTimerHandle))
        {
            // 输出成功日志
            float ActualRemaining = World->GetTimerManager().GetTimerRemaining(CooldownTimerHandle);
            UE_LOG(LogSGCard, Log, TEXT("  ✓ 冷却计时器已启动（实际剩余：%.2f 秒）"), ActualRemaining);
        }
        else
        {
            // 输出错误日志
            UE_LOG(LogSGCard, Error, TEXT("  ❌ 冷却计时器启动失败！"));
            // 强制完成冷却
            CompleteCooldown();
            return;
        }
    }
    else
    {
        // World 无效
        UE_LOG(LogSGCard, Error, TEXT("  ❌ World 为空，无法启动计时器"));
        // 直接完成冷却
        CompleteCooldown();
        return;
    }
    
    // 广播状态
    BroadcastActionState();
    
    // 输出日志
    UE_LOG(LogSGCard, Log, TEXT("========================================"));
}

/**
 * @brief 冷却结束
 * @details
 * 功能说明：
 * - 冷却计时器到期时调用
 * - 🔧 MODIFIED - 增强日志，便于调试
 * 详细流程：
 * 1. 输出日志
 * 2. 抽取一张新卡
 * 3. 恢复行动可用状态
 * 4. 重置冷却时间
 * 5. 广播状态变化
 * 注意事项：
 * - 抽卡可能失败（抽牌池为空）
 * - 即使抽卡失败，也要恢复行动状态
 */
void USG_CardDeckComponent::CompleteCooldown()
{
	UE_LOG(LogSGCard, Log, TEXT("冷却结束，抽取新卡"));
    
	// 抽取一张新卡
	FSGCardInstance NewCard;
	if (DrawSingleCard(NewCard))
	{
		HandCards.Add(NewCard);
		UE_LOG(LogSGCard, Log, TEXT("  抽到：%s"), 
			NewCard.CardData ? *NewCard.CardData->CardName.ToString() : TEXT("未知"));
        
		// 🔧 修改 - 在这里广播手牌变化
		OnHandChanged.Broadcast(HandCards);
	}
	else
	{
		UE_LOG(LogSGCard, Warning, TEXT("  抽卡失败"));
	}
    
	// 恢复行动可用状态
	bActionAvailable = true;
	CooldownRemaining = 0.0f;
    
	// 广播行动状态变化
	BroadcastActionState();
}

// 广播行动状态
void USG_CardDeckComponent::BroadcastActionState()
{
	
	// 广播可用状态与冷却时间
	OnActionStateChanged.Broadcast(bActionAvailable, CooldownRemaining);
}

// 加载卡牌数据
USG_CardDataBase* USG_CardDeckComponent::ResolveCardData(const FPrimaryAssetId& CardId)
{
	// 优先从配置中查找
	if (ResolvedDeckConfig)
	{
		// 🔧 MODIFIED - 使用 GetAllCardData() 获取所有卡牌数据
		TArray<USG_CardDataBase*> AllCards = ResolvedDeckConfig->GetAllCardData();
		
		for (USG_CardDataBase* CardAsset : AllCards)
		{
			if (CardAsset && CardAsset->GetPrimaryAssetId() == CardId)
			{
				return CardAsset;
			}
		}
	}
	
	// 从 AssetManager 加载
	if (USG_AssetManager* AssetManager = USG_AssetManager::Get())
	{
		return Cast<USG_CardDataBase>(AssetManager->GetPrimaryAssetObject(CardId));
	}
	
	return nullptr;
}

// 卡牌资产加载完成回调
void USG_CardDeckComponent::HandleCardAssetsLoaded()
{
	  // 重置加载状态
    bAssetsLoading = false;
    CurrentLoadHandle.Reset();

    UE_LOG(LogSGCard, Log, TEXT("========== 卡牌资产加载完成 =========="));

    // 检查配置有效性
    if (!ResolvedDeckConfig)
    {
        UE_LOG(LogSGCard, Error, TEXT("❌ 卡组配置无效！"));
        return;
    }

    // 初始化数据结构
    HandCards.Empty();
    DrawPile.Empty();
    DiscardPile.Empty();
    ConsumedUniqueCards.Empty();

    // 初始化随机流
    int32 Seed = ResolvedDeckConfig->GetEffectiveRNGSeed();
    RandomStream.Initialize(Seed);
    UE_LOG(LogSGCard, Log, TEXT("随机种子：%d"), Seed);

    // 构建抽牌池
    BuildDrawPile();

    // 标记为已初始化
    bInitialized = true;

    // 🔧 修改 - 延迟一帧后抽取初始手牌（包含保证卡牌逻辑）
    GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
    {
        if (!bInitialized || !ResolvedDeckConfig)
        {
            return;
        }

        int32 InitialHandSize = ResolvedDeckConfig->InitialHand;
        UE_LOG(LogSGCard, Log, TEXT("开始抽取初始手牌，目标数量：%d"), InitialHandSize);

        // ✨ 新增 - 步骤1：先抽取保证初始手牌的卡牌
        TArray<FSGCardInstance> GuaranteedCards;
        int32 GuaranteedCount = DrawGuaranteedCards(GuaranteedCards);
        
        if (GuaranteedCount > 0)
        {
            UE_LOG(LogSGCard, Log, TEXT("  ✓ 抽取了 %d 张保证卡牌"), GuaranteedCount);
            
            // 将保证卡牌加入手牌
            for (const FSGCardInstance& Card : GuaranteedCards)
            {
                HandCards.Add(Card);
                UE_LOG(LogSGCard, Log, TEXT("    - %s"), 
                    Card.CardData ? *Card.CardData->CardName.ToString() : TEXT("未知"));
            }
        }

        // ✨ 新增 - 步骤2：计算还需要抽取多少张卡牌
        int32 RemainingToDraw = InitialHandSize - HandCards.Num();
        
        if (RemainingToDraw > 0)
        {
            UE_LOG(LogSGCard, Log, TEXT("  继续抽取 %d 张普通卡牌"), RemainingToDraw);
            DrawCards(RemainingToDraw);
        }

        // 广播手牌变化
        OnHandChanged.Broadcast(HandCards);

        // 设置行动可用
        bActionAvailable = true;
        BroadcastActionState();

        // 广播初始化完成
        OnDeckInitialized.Broadcast();

        UE_LOG(LogSGCard, Log, TEXT("✓ 初始手牌抽取完成，共 %d 张"), HandCards.Num());
        UE_LOG(LogSGCard, Log, TEXT("========================================"));
    });
}

// ✨ NEW - 强制同步状态（供 UI 主动拉取）
void USG_CardDeckComponent::ForceSyncState()
{
	// 检查是否已初始化
	if (!bInitialized)
	{
		UE_LOG(LogSGCard, Warning, TEXT("ForceSyncState 失败：卡组尚未初始化"));
		return;
	}
	
	// 记录强制同步日志
	UE_LOG(LogSGCard, Log, TEXT("ForceSyncState - 强制同步当前状态到 UI"));
	
	// 广播当前手牌
	UE_LOG(LogSGCard, Log, TEXT("  广播手牌（%d 张）"), HandCards.Num());
	OnHandChanged.Broadcast(HandCards);
	
	// 广播当前选中状态
	UE_LOG(LogSGCard, Log, TEXT("  广播选中状态（%s）"), SelectedCardId.IsValid() ? TEXT("有选中") : TEXT("无选中"));
	OnSelectionChanged.Broadcast(SelectedCardId);
	
	// 广播当前行动状态
	UE_LOG(LogSGCard, Log, TEXT("  广播行动状态（可用: %d, 冷却: %.2f）"), bActionAvailable, CooldownRemaining);
	BroadcastActionState();
	
	// 记录同步完成
	UE_LOG(LogSGCard, Log, TEXT("✓ 状态同步完成"));
}

// 收集卡牌资产 ID
TArray<FPrimaryAssetId> USG_CardDeckComponent::GatherCardAssetIds() const
{
	// 结果数组
	TArray<FPrimaryAssetId> Result;
	
	// 检查配置有效性
	if (!ResolvedDeckConfig)
	{
		return Result;
	}
	
	// 使用 TSet 去重
	TSet<FPrimaryAssetId> UniqueIds;
	
	// 🔧 MODIFIED - 使用 GetAllCardData() 获取所有卡牌数据
	TArray<USG_CardDataBase*> AllCards = ResolvedDeckConfig->GetAllCardData();
	
	// 遍历所有卡牌数据
	for (USG_CardDataBase* CardAsset : AllCards)
	{
		// 跳过空引用
		if (!CardAsset)
		{
			continue;
		}
		
		// 获取资产 ID
		FPrimaryAssetId CardId = CardAsset->GetPrimaryAssetId();
		
		// 检查 ID 有效性和是否已存在
		if (!CardId.IsValid() || UniqueIds.Contains(CardId))
		{
			continue;
		}
		
		// 添加到集合和结果数组
		UniqueIds.Add(CardId);
		Result.Add(CardId);
	}
	
	return Result;
}
/**
 * @brief 抽取保证初始手牌的卡牌
 * @param OutInstances 输出的卡牌实例数组
 * @return 成功抽取的卡牌数量
 */
int32 USG_CardDeckComponent::DrawGuaranteedCards(TArray<FSGCardInstance>& OutInstances)
{
	OutInstances.Empty();
    
	// 检查配置有效性
	if (!ResolvedDeckConfig)
	{
		UE_LOG(LogSGCard, Warning, TEXT("DrawGuaranteedCards：配置无效"));
		return 0;
	}
    
	// 获取初始手牌数量限制
	int32 MaxGuaranteed = ResolvedDeckConfig->InitialHand;
    
	UE_LOG(LogSGCard, Log, TEXT("========== 抽取保证卡牌 =========="));
    
	// 遍历所有配置槽位，找出标记为保证初始手牌的卡牌
	for (int32 i = 0; i < ResolvedDeckConfig->AllowedCards.Num(); ++i)
	{
		const FSGCardConfigSlot& ConfigSlot = ResolvedDeckConfig->AllowedCards[i];
        
		// 检查是否标记为保证初始手牌
		if (!ConfigSlot.bGuaranteedInInitialHand)
		{
			continue;
		}
        
		// 检查是否已达到上限
		if (OutInstances.Num() >= MaxGuaranteed)
		{
			UE_LOG(LogSGCard, Warning, TEXT("  ⚠️ 保证卡牌数量已达到初始手牌上限 %d，跳过剩余保证卡牌"), MaxGuaranteed);
			break;
		}
        
		// 加载卡牌数据
		USG_CardDataBase* CardData = ConfigSlot.CardData.IsValid() 
			? ConfigSlot.CardData.Get() 
			: ConfigSlot.CardData.LoadSynchronous();
        
		if (!CardData)
		{
			UE_LOG(LogSGCard, Warning, TEXT("  ⚠️ 槽位 %d 的卡牌数据加载失败"), i);
			continue;
		}
        
		// 检查唯一卡牌是否已被消耗
		FPrimaryAssetId CardId = CardData->GetPrimaryAssetId();
		if (CardData->bIsUnique && ConsumedUniqueCards.Contains(CardId))
		{
			UE_LOG(LogSGCard, Log, TEXT("  跳过已消耗的唯一卡牌：%s"), *CardData->CardName.ToString());
			continue;
		}
        
		// 创建卡牌实例
		FSGCardInstance NewInstance;
		NewInstance.InstanceId = FGuid::NewGuid();
		NewInstance.CardData = CardData;
		NewInstance.CardId = CardId;
		NewInstance.bIsUnique = CardData->bIsUnique;
        
		// 添加到输出数组
		OutInstances.Add(NewInstance);
        
		UE_LOG(LogSGCard, Log, TEXT("  ✓ 保证抽取: %s (唯一: %s)"), 
			*CardData->CardName.ToString(),
			CardData->bIsUnique ? TEXT("是") : TEXT("否"));
        
		// 如果是唯一卡牌，标记为已消耗
		if (CardData->bIsUnique)
		{
			ConsumedUniqueCards.Add(CardId);
		}
        
		// 🔧 修改核心逻辑：处理抽牌池中的槽位
		// 查找对应的抽牌槽位
		for (int32 j = DrawPile.Num() - 1; j >= 0; --j)
		{
			if (DrawPile[j].CardId == CardId)
			{
				// 分支 A：如果是唯一卡牌，直接从抽牌池移除槽位（防止后续 DrawCards 再次抽到）
				if (CardData->bIsUnique)
				{
					DrawPile.RemoveAt(j);
					UE_LOG(LogSGCard, Verbose, TEXT("    [唯一] 从抽牌池移除槽位"));
				}
				// 分支 B：如果是普通卡牌（非唯一），保留槽位，但增加出现计数
				// 这样后续的 DrawCards 仍然可以从这个槽位抽卡，从而填满手牌
				else
				{
					DrawPile[j].OccurrenceCount++;
					DrawPile[j].MissCount = 0; // 重置 MissCount，因为它被“选中”了
					UE_LOG(LogSGCard, Verbose, TEXT("    [普通] 保留槽位，计数+1"));
					
					// 假设每个 ID 在 DrawPile 中只有一个槽位，找到后即可退出内层循环
					break; 
				}
			}
		}
	}
    
	UE_LOG(LogSGCard, Log, TEXT("  共抽取 %d 张保证卡牌"), OutInstances.Num());
	UE_LOG(LogSGCard, Log, TEXT("========================================"));
    
	return OutInstances.Num();
}

