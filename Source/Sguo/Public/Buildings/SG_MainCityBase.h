// 📄 文件：Source/Sguo/Public/Buildings/SG_MainCityBase.h
// 🔧 修改 - 添加击飞站桩单位相关配置

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

	// ========== 主城状态 ==========
	
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

	// ✨ 新增 - 主城摧毁时击飞站桩单位配置
	// ========== 主城摧毁冲击波配置 ==========

	/**
	 * @brief 是否在主城摧毁时击飞站桩单位
	 * @details 启用后，主城摧毁时会对同阵营的站桩单位产生冲击波效果
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Main City|Destruction Effect", 
		meta = (DisplayName = "启用摧毁冲击波"))
	bool bEnableDestructionBlast = true;

	/**
	 * @brief 冲击波影响范围（厘米）
	 * @details 主城摧毁时，此范围内的站桩单位会被击飞
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Main City|Destruction Effect", 
		meta = (DisplayName = "冲击波范围", ClampMin = "0.0", UIMin = "0.0", UIMax = "10000.0",
		EditCondition = "bEnableDestructionBlast", EditConditionHides))
	float BlastRadius = 5000.0f;

	/**
	 * @brief 冲击波基础力度
	 * @details 击飞力的基础大小，会根据距离衰减
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Main City|Destruction Effect", 
		meta = (DisplayName = "冲击波力度", ClampMin = "0.0", UIMin = "0.0", UIMax = "100000.0",
		EditCondition = "bEnableDestructionBlast", EditConditionHides))
	float BlastForce = 50000.0f;

	/**
	 * @brief 冲击波向上力度比例
	 * @details 向上抛起的力占总力的比例（0-1）
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Main City|Destruction Effect", 
		meta = (DisplayName = "向上力度比例", ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0",
		EditCondition = "bEnableDestructionBlast", EditConditionHides))
	float BlastUpwardRatio = 0.5f;

	/**
	 * @brief 冲击波是否影响所有站桩单位（忽略范围）
	 * @details 启用后，所有同阵营站桩单位都会被击飞，无论距离多远
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Main City|Destruction Effect", 
		meta = (DisplayName = "影响所有站桩单位",
		EditCondition = "bEnableDestructionBlast", EditConditionHides))
	bool bBlastAllStationaryUnits = true;

	/**
	 * @brief 击飞后延迟销毁时间（秒）
	 * @details 站桩单位被击飞后，经过此时间后销毁
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Main City|Destruction Effect", 
		meta = (DisplayName = "击飞后销毁延迟", ClampMin = "0.0", UIMin = "0.0", UIMax = "10.0",
		EditCondition = "bEnableDestructionBlast", EditConditionHides))
	float BlastDestroyDelay = 3.0f;

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

	// ✨ 新增 - 击飞站桩单位函数
	/**
	 * @brief 击飞同阵营的站桩单位
	 * @details
	 * 功能说明：
	 * - 查找所有同阵营的 SG_StationaryUnit
	 * - 杀死它们并启用布娃娃
	 * - 施加冲击波力使其被击飞
	 * 详细流程：
	 * 1. 获取所有 SG_StationaryUnit
	 * 2. 过滤同阵营单位
	 * 3. 根据配置过滤范围
	 * 4. 对每个单位执行击飞逻辑
	 */
	UFUNCTION(BlueprintCallable, Category = "Main City", meta = (DisplayName = "击飞站桩单位"))
	void BlastStationaryUnits();

	/**
	 * @brief 对单个站桩单位执行击飞
	 * @param Unit 目标单位
	 * @param BlastOrigin 冲击波原点
	 * @details
	 * 功能说明：
	 * - 设置单位死亡
	 * - 启用布娃娃物理
	 * - 施加冲击力
	 */
	void BlastSingleUnit(class ASG_StationaryUnit* Unit, const FVector& BlastOrigin);

private:
	void BindAttributeDelegates();
};
