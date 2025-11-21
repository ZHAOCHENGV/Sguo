// 📄 SG_MainCityBase.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "SG_MainCityBase.generated.h"

// 前向声明
class USG_AbilitySystemComponent;
class USG_BuildingAttributeSet;
class UStaticMeshComponent;
class UBoxComponent;
struct FOnAttributeChangeData;

UCLASS(BlueprintType, Blueprintable)
class SGUO_API ASG_MainCityBase : public AActor, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ASG_MainCityBase();

	// ========== GAS 组件 ==========
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (DisplayName = "能力系统组件"))
	USG_AbilitySystemComponent* AbilitySystemComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (DisplayName = "建筑属性集"))
	USG_BuildingAttributeSet* AttributeSet;

	// ========== 组件 ==========
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "主城网格体"))
	UStaticMeshComponent* CityMesh;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "攻击检测盒"))
	UBoxComponent* AttackDetectionBox;

	// ========== 主城配置 ==========
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Main City", 
		meta = (Categories = "Unit.Faction", DisplayName = "阵营标签"))
	FGameplayTag FactionTag;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Main City", meta = (DisplayName = "初始生命值"))
	float InitialHealth = 10000.0f;

	// ========== ✨ 新增 - 可视化调试配置 ==========
	
	/**
	 * @brief 是否显示攻击检测盒可视化
	 * @details
	 * 功能说明：
	 * - 在游戏运行时显示检测盒的边界
	 * - 显示检测盒的尺寸和位置信息
	 * - 显示单位到检测盒的距离线
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Visualization", 
		meta = (DisplayName = "显示攻击检测盒"))
	bool bShowAttackDetectionBox = false;
	
	/**
	 * @brief 是否显示生命值信息
	 * @details
	 * 功能说明：
	 * - 在主城上方显示生命值文本
	 * - 显示当前生命值 / 最大生命值
	 * - 显示生命值百分比
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Visualization", 
		meta = (DisplayName = "显示生命值信息"))
	bool bShowHealthInfo = false;
	
	/**
	 * @brief 是否输出详细伤害日志
	 * @details
	 * 功能说明：
	 * - 受到伤害时输出详细日志
	 * - 包含攻击者信息、伤害值等
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Visualization", 
		meta = (DisplayName = "输出详细伤害日志"))
	bool bShowDamageLog = true;
	
	/**
	 * @brief 检测盒可视化颜色
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Visualization", 
		meta = (DisplayName = "检测盒颜色"))
	FLinearColor DetectionBoxColor = FLinearColor(1.0f, 0.5f, 0.0f, 0.5f); // 橙色半透明
	
	/**
	 * @brief 生命值文本颜色
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Visualization", 
		meta = (DisplayName = "生命值文本颜色"))
	FLinearColor HealthTextColor = FLinearColor::Green;

	// ========== GAS 接口实现 ==========
	
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// ========== 初始化函数 ==========
	
	UFUNCTION(BlueprintCallable, Category = "Main City", meta = (DisplayName = "初始化主城"))
	void InitializeMainCity();

	// ========== 查询函数 ==========
	
	UFUNCTION(BlueprintPure, Category = "Main City", meta = (DisplayName = "获取当前生命值"))
	float GetCurrentHealth() const;
	
	UFUNCTION(BlueprintPure, Category = "Main City", meta = (DisplayName = "获取最大生命值"))
	float GetMaxHealth() const;
	
	UFUNCTION(BlueprintPure, Category = "Main City", meta = (DisplayName = "获取生命值百分比"))
	float GetHealthPercentage() const;
	
	UFUNCTION(BlueprintPure, Category = "Main City", meta = (DisplayName = "获取攻击检测盒"))
	UBoxComponent* GetAttackDetectionBox() const { return AttackDetectionBox; }

	// ========== ✨ 新增 - 调试函数 ==========
	
	/**
	 * @brief 切换攻击检测盒显示
	 */
	UFUNCTION(BlueprintCallable, Category = "Debug Visualization", meta = (DisplayName = "切换检测盒显示"))
	void ToggleDetectionBoxVisualization();
	
	/**
	 * @brief 切换生命值信息显示
	 */
	UFUNCTION(BlueprintCallable, Category = "Debug Visualization", meta = (DisplayName = "切换生命值显示"))
	void ToggleHealthInfoVisualization();
	
	/**
	 * @brief Tick 函数（用于绘制调试信息）
	 */
	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;
	
	void OnHealthChanged(const FOnAttributeChangeData& Data);
	
	UFUNCTION(BlueprintNativeEvent, Category = "Main City", meta = (DisplayName = "主城被摧毁时"))
	void OnMainCityDestroyed();
	virtual void OnMainCityDestroyed_Implementation();

private:
	void BindAttributeDelegates();
	bool bIsDestroyed = false;
	
	// ✨ 新增 - 绘制调试可视化
	void DrawDebugVisualization();
};
