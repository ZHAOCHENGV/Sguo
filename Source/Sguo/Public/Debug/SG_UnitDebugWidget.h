// ✨ 新增文件 - 单位属性调试 Widget
// Copyright notice placeholder
/**
 * @file SG_UnitDebugWidget.h
 * @brief 单位属性调试显示 Widget
 * @details
 * 功能说明：
 * - 在单位头顶显示血条和属性信息
 * - 实时更新属性变化
 * - 支持颜色区分阵营
 * 详细流程：
 * 1. 绑定到单位 Actor
 * 2. 每帧更新显示内容
 * 3. 根据阵营设置颜色
 * 注意事项：
 * - 仅用于调试，正式版本应禁用
 * - 性能开销较小（只更新可见单位）
 */

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SG_UnitDebugWidget.generated.h"

// 前向声明
class ASG_UnitsBase;
class UProgressBar;
class UTextBlock;

/**
 * @brief 单位属性调试 Widget
 * @details 显示单位的血条和详细属性信息
 */
UCLASS(BlueprintType, Blueprintable)
class SGUO_API USG_UnitDebugWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * @brief 绑定到单位
	 * @param InUnit 要显示属性的单位
	 * @details
	 * 功能说明：
	 * - 保存单位引用
	 * - 绑定属性变化事件
	 * - 初始化显示内容
	 */
	UFUNCTION(BlueprintCallable, Category = "Debug")
	void BindToUnit(ASG_UnitsBase* InUnit);

	/**
	 * @brief 更新显示内容
	 * @details
	 * 功能说明：
	 * - 读取单位当前属性
	 * - 更新血条和文本
	 * - 根据生命值百分比改变颜色
	 */
	UFUNCTION(BlueprintCallable, Category = "Debug")
	void UpdateDisplay();

protected:
	/**
	 * @brief Widget 构建时调用
	 */
	virtual void NativeConstruct() override;

	/**
	 * @brief 每帧更新
	 */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

protected:
	// ========== 显示配置 ==========
	
	/**
	 * @brief 是否显示详细属性
	 * @details True：显示所有属性；False：只显示血条
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Display", 
		meta = (DisplayName = "显示详细属性"))
	bool bShowDetailedStats = true;

	/**
	 * @brief 是否显示单位名称
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Display", 
		meta = (DisplayName = "显示单位名称"))
	bool bShowUnitName = true;

	/**
	 * @brief 是否显示阵营标签
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Display", 
		meta = (DisplayName = "显示阵营标签"))
	bool bShowFactionTag = true;

	/**
	 * @brief 更新频率（秒）
	 * @details 控制更新间隔，减少性能开销
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Display", 
		meta = (DisplayName = "更新频率(秒)", ClampMin = "0.0", UIMin = "0.0", UIMax = "1.0"))
	float UpdateInterval = 0.1f;

	/**
	 * @brief 玩家单位颜色
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Display", 
		meta = (DisplayName = "玩家单位颜色"))
	FLinearColor PlayerColor = FLinearColor::Blue;

	/**
	 * @brief 敌方单位颜色
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Display", 
		meta = (DisplayName = "敌方单位颜色"))
	FLinearColor EnemyColor = FLinearColor::Red;

	/**
	 * @brief 低血量颜色（<30%）
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Display", 
		meta = (DisplayName = "低血量颜色"))
	FLinearColor LowHealthColor = FLinearColor::Red;

	/**
	 * @brief 中血量颜色（30%-70%）
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Display", 
		meta = (DisplayName = "中血量颜色"))
	FLinearColor MidHealthColor = FLinearColor::Yellow;

	/**
	 * @brief 高血量颜色（>70%）
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Display", 
		meta = (DisplayName = "高血量颜色"))
	FLinearColor HighHealthColor = FLinearColor::Green;

	// ========== Widget 组件（在蓝图中绑定）==========
	
	/**
	 * @brief 血条进度条
	 */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UProgressBar* HealthBar;

	/**
	 * @brief 单位名称文本
	 */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UTextBlock* UnitNameText;

	/**
	 * @brief 生命值文本（当前/最大）
	 */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UTextBlock* HealthText;

	/**
	 * @brief 详细属性文本
	 */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UTextBlock* DetailedStatsText;

	// ========== 运行时数据 ==========
	
	/**
	 * @brief 绑定的单位引用
	 */
	UPROPERTY(Transient)
	TObjectPtr<ASG_UnitsBase> BoundUnit;

	/**
	 * @brief 上次更新时间
	 */
	float LastUpdateTime = 0.0f;

private:
	/**
	 * @brief 获取血条颜色（根据生命值百分比）
	 * @param HealthPercent 生命值百分比（0.0 ~ 1.0）
	 * @return 血条颜色
	 */
	FLinearColor GetHealthBarColor(float HealthPercent) const;

	/**
	 * @brief 格式化属性文本
	 * @return 格式化的属性字符串
	 */
	FString FormatStatsText() const;
};
