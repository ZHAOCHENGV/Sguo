// 🔧 MODIFIED FILE - 卡牌入口 Widget 实现
// Copyright notice placeholder
/**
 * @file SG_CardEntryWidget.cpp
 * @brief 手牌卡片入口 Widget 实现
 */
#include "UIHud/SG_CardEntryWidget.h"
// 引入卡牌 ViewModel
#include "UIHud/SG_CardViewModel.h"
// 引入卡组组件
#include "CardsAndUnits/SG_CardDeckComponent.h"
// ✨ NEW - 引入日志系统
#include "Debug/SG_LogCategories.h"

// 设置 ViewModel 与组件
void USG_CardEntryWidget::SetupCardEntry(USGCardViewModel* InViewModel, USG_CardDeckComponent* InDeckComponent)
{
	// 🔧 MODIFIED - 使用新的日志类别
	UE_LOG(LogSGUI, Verbose, TEXT("SetupCardEntry - 卡牌: %s"), 
		InViewModel ? *InViewModel->CardName.ToString() : TEXT("空"));
	
	// 如果之前有绑定的 ViewModel，先解绑
	if (BoundViewModel)
	{
		BoundViewModel->OnSelectionChanged.RemoveDynamic(this, &USG_CardEntryWidget::HandleViewModelSelectionChanged);
	}
	
	// 保存新的 ViewModel 引用
	BoundViewModel = InViewModel;
	DeckComponent = InDeckComponent;
	
	// 绑定新 ViewModel 的选中状态改变事件
	if (BoundViewModel)
	{
		BoundViewModel->OnSelectionChanged.AddDynamic(this, &USG_CardEntryWidget::HandleViewModelSelectionChanged);
		
		// 通知蓝图更新 UI
		OnViewModelSet(InViewModel);
		
		// 立即更新选中状态
		OnSelectionStateChanged(BoundViewModel->bIsSelected);
	}
}

// 初始化回调
void USG_CardEntryWidget::NativeConstruct()
{
	// 调用父类初始化
	Super::NativeConstruct();
}

// 销毁回调
void USG_CardEntryWidget::NativeDestruct()
{
	// 清理事件绑定
	if (BoundViewModel)
	{
		BoundViewModel->OnSelectionChanged.RemoveDynamic(this, &USG_CardEntryWidget::HandleViewModelSelectionChanged);
	}
	
	// 调用父类销毁
	Super::NativeDestruct();
}


void USG_CardEntryWidget::HandleViewModelSelectionChanged(USGCardViewModel* ViewModel, bool bIsSelected)
{
	// 记录选中状态变化
	UE_LOG(LogSGUI, Verbose, TEXT("HandleViewModelSelectionChanged - 卡牌: %s, 选中: %s"), 
		*ViewModel->CardName.ToString(),
		bIsSelected ? TEXT("是") : TEXT("否"));
	
	// 通知蓝图更新视觉效果
	OnSelectionStateChanged(bIsSelected);
}


/**
 * @brief 蓝图通知卡牌点击
 * @details 需要在蓝图中检查 CanInteract
 */
void USG_CardEntryWidget::NotifyCardClicked()
{
	
	// 🔧 MODIFIED - 使用新的日志类别
	UE_LOG(LogSGUI, Log, TEXT("NotifyCardClicked - 卡牌: %s"), 
		BoundViewModel ? *BoundViewModel->CardName.ToString() : TEXT("空"));
	
	// 检查组件和 ViewModel 有效性
	if (DeckComponent && BoundViewModel)
	{
		// 调用卡组组件选择卡牌
		DeckComponent->SelectCard(BoundViewModel->InstanceId);
	}
}

