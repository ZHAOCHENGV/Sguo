// 📄 文件：Source/Sguo/Public/Strategies/SG_StrategyEffectBase.h
// 🔧 修改 - 添加预览和交互接口，支持低耦合设计

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "SG_StrategyEffectBase.generated.h"

class USG_StrategyCardData;
class UAbilitySystemComponent;
class UGameplayEffect;

// ✨ 新增 - 效果状态枚举
/**
 * @brief 计谋效果状态枚举
 * @details 定义效果的生命周期状态
 */
UENUM(BlueprintType)
enum class ESGStrategyEffectState : uint8
{
	// 等待目标选择（显示预览中）
	WaitingForTarget    UMETA(DisplayName = "等待目标"),
	
	// 正在执行
	Executing           UMETA(DisplayName = "执行中"),
	
	// 已完成
	Completed           UMETA(DisplayName = "已完成"),
	
	// 被取消
	Cancelled           UMETA(DisplayName = "已取消"),
	
	// 被打断
	Interrupted         UMETA(DisplayName = "被打断")
};

// ✨ 新增 - 效果完成委托
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSGStrategyEffectFinished, ASG_StrategyEffectBase*, Effect, bool, bSuccess);

/**
 * @brief 计谋效果基类
 * @details
 * 功能说明：
 * - 所有计谋效果 Actor 的基类
 * - 提供通用的初始化、预览、确认、执行、清理接口
 * - 🔧 修改 - 效果类自己负责预览显示和交互逻辑
 * 详细流程：
 * 1. PlayerController 生成效果 Actor
 * 2. 调用 InitializeEffect 传入卡牌数据和施放者
 * 3. 对于需要选择目标的效果：
 *    - 调用 StartTargetSelection 开始目标选择
 *    - 每帧调用 UpdateTargetLocation 更新预览
 *    - 玩家确认时调用 ConfirmTarget
 *    - 玩家取消时调用 CancelEffect
 * 4. 对于全局效果：直接调用 ExecuteEffect
 * 5. 效果结束后广播 OnEffectFinished
 * 注意事项：
 * - 子类需要重写对应的虚函数
 * - 效果类自己管理预览显示
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class SGUO_API ASG_StrategyEffectBase : public AActor
{
	GENERATED_BODY()

public:
	ASG_StrategyEffectBase();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	// ========== 初始化接口 ==========
	
	/**
	 * @brief 初始化计谋效果
	 * @param InCardData 计谋卡数据
	 * @param InEffectInstigator 施放者
	 * @param InTargetLocation 初始目标位置
	 */
	UFUNCTION(BlueprintCallable, Category = "Strategy Effect")
	virtual void InitializeEffect(
		USG_StrategyCardData* InCardData,
		AActor* InEffectInstigator,
		const FVector& InTargetLocation
	);

	// ========== ✨ 新增 - 目标选择接口 ==========
	
	/**
	 * @brief 检查是否需要目标选择
	 * @return 是否需要选择目标
	 * @details
	 * 功能说明：
	 * - 根据放置类型判断是否需要玩家选择目标
	 * - Global 类型不需要，Area/Single 类型需要
	 * - 子类可以重写以实现特殊逻辑
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Strategy Effect", meta = (DisplayName = "是否需要目标选择"))
	bool RequiresTargetSelection() const;
	virtual bool RequiresTargetSelection_Implementation() const;

	/**
	 * @brief 检查是否可以执行
	 * @return 是否满足执行条件
	 * @details
	 * 功能说明：
	 * - 检查效果是否可以执行（如：是否有足够的弓手）
	 * - 子类应该重写此函数以实现特定检查
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Strategy Effect", meta = (DisplayName = "是否可执行"))
	bool CanExecute() const;
	virtual bool CanExecute_Implementation() const;

	/**
	 * @brief 获取不可执行的原因
	 * @return 原因文本
	 * @details
	 * 功能说明：
	 * - 当 CanExecute 返回 false 时，返回原因
	 * - 用于向玩家显示提示信息
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Strategy Effect", meta = (DisplayName = "获取不可执行原因"))
	FText GetCannotExecuteReason() const;
	virtual FText GetCannotExecuteReason_Implementation() const;

	/**
	 * @brief 开始目标选择
	 * @return 是否成功开始
	 * @details
	 * 功能说明：
	 * - 显示预览效果（如区域指示器）
	 * - 切换到目标选择状态
	 * - 子类应该重写以实现预览显示
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Strategy Effect", meta = (DisplayName = "开始目标选择"))
	bool StartTargetSelection();
	virtual bool StartTargetSelection_Implementation();

	/**
	 * @brief 更新目标位置
	 * @param NewLocation 新的目标位置
	 * @details
	 * 功能说明：
	 * - 每帧由 PlayerController 调用
	 * - 更新预览效果的位置
	 * - 子类应该重写以更新预览
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Strategy Effect", meta = (DisplayName = "更新目标位置"))
	void UpdateTargetLocation(const FVector& NewLocation);
	virtual void UpdateTargetLocation_Implementation(const FVector& NewLocation);

	/**
	 * @brief 确认目标
	 * @return 是否成功确认
	 * @details
	 * 功能说明：
	 * - 玩家点击确认时调用
	 * - 验证目标位置是否有效
	 * - 如果有效，开始执行效果
	 * - 子类可以重写以添加额外验证
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Strategy Effect", meta = (DisplayName = "确认目标"))
	bool ConfirmTarget();
	virtual bool ConfirmTarget_Implementation();

	/**
	 * @brief 取消效果
	 * @details
	 * 功能说明：
	 * - 玩家取消时调用
	 * - 隐藏预览并销毁效果 Actor
	 * - 广播取消事件
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Strategy Effect", meta = (DisplayName = "取消效果"))
	void CancelEffect();
	virtual void CancelEffect_Implementation();

	/**
	 * @brief 打断效果
	 * @details
	 * 功能说明：
	 * - 效果执行中被敌方打断时调用
	 * - 停止执行并清理
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Strategy Effect", meta = (DisplayName = "打断效果"))
	void InterruptEffect();
	virtual void InterruptEffect_Implementation();

	// ========== 执行接口 ==========
	
	/**
	 * @brief 执行计谋效果
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Strategy Effect")
	void ExecuteEffect();
	virtual void ExecuteEffect_Implementation();

	/**
	 * @brief 结束计谋效果
	 */
	UFUNCTION(BlueprintCallable, Category = "Strategy Effect")
	virtual void EndEffect();

	// ========== 状态查询 ==========
	
	/**
	 * @brief 获取当前状态
	 * @return 当前状态
	 */
	UFUNCTION(BlueprintPure, Category = "Strategy Effect", meta = (DisplayName = "获取状态"))
	ESGStrategyEffectState GetCurrentState() const { return CurrentState; }

	/**
	 * @brief 是否正在等待目标选择
	 */
	UFUNCTION(BlueprintPure, Category = "Strategy Effect", meta = (DisplayName = "是否等待目标"))
	bool IsWaitingForTarget() const { return CurrentState == ESGStrategyEffectState::WaitingForTarget; }

	/**
	 * @brief 是否正在执行
	 */
	UFUNCTION(BlueprintPure, Category = "Strategy Effect", meta = (DisplayName = "是否执行中"))
	bool IsExecuting() const { return CurrentState == ESGStrategyEffectState::Executing; }

public:
	// ========== 委托 ==========
	
	/**
	 * @brief 效果完成委托
	 * @details 效果执行完成、取消或被打断时广播
	 */
	UPROPERTY(BlueprintAssignable, Category = "Strategy Effect", meta = (DisplayName = "效果完成事件"))
	FSGStrategyEffectFinished OnEffectFinished;

protected:
	// ========== 辅助函数 ==========
	
	void GetAllUnitsOfFaction(FGameplayTag FactionTag, TArray<AActor*>& OutUnits);

	void GetUnitsInRadius(
		const FVector& Center,
		float Radius,
		FGameplayTag FactionTag,
		TArray<AActor*>& OutUnits
	);

	bool ApplyGameplayEffectToTarget(
		AActor* TargetActor,
		TSubclassOf<UGameplayEffect> EffectClass,
		float Level = 1.0f
	);

	UFUNCTION(BlueprintPure, Category = "Strategy Effect")
	FGameplayTag GetInstigatorFactionTag() const;

	UFUNCTION(BlueprintPure, Category = "Strategy Effect")
	AActor* GetEffectInstigator() const { return EffectInstigator; }

	// ✨ 新增 - 设置状态
	/**
	 * @brief 设置当前状态
	 * @param NewState 新状态
	 */
	void SetState(ESGStrategyEffectState NewState);

protected:
	// ========== 配置属性 ==========
	
	UPROPERTY(BlueprintReadOnly, Category = "Strategy Effect", meta = (DisplayName = "卡牌数据"))
	TObjectPtr<USG_StrategyCardData> CardData;

	UPROPERTY(BlueprintReadOnly, Category = "Strategy Effect", meta = (DisplayName = "效果施放者"))
	TObjectPtr<AActor> EffectInstigator;

	UPROPERTY(BlueprintReadOnly, Category = "Strategy Effect", meta = (DisplayName = "目标位置"))
	FVector TargetLocation;

	UPROPERTY(BlueprintReadOnly, Category = "Strategy Effect", meta = (DisplayName = "施放者阵营"))
	FGameplayTag InstigatorFactionTag;

	UPROPERTY(BlueprintReadOnly, Category = "Strategy Effect", meta = (DisplayName = "持续时间"))
	float EffectDuration;

	// ✨ 新增 - 当前状态
	UPROPERTY(BlueprintReadOnly, Category = "Strategy Effect", meta = (DisplayName = "当前状态"))
	ESGStrategyEffectState CurrentState = ESGStrategyEffectState::WaitingForTarget;

	// 🔧 修改 - 重命名为更清晰的名称
	UPROPERTY(BlueprintReadOnly, Category = "Strategy Effect", meta = (DisplayName = "是否已初始化"))
	bool bIsInitialized = false;
};