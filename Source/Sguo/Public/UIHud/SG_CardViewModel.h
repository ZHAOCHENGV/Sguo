// Copyright notice placeholder
/**
 * @file SG_CardViewModel.h
 * @brief 单张卡牌的 MVVM ViewModel 声明
 * @details
 * 功能说明：
 * - 提供手牌卡片在 UI 层显示与交互所需的数据。
 * 详细流程：
 * - 通过 `InitializeFromInstance` 将运行时卡牌实例映射为展示字段。
 * - 属性支持 FieldNotify，方便 CommonUI 绑定自动刷新。
 * 注意事项：
 * - ViewModel 生命周期由手牌 ViewModel 统一管理。
 */
#pragma once

// 引入核心头文件
#include "CoreMinimal.h"
// 引入 MVVM ViewModel 基类
#include "MVVMViewModelBase.h"
// 引入运行时卡牌结构
#include "CardsAndUnits/SG_CardRuntimeTypes.h"
// 引入生成宏
#include "SG_CardViewModel.generated.h"


// 选中状态改变委托
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSGCardViewModelSelectionChangedSignature, USGCardViewModel*, ViewModel, bool, bIsSelected);
// ✨ NEW - 卡牌使用通知委托
/**
 * @brief 卡牌被使用时的通知委托
 * @details
 * 功能说明：
 * - 当卡牌被使用时触发
 * - 蓝图可以监听此事件播放动画
 * 使用场景：
 * - 播放卡牌使用动画
 * - 播放音效
 * - 延迟移除 Widget
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSGCardUsedNotification, USGCardViewModel*, UsedCard);
/**
 * @brief 单张卡牌 ViewModel
 */
UCLASS(BlueprintType)
class SGUO_API USGCardViewModel : public UMVVMViewModelBase
{
public:
	// 生成类宏
	GENERATED_BODY()

public:
	/**
	* @brief 卡牌被使用事件
	* @details 蓝图可以绑定此事件
	*/
	UPROPERTY(BlueprintAssignable, Category = "Card")
	FSGCardUsedNotification OnCardUsedNotification;
    
	/**
	 * @brief 通知卡牌被使用
	 * @details
	 * 功能说明：
	 * - 广播卡牌使用事件
	 * - 触发蓝图动画
	 */
	UFUNCTION(BlueprintCallable, Category = "Card")
	void NotifyCardUsed();
	
	/** @brief 初始化 ViewModel */
	void InitializeFromInstance(const FSGCardInstance& Instance, bool bInIsSelected, bool bInPlayable);

	/** @brief 设置选中状态 */
	void SetSelected(bool bInSelected);

	/** @brief 设置可用状态 */
	void SetPlayable(bool bInPlayable);

	/** @brief 获取卡牌数据 */
	USG_CardDataBase* GetCardData() const { return CardData; }

	/** @brief 检查是否为同一张卡牌 */
	bool IsSameCard(const FGuid& OtherInstanceId) const { return InstanceId == OtherInstanceId; }

	// 选中状态改变事件
	UPROPERTY(BlueprintAssignable, Category = "Card")
	FSGCardViewModelSelectionChangedSignature OnSelectionChanged;

public:
	// 卡牌实例 ID
	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "Card")
	FGuid InstanceId;

	// 卡牌名称
	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "Card")
	FText CardName;

	// 卡牌描述
	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "Card")
	FText CardDescription;

	// 卡牌图标
	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "Card")
	TObjectPtr<UTexture2D> CardIcon = nullptr;

	// 是否选中
	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "Card")
	bool bIsSelected = false;

	// 是否可用
	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "Card")
	bool bIsPlayable = true;

	// 是否唯一卡
	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "Card")
	bool bIsUnique = false;

protected:
	// 卡牌数据引用
	UPROPERTY(Transient)
	TObjectPtr<USG_CardDataBase> CardData = nullptr;
};

