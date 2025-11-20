// 🔧 简化 - SG_MainCityBase.h

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

/**
 * @brief 主城基类
 * @details
 * 功能说明：
 * - 可以在场景中放置
 * - 具有血量属性（使用 GAS）
 * - 支持玩家/敌人两个阵营
 * - 被摧毁时触发游戏结束
 * - ✨ 攻击检测盒：直接使用 BoxComponent 的原生属性调整
 */
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
	
	/**
	 * @brief 主城网格体（根组件）
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "主城网格体"))
	UStaticMeshComponent* CityMesh;
	
	/**
	 * @brief 攻击检测碰撞盒
	 * @details
	 * 功能说明：
	 * - 敌方单位检测到此碰撞盒时开始攻击
	 * - ✨ 直接在编辑器中调整 Box Extent 和 Location 即可
	 * - 不需要额外的配置属性
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "攻击检测盒"))
	UBoxComponent* AttackDetectionBox;

	// ========== 主城配置 ==========
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Main City", 
		meta = (Categories = "Unit.Faction", DisplayName = "阵营标签"))
	FGameplayTag FactionTag;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Main City", meta = (DisplayName = "初始生命值"))
	float InitialHealth = 10000.0f;

	// ❌ 删除 - 不再需要这些属性
	// UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Main City|Attack Detection")
	// FVector AttackDetectionBoxExtent = FVector(800.0f, 800.0f, 500.0f);
	
	// UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Main City|Attack Detection")
	// FVector AttackDetectionBoxOffset = FVector(0.0f, 0.0f, 500.0f);

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

protected:
	virtual void BeginPlay() override;
	
	void OnHealthChanged(const FOnAttributeChangeData& Data);
	
	UFUNCTION(BlueprintNativeEvent, Category = "Main City", meta = (DisplayName = "主城被摧毁时"))
	void OnMainCityDestroyed();
	virtual void OnMainCityDestroyed_Implementation();

private:
	void BindAttributeDelegates();
	bool bIsDestroyed = false;
};
