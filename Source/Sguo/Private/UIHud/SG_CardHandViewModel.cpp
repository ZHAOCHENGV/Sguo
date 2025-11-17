// 🔧 MODIFIED FILE - 手牌 ViewModel 实现
// Copyright notice placeholder
/**
 * @file SG_CardHandViewModel.cpp
 * @brief 手牌 ViewModel 实现
 */
#include "UIHud/SG_CardHandViewModel.h"
// 引入卡组组件
#include "CardsAndUnits/SG_CardDeckComponent.h"
// 引入卡牌 ViewModel
#include "UIHud/SG_CardViewModel.h"
// ✨ NEW - 引入日志系统
#include "Debug/SG_LogCategories.h"

// 初始化 ViewModel
void USGCardHandViewModel::Initialize(USG_CardDeckComponent* InDeckComponent)
{
	// 🔧 MODIFIED - 使用新的日志类别
	UE_LOG(LogSGUI, Log, TEXT("CardHandViewModel::Initialize 被调用"));
	
	// 保存被观察的组件
	ObservedDeck = InDeckComponent;
	
	// 若组件无效则返回
	if (!ObservedDeck)
	{
		// 🔧 MODIFIED - 使用新的日志类别
		UE_LOG(LogSGUI, Error, TEXT("❌ ObservedDeck 为空"));
		return;
	}
	
	// 🔧 MODIFIED - 使用新的日志类别
	UE_LOG(LogSGUI, Log, TEXT("绑定事件委托..."));
	
	// 绑定手牌更新事件
	ObservedDeck->OnHandChanged.AddDynamic(this, &USGCardHandViewModel::HandleHandChanged);
	// 绑定选中变化事件
	ObservedDeck->OnSelectionChanged.AddDynamic(this, &USGCardHandViewModel::HandleSelectionChanged);
	// 绑定行动状态事件
	ObservedDeck->OnActionStateChanged.AddDynamic(this, &USGCardHandViewModel::HandleActionStateChanged);
	
	// 🔧 MODIFIED - 使用新的日志类别
	UE_LOG(LogSGUI, Log, TEXT("初始化当前状态（手牌数：%d）..."), ObservedDeck->GetHand().Num());
	
	// 初始化当前手牌
	HandleHandChanged(ObservedDeck->GetHand());
	// 初始化选中状态
	HandleSelectionChanged(ObservedDeck->GetSelectedCardId());
	// 初始化行动状态
	HandleActionStateChanged(ObservedDeck->CanAct(), ObservedDeck->GetCooldownRemaining());
	
	// 🔧 MODIFIED - 使用新的日志类别
	UE_LOG(LogSGUI, Log, TEXT("✓ CardHandViewModel 初始化完成"));

}

// 析构前解绑
void USGCardHandViewModel::BeginDestroy()
{
	// 若存在组件则解绑委托
	if (ObservedDeck)
	{
		// 解绑手牌更新
		ObservedDeck->OnHandChanged.RemoveDynamic(this, &USGCardHandViewModel::HandleHandChanged);
		// 解绑选中变化
		ObservedDeck->OnSelectionChanged.RemoveDynamic(this, &USGCardHandViewModel::HandleSelectionChanged);
		// 解绑行动状态
		ObservedDeck->OnActionStateChanged.RemoveDynamic(this, &USGCardHandViewModel::HandleActionStateChanged);
	}
	
	// 调用父类析构
	Super::BeginDestroy();
}

// 处理手牌更新
void USGCardHandViewModel::HandleHandChanged(const TArray<FSGCardInstance>& NewHand)
{
	/*// 🔧 MODIFIED - 使用新的日志类别
	UE_LOG(LogSGUI, Log, TEXT("HandleHandChanged - 新手牌数：%d"), NewHand.Num());
	
	// 构建新旧映射，用于复用已有的 ViewModel
	TMap<FGuid, int32> OldIndexMap;
	for (int32 i = 0; i < CardViewModels.Num(); ++i)
	{
		if (CardViewModels[i])
		{
			OldIndexMap.Add(CardViewModels[i]->InstanceId, i);
		}
	}
	
	// 创建临时数组
	TArray<TObjectPtr<USGCardViewModel>> NewViewModels;
	NewViewModels.Reserve(NewHand.Num()); // 预分配空间
	
	// 遍历新手牌
	for (const FSGCardInstance& Instance : NewHand)
	{
		// 检查卡牌数据有效性
		if (!Instance.CardData)
		{
			// 🔧 MODIFIED - 使用新的日志类别
			UE_LOG(LogSGUI, Warning, TEXT("⚠️ 卡牌实例的 CardData 为空，跳过"));
			continue;
		}
		
		// ✨ NEW - 尝试复用已有的 ViewModel
		USGCardViewModel* ViewModel = nullptr;
		if (int32* OldIndex = OldIndexMap.Find(Instance.InstanceId))
		{
			// 复用已有的 ViewModel
			ViewModel = CardViewModels[*OldIndex].Get();
			
			// 🔧 MODIFIED - 使用新的日志类别
			UE_LOG(LogSGUI, Verbose, TEXT("  ♻️ 复用 ViewModel - 名称: %s, ID: %s"), 
				*ViewModel->CardName.ToString(), 
				*ViewModel->InstanceId.ToString());
		}
		else
		{
			// 新建视图模型
			ViewModel = NewObject<USGCardViewModel>(this);
			if (!ViewModel)
			{
				// 🔧 MODIFIED - 使用新的日志类别
				UE_LOG(LogSGUI, Error, TEXT("❌ 创建 ViewModel 失败"));
				continue;
			}
			
			// 初始化视图模型
			ViewModel->InitializeFromInstance(Instance, false, ObservedDeck ? ObservedDeck->CanAct() : true);
			
			// 🔧 MODIFIED - 使用新的日志类别
			UE_LOG(LogSGUI, Verbose, TEXT("  ✓ 创建 ViewModel - 名称: %s, ID: %s"), 
				*ViewModel->CardName.ToString(), 
				*ViewModel->InstanceId.ToString());
		}
		
		// 保存到临时数组
		NewViewModels.Add(ViewModel);
	}
	
	// 使用 MVVM 宏更新属性，这会触发 FieldNotify
	UE_MVVM_SET_PROPERTY_VALUE(CardViewModels, NewViewModels);
	
	// 🔧 MODIFIED - 使用新的日志类别
	UE_LOG(LogSGUI, Log, TEXT("✓ CardViewModels 已更新，数量：%d"), CardViewModels.Num());
	
	// 重新应用选中状态
	if (ObservedDeck)
	{
		HandleSelectionChanged(ObservedDeck->GetSelectedCardId());
	}*/
	 // 输出日志
    UE_LOG(LogSGUI, Log, TEXT("HandleHandChanged - 新手牌数：%d"), NewHand.Num());
    
    // 🔧 MODIFIED - 找出被移除的卡牌
    TSet<FGuid> NewHandIds;
    for (const FSGCardInstance& Instance : NewHand)
    {
        NewHandIds.Add(Instance.InstanceId);
    }
    
    // 遍历旧的 ViewModel，通知不在新手牌中的卡
    for (USGCardViewModel* OldVM : CardViewModels)
    {
        if (OldVM && !NewHandIds.Contains(OldVM->InstanceId))
        {
            // 通知卡牌被使用
            OldVM->NotifyCardUsed();
            // 输出日志
            UE_LOG(LogSGUI, Log, TEXT("  📢 通知卡牌被使用：%s"), *OldVM->CardName.ToString());
        }
    }
    
    // 构建新旧映射，用于复用已有的 ViewModel
    TMap<FGuid, int32> OldIndexMap;
    for (int32 i = 0; i < CardViewModels.Num(); ++i)
    {
        if (CardViewModels[i])
        {
            OldIndexMap.Add(CardViewModels[i]->InstanceId, i);
        }
    }
    
    // 创建临时数组
    TArray<TObjectPtr<USGCardViewModel>> NewViewModels;
    NewViewModels.Reserve(NewHand.Num());
    
    // 遍历新手牌
    for (const FSGCardInstance& Instance : NewHand)
    {
        // 检查卡牌数据有效性
        if (!Instance.CardData)
        {
            UE_LOG(LogSGUI, Warning, TEXT("⚠️ 卡牌实例的 CardData 为空，跳过"));
            continue;
        }
        
        // 尝试复用已有的 ViewModel
        USGCardViewModel* ViewModel = nullptr;
        if (int32* OldIndex = OldIndexMap.Find(Instance.InstanceId))
        {
            // 复用已有的 ViewModel
            ViewModel = CardViewModels[*OldIndex].Get();
            UE_LOG(LogSGUI, Verbose, TEXT("  ♻️ 复用 ViewModel - 名称: %s"), 
                *ViewModel->CardName.ToString());
        }
        else
        {
            // 新建视图模型
            ViewModel = NewObject<USGCardViewModel>(this);
            if (!ViewModel)
            {
                UE_LOG(LogSGUI, Error, TEXT("❌ 创建 ViewModel 失败"));
                continue;
            }
            
            // 初始化视图模型
            ViewModel->InitializeFromInstance(Instance, false, 
                ObservedDeck ? ObservedDeck->CanAct() : true);
            
            UE_LOG(LogSGUI, Verbose, TEXT("  ✓ 创建 ViewModel - 名称: %s"), 
                *ViewModel->CardName.ToString());
        }
        
        // 保存到临时数组
        NewViewModels.Add(ViewModel);
    }
    
    // 使用 MVVM 宏更新属性
    UE_MVVM_SET_PROPERTY_VALUE(CardViewModels, NewViewModels);
    
    // 输出日志
    UE_LOG(LogSGUI, Log, TEXT("✓ CardViewModels 已更新，数量：%d"), CardViewModels.Num());
    
    // 重新应用选中状态
    if (ObservedDeck)
    {
        HandleSelectionChanged(ObservedDeck->GetSelectedCardId());
    }	
	// 🔧 MODIFIED - 使用新的日志类别
	UE_LOG(LogSGUI, Log, TEXT("✓ HandChanged 处理完成"));
}

// 处理选中变化
void USGCardHandViewModel::HandleSelectionChanged(const FGuid& SelectedId)
{
	// 遍历所有视图模型
	for (USGCardViewModel* ViewModel : CardViewModels)
	{
		// 若为空则跳过
		if (!ViewModel)
		{
			continue;
		}
		// 更新选中状态
		ViewModel->SetSelected(ViewModel->InstanceId == SelectedId);
	}
}

// 处理行动状态
void USGCardHandViewModel::HandleActionStateChanged(bool bCanActValue, float CooldownRemaining)
{
	// 更新行动可用性
	// 更新并广播行动可用性
	UE_MVVM_SET_PROPERTY_VALUE(bCanAct, bCanActValue);
	// 更新冷却时间
	UE_MVVM_SET_PROPERTY_VALUE(Cooldown, CooldownRemaining);
	// 同步每张视图模型的可用性
	for (USGCardViewModel* ViewModel : CardViewModels)
	{
		// 跳过空指针
		if (!ViewModel)
		{
			continue;
		}
		// 设置可用标记
		ViewModel->SetPlayable(bCanActValue);
	}
}

TArray<USGCardViewModel*> USGCardHandViewModel::GetCardViewModels() const
{
	// 需要转换 TObjectPtr 数组为原始指针数组
	// 由于 UFUNCTION 限制，我们需要使用一个临时变量
	static TArray<USGCardViewModel*> TempArray;
	TempArray.Reset();
	for (const TObjectPtr<USGCardViewModel>& VM : CardViewModels)
	{
		TempArray.Add(VM.Get());
	}
	return TempArray;
}


