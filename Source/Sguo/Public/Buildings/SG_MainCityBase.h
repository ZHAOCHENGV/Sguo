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

	// ========== ✨ 新增 - 主城状态 ==========
	
	/**
	 * @brief 主城是否已被摧毁
	 * @details
	 * 功能说明：
	 * - 标记主城是否已被摧毁
	 * - 用于 AI 目标有效性检查
	 * - 防止攻击已死亡的主城
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Main City", meta = (DisplayName = "是否已被摧毁"))
	bool bIsDestroyed = false;

	/**
	 * @brief 检查主城是否存活
	 * @return 是否存活
	 */
	UFUNCTION(BlueprintPure, Category = "Main City", meta = (DisplayName = "是否存活"))
	bool IsAlive() const;


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


};
