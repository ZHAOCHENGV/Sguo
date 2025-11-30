// 📄 文件：Source/Sguo/Public/Strategies/SG_FireArrowEffect.h
// 🔧 修改 - 修复所有编译错误，使用基类状态枚举

#pragma once

#include "CoreMinimal.h"
#include "Strategies/SG_StrategyEffectBase.h"
#include "SG_FireArrowEffect.generated.h"

class USG_FireArrowCardData;
class ASG_StationaryUnit;
class UDecalComponent;

/**
 * @brief 火矢计效果 Actor
 * @details
 * 功能说明：
 * - 继承自 ASG_StrategyEffectBase
 * - 重写目标选择和执行相关函数
 * - 自己负责预览显示和射击逻辑
 */
UCLASS(BlueprintType, Blueprintable)
class SGUO_API ASG_FireArrowEffect : public ASG_StrategyEffectBase
{
	GENERATED_BODY()

public:
	ASG_FireArrowEffect();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	// ========== 重写基类接口 ==========
	
	virtual void InitializeEffect(
		USG_StrategyCardData* InCardData,
		AActor* InEffectInstigator,
		const FVector& InTargetLocation
	) override;

	virtual bool CanExecute_Implementation() const override;
	virtual FText GetCannotExecuteReason_Implementation() const override;
	virtual bool StartTargetSelection_Implementation() override;
	virtual void UpdateTargetLocation_Implementation(const FVector& NewLocation) override;
	virtual bool ConfirmTarget_Implementation() override;
	virtual void CancelEffect_Implementation() override;
	virtual void InterruptEffect_Implementation() override;
	virtual void ExecuteEffect_Implementation() override;

	// ========== 火矢计特有接口 ==========
	
	/**
	 * @brief 获取参与的弓手数量
	 */
	UFUNCTION(BlueprintPure, Category = "Fire Arrow Effect", meta = (DisplayName = "获取弓手数量"))
	int32 GetArcherCount() const { return ParticipatingArchers.Num(); }

protected:
	// ========== 火矢计特有逻辑 ==========
	
	void FindParticipatingArchers();
	void ExecuteFireRound();
	void FireArrowsFromArcher(ASG_StationaryUnit* Archer, int32 ArrowCount);
	FVector GenerateRandomTargetInArea() const;
	void CreatePreviewDecal();
	void UpdatePreviewDecal();
	void HidePreviewDecal();
	void NotifyArchersStartFireArrow();
	void NotifyArchersEndFireArrow();

	UFUNCTION()
	void OnFireTimerTick();

	UFUNCTION()
	void OnSkillDurationEnd();

protected:
	// ========== 组件 ==========
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "预览贴花"))
	TObjectPtr<UDecalComponent> PreviewDecal;

	// ========== 运行时数据 ==========
	
	UPROPERTY(BlueprintReadOnly, Category = "Fire Arrow Effect", meta = (DisplayName = "火矢计数据"))
	TObjectPtr<USG_FireArrowCardData> FireArrowCardData;

	// 🔧 修改 - 不暴露给蓝图，TWeakObjectPtr 数组不支持
	TArray<TWeakObjectPtr<ASG_StationaryUnit>> ParticipatingArchers;

	UPROPERTY(BlueprintReadOnly, Category = "Fire Arrow Effect", meta = (DisplayName = "已发射轮数"))
	int32 FiredRounds = 0;

	// ✨ 新增 - 技能开始时间
	float SkillStartTime = 0.0f;

	FTimerHandle FireTimerHandle;
	FTimerHandle DurationTimerHandle;
	bool bPreviewVisible = false;

public:
	// ========== 蓝图事件 ==========
	
	UFUNCTION(BlueprintImplementableEvent, Category = "Fire Arrow Effect", meta = (DisplayName = "On Fire Round Started"))
	void K2_OnFireRoundStarted(int32 RoundIndex, int32 ArcherCount);

	// 🔧 修改 - 重命名参数避免与基类冲突
	UFUNCTION(BlueprintImplementableEvent, Category = "Fire Arrow Effect", meta = (DisplayName = "On Arrow Fired"))
	void K2_OnArrowFired(AActor* Archer, FVector ArrowTargetLocation);

	UFUNCTION(BlueprintImplementableEvent, Category = "Fire Arrow Effect", meta = (DisplayName = "On Fire Arrow Completed"))
	void K2_OnFireArrowCompleted(int32 TotalRounds);
};
