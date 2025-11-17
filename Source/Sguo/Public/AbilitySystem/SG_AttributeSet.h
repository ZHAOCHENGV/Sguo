// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "SG_AttributeSet.generated.h"

// ========== 属性访问宏 ==========
// 这个宏简化了属性的Getter/Setter定义
// 为什么需要这些宏：
// - GAS系统需要特定的访问器函数
// - 手动写这些函数很繁琐且容易出错
// - 宏确保所有属性有统一的访问接口
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

	


/**
 * @brief 角色属性集
 * 
 * 包含所有角色属性：
 * - 生命值系统（当前生命、最大生命）
 * - 战斗属性（攻击力、攻击速度、攻击范围）
 * - 移动属性（移动速度）
 * - Meta属性（用于计算，不持久化）
 * 
 * 属性的工作流程：
 * 1. 初始化：在角色生成时设置基础值
 * 2. 修改：通过GameplayEffect修改属性
 * 3. 计算：PreAttributeChange中Clamp值
 * 4. 应用：PostGameplayEffectExecute中处理副作用
 * 5. 同步：自动复制到客户端
 */
UCLASS()
class SGUO_API USG_AttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	USG_AttributeSet();	

	// ========== 核心属性 ==========
	
	// 当前生命值
	// 角色的当前血量，降到0时角色死亡
	// 为什么使用FGameplayAttributeData：
	// - GAS系统要求的数据类型
	// - 包含基础值和当前值，支持临时修改
	// - 支持网络复制
	// ReplicatedUsing：指定网络复制时调用的函数
	UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_Health)
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(USG_AttributeSet, Health)
	
	// 最大生命值
	// 角色的生命值上限
	// 当前生命值不能超过最大生命值
	// 可以通过装备、Buff等临时或永久提升
	UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_MaxHealth)
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(USG_AttributeSet, MaxHealth)
	
	// 攻击力
	// 角色的基础伤害值
	// 实际伤害 = 攻击力 * 技能倍率 * 其他加成
	// 可以通过Buff、装备等修改
	UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_AttackDamage)
	FGameplayAttributeData AttackDamage;
	ATTRIBUTE_ACCESSORS(USG_AttributeSet, AttackDamage)
	
	// 移动速度
	// 角色的移动速度（单位：厘米/秒）
	// UE默认移动速度约为600
	// 影响角色的移动快慢和战场机动性
	UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_MoveSpeed)
	FGameplayAttributeData MoveSpeed;
	ATTRIBUTE_ACCESSORS(USG_AttributeSet, MoveSpeed)
	
	// 攻击速度
	// 每秒可以攻击的次数
	// 例如：1.0 = 每秒1次，2.0 = 每秒2次
	// 影响角色的DPS（每秒伤害）
	// 为什么不用攻击间隔：
	// - 攻击速度更直观（数值越大越快）
	// - 便于计算百分比加成（+50%攻速）
	UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_AttackSpeed)
	FGameplayAttributeData AttackSpeed;
	ATTRIBUTE_ACCESSORS(USG_AttributeSet, AttackSpeed)
	
	// 攻击范围
	// 角色可以攻击的最大距离（单位：厘米）
	// 近战单位通常是150-200
	// 远程单位通常是800-1500
	UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_AttackRange)
	FGameplayAttributeData AttackRange;
	ATTRIBUTE_ACCESSORS(USG_AttributeSet, AttackRange)

	// ========== Meta属性（用于计算，不持久化） ==========
	
	// 即将受到的伤害
	// 这是一个临时属性，用于伤害计算流程
	// 为什么需要这个属性：
	// - GAS的伤害系统通过修改这个属性来传递伤害
	// - 在PostGameplayEffectExecute中处理伤害，然后扣除生命值
	// - 允许在伤害应用前进行修改（如护甲减伤）
	// 为什么不直接修改Health：
	// - 分离伤害计算和生命值修改逻辑
	// - 便于实现复杂的伤害系统（暴击、护甲等）
	// - 符合GAS的最佳实践
	// 注意：此属性不需要网络复制，因为它只是中间值
	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData IncomingDamage;
	ATTRIBUTE_ACCESSORS(USG_AttributeSet, IncomingDamage)

	// ========== 网络复制 ==========
	
	/**
	 * @brief 注册需要网络复制的属性
	 * 
	 * 为什么需要网络复制：
	 * - 多人游戏需要同步属性到所有客户端
	 * - 客户端需要显示正确的生命值、伤害等
	 * 
	 * 注意：
	 * - 服务器是权威的，客户端只是显示
	 * - 只有标记为Replicated的属性才会同步
	 */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ========== 属性变化处理 ==========
	
	/**
	 * @brief 属性变化前的处理
	 * 
	 * 调用时机：
	 * - 在属性即将被修改之前调用
	 * - 可以修改NewValue来Clamp或调整值
	 * 
	 * 用途：
	 * - Clamp属性值（如生命值不能超过最大值）
	 * - 阻止非法值（如负数攻击力）
	 * - 应用最小/最大值限制
	 * 
	 * 注意：
	 * - 不要在这里处理游戏逻辑（如死亡）
	 * - 只做数值验证和调整
	 */
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	
	/**
	 * @brief GameplayEffect执行后的处理
	 * 
	 * 调用时机：
	 * - 在GameplayEffect修改属性后立即调用
	 * - 此时属性已经被修改
	 * 
	 * 用途：
	 * - 处理伤害计算（IncomingDamage -> Health）
	 * - 触发死亡事件
	 * - 更新UI
	 * - 播放特效和音效
	 * 
	 * 为什么在这里处理而不是PreAttributeChange：
	 * - 需要知道修改的来源（Data.EffectSpec）
	 * - 可以访问完整的上下文信息
	 * - 符合GAS的设计模式
	 */
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

protected:
	// ========== OnRep函数（网络复制回调） ==========
	// 这些函数在客户端接收到属性更新时调用
	// 用途：
	// - 更新UI显示
	// - 播放视觉/音效反馈
	// - 触发客户端特定的逻辑
	
	// 生命值复制回调
	// 可以在这里更新生命条UI
	UFUNCTION()
	virtual void OnRep_Health(const FGameplayAttributeData& OldHealth);
	
	// 最大生命值复制回调
	UFUNCTION()
	virtual void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth);
	
	// 攻击力复制回调
	UFUNCTION()
	virtual void OnRep_AttackDamage(const FGameplayAttributeData& OldAttackDamage);
	
	// 移动速度复制回调
	// 可以在这里更新角色的移动组件速度
	UFUNCTION()
	virtual void OnRep_MoveSpeed(const FGameplayAttributeData& OldMoveSpeed);
	
	// 攻击速度复制回调
	UFUNCTION()
	virtual void OnRep_AttackSpeed(const FGameplayAttributeData& OldAttackSpeed);
	
	// 攻击范围复制回调
	UFUNCTION()
	virtual void OnRep_AttackRange(const FGameplayAttributeData& OldAttackRange);
};
