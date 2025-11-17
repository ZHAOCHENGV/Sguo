// Copyright notice placeholder
/**
 * @file SG_CardHandWidget.cpp
 * @brief 卡牌手牌 Widget 实现（修复开局展开动画）
 */
#include "UIHud/SG_CardHandWidget.h"
#include "CardsAndUnits/SG_CardDeckComponent.h"
#include "UIHud/SG_CardHandViewModel.h"
#include "UIHud/SG_CardViewModel.h"
#include "Debug/SG_LogCategories.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "UIHud/SG_CardEntryWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

/**
 * @brief 初始化卡牌 UI
 */
void USG_CardHandWidget::InitializeCardHand(USG_CardDeckComponent* InDeckComponent)
{
	UE_LOG(LogSGUI, Log, TEXT("InitializeCardHand 被调用"));
	
	DeckComponent = InDeckComponent;
	
	if (!DeckComponent)
	{
		UE_LOG(LogSGUI, Error, TEXT("❌ DeckComponent 为空"));
		return;
	}
	
	UE_LOG(LogSGUI, Log, TEXT("创建 HandViewModel..."));
	HandViewModel = NewObject<USGCardHandViewModel>(this);
	
	HandViewModel->Initialize(DeckComponent);
	
	BindDeckEvents();
	
	if (DeckComponent->IsInitialized())
	{
		UE_LOG(LogSGUI, Log, TEXT("卡组已初始化，主动拉取状态..."));
		
		DeckComponent->ForceSyncState();

		RefreshCardsArea();
		
		UE_LOG(LogSGUI, Log, TEXT("通知蓝图 HandleCardHandInitialized..."));
		if (GetClass()->IsFunctionImplementedInScript(TEXT("HandleCardHandInitialized")))
		{
			HandleCardHandInitialized(HandViewModel);
		}
		else
		{
			UE_LOG(LogSGUI, Warning, TEXT("⚠️ 蓝图未实现 HandleCardHandInitialized 事件"));
		}
	}
	else
	{
		UE_LOG(LogSGUI, Log, TEXT("卡组尚未初始化，等待初始化完成事件..."));
		
		DeckComponent->OnDeckInitialized.AddDynamic(this, &USG_CardHandWidget::OnDeckInitialized);
	}
	
	UE_LOG(LogSGUI, Log, TEXT("✓ CardHandWidget 初始化完成"));
}

/**
 * @brief 绑定卡组事件
 */
void USG_CardHandWidget::BindDeckEvents()
{
	if (!DeckComponent)
	{
		return;
	}
	
	DeckComponent->OnHandChanged.AddDynamic(this, &USG_CardHandWidget::OnDeckHandChanged);
	DeckComponent->OnSelectionChanged.AddDynamic(this, &USG_CardHandWidget::OnDeckSelectionChanged);
	DeckComponent->OnActionStateChanged.AddDynamic(this, &USG_CardHandWidget::OnDeckActionStateChanged);
	
	UE_LOG(LogSGUI, Log, TEXT("✓ 已绑定卡组事件"));
}

/**
 * @brief 处理手牌变化（C++ 回调）
 */
void USG_CardHandWidget::OnDeckHandChanged(const TArray<FSGCardInstance>& NewHand)
{
	if (bEnablePushAnimationDebug)
	{
		UE_LOG(LogSGUI, Log, TEXT("========== OnDeckHandChanged =========="));
		UE_LOG(LogSGUI, Log, TEXT("  新手牌数：%d"), NewHand.Num());
		UE_LOG(LogSGUI, Log, TEXT("  当前布局数：%d"), CardLayouts.Num());
	}
	
	if (!HandViewModel)
	{
		UE_LOG(LogSGUI, Error, TEXT("❌ HandViewModel 为空"));
		return;
	}
	
	TArray<USGCardViewModel*> NewCardVMs = HandViewModel->GetCardViewModels();
	
	// 找出新增的卡牌
	TArray<USGCardViewModel*> NewCards;
	
	for (USGCardViewModel* CardVM : NewCardVMs)
	{
		if (!CardVM)
		{
			continue;
		}
		
		bool bAlreadyExists = false;
		for (const FSGCardLayoutInfo& LayoutInfo : CardLayouts)
		{
			if (LayoutInfo.CardViewModel == CardVM)
			{
				bAlreadyExists = true;
				break;
			}
		}
		
		if (!bAlreadyExists)
		{
			NewCards.Add(CardVM);
			if (bEnablePushAnimationDebug)
			{
				UE_LOG(LogSGUI, Log, TEXT("  ✨ 发现新卡牌：%s"), *CardVM->CardName.ToString());
			}
		}
	}
	
	// 处理新增的卡牌
	if (NewCards.Num() > 0)
	{
		if (bEnablePushAnimationDebug)
		{
			UE_LOG(LogSGUI, Log, TEXT("  📥 添加 %d 张新卡牌（所有新卡牌都从右侧推入）"), NewCards.Num());
		}
		
		// 先重新计算现有卡牌布局（为新卡牌腾出空间）
		CalculateCardLayout();
		
		// 所有新卡牌都从右侧推入
		for (USGCardViewModel* NewCard : NewCards)
		{
			AddNewCardWithPushAnimation(NewCard);
		}
		
		// 再次计算布局（包含新卡牌）
		CalculateCardLayout();
	}
	
	if (bEnablePushAnimationDebug)
	{
		UE_LOG(LogSGUI, Log, TEXT("========================================"));
	}
	
	HandleHandDataChanged();
}

/**
 * @brief 处理选中变化（C++ 回调）
 */
void USG_CardHandWidget::OnDeckSelectionChanged(const FGuid& SelectedId)
{
	UE_LOG(LogSGUI, Verbose, TEXT("OnDeckSelectionChanged"));
	
	// 播放选中音效
	if (SelectedId.IsValid())
	{
		PlaySound2D(CardSelectSound);
	}
	
	HandleHandDataChanged();
}

/**
 * @brief 处理行动状态变化（C++ 回调）
 */
void USG_CardHandWidget::OnDeckActionStateChanged(bool bCanAct, float CooldownRemaining)
{
	UE_LOG(LogSGUI, Verbose, TEXT("OnDeckActionStateChanged - CanAct: %d, Cooldown: %.2f"), bCanAct, CooldownRemaining);
	
	HandleHandDataChanged();
}

/**
 * @brief 卡组初始化完成回调
 */
void USG_CardHandWidget::OnDeckInitialized()
{
	UE_LOG(LogSGUI, Log, TEXT("OnDeckInitialized - 卡组初始化完成，主动拉取状态..."));
	
	if (!DeckComponent)
	{
		UE_LOG(LogSGUI, Error, TEXT("❌ DeckComponent 为空"));
		return;
	}
	
	DeckComponent->ForceSyncState();

	RefreshCardsArea();
	
	UE_LOG(LogSGUI, Log, TEXT("通知蓝图 HandleCardHandInitialized..."));
	if (GetClass()->IsFunctionImplementedInScript(TEXT("HandleCardHandInitialized")))
	{
		HandleCardHandInitialized(HandViewModel);
	}
	else
	{
		UE_LOG(LogSGUI, Warning, TEXT("⚠️ 蓝图未实现 HandleCardHandInitialized 事件"));
	}
	
	UE_LOG(LogSGUI, Log, TEXT("✓ UI 初始化完成"));
}

/**
 * @brief Widget 构建
 */
void USG_CardHandWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!CardsArea)
	{
		UE_LOG(LogSGUI, Error, TEXT("❌ CardsArea 未绑定！"));
	}
	else
	{
		UE_LOG(LogSGUI, Log, TEXT("✓ CardsArea 已绑定"));
	}
	
	if (!CardEntryWidgetClass)
	{
		UE_LOG(LogSGUI, Error, TEXT("❌ CardEntryWidgetClass 未设置！"));
	}
	else
	{
		UE_LOG(LogSGUI, Log, TEXT("✓ CardEntryWidgetClass 已设置：%s"), 
			*CardEntryWidgetClass->GetName());
	}
}

/**
 * @brief Widget 销毁
 */
void USG_CardHandWidget::NativeDestruct()
{
	// 清理开局展开动画定时器
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(OpeningDelayTimerHandle);
	}
	
	if (DeckComponent)
	{
		DeckComponent->OnDeckInitialized.RemoveDynamic(this, &USG_CardHandWidget::OnDeckInitialized);
	}
	
	Super::NativeDestruct();
}

/**
 * @brief 每帧更新
 */
void USG_CardHandWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	
	// 检查是否需要延迟刷新
	if (bPendingRefresh)
	{
		// 检查 CardsArea 是否已完成布局
		FGeometry CardsAreaGeometry = CardsArea->GetCachedGeometry();
		FVector2D LocalSize = CardsAreaGeometry.GetLocalSize();
		
		if (LocalSize.X > 0.0f && LocalSize.Y > 0.0f)
		{
			UE_LOG(LogSGUI, Log, TEXT("✓ CardsArea 布局完成，开始初始化卡牌"));
			UE_LOG(LogSGUI, Log, TEXT("   CardsArea 尺寸：[%.2f, %.2f]"), LocalSize.X, LocalSize.Y);
			
			// 清除标志
			bPendingRefresh = false;
			
			// 重新刷新
			RefreshCardsArea();
		}
	}
	
	UpdateCardPositions(InDeltaTime);
}

/**
 * @brief 绘制调试信息
 */
int32 USG_CardHandWidget::NativePaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled
) const
{
	// 调用父类绘制
	int32 MaxLayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	
	// 如果启用了调试框，绘制卡牌区域边框
	if (bShowCardAreaDebugBox)
	{
		// 计算卡牌区域的矩形
		FVector2D TopLeft(CardAreaStartX, CardYPosition - 20.0f);
		FVector2D BottomRight(CardAreaStartX + CardAreaWidth, CardYPosition + CardHeight + 20.0f);
		
		// 创建线条点数组（绘制矩形）
		TArray<FVector2D> LinePoints;
		LinePoints.Add(TopLeft);
		LinePoints.Add(FVector2D(BottomRight.X, TopLeft.Y));
		LinePoints.Add(BottomRight);
		LinePoints.Add(FVector2D(TopLeft.X, BottomRight.Y));
		LinePoints.Add(TopLeft);
		
		// 绘制线条
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			MaxLayerId + 1,
			AllottedGeometry.ToPaintGeometry(),
			LinePoints,
			ESlateDrawEffect::None,
			DebugBoxColor.ToFColor(true),
			true,
			DebugBoxThickness
		);
		
		// 绘制配置的牌堆位置（蓝色十字）
		TArray<FVector2D> ConfigPilePoints;
		float MarkerSize = 20.0f;
		FVector2D ConfigCenter(DeckPilePositionX + CardWidth / 2.0f, DeckPilePositionY + CardHeight / 2.0f);
		
		ConfigPilePoints.Add(FVector2D(ConfigCenter.X - MarkerSize, ConfigCenter.Y));
		ConfigPilePoints.Add(FVector2D(ConfigCenter.X + MarkerSize, ConfigCenter.Y));
		ConfigPilePoints.Add(FVector2D(ConfigCenter.X, ConfigCenter.Y - MarkerSize));
		ConfigPilePoints.Add(FVector2D(ConfigCenter.X, ConfigCenter.Y + MarkerSize));
		
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			MaxLayerId + 2,
			AllottedGeometry.ToPaintGeometry(),
			ConfigPilePoints,
			ESlateDrawEffect::None,
			FLinearColor::Blue.ToFColor(true),
			true,
			DebugBoxThickness * 1.5f
		);
		
		// 绘制动态牌堆位置（红色十字）
		FVector2D DeckPilePos = GetCurrentDeckPilePosition();
		
		TArray<FVector2D> DeckPilePoints;
		FVector2D DeckCenter(DeckPilePos.X + CardWidth / 2.0f, DeckPilePos.Y + CardHeight / 2.0f);
		
		DeckPilePoints.Add(FVector2D(DeckCenter.X - MarkerSize, DeckCenter.Y));
		DeckPilePoints.Add(FVector2D(DeckCenter.X + MarkerSize, DeckCenter.Y));
		DeckPilePoints.Add(FVector2D(DeckCenter.X, DeckCenter.Y - MarkerSize));
		DeckPilePoints.Add(FVector2D(DeckCenter.X, DeckCenter.Y + MarkerSize));
		
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			MaxLayerId + 3,
			AllottedGeometry.ToPaintGeometry(),
			DeckPilePoints,
			ESlateDrawEffect::None,
			FLinearColor::Red.ToFColor(true),
			true,
			DebugBoxThickness
		);
		
		// 绘制文字
		FString DebugText = FString::Printf(TEXT("卡牌区域 [%.0f, %.0f] | 配置牌堆 [%.0f, %.0f] (蓝) | 动态牌堆 [%.0f, %.0f] (红)"), 
			CardAreaStartX, CardYPosition, DeckPilePositionX, DeckPilePositionY, DeckPilePos.X, DeckPilePos.Y);
		
		FVector2D TextPosition(CardAreaStartX, CardYPosition - 40.0f);
		FVector2D TextSize(CardAreaWidth, 20.0f);
		
		FPaintGeometry TextGeometry = AllottedGeometry.ToPaintGeometry(
			FVector2f(TextSize),
			FSlateLayoutTransform(FVector2f(TextPosition))
		);
		
		FSlateDrawElement::MakeText(
			OutDrawElements,
			MaxLayerId + 4,
			TextGeometry,
			DebugText,
			FCoreStyle::GetDefaultFontStyle("Regular", 10),
			ESlateDrawEffect::None,
			DebugBoxColor.ToFColor(true)
		);
		
		MaxLayerId += 4;
	}
	
	return MaxLayerId;
}

/**
 * @brief 请求跳过行动
 */
void USG_CardHandWidget::RequestSkip()
{
	// 检查是否可以交互
	if (!bCanInteract)
	{
		UE_LOG(LogSGUI, Warning, TEXT("⚠️ 开局展开动画中，无法跳过行动"));
		return;
	}
	
	if (!DeckComponent)
	{
		return;
	}

	FGuid PreviousSelectedId = DeckComponent->GetSelectedCardId();
	bool bHadSelection = PreviousSelectedId.IsValid();
	
	if (bHadSelection)
	{
		UE_LOG(LogSGUI, Log, TEXT("跳过行动前有选中的卡牌，将自动取消选中"));
	}
	
	if (DeckComponent->SkipAction())
	{
		if (bHadSelection)
		{
			UE_LOG(LogSGUI, Log, TEXT("✓ 已取消选中的卡牌并跳过行动"));
		}
		
		HandleSkipRequested();
	}
}

// ========== ❌ 删除 - 删除第1次定义的 RefreshCardsArea（这个版本有问题）==========
// 从这里删除到第247行之前的 RefreshCardsArea 函数

// ========== 卡牌布局和动画 ==========

/**
 * @brief 🔧 完全修复 - 刷新 Canvas Panel 中的卡牌（唯一正确的版本）
 */
void USG_CardHandWidget::RefreshCardsArea()
{
	if (!CardsArea || !HandViewModel || !CardEntryWidgetClass)
	{
		UE_LOG(LogSGUI, Error, TEXT("RefreshCardsArea 失败：必要组件为空"));
		return;
	}
	
	// 检查 CardsArea 是否已完成布局
	FGeometry CardsAreaGeometry = CardsArea->GetCachedGeometry();
	FVector2D LocalSize = CardsAreaGeometry.GetLocalSize();
	
	if (LocalSize.X <= 0.0f || LocalSize.Y <= 0.0f)
	{
		UE_LOG(LogSGUI, Warning, TEXT("⚠️ CardsArea 尺寸为 0，延迟到下一帧初始化"));
		UE_LOG(LogSGUI, Warning, TEXT("   当前尺寸：[%.2f, %.2f]"), LocalSize.X, LocalSize.Y);
		
		// 标记需要刷新
		bPendingRefresh = true;
		return;
	}
	
	UE_LOG(LogSGUI, Log, TEXT("========== 刷新 CardsArea（初始化）=========="));
	
	TArray<USGCardViewModel*> CardVMs = HandViewModel->GetCardViewModels();
	
	UE_LOG(LogSGUI, Log, TEXT("  CardViewModels 数量：%d"), CardVMs.Num());
	
	// 清理旧的定时器
	if (UWorld* World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		if (TimerManager.IsTimerActive(OpeningDelayTimerHandle))
		{
			UE_LOG(LogSGUI, Warning, TEXT("  ⚠️ 清除旧的开局展开定时器"));
			TimerManager.ClearTimer(OpeningDelayTimerHandle);
		}
	}
	
	// 重置开局动画状态
	bIsPlayingOpeningAnimation = false;
	CurrentFlyOutCardIndex = 0;
	NextCardFlyOutTime = 0.0f;
	bCanInteract = false;
	
	CardsArea->ClearChildren();
	CardLayouts.Empty();
	
	// 配置的牌堆位置（开局时所有卡牌堆叠的位置）
	UE_LOG(LogSGUI, Log, TEXT("  🎯 配置的牌堆位置（开局）：[%.2f, %.2f]"), DeckPilePositionX, DeckPilePositionY);
	
	// 所有卡牌完全堆叠在配置的牌堆位置
	for (int32 i = 0; i < CardVMs.Num(); ++i)
	{
		USGCardViewModel* CardVM = CardVMs[i];
		
		if (!CardVM)
		{
			UE_LOG(LogSGUI, Warning, TEXT("  ⚠️ CardViewModel[%d] 为空，跳过"), i);
			continue;
		}
		
		// 创建卡牌 Widget
		USG_CardEntryWidget* CardEntry = CreateWidget<USG_CardEntryWidget>(
			this, 
			CardEntryWidgetClass
		);
		
		if (!CardEntry)
		{
			UE_LOG(LogSGUI, Error, TEXT("  ❌ 创建 CardEntry Widget 失败"));
			continue;
		}
		
		// 设置卡牌数据
		CardEntry->SetupCardEntry(CardVM, DeckComponent);
		
		// 绑定卡牌使用事件
		CardVM->OnCardUsedNotification.AddDynamic(this, &USG_CardHandWidget::OnCardUsed);
		
		// 添加到 Canvas
		UCanvasPanelSlot* CanvasSlot = CardsArea->AddChildToCanvas(CardEntry);
		
		if (CanvasSlot)
		{
			CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
			CanvasSlot->SetAlignment(FVector2D(0.0f, 0.0f));
			CanvasSlot->SetSize(FVector2D(CardWidth, CardHeight));
			
			// 🔧 完全修复 - 使用配置的牌堆位置
			CanvasSlot->SetPosition(FVector2D(DeckPilePositionX, DeckPilePositionY));
			
			// 所有卡牌相同的 Z 层级（完全重叠）
			CanvasSlot->SetZOrder(0);
			
			UE_LOG(LogSGUI, Log, TEXT("  ✓ [%d] 设置卡牌位置：[%.2f, %.2f], Z=0"), 
				i, DeckPilePositionX, DeckPilePositionY);
		}
		
		// 所有卡牌初始旋转为 0
		CardEntry->SetRenderTransformAngle(0.0f);
		
		// 强制刷新 Widget 布局
		CardEntry->ForceLayoutPrepass();
		
		// 创建布局信息
		FSGCardLayoutInfo LayoutInfo;
		LayoutInfo.CardWidget = CardEntry;
		LayoutInfo.CardViewModel = CardVM;
		LayoutInfo.TargetPositionX = 0.0f;
		
		// 初始位置使用配置的牌堆位置
		LayoutInfo.CurrentPositionX = DeckPilePositionX;
		
		LayoutInfo.TargetOffsetY = 0.0f;
		LayoutInfo.CurrentOffsetY = 0.0f;
		LayoutInfo.TargetRotation = 0.0f;
		LayoutInfo.CurrentRotation = 0.0f;
		LayoutInfo.TargetZOrder = i;
		LayoutInfo.bIsNewCard = false;
		LayoutInfo.PushInProgress = 1.0f;
		LayoutInfo.bIsPlayingRemoveAnimation = false;
		LayoutInfo.RemoveAnimationProgress = 0.0f;
		
		// 初始化开局飞出动画相关
		LayoutInfo.bIsPlayingOpeningFlyOut = false;
		LayoutInfo.OpeningFlyOutProgress = 0.0f;
		LayoutInfo.FlyOutIndex = i;
		
		CardLayouts.Add(LayoutInfo);
	}
	
	// 立即应用初始位置（确保所有卡牌立即显示在牌堆位置）
	for (FSGCardLayoutInfo& LayoutInfo : CardLayouts)
	{
		if (LayoutInfo.CardWidget)
		{
			if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(LayoutInfo.CardWidget->Slot))
			{
				CanvasSlot->SetPosition(FVector2D(DeckPilePositionX, DeckPilePositionY));
				LayoutInfo.CardWidget->SetRenderTransformAngle(0.0f);
				CanvasSlot->SetZOrder(0);
			}
		}
	}
	
	UE_LOG(LogSGUI, Log, TEXT("  ✓ 所有卡牌已堆叠在牌堆位置 [%.2f, %.2f]"), DeckPilePositionX, DeckPilePositionY);
	
	// 启动开局动画
	if (bEnableOpeningAnimation)
	{
		StartOpeningAnimation();
	}
	else
	{
		// 立即计算并应用布局（无动画）
		CalculateCardLayout();
		
		for (FSGCardLayoutInfo& LayoutInfo : CardLayouts)
		{
			LayoutInfo.CurrentPositionX = LayoutInfo.TargetPositionX;
			LayoutInfo.CurrentOffsetY = LayoutInfo.TargetOffsetY;
			LayoutInfo.CurrentRotation = LayoutInfo.TargetRotation;
			ApplyCardPosition(LayoutInfo);
		}
		
		bCanInteract = true;
	}
	
	UE_LOG(LogSGUI, Log, TEXT("✓ CardsArea 刷新完成"));
	UE_LOG(LogSGUI, Log, TEXT("========================================"));
}

/**
 * @brief 计算开局飞出动画的目标位置
 */
void USG_CardHandWidget::CalculateOpeningFlyOutTarget(
	int32 FlyOutIndex, 
	int32 TotalCards,
	float& OutTargetX,
	float& OutTargetOffsetY,
	float& OutTargetRotation
) const
{
	// 计算间距
	float AvailableSpace = CardAreaWidth - CardWidth;
	float Spacing = (TotalCards > 1) ? (AvailableSpace / (TotalCards - 1)) : 0.0f;
	
	// 检查是否需要重叠
	float EffectiveSpacing = Spacing;
	if (Spacing < MinCardSpacing && TotalCards > 1)
	{
		EffectiveSpacing = AvailableSpace / (TotalCards - 1);
	}
	
	// 计算 X 坐标（从左到右排列）
	OutTargetX = CardAreaStartX + FlyOutIndex * EffectiveSpacing;
	
	// 计算弧形效果
	if (bEnableArcLayout && TotalCards > 1)
	{
		// 归一化位置（0.0 ~ 1.0）
		float NormalizedPos = (float)FlyOutIndex / (TotalCards - 1);
		
		// 计算距离中心的偏移（-1.0 ~ 1.0，中间为 0）
		float CenterOffset = (NormalizedPos * 2.0f) - 1.0f;
		
		// 使用曲线计算弧形
		float ArcFactor = FMath::Pow(FMath::Abs(CenterOffset), ArcCurvePower);
		
		// 计算 Y 偏移（边缘卡牌向下）
		OutTargetOffsetY = ArcFactor * ArcMaxYOffset;
		
		// 计算旋转角度（边缘卡牌旋转）
		OutTargetRotation = CenterOffset * ArcMaxRotation;
	}
	else
	{
		OutTargetOffsetY = 0.0f;
		OutTargetRotation = 0.0f;
	}
}

/**
 * @brief 获取当前牌堆的动态位置
 */
FVector2D USG_CardHandWidget::GetCurrentDeckPilePosition() const
{
	// 统计已完成飞出的卡牌数量
	int32 FlewOutCount = 0;
	
	for (int32 i = 0; i < CardLayouts.Num(); ++i)
	{
		const FSGCardLayoutInfo& LayoutInfo = CardLayouts[i];
		
		// 如果卡牌已完成飞出（进度 >= 1.0）
		if (!LayoutInfo.bIsPlayingOpeningFlyOut && LayoutInfo.OpeningFlyOutProgress >= 1.0f)
		{
			FlewOutCount++;
		}
	}
	
	// 如果没有卡牌飞出，返回配置的牌堆位置
	if (FlewOutCount == 0)
	{
		return FVector2D(DeckPilePositionX, DeckPilePositionY);
	}
	
	// 如果所有卡牌都飞出了，返回配置的牌堆位置
	if (FlewOutCount >= CardLayouts.Num())
	{
		return FVector2D(DeckPilePositionX, DeckPilePositionY);
	}
	
	// 牌堆位置 = 下一张要飞出的卡牌的目标位置
	int32 NextCardIndex = FlewOutCount;
	
	// 计算下一张卡牌的目标位置
	float NextCardTargetX = 0.0f;
	float DummyOffsetY = 0.0f;
	float DummyRotation = 0.0f;
	
	CalculateOpeningFlyOutTarget(
		NextCardIndex, 
		CardLayouts.Num(),
		NextCardTargetX,
		DummyOffsetY,
		DummyRotation
	);
	
	// 牌堆位置 = 下一张卡牌的目标位置
	return FVector2D(NextCardTargetX, DeckPilePositionY);
}

/**
 * @brief 获取当前牌堆的弧形旋转信息
 */
void USG_CardHandWidget::GetCurrentDeckPileArcInfo(float& OutOffsetY, float& OutRotation) const
{
	// 统计已完成飞出的卡牌数量
	int32 FlewOutCount = 0;
	
	for (int32 i = 0; i < CardLayouts.Num(); ++i)
	{
		const FSGCardLayoutInfo& LayoutInfo = CardLayouts[i];
		
		// 如果卡牌已完成飞出（进度 >= 1.0）
		if (!LayoutInfo.bIsPlayingOpeningFlyOut && LayoutInfo.OpeningFlyOutProgress >= 1.0f)
		{
			FlewOutCount++;
		}
	}
	
	// 如果没有卡牌飞出，牌堆没有弧形旋转
	if (FlewOutCount == 0)
	{
		OutOffsetY = 0.0f;
		OutRotation = 0.0f;
		return;
	}
	
	// 如果所有卡牌都飞出了，牌堆没有弧形旋转
	if (FlewOutCount >= CardLayouts.Num())
	{
		OutOffsetY = 0.0f;
		OutRotation = 0.0f;
		return;
	}
	
	// 牌堆的弧形位置 = 下一张要飞出的卡牌的弧形位置
	int32 NextCardIndex = FlewOutCount;
	
	// 计算下一张卡牌的弧形信息
	float DummyX = 0.0f;
	CalculateOpeningFlyOutTarget(
		NextCardIndex,
		CardLayouts.Num(),
		DummyX,
		OutOffsetY,
		OutRotation
	);
}

/**
 * @brief 开始开局展开动画
 */
void USG_CardHandWidget::StartOpeningAnimation()
{
	UE_LOG(LogSGUI, Log, TEXT("========== 开始开局展开动画 =========="));
	UE_LOG(LogSGUI, Log, TEXT("  延迟时间：%.2f 秒"), OpeningDelayTime);
	UE_LOG(LogSGUI, Log, TEXT("  单张卡牌飞出时长：%.2f 秒"), CardFlyOutDuration);
	UE_LOG(LogSGUI, Log, TEXT("  卡牌飞出间隔：%.2f 秒"), CardFlyOutInterval);
	
	// 禁用交互
	bCanInteract = false;
	
	// 标记正在播放开局动画（但还未开始飞出）
	bIsPlayingOpeningAnimation = false;
	
	// 重置飞出状态
	CurrentFlyOutCardIndex = 0;
	NextCardFlyOutTime = 0.0f;
	
	// 检查延迟时间
	if (OpeningDelayTime <= 0.0f)
	{
		UE_LOG(LogSGUI, Warning, TEXT("  ⚠️ 延迟时间 <= 0，立即开始飞出"));
		OnOpeningDelayCompleted();
	}
	else
	{
		// 设置延迟定时器
		UWorld* World = GetWorld();
		if (!World)
		{
			UE_LOG(LogSGUI, Error, TEXT("  ❌ 获取 World 失败，无法设置定时器"));
			OnOpeningDelayCompleted();
			return;
		}
		
		FTimerManager& TimerManager = World->GetTimerManager();
		
		// 清除可能存在的旧定时器
		if (TimerManager.IsTimerActive(OpeningDelayTimerHandle))
		{
			UE_LOG(LogSGUI, Warning, TEXT("  ⚠️ 旧定时器仍然活跃，先清除"));
			TimerManager.ClearTimer(OpeningDelayTimerHandle);
		}
		
		// 设置新的延迟定时器
		TimerManager.SetTimer(
			OpeningDelayTimerHandle,
			this,
			&USG_CardHandWidget::OnOpeningDelayCompleted,
			OpeningDelayTime,
			false
		);
		
		// 验证定时器是否设置成功
		if (TimerManager.IsTimerActive(OpeningDelayTimerHandle))
		{
			float RemainingTime = TimerManager.GetTimerRemaining(OpeningDelayTimerHandle);
			UE_LOG(LogSGUI, Log, TEXT("  ✓ 延迟定时器设置成功，剩余时间：%.2f 秒"), RemainingTime);
		}
		else
		{
			UE_LOG(LogSGUI, Error, TEXT("  ❌ 延迟定时器设置失败"));
			OnOpeningDelayCompleted();
		}
	}
	
	UE_LOG(LogSGUI, Log, TEXT("  ✓ 卡牌已完全堆叠在牌堆，等待飞出..."));
	UE_LOG(LogSGUI, Log, TEXT("========================================"));
}

/**
 * @brief 开局展开延迟完成回调
 */
void USG_CardHandWidget::OnOpeningDelayCompleted()
{
	UE_LOG(LogSGUI, Log, TEXT("========== 开局展开延迟完成，开始飞出卡牌 =========="));
	
	// 标记正在播放开局飞出动画
	bIsPlayingOpeningAnimation = true;
	
	// 播放展开音效
	PlaySound2D(CardOpeningSound);
	
	// 通知蓝图
	HandleOpeningAnimationStarted();
	
	// 立即开始第一张卡牌的飞出
	if (CardLayouts.Num() > 0)
	{
		StartCardFlyOut(0);
	}
	else
	{
		UE_LOG(LogSGUI, Warning, TEXT("  ⚠️ 没有卡牌需要飞出"));
		bIsPlayingOpeningAnimation = false;
		bCanInteract = true;
		HandleOpeningAnimationCompleted();
	}
	
	UE_LOG(LogSGUI, Log, TEXT("========================================"));
}

/**
 * @brief 开始单张卡牌的飞出动画
 */
void USG_CardHandWidget::StartCardFlyOut(int32 CardIndex)
{
	// 检查索引有效性
	if (!CardLayouts.IsValidIndex(CardIndex))
	{
		UE_LOG(LogSGUI, Warning, TEXT("  ⚠️ 卡牌索引 %d 无效"), CardIndex);
		return;
	}
	
	// 获取卡牌布局信息
	FSGCardLayoutInfo& LayoutInfo = CardLayouts[CardIndex];
	
	// 标记正在播放飞出动画
	LayoutInfo.bIsPlayingOpeningFlyOut = true;
	LayoutInfo.OpeningFlyOutProgress = 0.0f;
	
	// 计算目标位置
	CalculateOpeningFlyOutTarget(
		CardIndex, 
		CardLayouts.Num(),
		LayoutInfo.TargetPositionX,
		LayoutInfo.TargetOffsetY,
		LayoutInfo.TargetRotation
	);
	
	// 设置目标 Z 层级
	LayoutInfo.TargetZOrder = CardIndex;
	
	// 播放单张卡牌飞出音效
	PlaySound2D(CardFlyOutSound);
	
	if (bEnablePushAnimationDebug)
	{
		UE_LOG(LogSGUI, Log, TEXT("  🚀 开始飞出卡牌 [%d]：%s → 目标位置 [%.2f, %.2f], Z层级：%d"), 
			CardIndex, 
			*LayoutInfo.CardViewModel->CardName.ToString(),
			LayoutInfo.TargetPositionX,
			LayoutInfo.TargetOffsetY,
			LayoutInfo.TargetZOrder);
	}
}

/**
 * @brief 计算卡牌布局（包含弧形排列）
 */
void USG_CardHandWidget::CalculateCardLayout()
{
	int32 CardCount = CardLayouts.Num();
	
	if (CardCount == 0)
	{
		return;
	}
	
	float StartX = CardAreaStartX;
	
	if (CardCount == 1)
	{
		// 只有一张卡牌，居中放置
		CardLayouts[0].TargetPositionX = StartX + (CardAreaWidth - CardWidth) / 2.0f;
		CardLayouts[0].TargetOffsetY = 0.0f;
		CardLayouts[0].TargetRotation = 0.0f;
		CardLayouts[0].TargetZOrder = 0;
	}
	else
	{
		// 多张卡牌，计算间距
		float AvailableSpace = CardAreaWidth - CardWidth;
		float Spacing = AvailableSpace / (CardCount - 1);
		
		// 检查是否需要重叠
		float EffectiveSpacing = Spacing;
		if (Spacing < MinCardSpacing)
		{
			float EffectiveCardWidth = AvailableSpace / (CardCount - 1);
			EffectiveSpacing = EffectiveCardWidth;
		}
		
		// 计算每张卡牌的位置
		for (int32 i = 0; i < CardCount; ++i)
		{
			// 计算 X 坐标
			float PositionX = StartX + i * EffectiveSpacing;
			CardLayouts[i].TargetPositionX = PositionX;
			
			// 计算弧形效果
			if (bEnableArcLayout)
			{
				// 归一化位置（0.0 ~ 1.0）
				float NormalizedPos = (float)i / (CardCount - 1);
				
				// 计算距离中心的偏移（-1.0 ~ 1.0，中间为 0）
				float CenterOffset = (NormalizedPos * 2.0f) - 1.0f;
				
				// 使用曲线计算弧形
				float ArcFactor = FMath::Pow(FMath::Abs(CenterOffset), ArcCurvePower);
				
				// 计算 Y 偏移（边缘卡牌向下）
				CardLayouts[i].TargetOffsetY = ArcFactor * ArcMaxYOffset;
				
				// 计算旋转角度（边缘卡牌旋转）
				CardLayouts[i].TargetRotation = CenterOffset * ArcMaxRotation;
			}
			else
			{
				CardLayouts[i].TargetOffsetY = 0.0f;
				CardLayouts[i].TargetRotation = 0.0f;
			}
			
			// Z 层级从左到右递增
			CardLayouts[i].TargetZOrder = i;
		}
	}
}

/**
 * @brief 更新卡牌位置和动画
 */
void USG_CardHandWidget::UpdateCardPositions(float DeltaTime)
{
	// 如果还在等待刷新，不更新位置
	if (bPendingRefresh)
	{
		return;
	}
	
	// ========== 处理开局飞出动画 ==========
	
	if (bIsPlayingOpeningAnimation)
	{
		// 统计已飞出的卡牌数量
		int32 FlewOutCount = 0;
		for (int32 i = 0; i < CardLayouts.Num(); ++i)
		{
			const FSGCardLayoutInfo& LayoutInfo = CardLayouts[i];
			if (!LayoutInfo.bIsPlayingOpeningFlyOut && LayoutInfo.OpeningFlyOutProgress >= 1.0f)
			{
				FlewOutCount++;
			}
		}
		
		// 计算牌堆目标位置（下一张要飞出的卡牌的目标位置）
		float DeckPileTargetX = DeckPilePositionX;
		float DeckPileTargetOffsetY = 0.0f;
		float DeckPileTargetRotation = 0.0f;
		
		if (FlewOutCount > 0 && FlewOutCount < CardLayouts.Num())
		{
			int32 NextCardIndex = FlewOutCount;
			
			CalculateOpeningFlyOutTarget(
				NextCardIndex,
				CardLayouts.Num(),
				DeckPileTargetX,
				DeckPileTargetOffsetY,
				DeckPileTargetRotation
			);
		}
		
		// 输出牌堆位置变化
		static float LastLoggedDeckPileX = -1.0f;
		static float LastLoggedOffsetY = -1.0f;
		static float LastLoggedRotation = -1.0f;
		
		if (FMath::Abs(DeckPileTargetX - LastLoggedDeckPileX) > 5.0f || 
		    FMath::Abs(DeckPileTargetOffsetY - LastLoggedOffsetY) > 1.0f ||
		    FMath::Abs(DeckPileTargetRotation - LastLoggedRotation) > 1.0f)
		{
			UE_LOG(LogSGUI, Log, TEXT("  🎯 牌堆目标位置：[%.2f, %.2f], Y偏移=%.2f, 旋转=%.2f°（已飞出=%d张）"), 
				DeckPileTargetX, DeckPilePositionY + DeckPileTargetOffsetY, 
				DeckPileTargetOffsetY, DeckPileTargetRotation, FlewOutCount);
			
			LastLoggedDeckPileX = DeckPileTargetX;
			LastLoggedOffsetY = DeckPileTargetOffsetY;
			LastLoggedRotation = DeckPileTargetRotation;
		}
		
		// 检查是否所有卡牌都飞出完成
		bool bAllCardsFlewOut = true;
		
		for (int32 i = 0; i < CardLayouts.Num(); ++i)
		{
			FSGCardLayoutInfo& LayoutInfo = CardLayouts[i];
			
			if (!LayoutInfo.CardWidget)
			{
				continue;
			}
			
			// ========== 1. 处理正在飞出的卡牌 ==========
			if (LayoutInfo.bIsPlayingOpeningFlyOut)
			{
				// 更新飞出动画进度
				LayoutInfo.OpeningFlyOutProgress += DeltaTime / CardFlyOutDuration;
				LayoutInfo.OpeningFlyOutProgress = FMath::Clamp(LayoutInfo.OpeningFlyOutProgress, 0.0f, 1.0f);
				
				// 检查单张卡牌飞出是否完成
				if (LayoutInfo.OpeningFlyOutProgress >= 1.0f)
				{
					LayoutInfo.bIsPlayingOpeningFlyOut = false;
					LayoutInfo.OpeningFlyOutProgress = 1.0f;
					
					// 重新计算所有已飞出卡牌的目标位置
					for (int32 j = 0; j <= i; ++j)
					{
						if (CardLayouts.IsValidIndex(j))
						{
							FSGCardLayoutInfo& PreviousCard = CardLayouts[j];
							CalculateOpeningFlyOutTarget(
								j,
								CardLayouts.Num(),
								PreviousCard.TargetPositionX,
								PreviousCard.TargetOffsetY,
								PreviousCard.TargetRotation
							);
						}
					}
					
					if (bEnablePushAnimationDebug)
					{
						UE_LOG(LogSGUI, Log, TEXT("  ✓ 卡牌飞出完成 [%d]：%s"), 
							i, *LayoutInfo.CardViewModel->CardName.ToString());
					}
				}
				else
				{
					bAllCardsFlewOut = false;
				}
				
				// 使用曲线计算平滑插值
				float Alpha = GetCurveValue(OpeningAnimationCurve, LayoutInfo.OpeningFlyOutProgress);
				
				// 从当前牌堆位置飞出
				float StartX = DeckPileTargetX;
				float StartOffsetY = DeckPileTargetOffsetY;
				float StartRotation = DeckPileTargetRotation;
				
				// 如果是第一张卡牌，起始位置是配置的牌堆位置
				if (i == 0)
				{
					StartX = DeckPilePositionX;
					StartOffsetY = 0.0f;
					StartRotation = 0.0f;
				}
				
				// 从起始位置插值到目标位置
				LayoutInfo.CurrentPositionX = FMath::Lerp(StartX, LayoutInfo.TargetPositionX, Alpha);
				LayoutInfo.CurrentOffsetY = FMath::Lerp(StartOffsetY, LayoutInfo.TargetOffsetY, Alpha);
				LayoutInfo.CurrentRotation = FMath::Lerp(StartRotation, LayoutInfo.TargetRotation, Alpha);
				
				// Z 层级随着飞出进度逐渐分离
				if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(LayoutInfo.CardWidget->Slot))
				{
					int32 CurrentZOrder = FMath::RoundToInt(FMath::Lerp(0.0f, (float)LayoutInfo.TargetZOrder, Alpha));
					CanvasSlot->SetZOrder(CurrentZOrder);
					CanvasSlot->SetPosition(FVector2D(LayoutInfo.CurrentPositionX, DeckPilePositionY + LayoutInfo.CurrentOffsetY));
					LayoutInfo.CardWidget->SetRenderTransformAngle(LayoutInfo.CurrentRotation);
				}
			}
			// ========== 2. 处理已飞出的卡牌（被推动） ==========
			else if (LayoutInfo.OpeningFlyOutProgress >= 1.0f)
			{
				// 平滑移动到新的目标位置
				LayoutInfo.CurrentPositionX = FMath::FInterpTo(
					LayoutInfo.CurrentPositionX,
					LayoutInfo.TargetPositionX,
					DeltaTime,
					PositionInterpSpeed
				);
				
				LayoutInfo.CurrentOffsetY = FMath::FInterpTo(
					LayoutInfo.CurrentOffsetY,
					LayoutInfo.TargetOffsetY,
					DeltaTime,
					PositionInterpSpeed
				);
				
				LayoutInfo.CurrentRotation = FMath::FInterpTo(
					LayoutInfo.CurrentRotation,
					LayoutInfo.TargetRotation,
					DeltaTime,
					RotationInterpSpeed
				);
				
				if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(LayoutInfo.CardWidget->Slot))
				{
					CanvasSlot->SetPosition(FVector2D(LayoutInfo.CurrentPositionX, DeckPilePositionY + LayoutInfo.CurrentOffsetY));
					LayoutInfo.CardWidget->SetRenderTransformAngle(LayoutInfo.CurrentRotation);
					CanvasSlot->SetZOrder(LayoutInfo.TargetZOrder);
				}
			}
			// ========== 3. 处理未飞出的卡牌（牌堆） ==========
			else
			{
				// 所有未飞出的卡牌都移动到牌堆目标位置
				LayoutInfo.CurrentPositionX = FMath::FInterpTo(
					LayoutInfo.CurrentPositionX,
					DeckPileTargetX,
					DeltaTime,
					PositionInterpSpeed * 2.0f
				);
				
				// 牌堆也有弧形旋转
				LayoutInfo.CurrentOffsetY = FMath::FInterpTo(
					LayoutInfo.CurrentOffsetY,
					DeckPileTargetOffsetY,
					DeltaTime,
					PositionInterpSpeed * 2.0f
				);
				
				LayoutInfo.CurrentRotation = FMath::FInterpTo(
					LayoutInfo.CurrentRotation,
					DeckPileTargetRotation,
					DeltaTime,
					RotationInterpSpeed * 2.0f
				);
				
				if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(LayoutInfo.CardWidget->Slot))
				{
					CanvasSlot->SetPosition(FVector2D(LayoutInfo.CurrentPositionX, DeckPilePositionY + LayoutInfo.CurrentOffsetY));
					LayoutInfo.CardWidget->SetRenderTransformAngle(LayoutInfo.CurrentRotation);
					CanvasSlot->SetZOrder(0);
				}
				
				bAllCardsFlewOut = false;
			}
		}
		
		// 处理依次飞出逻辑
		if (CurrentFlyOutCardIndex < CardLayouts.Num())
		{
			NextCardFlyOutTime += DeltaTime;
			
			if (NextCardFlyOutTime >= CardFlyOutInterval)
			{
				StartCardFlyOut(CurrentFlyOutCardIndex);
				NextCardFlyOutTime = 0.0f;
				CurrentFlyOutCardIndex++;
			}
		}
		
		// 检查是否所有卡牌都飞出完成
		if (bAllCardsFlewOut && CurrentFlyOutCardIndex >= CardLayouts.Num())
		{
			bIsPlayingOpeningAnimation = false;
			bCanInteract = true;
			
			UE_LOG(LogSGUI, Log, TEXT("✓ 开局飞出动画完成，启用交互"));
			HandleOpeningAnimationCompleted();
		}
		
		return;
	}
	
	// ========== 正常的动画处理（非开局） ==========
	
	for (int32 i = CardLayouts.Num() - 1; i >= 0; --i)
	{
		FSGCardLayoutInfo& LayoutInfo = CardLayouts[i];
		
		if (!LayoutInfo.CardWidget)
		{
			continue;
		}
		
		// 处理移除动画
		if (LayoutInfo.bIsPlayingRemoveAnimation)
		{
			LayoutInfo.RemoveAnimationProgress += DeltaTime / RemoveAnimationDuration;
			
			if (LayoutInfo.RemoveAnimationProgress >= 1.0f)
			{
				RemoveCardWidget(LayoutInfo.CardWidget);
				continue;
			}
			
			float Alpha = GetCurveValue(RemoveAnimationCurve, LayoutInfo.RemoveAnimationProgress);
			float AnimatedY = CardYPosition + LayoutInfo.CurrentOffsetY + RemoveAnimationYOffset * Alpha;
			float Opacity = 1.0f - Alpha;
			
			if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(LayoutInfo.CardWidget->Slot))
			{
				CanvasSlot->SetPosition(FVector2D(LayoutInfo.CurrentPositionX, AnimatedY));
				LayoutInfo.CardWidget->SetRenderOpacity(Opacity);
			}
			
			continue;
		}
		
		// 处理推入动画
		if (LayoutInfo.bIsNewCard)
		{
			LayoutInfo.PushInProgress += DeltaTime / PushInAnimationDuration;
			
			if (LayoutInfo.PushInProgress >= 1.0f)
			{
				LayoutInfo.bIsNewCard = false;
				LayoutInfo.PushInProgress = 1.0f;
			}
			
			float Alpha = GetCurveValue(PushInAnimationCurve, LayoutInfo.PushInProgress);
			float StartX = CardAreaStartX + CardAreaWidth - CardWidth;
			
			LayoutInfo.CurrentPositionX = FMath::Lerp(StartX, LayoutInfo.TargetPositionX, Alpha);
			
			LayoutInfo.CurrentOffsetY = FMath::FInterpTo(
				LayoutInfo.CurrentOffsetY,
				LayoutInfo.TargetOffsetY,
				DeltaTime,
				PositionInterpSpeed
			);
			
			LayoutInfo.CurrentRotation = FMath::FInterpTo(
				LayoutInfo.CurrentRotation,
				LayoutInfo.TargetRotation,
				DeltaTime,
				RotationInterpSpeed
			);
		}
		else
		{
			float Distance = FMath::Abs(LayoutInfo.CurrentPositionX - LayoutInfo.TargetPositionX);
			
			if (Distance < 1.0f)
			{
				LayoutInfo.CurrentPositionX = LayoutInfo.TargetPositionX;
			}
			else
			{
				LayoutInfo.CurrentPositionX = FMath::FInterpTo(
					LayoutInfo.CurrentPositionX,
					LayoutInfo.TargetPositionX,
					DeltaTime,
					PositionInterpSpeed
				);
			}
			
			LayoutInfo.CurrentOffsetY = FMath::FInterpTo(
				LayoutInfo.CurrentOffsetY,
				LayoutInfo.TargetOffsetY,
				DeltaTime,
				PositionInterpSpeed
			);
			
			LayoutInfo.CurrentRotation = FMath::FInterpTo(
				LayoutInfo.CurrentRotation,
				LayoutInfo.TargetRotation,
				DeltaTime,
				RotationInterpSpeed
			);
		}
		
		ApplyCardPosition(LayoutInfo);
	}
}

/**
 * @brief 应用卡牌位置到 Widget
 */
void USG_CardHandWidget::ApplyCardPosition(FSGCardLayoutInfo& LayoutInfo)
{
	if (!LayoutInfo.CardWidget)
	{
		return;
	}
	
	UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(LayoutInfo.CardWidget->Slot);
	if (!CanvasSlot)
	{
		return;
	}
	
	// 只在非开局飞出动画时使用此函数
	if (!bIsPlayingOpeningAnimation)
	{
		// 计算最终 Y 坐标
		float FinalY = CardYPosition + LayoutInfo.CurrentOffsetY;
		
		// 应用位置
		CanvasSlot->SetPosition(FVector2D(LayoutInfo.CurrentPositionX, FinalY));
		
		// 应用旋转
		LayoutInfo.CardWidget->SetRenderTransformAngle(LayoutInfo.CurrentRotation);
		
		// 检查是否选中，设置Z层级
		int32 FinalZOrder = LayoutInfo.TargetZOrder;
		if (LayoutInfo.CardViewModel && LayoutInfo.CardViewModel->bIsSelected)
		{
			FinalZOrder = 9999;
		}
		
		CanvasSlot->SetZOrder(FinalZOrder);
	}
}

/**
 * @brief 添加新卡牌
 */
void USG_CardHandWidget::AddNewCardWithPushAnimation(USGCardViewModel* CardVM)
{
	if (!CardVM || !CardsArea || !CardEntryWidgetClass)
	{
		return;
	}
	
	USG_CardEntryWidget* CardEntry = CreateWidget<USG_CardEntryWidget>(this, CardEntryWidgetClass);
	
	if (!CardEntry)
	{
		return;
	}
	
	CardEntry->SetupCardEntry(CardVM, DeckComponent);
	CardVM->OnCardUsedNotification.AddDynamic(this, &USG_CardHandWidget::OnCardUsed);
	
	UCanvasPanelSlot* CanvasSlot = CardsArea->AddChildToCanvas(CardEntry);
	
	float InitialX = CardAreaStartX + CardAreaWidth - CardWidth;
	
	int32 NewCardIndex = CardLayouts.Num();
	int32 TotalCardCount = CardLayouts.Num() + 1;
	
	float InitialOffsetY = 0.0f;
	float InitialRotation = 0.0f;
	
	if (bEnableArcLayout && TotalCardCount > 1)
	{
		float NormalizedPos = (float)NewCardIndex / (TotalCardCount - 1);
		float CenterOffset = (NormalizedPos * 2.0f) - 1.0f;
		float ArcFactor = FMath::Pow(FMath::Abs(CenterOffset), ArcCurvePower);
		InitialOffsetY = ArcFactor * ArcMaxYOffset;
		InitialRotation = CenterOffset * ArcMaxRotation;
	}
	
	if (CanvasSlot)
	{
		CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
		CanvasSlot->SetAlignment(FVector2D(0.0f, 0.0f));
		CanvasSlot->SetSize(FVector2D(CardWidth, CardHeight));
		CanvasSlot->SetPosition(FVector2D(InitialX, CardYPosition + InitialOffsetY));
		CanvasSlot->SetZOrder(9999);
	}
	
	CardEntry->SetRenderTransformAngle(InitialRotation);
	
	FSGCardLayoutInfo LayoutInfo;
	LayoutInfo.CardWidget = CardEntry;
	LayoutInfo.CardViewModel = CardVM;
	LayoutInfo.TargetPositionX = 0.0f;
	LayoutInfo.CurrentPositionX = InitialX;
	LayoutInfo.TargetOffsetY = 0.0f;
	LayoutInfo.CurrentOffsetY = InitialOffsetY;
	LayoutInfo.TargetRotation = 0.0f;
	LayoutInfo.CurrentRotation = InitialRotation;
	LayoutInfo.TargetZOrder = CardLayouts.Num();
	LayoutInfo.bIsNewCard = true;
	LayoutInfo.PushInProgress = 0.0f;
	LayoutInfo.bIsPlayingRemoveAnimation = false;
	LayoutInfo.RemoveAnimationProgress = 0.0f;
	LayoutInfo.bIsPlayingOpeningFlyOut = false;
	LayoutInfo.OpeningFlyOutProgress = 1.0f;
	LayoutInfo.FlyOutIndex = CardLayouts.Num();
	
	CardLayouts.Add(LayoutInfo);
	
	PlaySound2D(CardDrawSound);
}

/**
 * @brief 从曲线获取插值值
 */
float USG_CardHandWidget::GetCurveValue(UCurveFloat* Curve, float Progress, float DefaultPower) const
{
	Progress = FMath::Clamp(Progress, 0.0f, 1.0f);
	
	if (Curve)
	{
		return Curve->GetFloatValue(Progress);
	}
	
	return 1.0f - FMath::Pow(1.0f - Progress, DefaultPower);
}

/**
 * @brief 处理卡牌使用事件
 */
void USG_CardHandWidget::OnCardUsed(USGCardViewModel* UsedCard)
{
	if (!UsedCard)
	{
		return;
	}
	
	UE_LOG(LogSGUI, Log, TEXT("OnCardUsed - 卡牌：%s"), *UsedCard->CardName.ToString());
	
	PlaySound2D(CardUseSound);
	
	for (FSGCardLayoutInfo& LayoutInfo : CardLayouts)
	{
		if (LayoutInfo.CardViewModel == UsedCard)
		{
			LayoutInfo.bIsPlayingRemoveAnimation = true;
			LayoutInfo.RemoveAnimationProgress = 0.0f;
			
			UE_LOG(LogSGUI, Log, TEXT("  ✓ 开始播放卡牌移除动画"));
			return;
		}
	}
	
	UE_LOG(LogSGUI, Warning, TEXT("  ⚠️ 未找到对应的卡牌 Widget"));
}

/**
 * @brief 播放2D音效
 */
void USG_CardHandWidget::PlaySound2D(USoundBase* Sound)
{
	if (!Sound)
	{
		return;
	}
	
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	
	UGameplayStatics::PlaySound2D(
		World,
		Sound,
		AudioVolume,
		AudioPitch,
		0.0f,
		nullptr,
		nullptr,
		true
	);
}

/**
 * @brief 移除已使用的卡牌 Widget
 */
void USG_CardHandWidget::RemoveCardWidget(USG_CardEntryWidget* CardWidget)
{
	if (!CardWidget)
	{
		return;
	}
	
	UE_LOG(LogSGUI, Log, TEXT("RemoveCardWidget - 移除卡牌 Widget"));
	
	if (CardsArea)
	{
		CardsArea->RemoveChild(CardWidget);
	}
	
	CardLayouts.RemoveAll([CardWidget](const FSGCardLayoutInfo& Info) {
		return Info.CardWidget == CardWidget;
	});
	
	CalculateCardLayout();
	
	UE_LOG(LogSGUI, Log, TEXT("  ✓ 卡牌 Widget 已移除，剩余：%d"), CardLayouts.Num());
}