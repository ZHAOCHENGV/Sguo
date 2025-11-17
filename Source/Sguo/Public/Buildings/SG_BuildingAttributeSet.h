// 📄 文件：Buildings/SG_BuildingAttributeSet.h

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "SG_BuildingAttributeSet.generated.h"

// 属性访问宏
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * @brief 建筑属性集
 * @details
 * 功能说明：
 * - 定义建筑（主城、防御塔等）的属性
 * - 支持 GAS 系统
 * 包含属性：
 * - Health（当前生命值）
 * - MaxHealth（最大生命值）
 * - IncomingDamage（即将受到的伤害，Meta 属性）
 * 注意事项：
 * - 建筑不需要移动速度、攻击速度等属性
 * - 可以根据需要扩展（护甲、防御等）
 */
UCLASS()
class SGUO_API USG_BuildingAttributeSet : public UAttributeSet
{
    GENERATED_BODY()

public:
    /**
     * @brief 构造函数
     */
    USG_BuildingAttributeSet();

    // ========== 核心属性 ==========
    
    /**
     * @brief 当前生命值
     * @details 建筑的当前血量，降到 0 时建筑被摧毁
     */
    UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_Health)
    FGameplayAttributeData Health;
    ATTRIBUTE_ACCESSORS(USG_BuildingAttributeSet, Health)
    
    /**
     * @brief 最大生命值
     * @details 建筑的生命值上限
     */
    UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_MaxHealth)
    FGameplayAttributeData MaxHealth;
    ATTRIBUTE_ACCESSORS(USG_BuildingAttributeSet, MaxHealth)

    // ========== Meta 属性 ==========
    
    /**
     * @brief 即将受到的伤害
     * @details
     * 功能说明：
     * - 用于伤害计算流程
     * - 不持久化，不复制
     */
    UPROPERTY(BlueprintReadOnly, Category = "Attributes")
    FGameplayAttributeData IncomingDamage;
    ATTRIBUTE_ACCESSORS(USG_BuildingAttributeSet, IncomingDamage)

    // ========== 网络复制 ==========
    
    /**
     * @brief 注册需要网络复制的属性
     */
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // ========== 属性变化处理 ==========
    
    /**
     * @brief 属性变化前的处理
     * @details Clamp 属性值
     */
    virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
    
    /**
     * @brief GameplayEffect 执行后的处理
     * @details 处理伤害计算
     */
    virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

protected:
    // ========== OnRep 函数 ==========
    
    /**
     * @brief 生命值复制回调
     */
    UFUNCTION()
    virtual void OnRep_Health(const FGameplayAttributeData& OldHealth);
    
    /**
     * @brief 最大生命值复制回调
     */
    UFUNCTION()
    virtual void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth);
};
