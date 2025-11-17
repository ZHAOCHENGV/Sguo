// 🔧 MODIFIED FILE - 单张卡牌 ViewModel 实现
// Copyright notice placeholder
/**
 * @file SG_CardViewModel.cpp
 * @brief 单张卡牌 ViewModel 实现
 */
#include "UIHud/SG_CardViewModel.h"
// 引入卡牌数据基类
#include "Data/SG_CardDataBase.h"
// ✨ NEW - 引入日志系统
#include "Debug/SG_LogCategories.h"

// 初始化 ViewModel
void USGCardViewModel::InitializeFromInstance(const FSGCardInstance& Instance, bool bInIsSelected, bool bInPlayable)
{
	// 保存卡牌数据引用
	CardData = Instance.CardData;
	// 设置实例 ID
	UE_MVVM_SET_PROPERTY_VALUE(InstanceId, Instance.InstanceId);
	// 设置卡牌名称
	UE_MVVM_SET_PROPERTY_VALUE(CardName, Instance.CardData ? Instance.CardData->CardName : FText::GetEmpty());
	// 设置卡牌描述
	UE_MVVM_SET_PROPERTY_VALUE(CardDescription, Instance.CardData ? Instance.CardData->CardDescription : FText::GetEmpty());
	// 设置卡牌图标
	UE_MVVM_SET_PROPERTY_VALUE(CardIcon, Instance.CardData ? Instance.CardData->CardIcon : nullptr);
	// 更新选中状态
	UE_MVVM_SET_PROPERTY_VALUE(bIsSelected, bInIsSelected);
	// 更新可用状态
	UE_MVVM_SET_PROPERTY_VALUE(bIsPlayable, bInPlayable);
	// 记录是否唯一
	UE_MVVM_SET_PROPERTY_VALUE(bIsUnique, Instance.bIsUnique);
}

// 设置选中状态
void USGCardViewModel::SetSelected(bool bInSelected)
{
	if (bIsSelected != bInSelected)
	{
		// 🔧 MODIFIED - 使用新的日志类别
		UE_LOG(LogSGUI, Verbose, TEXT("SetSelected - 卡牌: %s, 从 %s 变为 %s"), 
			*CardName.ToString(),
			bIsSelected ? TEXT("选中") : TEXT("未选中"),
			bInSelected ? TEXT("选中") : TEXT("未选中"));
		
		UE_MVVM_SET_PROPERTY_VALUE(bIsSelected, bInSelected);
		
		// 广播选中状态改变事件
		OnSelectionChanged.Broadcast(this, bInSelected);
	}
}

// 设置可用状态
void USGCardViewModel::SetPlayable(bool bInPlayable)
{
	if (bIsPlayable != bInPlayable)
	{
		UE_MVVM_SET_PROPERTY_VALUE(bIsPlayable, bInPlayable);
	}
}

/**
 * @brief 通知卡牌被使用
 * @details
 * 功能说明：
 * - 广播卡牌使用事件
 * - 供 HandViewModel 调用
 */
void USGCardViewModel::NotifyCardUsed()
{
	// 输出日志
	UE_LOG(LogSGUI, Log, TEXT("📢 通知卡牌被使用：%s"), *CardName.ToString());
    
	// 广播事件
	OnCardUsedNotification.Broadcast(this);
}