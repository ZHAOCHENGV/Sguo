// Copyright notice placeholder
/**
 * @file SG_CardHandWidget.h
 * @brief 卡牌手牌 Widget 声明（修复开局展开动画 - 牌堆推移版）
 * @details
 * 功能说明：
 * - 🔧 修复 - 牌堆会随着卡牌飞出而向右推移
 * - ✨ 新增 - 动态计算牌堆位置（考虑已飞出的卡牌）
 * - ✨ 新增 - 卡牌从牌堆依次飞到左侧，推动牌堆和已有卡牌
 */
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Curves/CurveFloat.h"
#include "Sound/SoundBase.h"
#include "SG_CardHandWidget.generated.h"

// 前向声明
class USG_CardDeckComponent;
class USGCardHandViewModel;
class USG_CardEntryWidget;
class USGCardViewModel;
class UCanvasPanel;
class UCanvasPanelSlot;

// 卡牌布局信息结构体
USTRUCT(BlueprintType)
struct FSGCardLayoutInfo
{
	GENERATED_BODY()

	// 卡牌 Widget
	UPROPERTY()
	TObjectPtr<USG_CardEntryWidget> CardWidget = nullptr;

	// 卡牌 ViewModel（用于识别）
	UPROPERTY()
	TObjectPtr<USGCardViewModel> CardViewModel = nullptr;

	// 目标位置（X 坐标）
	UPROPERTY()
	float TargetPositionX = 0.0f;

	// 当前位置（X 坐标）
	UPROPERTY()
	float CurrentPositionX = 0.0f;

	// 目标 Y 偏移（弧形效果）
	UPROPERTY()
	float TargetOffsetY = 0.0f;

	// 当前 Y 偏移
	UPROPERTY()
	float CurrentOffsetY = 0.0f;

	// 目标旋转角度（Z轴）
	UPROPERTY()
	float TargetRotation = 0.0f;

	// 当前旋转角度
	UPROPERTY()
	float CurrentRotation = 0.0f;

	// 目标 Z 层级
	UPROPERTY()
	int32 TargetZOrder = 0;

	// 是否是新卡牌（正在推入）
	UPROPERTY()
	bool bIsNewCard = false;

	// 推入动画进度（0.0 ~ 1.0）
	UPROPERTY()
	float PushInProgress = 0.0f;

	// 是否正在播放移除动画
	UPROPERTY()
	bool bIsPlayingRemoveAnimation = false;

	// 移除动画进度（0.0 ~ 1.0）
	UPROPERTY()
	float RemoveAnimationProgress = 0.0f;

	// ✨ 新增 - 开局飞出动画相关
	// 是否正在播放开局飞出动画
	UPROPERTY()
	bool bIsPlayingOpeningFlyOut = false;

	// 开局飞出动画进度（0.0 ~ 1.0）
	UPROPERTY()
	float OpeningFlyOutProgress = 0.0f;

	// ✨ 新增 - 卡牌在飞出序列中的索引（用于计算目标位置）
	UPROPERTY()
	int32 FlyOutIndex = 0;
};

/**
 * @brief 卡牌手牌面板 Widget（修复开局展开动画 - 牌堆推移版）
 */
UCLASS(BlueprintType, Blueprintable)
class SGUO_API USG_CardHandWidget : public UUserWidget
{
public:
	GENERATED_BODY()

public:
	/**
	 * @brief 初始化手牌 UI
	 * @param InDeckComponent 卡组组件
	 */
	UFUNCTION(BlueprintCallable, Category = "Card")
	void InitializeCardHand(USG_CardDeckComponent* InDeckComponent);

	/**
	 * @brief 检查是否可以交互
	 * @return true：可以交互；false：不可以交互（开局展开动画中）
	 */
	UFUNCTION(BlueprintPure, Category = "Card")
	bool CanInteract() const { return bCanInteract; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled
	) const override;

protected:
	// ViewModel 实例
	UPROPERTY(BlueprintReadOnly, Category = "Card", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USGCardHandViewModel> HandViewModel;

	// 绑定的卡组组件
	UPROPERTY(BlueprintReadOnly, Category = "Card", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USG_CardDeckComponent> DeckComponent;

	/**
	 * @brief 卡牌容器（Canvas Panel）
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Card", meta = (BindWidget))
	TObjectPtr<UCanvasPanel> CardsArea;

	/**
	 * @brief Card Entry Widget 类
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card", meta = (DisplayName = "卡牌Widget类"))
	TSubclassOf<USG_CardEntryWidget> CardEntryWidgetClass;

	// ========== 卡牌布局配置 ==========

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card Layout", meta = (DisplayName = "卡牌区域宽度"))
	float CardAreaWidth = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card Layout", meta = (DisplayName = "单张卡牌宽度"))
	float CardWidth = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card Layout", meta = (DisplayName = "单张卡牌高度"))
	float CardHeight = 280.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card Layout", meta = (DisplayName = "最小卡牌间距", ClampMin = "0.0"))
	float MinCardSpacing = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card Layout", meta = (DisplayName = "最大卡牌间距", ClampMin = "0.0"))
	float MaxCardSpacing = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card Layout", meta = (DisplayName = "区域起始X坐标"))
	float CardAreaStartX = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card Layout", meta = (DisplayName = "卡牌Y坐标"))
	float CardYPosition = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card Layout", meta = (DisplayName = "卡牌重叠比例", ClampMin = "0.1", ClampMax = "1.0"))
	float CardOverlapRatio = 0.7f;

	// ========== 弧形排列配置 ==========

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card Arc Layout", meta = (DisplayName = "启用弧形排列"))
	bool bEnableArcLayout = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card Arc Layout", meta = (DisplayName = "弧形最大Y偏移"))
	float ArcMaxYOffset = 80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card Arc Layout", meta = (DisplayName = "弧形最大旋转角度", ClampMin = "0.0", ClampMax = "45.0"))
	float ArcMaxRotation = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card Arc Layout", meta = (DisplayName = "弧形曲线类型", ClampMin = "1.0", ClampMax = "5.0"))
	float ArcCurvePower = 2.0f;

	// ========== 🔧 修改 - 开局展开动画配置 ==========

	/**
	 * @brief 是否启用开局展开动画
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card Opening Animation", meta = (DisplayName = "启用开局展开动画"))
	bool bEnableOpeningAnimation = true;

	/**
	 * @brief 🔧 修改 - 牌堆位置 X 坐标
	 * @details 牌堆的绝对 X 坐标（默认在卡牌区域中心）
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card Opening Animation", meta = (DisplayName = "牌堆位置X坐标"))
	float DeckPilePositionX = 600.0f;


	/**
	 * @brief 牌堆位置 Y 坐标
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card Opening Animation", meta = (DisplayName = "牌堆位置Y坐标"))
	float DeckPilePositionY = 300.0f;

	/**
	 * @brief 开局展开延迟时间
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card Opening Animation", meta = (DisplayName = "展开延迟时间(秒)", ClampMin = "0.0", ClampMax = "10.0"))
	float OpeningDelayTime = 1.0f;

	/**
	 * @brief 单张卡牌飞出动画时长
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card Opening Animation", meta = (DisplayName = "单张卡牌飞出时长(秒)", ClampMin = "0.1", ClampMax = "2.0"))
	float CardFlyOutDuration = 0.5f;

	/**
	 * @brief 卡牌依次飞出的间隔时间
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card Opening Animation", meta = (DisplayName = "卡牌飞出间隔(秒)", ClampMin = "0.0", ClampMax = "1.0"))
	float CardFlyOutInterval = 0.1f;

	/**
	 * @brief 开局展开动画曲线
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card Opening Animation", meta = (DisplayName = "飞出动画曲线"))
	TObjectPtr<UCurveFloat> OpeningAnimationCurve = nullptr;

	/**
	 * @brief 开局展开音效
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card Opening Animation", meta = (DisplayName = "展开音效"))
	TObjectPtr<USoundBase> CardOpeningSound = nullptr;

	/**
	 * @brief 单张卡牌飞出音效
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card Opening Animation", meta = (DisplayName = "单张卡牌飞出音效"))
	TObjectPtr<USoundBase> CardFlyOutSound = nullptr;

	// ========== 卡牌动画配置 ==========

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card Animation", meta = (DisplayName = "位置插值速度", ClampMin = "0.5", ClampMax = "20.0"))
	float PositionInterpSpeed = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card Animation", meta = (DisplayName = "旋转插值速度", ClampMin = "0.5", ClampMax = "20.0"))
	float RotationInterpSpeed = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card Animation", meta = (DisplayName = "推入动画时长(秒)", ClampMin = "0.1", ClampMax = "5.0"))
	float PushInAnimationDuration = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card Animation", meta = (DisplayName = "移除动画时长(秒)", ClampMin = "0.1", ClampMax = "2.0"))
	float RemoveAnimationDuration = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card Animation", meta = (DisplayName = "移除动画Y偏移"))
	float RemoveAnimationYOffset = -200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card Animation", meta = (DisplayName = "推入动画曲线"))
	TObjectPtr<UCurveFloat> PushInAnimationCurve = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card Animation", meta = (DisplayName = "移除动画曲线"))
	TObjectPtr<UCurveFloat> RemoveAnimationCurve = nullptr;

	// ========== 音效配置 ==========

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card Audio", meta = (DisplayName = "选中卡牌音效"))
	TObjectPtr<USoundBase> CardSelectSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card Audio", meta = (DisplayName = "使用卡牌音效"))
	TObjectPtr<USoundBase> CardUseSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card Audio", meta = (DisplayName = "新卡牌出现音效"))
	TObjectPtr<USoundBase> CardDrawSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card Audio", meta = (DisplayName = "音效音量", ClampMin = "0.0", ClampMax = "1.0"))
	float AudioVolume = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card Audio", meta = (DisplayName = "音效音调", ClampMin = "0.5", ClampMax = "2.0"))
	float AudioPitch = 1.0f;

	// ========== 调试配置 ==========

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card Debug", meta = (DisplayName = "启用推入动画调试"))
	bool bEnablePushAnimationDebug = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card Debug", meta = (DisplayName = "显示区域调试框"))
	bool bShowCardAreaDebugBox = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card Debug", meta = (DisplayName = "调试框颜色"))
	FLinearColor DebugBoxColor = FLinearColor::Green;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card Debug", meta = (DisplayName = "调试框线条粗细", ClampMin = "1.0", ClampMax = "10.0"))
	float DebugBoxThickness = 2.0f;

protected:
	// 蓝图事件
	UFUNCTION(BlueprintImplementableEvent, Category = "Card")
	void HandleCardHandInitialized(USGCardHandViewModel* ViewModel);

	UFUNCTION(BlueprintImplementableEvent, Category = "Card")
	void HandleHandDataChanged();

	UFUNCTION(BlueprintImplementableEvent, Category = "Card")
	void HandleSkipRequested();

	UFUNCTION(BlueprintImplementableEvent, Category = "Card")
	void HandleOpeningAnimationStarted();

	UFUNCTION(BlueprintImplementableEvent, Category = "Card")
	void HandleOpeningAnimationCompleted();

private:
	// ========== 卡牌布局和动画 ==========

	void CalculateCardLayout();
	
	/**
	 * @brief ✨ 新增 - 计算开局飞出动画的目标位置
	 * @param FlyOutIndex 已飞出的卡牌索引（0 = 第1张，1 = 第2张...）
	 * @param TotalCards 总卡牌数
	 * @param OutTargetX 输出：目标 X 坐标
	 * @param OutTargetOffsetY 输出：目标 Y 偏移
	 * @param OutTargetRotation 输出：目标旋转角度
	 */
	void CalculateOpeningFlyOutTarget(
		int32 FlyOutIndex, 
		int32 TotalCards,
		float& OutTargetX,
		float& OutTargetOffsetY,
		float& OutTargetRotation
	) const;
	
	/**
	 * @brief ✨ 新增 - 获取当前牌堆的动态位置
	 * @details 牌堆位置 = 最右侧已飞出卡牌的右边
	 */
	FVector2D GetCurrentDeckPilePosition() const;
	
	void UpdateCardPositions(float DeltaTime);
	void ApplyCardPosition(FSGCardLayoutInfo& LayoutInfo);

	UFUNCTION()
	void OnCardUsed(USGCardViewModel* UsedCard);

	void RemoveCardWidget(USG_CardEntryWidget* CardWidget);
	void AddNewCardWithPushAnimation(USGCardViewModel* CardVM);
	float GetCurveValue(UCurveFloat* Curve, float Progress, float DefaultPower = 3.0f) const;
	void PlaySound2D(USoundBase* Sound);

	// 开局展开动画相关
	void StartOpeningAnimation();

	UFUNCTION()
	void OnOpeningDelayCompleted();

	/**
	 * @brief 🔧 修改 - 开始单张卡牌的飞出动画
	 * @param CardIndex 卡牌索引
	 */
	void StartCardFlyOut(int32 CardIndex);

	// 订阅卡组事件
	void BindDeckEvents();

	// 卡组事件回调
	UFUNCTION()
	void OnDeckHandChanged(const TArray<FSGCardInstance>& NewHand);

	UFUNCTION()
	void OnDeckSelectionChanged(const FGuid& SelectedId);

	UFUNCTION()
	void OnDeckActionStateChanged(bool bCanAct, float CooldownRemaining);

	UFUNCTION()
	void OnDeckInitialized();

	UFUNCTION(BlueprintCallable, Category = "Card")
	void RefreshCardsArea();

private:

	// ✨ 新增 - 延迟初始化标志
	bool bPendingRefresh = false;

	/**
	 * @brief ✨ 新增 - 获取当前牌堆的弧形旋转信息
	 * @param OutOffsetY 输出：Y 偏移
	 * @param OutRotation 输出：旋转角度
	 */
	void GetCurrentDeckPileArcInfo(float& OutOffsetY, float& OutRotation) const;

	
	// 卡牌布局信息数组
	UPROPERTY(Transient)
	TArray<FSGCardLayoutInfo> CardLayouts;

	// ========== 开局展开动画状态 ==========

	// 是否正在播放开局展开动画
	bool bIsPlayingOpeningAnimation = false;

	// 当前正在飞出的卡牌索引
	int32 CurrentFlyOutCardIndex = 0;

	// 下一张卡牌飞出的累计时间
	float NextCardFlyOutTime = 0.0f;

	// 是否可以交互（展开完成前禁用）
	bool bCanInteract = false;

	// 开局展开延迟定时器句柄
	FTimerHandle OpeningDelayTimerHandle;

public:
	UFUNCTION(BlueprintCallable, Category = "Card")
	void RequestSkip();
};
