/**
 * @file SG_UnitsBase.h
 * @brief 角色基类
 */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "SG_UnitsBase.generated.h"

// 前置声明
class USG_AbilitySystemComponent;
class USG_AttributeSet;
struct FOnAttributeChangeData;  // 添加这个前置声明
// ✨ 新增 - 单位死亡委托声明
/**
 * @brief 单位死亡委托
 * @details 当单位死亡时广播，供前线管理器等系统监听
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSGUnitDeathSignature, ASG_UnitsBase*, DeadUnit);
/**
 * @brief 角色基类
 */
UCLASS()
class SGUO_API ASG_UnitsBase : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	// 构造函数
	ASG_UnitsBase();
	
	// ✨ 新增 - 单位死亡事件
	/**
	 * @brief 单位死亡事件
	 * @details 当单位死亡时广播此事件
	 */
	UPROPERTY(BlueprintAssignable, Category = "Unit Events")
	FSGUnitDeathSignature OnUnitDeathEvent;
	// ========== GAS 组件 ==========
	
	// Ability System Component
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	USG_AbilitySystemComponent* AbilitySystemComponent;
	
	// Attribute Set
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	USG_AttributeSet* AttributeSet;

	// ========== 角色信息 ==========
	
	// 阵营标签（玩家/敌人）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Info", meta = (Categories = "Unit.Faction"))
	FGameplayTag FactionTag;
	
	// 单位类型标签（步兵/骑兵等）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Info", meta = (Categories = "Unit.Type"))
	FGameplayTag UnitTypeTag;
	
	// 当前攻击目标
	UPROPERTY(BlueprintReadWrite, Category = "Combat")
	AActor* CurrentTarget;

	// ========== 基础属性配置 ==========
	
	// 基础生命值
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Attributes")
	float BaseHealth = 500.0f;
	
	// 基础攻击力
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Attributes")
	float BaseAttackDamage = 50.0f;
	
	// 基础移动速度
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Attributes")
	float BaseMoveSpeed = 400.0f;
	
	// 基础攻击速度（每秒攻击次数）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Attributes")
	float BaseAttackSpeed = 1.0f;
	
	// 基础攻击范围
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Attributes")
	float BaseAttackRange = 150.0f;

	// ========== GAS 接口实现 ==========
	
	/**
	 * @brief 获取 AbilitySystemComponent（GAS 接口要求）
	 */
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// ========== 初始化函数 ==========
	
	/**
	 * @brief 初始化角色属性
	 */
	UFUNCTION(BlueprintCallable, Category = "Character")
	void InitializeCharacter(
		FGameplayTag InFactionTag,
		float HealthMultiplier = 1.0f,
		float DamageMultiplier = 1.0f,
		float SpeedMultiplier = 1.0f
	);

	// ========== 战斗相关函数 ==========
	
	/**
	 * @brief 查找最近的敌人
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	AActor* FindNearestTarget();
	
	/**
	 * @brief 设置当前目标
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void SetTarget(AActor* NewTarget);

protected:
	// ========== 生命周期函数 ==========
	
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;

	// ========== GAS 初始化 ==========
	
	/**
	 * @brief 初始化 AttributeSet 的值
	 */
	void InitializeAttributes(float HealthMult, float DamageMult, float SpeedMult);
	
	/**
	 * @brief 绑定属性变化委托
	 */
	void BindAttributeDelegates();

	// ========== 属性变化回调 ==========
	
	/**
	 * @brief 生命值变化时调用
	 * 
	 * 注意：参数类型必须是 const FOnAttributeChangeData&
	 * 这是 GAS 属性变化委托要求的签名
	 */
	void OnHealthChanged(const FOnAttributeChangeData& Data);
	
	/**
	 * @brief 角色死亡时调用
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Character")
	void OnDeath();
	virtual void OnDeath_Implementation();

public:
	// ✨ NEW - 死亡标记（防止重复触发）
	/**
	 * @brief 是否已经死亡
	 * @details 防止死亡逻辑被重复触发
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Character")
	bool bIsDead = false;
};
