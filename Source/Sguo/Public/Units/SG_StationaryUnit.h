// 📄 文件：Source/Sguo/Public/Units/SG_StationaryUnit.h
// 🔧 修改 - 添加火矢计相关功能

#pragma once

#include "CoreMinimal.h"
#include "Units/SG_UnitsBase.h"
#include "SG_StationaryUnit.generated.h"

class UAnimMontage;

/**
 * @brief 站桩单位类
 */
UCLASS(BlueprintType, Blueprintable)
class SGUO_API ASG_StationaryUnit : public ASG_UnitsBase
{
	GENERATED_BODY()

public:
	ASG_StationaryUnit();

protected:
	virtual void BeginPlay() override;

public:
	// ========== 站桩配置 ==========
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stationary Unit", 
		meta = (DisplayName = "启用浮空模式"))
	bool bEnableHover = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stationary Unit", 
		meta = (DisplayName = "浮空高度(厘米)", EditCondition = "bEnableHover", EditConditionHides, ClampMin = "-500.0", ClampMax = "1000.0"))
	float HoverHeight = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stationary Unit", 
		meta = (DisplayName = "禁用重力"))
	bool bDisableGravity = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stationary Unit", 
		meta = (DisplayName = "可被选为目标"))
	bool bCanBeTargeted = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stationary Unit", 
		meta = (DisplayName = "禁用移动"))
	bool bDisableMovement = true;

	// ========== ✨ 新增 - 火矢计配置 ==========
	
	/**
	 * @brief 火矢计攻击蒙太奇
	 * @details 火矢计发射时播放的动画
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stationary Unit|Fire Arrow", 
		meta = (DisplayName = "火矢攻击蒙太奇"))
	TObjectPtr<UAnimMontage> FireArrowMontage;

	/**
	 * @brief 火矢计投射物类
	 * @details 火矢计使用的投射物类，如果为空则使用默认投射物
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stationary Unit|Fire Arrow", 
		meta = (DisplayName = "火矢投射物类"))
	TSubclassOf<AActor> FireArrowProjectileClass;

	/**
	 * @brief 是否正在执行火矢技能
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Stationary Unit|Fire Arrow", 
		meta = (DisplayName = "正在执行火矢计"))
	bool bIsExecutingFireArrow = false;

	// ========== 查询接口 ==========
	
	virtual bool CanBeTargeted() const;

	UFUNCTION(BlueprintPure, Category = "Stationary Unit", meta = (DisplayName = "是否浮空"))
	bool IsHovering() const { return bEnableHover; }

	UFUNCTION(BlueprintPure, Category = "Stationary Unit", meta = (DisplayName = "获取浮空高度"))
	float GetHoverHeight() const { return HoverHeight; }

	// ========== ✨ 新增 - 火矢计接口 ==========
	
	/**
	 * @brief 开始火矢技能
	 * @details
	 * 功能说明：
	 * - 打断当前普通攻击
	 * - 设置火矢技能状态
	 * - 保存原始投射物类（如果有的话）
	 */
	UFUNCTION(BlueprintCallable, Category = "Stationary Unit|Fire Arrow", 
		meta = (DisplayName = "开始火矢技能"))
	void StartFireArrowSkill();

	/**
	 * @brief 结束火矢技能
	 * @details
	 * 功能说明：
	 * - 清除火矢技能状态
	 * - 恢复原始投射物类
	 */
	UFUNCTION(BlueprintCallable, Category = "Stationary Unit|Fire Arrow", 
		meta = (DisplayName = "结束火矢技能"))
	void EndFireArrowSkill();

	/**
	 * @brief 发射火矢
	 * @param TargetLocation 目标位置
	 * @param ProjectileClassOverride 投射物类覆盖（可选）
	 * @return 生成的投射物
	 * @details
	 * 功能说明：
	 * - 播放火矢攻击动画
	 * - 生成火矢投射物
	 */
	UFUNCTION(BlueprintCallable, Category = "Stationary Unit|Fire Arrow", 
		meta = (DisplayName = "发射火矢"))
	AActor* FireArrow(const FVector& TargetLocation, TSubclassOf<AActor> ProjectileClassOverride = nullptr);

	/**
	 * @brief 获取火矢投射物类
	 * @return 投射物类
	 */
	UFUNCTION(BlueprintPure, Category = "Stationary Unit|Fire Arrow", 
		meta = (DisplayName = "获取火矢投射物类"))
	TSubclassOf<AActor> GetFireArrowProjectileClass() const;

protected:
	void ApplyStationarySettings();
	void DisableMovementCapability();
	void ApplyHoverEffect();

	// ✨ 新增 - 缓存的原始投射物类
	UPROPERTY(Transient)
	TSubclassOf<AActor> CachedOriginalProjectileClass;
};
