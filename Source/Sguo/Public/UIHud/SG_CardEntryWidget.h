	// Copyright notice placeholder
/**
 * @file SG_CardEntryWidget.h
 * @brief 手牌卡片入口 Widget 声明
 * @details
 * 功能说明：
 * - 负责在列表中展示单张卡牌并处理点击选中事件。
 * 详细流程：
 * - 绑定 `USGCardViewModel` 与 `USG_CardDeckComponent`。
 * - 点击后调用组件进行卡牌选中。
 * 注意事项：
 * - 视觉效果建议在蓝图派生类中实现。
 */
#pragma once

// 引入核心头文件
#include "CoreMinimal.h"
// 引入基础控件
#include "Blueprint/UserWidget.h"
// 引入生成宏
#include "SG_CardEntryWidget.generated.h"

// 前向声明
class USGCardViewModel;
class USG_CardDeckComponent;

/**
 * @brief 单张卡牌按钮
 */
UCLASS(BlueprintType, Blueprintable)
class SGUO_API USG_CardEntryWidget : public UUserWidget
{
public:
	// 生成类宏
	
	GENERATED_BODY()

public:
	/** @brief 设置 ViewModel 与卡组组件 */
	UFUNCTION(BlueprintCallable, Category = "Card")
	void SetupCardEntry(USGCardViewModel* InViewModel, USG_CardDeckComponent* InDeckComponent);

	/** @brief 通知控件卡牌被点击（供蓝图按钮调用） */
	UFUNCTION(BlueprintCallable, Category = "Card")
	void NotifyCardClicked();

protected:
	/** @brief 构建时回调 */
	virtual void NativeConstruct() override;

	/** @brief 销毁时回调 */
	virtual void NativeDestruct() override;

protected:
	// 蓝图事件：当选中状态改变时调用
	UFUNCTION(BlueprintImplementableEvent, Category = "Card")
	void OnSelectionStateChanged(bool bIsSelected);

	// 蓝图事件：当 ViewModel 设置完成时调用
	UFUNCTION(BlueprintImplementableEvent, Category = "Card")
	void OnViewModelSet(USGCardViewModel* ViewModel);

private:
	// ViewModel 选中状态改变回调
	UFUNCTION()
	void HandleViewModelSelectionChanged(USGCardViewModel* ViewModel, bool bIsSelected);
	
protected:
	// 绑定的 ViewModel
	UPROPERTY(BlueprintReadOnly, Category = "Card", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USGCardViewModel> BoundViewModel;

	// 绑定的卡组组件
	UPROPERTY(BlueprintReadOnly, Category = "Card", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USG_CardDeckComponent> DeckComponent;
};

