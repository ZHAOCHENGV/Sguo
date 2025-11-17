// 📄 文件：Buildings/SG_BuildingAttributeSet.cpp

#include "Buildings/SG_BuildingAttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "GameplayEffectExtension.h"
#include "Debug/SG_LogCategories.h"

/**
 * @brief 构造函数
 */
USG_BuildingAttributeSet::USG_BuildingAttributeSet()
{
    // 不需要在构造函数中初始化属性值
    // 属性值会在建筑初始化时设置
}

/**
 * @brief 注册需要网络复制的属性
 */
void USG_BuildingAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    // 调用父类实现
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // 注册需要复制的属性
    DOREPLIFETIME_CONDITION_NOTIFY(USG_BuildingAttributeSet, Health, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(USG_BuildingAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
}

/**
 * @brief 属性变化前的处理
 */
void USG_BuildingAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
    // 调用父类实现
    Super::PreAttributeChange(Attribute, NewValue);

    // Clamp 生命值
    if (Attribute == GetHealthAttribute())
    {
        // 限制在 [0, MaxHealth] 范围内
        NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
    }
    // Clamp 最大生命值
    else if (Attribute == GetMaxHealthAttribute())
    {
        // 最大生命值不能小于 1
        NewValue = FMath::Max(NewValue, 1.0f);
    }
}

/**
 * @brief GameplayEffect 执行后的处理
 */
void USG_BuildingAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
    // 调用父类实现
    Super::PostGameplayEffectExecute(Data);

    // 处理即将受到的伤害
    if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
    {
        // 获取伤害值
        const float LocalIncomingDamage = GetIncomingDamage();
        
        // 清空 IncomingDamage
        SetIncomingDamage(0.0f);

        // 只处理正数伤害
        if (LocalIncomingDamage > 0.0f)
        {
            // 计算新的生命值
            const float NewHealth = GetHealth() - LocalIncomingDamage;
            
            // 设置生命值
            SetHealth(FMath::Clamp(NewHealth, 0.0f, GetMaxHealth()));
        }
    }
    // 确保 Health 不超过 MaxHealth
    else if (Data.EvaluatedData.Attribute == GetHealthAttribute())
    {
        // Clamp 生命值
        SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));
    }
}

/**
 * @brief 生命值复制回调
 */
void USG_BuildingAttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth)
{
    // 通知 GAS 系统属性已更新
    GAMEPLAYATTRIBUTE_REPNOTIFY(USG_BuildingAttributeSet, Health, OldHealth);
}

/**
 * @brief 最大生命值复制回调
 */
void USG_BuildingAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth)
{
    // 通知 GAS 系统属性已更新
    GAMEPLAYATTRIBUTE_REPNOTIFY(USG_BuildingAttributeSet, MaxHealth, OldMaxHealth);
}
