// 🔧 MODIFIED FILE - 卡组组件头文件
// Copyright notice placeholder
/**
 * @file SG_CardDeckComponent.h
 * @brief 卡组运行时组件声明
 * @details
 * 功能说明：
 * - 负责根据卡组配置抽牌、管理手牌、处理冷却以及跳过操作
 * - 提供事件与函数供 UI 与游戏逻辑交互
 * - 支持权重随机抽卡和保底机制
 * 详细流程：
 * - 初始化时读取 USG_DeckConfig 构建抽牌池并抽取初始手牌
 * - 玩家使用卡牌或放弃行动后进入冷却，冷却结束自动补牌
 * - 通过广播委托使 MVVM ViewModel 同步状态
 * - 权重系统确保卡牌分布均匀，避免连续抽到同一张卡
 * 注意事项：
 * - 组件需附加在玩家控制器或 Pawn 上，并在 BeginPlay 前指定卡组配置
 * - 唯一卡牌抽到后加入 ConsumedUniqueCards 集合，不再参与抽卡
 * - 权重系统需要配合 FSGCardDrawSlot 的 DrawWeight 和 MissCount 使用
 */
#pragma once

// 引入核心头文件
#include "CoreMinimal.h"
// 引入 Actor 组件基类
#include "Components/ActorComponent.h"
// 引入运行时卡牌类型
#include "CardsAndUnits/SG_CardRuntimeTypes.h"
// 引入异步加载管理器
#include "Engine/StreamableManager.h"
// 引入卡组组件生成宏
#include "SG_CardDeckComponent.generated.h"

// 前向声明卡组配置
class USG_DeckConfig;
// 前向声明卡牌数据资产
class USG_CardDataBase;

// 委托：手牌更新
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSGCardHandChangedSignature, const TArray<FSGCardInstance>&, NewHand);
// 委托：选中卡牌变化
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSGCardSelectionChangedSignature, const FGuid&, SelectedId);
// 委托：行动可用状态变化（bCanAct, CooldownRemaining）
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSGCardActionStateSignature, bool, bCanAct, float, CooldownRemaining);
// 委托：卡牌被使用
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSGCardUsedSignature, const FSGCardInstance&, UsedCard);
// ✨ NEW - 初始化完成委托
// 当卡组完全初始化完成后广播，UI 可以监听此事件
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSGDeckInitializedSignature);
/**
 * @brief 卡组运行时组件
 * @details
 * 功能说明：
 * - 管理抽牌、手牌、冷却与跳过行动
 * - 与 AssetManager 协作加载卡牌数据资产
 * - 实现权重随机抽卡和保底机制
 * 详细流程：
 * - InitializeDeck 根据配置构建抽牌池并抽取初始手牌
 * - UseCard / SkipAction 触发冷却并自动补牌
 * - SelectCard 用于 UI 高亮与预览
 * - DrawSingleCard 使用权重系统选择卡牌，确保分布均匀
 * 注意事项：
 * - 冷却时间与初始手牌数量来源于 USG_DeckConfig
 * - 唯一卡牌抽到后不再参与抽卡，但槽位保留
 * - 权重系统会自动调整，长期来看所有卡牌出现概率趋于均匀
 */
UCLASS(BlueprintType, Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class SGUO_API USG_CardDeckComponent : public UActorComponent
{
public:
	// 生成类宏
	GENERATED_BODY()

public:
	/** 
		 * @brief 构造函数
		 * @details
		 * 功能说明：
		 * - 初始化组件默认设置
		 * - 启用 Tick 以便更新冷却时间
		 */
	USG_CardDeckComponent();

protected:
	/** 
		 * @brief 生命周期开始
		 * @details
		 * 功能说明：
		 * - 如果启用自动初始化且不是 PlayerController，则自动初始化卡组
		 * - PlayerController 会手动控制初始化时机
		 */
	virtual void BeginPlay() override;

	/** 
		 * @brief 每帧更新
		 * @param DeltaTime 帧间隔时间
		 * @param TickType Tick 类型
		 * @param ThisTickFunction Tick 函数指针
		 * @details
		 * 功能说明：
		 * - 更新冷却剩余时间
		 * - 广播行动状态变化
		 */
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:
	/**
		 * @brief 初始化卡组
		 * @details
		 * 功能说明：
		 * - 加载卡组配置并异步加载所有卡牌资产
		 * - 构建抽牌池并抽取初始手牌
		 * 详细流程：
		 * 1. 检查是否已初始化或正在加载
		 * 2. 加载卡组配置资产
		 * 3. 收集需要加载的卡牌资产 ID
		 * 4. 通过 AssetManager 异步批量加载
		 * 5. 加载完成后调用 HandleCardAssetsLoaded
		 * 注意事项：
		 * - 避免重复初始化
		 * - 异步加载期间不会阻塞主线程
		 */
	UFUNCTION(BlueprintCallable, Category = "CardDeck")
	void InitializeDeck();

	/**
		 * @brief 获取当前手牌
		 * @return 手牌数组的常量引用
		 * @details
		 * 功能说明：
		 * - 返回当前手牌列表
		 * - 用于 UI 显示和游戏逻辑判断
		 */
	UFUNCTION(BlueprintCallable, Category = "CardDeck")
	const TArray<FSGCardInstance>& GetHand() const;

	/**
	 * @brief 选择卡牌
	 * @param InstanceId 卡牌实例 ID
	 * @details
	 * 功能说明：
	 * - 设置当前选中的卡牌
	 * - 广播选中变化事件供 UI 更新
	 * 注意事项：
	 * - 不会检查卡牌是否存在，由调用方保证
	 */
	UFUNCTION(BlueprintCallable, Category = "CardDeck")
	void SelectCard(const FGuid& InstanceId);

	/**
	 * @brief 获取当前选择的卡牌 ID
	 * @return 选中的卡牌实例 ID
	 * @details
	 * 功能说明：
	 * - 返回当前选中的卡牌 ID
	 * - 用于 UI 高亮显示
	 */
	UFUNCTION(BlueprintCallable, Category = "CardDeck")
	FGuid GetSelectedCardId() const;

	/**
	 * @brief 使用指定卡牌
	 * @param InstanceId 卡牌实例 ID
	 * @return 是否成功使用
	 * @details
	 * 功能说明：
	 * - 使用指定卡牌并触发冷却
	 * - 非唯一卡牌加入弃牌堆，唯一卡牌加入消耗列表
	 * 详细流程：
	 * 1. 检查是否处于冷却中
	 * 2. 在手牌中查找目标卡牌
	 * 3. 从手牌移除并处理弃牌/消耗
	 * 4. 广播手牌变化和卡牌使用事件
	 * 5. 启动冷却计时器
	 * 注意事项：
	 * - 冷却中无法使用卡牌
	 * - 唯一卡牌使用后不会再次出现
	 */
	UFUNCTION(BlueprintCallable, Category = "CardDeck")
	bool UseCard(const FGuid& InstanceId);

	/**
	 * @brief 请求跳过行动
	 * @return 是否成功跳过
	 * @details
	 * 功能说明：
	 * - 放弃本次行动机会并进入冷却
	 * - 冷却结束后抽取新卡
	 * 注意事项：
	 * - 冷却中无法跳过
	 */
	UFUNCTION(BlueprintCallable, Category = "CardDeck")
	bool SkipAction();

	/**
	 * @brief 是否可行动
	 * @return 当前是否可以使用卡牌或跳过
	 * @details
	 * 功能说明：
	 * - 返回当前行动可用状态
	 * - 用于 UI 禁用/启用按钮
	 */
	UFUNCTION(BlueprintCallable, Category = "CardDeck")
	bool CanAct() const;

	/**
	 * @brief 获取冷却剩余时间
	 * @return 冷却剩余秒数
	 * @details
	 * 功能说明：
	 * - 返回当前冷却剩余时间
	 * - 用于 UI 显示倒计时
	 */
	UFUNCTION(BlueprintCallable, Category = "CardDeck")
	float GetCooldownRemaining() const;

	
	/**
	 * @brief 获取卡组配置
	 * @return 卡组配置对象指针
	 * @details
	 * 功能说明：
	 * - 返回当前使用的卡组配置
	 * - 用于读取配置参数
	 */
	UFUNCTION(BlueprintCallable, Category = "CardDeck")
	USG_DeckConfig* GetDeckConfig() const;

	// ✨ NEW - 检查是否已初始化
	/**
	 * @brief 检查卡组是否已完成初始化
	 * @return 是否已初始化
	 * @details
	 * 功能说明：
	 * - 返回卡组是否已完成初始化（资产加载完成且抽取了初始手牌）
	 * - UI 可以通过此函数检查卡组状态
	 * 使用场景：
	 * - UI 初始化时检查卡组是否就绪
	 * - 避免在卡组未就绪时访问数据
	 */
	UFUNCTION(BlueprintCallable, Category = "CardDeck")
	bool IsInitialized() const { return bInitialized; }

	// ✨ NEW - 强制同步状态（供 UI 主动拉取）
	/**
	 * @brief 强制同步当前状态到 UI
	 * @details
	 * 功能说明：
	 * - 主动广播当前的手牌、选中状态和行动状态
	 * - 用于 UI 初始化时主动拉取数据，不依赖事件时序
	 * 详细流程：
	 * 1. 检查是否已初始化
	 * 2. 广播当前手牌
	 * 3. 广播当前选中状态
	 * 4. 广播当前行动状态
	 * 使用场景：
	 * - UI Widget 初始化时调用
	 * - 确保 UI 能获取到最新状态
	 * 注意事项：
	 * - 只在卡组已初始化后调用
	 * - 不会触发重复的数据更新
	 */
	UFUNCTION(BlueprintCallable, Category = "CardDeck")
	void ForceSyncState();

public:
	// 手牌更新广播
	// 当手牌发生变化时触发（抽卡、使用卡牌）
	UPROPERTY(BlueprintAssignable, Category = "CardDeck")
	FSGCardHandChangedSignature OnHandChanged;

	// 选中卡牌广播
	// 当选中的卡牌发生变化时触发
	UPROPERTY(BlueprintAssignable, Category = "CardDeck")
	FSGCardSelectionChangedSignature OnSelectionChanged;


	// 行动状态广播
	// 当行动可用状态或冷却时间发生变化时触发
	UPROPERTY(BlueprintAssignable, Category = "CardDeck")
	FSGCardActionStateSignature OnActionStateChanged;

	// 卡牌使用广播
	// 当卡牌被成功使用时触发
	UPROPERTY(BlueprintAssignable, Category = "CardDeck")
	FSGCardUsedSignature OnCardUsed;

	// 初始化完成委托
	// 当卡组完全初始化完成后广播
	// UI 可以监听此事件来确保在正确的时机初始化
	UPROPERTY(BlueprintAssignable, Category = "CardDeck")
	FSGDeckInitializedSignature OnDeckInitialized;

protected:
	// 卡组配置引用
	// 软引用，支持异步加载
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CardDeck", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<USG_DeckConfig> DeckConfigAsset;

	// 是否在 BeginPlay 初始化
	// PlayerController 通常设置为 false，手动控制初始化时机
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CardDeck", meta = (AllowPrivateAccess = "true"))
	bool bAutoInitialize = true;

	// 当前手牌数组
	// 存储玩家手中的所有卡牌实例
	UPROPERTY(BlueprintReadOnly, Category = "CardDeck", meta = (AllowPrivateAccess = "true"))
	TArray<FSGCardInstance> HandCards;

	// 当前抽牌池
	// 存储所有可抽取的卡牌槽位（包括权重信息）
	UPROPERTY(BlueprintReadOnly, Category = "CardDeck", meta = (AllowPrivateAccess = "true"))
	TArray<FSGCardDrawSlot> DrawPile;

	// 弃牌堆
	// 存储已使用的非唯一卡牌，用于重新填充抽牌池
	UPROPERTY(BlueprintReadOnly, Category = "CardDeck", meta = (AllowPrivateAccess = "true"))
	TArray<FSGCardDrawSlot> DiscardPile;

	// 已使用的唯一卡牌
	// 存储已使用的唯一卡牌 ID，这些卡牌不会再次出现
	UPROPERTY(BlueprintReadOnly, Category = "CardDeck", meta = (AllowPrivateAccess = "true"))
	TSet<FPrimaryAssetId> ConsumedUniqueCards;


	// 当前选中卡牌 ID
	// 用于 UI 高亮显示
	UPROPERTY(BlueprintReadOnly, Category = "CardDeck", meta = (AllowPrivateAccess = "true"))
	FGuid SelectedCardId;


	// 当前冷却剩余时间
	// 每帧更新，用于 UI 显示倒计时
	UPROPERTY(BlueprintReadOnly, Category = "CardDeck", meta = (AllowPrivateAccess = "true"))
	float CooldownRemaining = 0.0f;

	// 行动是否可用
	// True：可以使用卡牌或跳过
	// False：处于冷却中
	UPROPERTY(BlueprintReadOnly, Category = "CardDeck", meta = (AllowPrivateAccess = "true"))
	bool bActionAvailable = false;

protected:
	// 卡组配置缓存指针
	// 加载完成后的卡组配置对象
	UPROPERTY(Transient)
	TObjectPtr<USG_DeckConfig> ResolvedDeckConfig;

	// 冷却计时器句柄
	// 用于管理冷却计时器
	FTimerHandle CooldownTimerHandle;

	// 随机流
	// 用于生成可重现的随机数（基于种子）
	FRandomStream RandomStream;

	// 是否已经初始化
	// 防止重复初始化
	bool bInitialized = false;


	// 是否正在加载资产
	// 防止加载期间重复触发加载
	bool bAssetsLoading = false;

	// 异步加载句柄
	// 用于管理异步加载流程
	TSharedPtr<FStreamableHandle> CurrentLoadHandle;


	// 🔧 MODIFIED - 移除 CardCountMap，不再需要统计卡牌数量
	// UPROPERTY(BlueprintReadOnly, Category = "CardDeck", meta = (AllowPrivateAccess = "true"))
	// TMap<FString, int32> CardCountMap;

protected:
	/**
	 * @brief 构建抽牌池
	 * @details
	 * 功能说明：
	 * - 根据卡组配置构建初始抽牌池
	 * - 为每个卡牌创建槽位并初始化权重
	 * 详细流程：
	 * 1. 清空现有抽牌池和消耗列表
	 * 2. 遍历配置中的所有卡牌
	 * 3. 为每张卡创建槽位并设置初始权重
	 * 4. 洗牌（随机打乱槽位顺序）
	 * 注意事项：
	 * - 洗牌使用随机流，确保可重现
	 */
	void BuildDrawPile();

	/**
	 * @brief 抽取指定数量的卡牌
	 * @param Count 要抽取的卡牌数量
	 * @details
	 * 功能说明：
	 * - 连续抽取多张卡牌并加入手牌
	 * - 抽取完成后广播手牌变化事件
	 * 详细流程：
	 * 1. 循环调用 DrawSingleCard 抽取卡牌
	 * 2. 将抽到的卡牌加入手牌数组
	 * 3. 广播手牌变化事件
	 * 注意事项：
	 * - 如果抽牌池为空，会自动重新填充
	 * - 唯一卡牌抽到后不会再次出现
	 */
	void DrawCards(int32 Count);

	/**
	 * @brief 抽取单张卡牌（权重随机系统）
	 * @param OutInstance 输出参数，抽到的卡牌实例
	 * @return 是否成功抽取
	 * @details
	 * 功能说明：
	 * - 使用权重随机系统抽取一张卡牌
	 * - 支持保底机制，长期来看分布均匀
	 * 详细流程：
	 * 1. 收集所有可抽取的槽位（排除已消耗的唯一卡牌）
	 * 2. 计算每个槽位的实际权重（基础权重 * 保底系数）
	 * 3. 使用轮盘赌算法随机选择一个槽位
	 * 4. 更新所有槽位的 MissCount（抽到的重置为 0，未抽到的 +1）
	 * 5. 解析卡牌数据并构建实例
	 * 6. 唯一卡牌加入消耗列表，非唯一卡牌加入弃牌堆
	 * 注意事项：
	 * - 实际权重 = DrawWeight * (1.0 + MissCount * 0.1)
	 * - 保底系数确保长期来看所有卡牌出现概率趋于均匀
	 * - 如果抽牌池为空，会自动重新填充
	 */
	UFUNCTION(BlueprintCallable, Category = "CardDeck")
	bool DrawSingleCard(FSGCardInstance& OutInstance);

	/**
	 * @brief 将弃牌堆重新洗入抽牌堆
	 * @details
	 * 功能说明：
	 * - 当抽牌池为空时，将弃牌堆的卡牌重新加入抽牌池
	 * - 移除已消耗的唯一卡牌
	 * 详细流程：
	 * 1. 将弃牌堆的所有槽位加入抽牌池
	 * 2. 清空弃牌堆
	 * 3. 移除已消耗的唯一卡牌槽位
	 * 4. 洗牌（随机打乱顺序）
	 * 注意事项：
	 * - 唯一卡牌使用后不会重新加入抽牌池
	 * - 洗牌使用随机流，确保可重现
	 */
	void RefillDrawPile();

	/**
	 * @brief 启动冷却
	 * @details
	 * 功能说明：
	 * - 开始冷却计时器
	 * - 设置行动不可用状态
	 * 详细流程：
	 * 1. 标记行动不可用
	 * 2. 读取冷却时长（来自卡组配置）
	 * 3. 启动计时器，到期后调用 CompleteCooldown
	 * 4. 广播行动状态变化
	 * 注意事项：
	 * - 如果冷却时长为 0，立即完成冷却
	 */
	void StartCooldown();

	/**
	 * @brief 冷却结束回调
	 * @details
	 * 功能说明：
	 * - 冷却计时器到期时调用
	 * - 抽取新卡并恢复行动可用状态
	 * 详细流程：
	 * 1. 抽取一张新卡
	 * 2. 恢复行动可用状态
	 * 3. 重置冷却剩余时间
	 * 4. 广播行动状态变化
	 */
	void CompleteCooldown();

	/**
	 * @brief 更新行动状态广播
	 * @details
	 * 功能说明：
	 * - 广播当前行动可用状态和冷却剩余时间
	 * - 供 UI 更新显示
	 */
	void BroadcastActionState();

	/**
	 * @brief 通过资产 ID 同步卡牌数据
	 * @param CardId 卡牌资产 ID
	 * @return 卡牌数据对象指针，失败返回 nullptr
	 * @details
	 * 功能说明：
	 * - 根据资产 ID 获取卡牌数据对象
	 * - 优先从配置中查找，失败则从 AssetManager 加载
	 * 注意事项：
	 * - 如果资产未加载，可能返回 nullptr
	 */
	USG_CardDataBase* ResolveCardData(const FPrimaryAssetId& CardId);

	/**
	 * @brief 预加载所需卡牌资产
	 * @details
	 * 功能说明：
	 * - 异步加载完成后的回调函数
	 * - 初始化抽牌池并抽取初始手牌
	 * 详细流程：
	 * 1. 重置加载状态
	 * 2. 检查配置有效性
	 * 3. 初始化数据结构（手牌、抽牌池、弃牌堆）
	 * 4. 构建抽牌池
	 * 5. 标记为已初始化
	 * 6. 延迟一帧后抽取初始手牌并广播事件
	 * 注意事项：
	 * - 延迟一帧确保 UI 已准备好
	 * - 避免初始化时序问题
	 */
	void HandleCardAssetsLoaded();

	/**
	 * @brief 收集卡牌资产 ID
	 * @return 需要加载的卡牌资产 ID 数组
	 * @details
	 * 功能说明：
	 * - 从卡组配置中收集所有需要加载的卡牌资产 ID
	 * - 去重，避免重复加载
	 * 注意事项：
	 * - 使用 TSet 去重，确保每个资产只加载一次
	 */
	TArray<FPrimaryAssetId> GatherCardAssetIds() const;

};

