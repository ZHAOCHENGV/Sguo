// Copyright notice placeholder
/**
 * @file SG_CardHandViewModel.h
 * @brief 手牌 MVVM ViewModel 声明
 * @details
 * 功能说明：
 * - 聚合手牌列表、冷却状态等数据供 UI 绑定。
 * 详细流程：
 * - 订阅卡组组件事件并同步内部 `CardViewModels`。
 * 注意事项：
 * - 初始化时需传入 `USG_CardDeckComponent`。
 */
#pragma once

// 引入核心头文件
#include "CoreMinimal.h"
// 引入 MVVM ViewModel 基类
#include "MVVMViewModelBase.h"
// 引入卡牌运行时类型
#include "SG_CardViewModel.h"
#include "CardsAndUnits/SG_CardRuntimeTypes.h"
// 引入头文件生成宏
#include "SG_CardHandViewModel.generated.h"

// 前向声明卡组组件
class USG_CardDeckComponent;
// 前向声明卡牌 ViewModel
class USGCardViewModel;

/**
 * @brief 手牌 ViewModel
 */
UCLASS(BlueprintType)
class SGUO_API USGCardHandViewModel : public UMVVMViewModelBase
{
public:
	// 生成类宏
	GENERATED_BODY()

public:
	/** @brief 初始化 ViewModel */
	void Initialize(USG_CardDeckComponent* InDeckComponent);

	/** @brief 清理订阅 */
	virtual void BeginDestroy() override;
	
	/** @brief 获取卡牌 ViewModel 列表 */
	UFUNCTION(BlueprintCallable, Category = "Card")
	TArray<USGCardViewModel*> GetCardViewModels() const; 
protected:
	// 处理手牌更新
	UFUNCTION()
	void HandleHandChanged(const TArray<FSGCardInstance>& NewHand);

	// 处理选中变化
	UFUNCTION()
	void HandleSelectionChanged(const FGuid& SelectedId);

	// 处理行动状态变化
	UFUNCTION()
	void HandleActionStateChanged(bool bCanAct, float CooldownRemaining);
	

	


protected:
	// 卡牌 ViewModel 列表
	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "Card")
	TArray<TObjectPtr<USGCardViewModel>> CardViewModels;

	// 行动是否可用
	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "Card")
	bool bCanAct = true;

	// 冷却剩余时间
	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "Card")
	float Cooldown = 0.0f;

protected:
	// 被观察的卡组组件
	UPROPERTY(Transient)
	TObjectPtr<USG_CardDeckComponent> ObservedDeck;
};

