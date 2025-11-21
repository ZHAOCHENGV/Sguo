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
	UE_LOG(LogSGGameplay, Warning, TEXT("========== BuildingAttributeSet 构造 =========="));
	UE_LOG(LogSGGameplay, Warning, TEXT("  AttributeSet：%s"), *GetName());
	UE_LOG(LogSGGameplay, Warning, TEXT("  所属 Actor：%s"), GetOwningActor() ? *GetOwningActor()->GetName() : TEXT("None"));
	
	// 验证属性定义
	FGameplayAttribute HealthAttr = GetHealthAttribute();
	FGameplayAttribute MaxHealthAttr = GetMaxHealthAttribute();
	FGameplayAttribute IncomingDamageAttr = GetIncomingDamageAttribute();
	
	UE_LOG(LogSGGameplay, Warning, TEXT("  Health 属性：%s"), HealthAttr.IsValid() ? TEXT("✅") : TEXT("❌"));
	UE_LOG(LogSGGameplay, Warning, TEXT("  MaxHealth 属性：%s"), MaxHealthAttr.IsValid() ? TEXT("✅") : TEXT("❌"));
	UE_LOG(LogSGGameplay, Warning, TEXT("  IncomingDamage 属性：%s"), IncomingDamageAttr.IsValid() ? TEXT("✅") : TEXT("❌"));
	
	if (IncomingDamageAttr.IsValid())
	{
		UE_LOG(LogSGGameplay, Warning, TEXT("    IncomingDamage 属性名称：%s"), *IncomingDamageAttr.GetName());
		UE_LOG(LogSGGameplay, Warning, TEXT("    IncomingDamage 所属类：%s"), *IncomingDamageAttr.GetAttributeSetClass()->GetName());
	}
	
	UE_LOG(LogSGGameplay, Warning, TEXT("========================================"));
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
	// ✨ 关键日志 - 必须在最开始
	UE_LOG(LogSGGameplay, Error, TEXT("========== PostGameplayEffectExecute 被调用！=========="));
	UE_LOG(LogSGGameplay, Error, TEXT("  建筑：%s"), GetOwningActor() ? *GetOwningActor()->GetName() : TEXT("None"));
	UE_LOG(LogSGGameplay, Error, TEXT("  修改的属性：%s"), *Data.EvaluatedData.Attribute.GetName());
	UE_LOG(LogSGGameplay, Error, TEXT("  修改值：%.2f"), Data.EvaluatedData.Magnitude);
	
	// 调用父类实现
	Super::PostGameplayEffectExecute(Data);

	// ========== ✨ 新增 - 输出所有属性变化 ==========
	UE_LOG(LogSGGameplay, Log, TEXT("========== PostGameplayEffectExecute =========="));
	UE_LOG(LogSGGameplay, Log, TEXT("  建筑：%s"), *GetOwningActor()->GetName());
	UE_LOG(LogSGGameplay, Log, TEXT("  修改的属性：%s"), *Data.EvaluatedData.Attribute.GetName());
	UE_LOG(LogSGGameplay, Log, TEXT("  修改值：%.2f"), Data.EvaluatedData.Magnitude);

	// ========== 处理即将受到的伤害 ==========
	if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		UE_LOG(LogSGGameplay, Warning, TEXT("  🔥 检测到 IncomingDamage 属性"));
		
		// 获取伤害值
		const float LocalIncomingDamage = GetIncomingDamage();
		
		UE_LOG(LogSGGameplay, Warning, TEXT("  IncomingDamage 值：%.2f"), LocalIncomingDamage);
		
		// 清空 IncomingDamage
		SetIncomingDamage(0.0f);

		// 只处理正数伤害
		if (LocalIncomingDamage > 0.0f)
		{
			// 获取旧生命值
			const float OldHealth = GetHealth();
			
			// 计算新的生命值
			const float NewHealth = OldHealth - LocalIncomingDamage;
			
			// 限制范围
			const float ClampedHealth = FMath::Clamp(NewHealth, 0.0f, GetMaxHealth());
			
			// 设置生命值
			SetHealth(ClampedHealth);
			
			// ========== ✨ 新增 - 详细的伤害日志 ==========
			UE_LOG(LogSGGameplay, Warning, TEXT("========================================"));
			UE_LOG(LogSGGameplay, Warning, TEXT("🩸 建筑受到伤害"));
			UE_LOG(LogSGGameplay, Warning, TEXT("  建筑：%s"), *GetOwningActor()->GetName());
			UE_LOG(LogSGGameplay, Warning, TEXT("  伤害值：%.2f"), LocalIncomingDamage);
			UE_LOG(LogSGGameplay, Warning, TEXT("  旧生命值：%.0f"), OldHealth);
			UE_LOG(LogSGGameplay, Warning, TEXT("  计算的新生命值：%.0f"), NewHealth);
			UE_LOG(LogSGGameplay, Warning, TEXT("  限制后的生命值：%.0f"), ClampedHealth);
			UE_LOG(LogSGGameplay, Warning, TEXT("  最大生命值：%.0f"), GetMaxHealth());
			UE_LOG(LogSGGameplay, Warning, TEXT("  剩余百分比：%.1f%%"), (ClampedHealth / GetMaxHealth()) * 100.0f);
			
			// ✨ 新增 - 输出攻击者信息
			if (Data.EffectSpec.GetContext().GetInstigator())
			{
				UE_LOG(LogSGGameplay, Warning, TEXT("  攻击者：%s"), 
					*Data.EffectSpec.GetContext().GetInstigator()->GetName());
			}
			else
			{
				UE_LOG(LogSGGameplay, Warning, TEXT("  攻击者：未知"));
			}
			
			// ✨ 新增 - 输出 GE 信息
			if (Data.EffectSpec.Def)
			{
				UE_LOG(LogSGGameplay, Warning, TEXT("  GE 类：%s"), *Data.EffectSpec.Def->GetName());
			}
			
			UE_LOG(LogSGGameplay, Warning, TEXT("========================================"));
		}
		else
		{
			UE_LOG(LogSGGameplay, Warning, TEXT("  ⚠️ IncomingDamage <= 0，不处理"));
		}
	}
	// ========== 确保 Health 不超过 MaxHealth ==========
	else if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		UE_LOG(LogSGGameplay, Log, TEXT("  🏥 检测到 Health 属性直接修改"));
		
		const float OldHealth = GetHealth();
		const float ClampedHealth = FMath::Clamp(OldHealth, 0.0f, GetMaxHealth());
		
		if (OldHealth != ClampedHealth)
		{
			SetHealth(ClampedHealth);
			UE_LOG(LogSGGameplay, Log, TEXT("  Health 被限制：%.0f → %.0f"), OldHealth, ClampedHealth);
		}
	}
	else
	{
		UE_LOG(LogSGGameplay, Log, TEXT("  其他属性修改，不处理"));
	}
	
	UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
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
