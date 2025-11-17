// Fill out your copyright notice in the Description page of Project Settings.
/**
 * @file SG_AttributeSet.cpp
 * @brief 角色属性集实现
 * 
 * 实现说明：
 * - 实现所有虚函数
 * - 处理属性变化逻辑
 * - 注册网络复制属性
 */

#include "AbilitySystem/SG_AttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "GameplayEffectExtension.h"


// 构造函数
// 初始化默认值（这些值通常会被DataAsset或GameplayEffect覆盖）
USG_AttributeSet::USG_AttributeSet()
{
	// 不需要在这里初始化属性值
	// 属性会在角色生成时通过InitializeAttributes函数设置
}


// 注册需要网络复制的属性
// 这个函数告诉UE哪些属性需要在网络游戏中同步
void USG_AttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	// 调用父类实现
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 注册所有需要复制的属性
	// DOREPLIFETIME_CONDITION_NOTIFY 宏的参数：
	// - 类名
	// - 属性名
	// - COND_None：无条件复制（总是复制）
	// - REPNOTIFY_Always：总是调用OnRep函数，即使值没变
	
	// 复制生命值
	// 为什么需要复制：客户端需要显示正确的血条
	DOREPLIFETIME_CONDITION_NOTIFY(USG_AttributeSet, Health, COND_None, REPNOTIFY_Always);
	
	// 复制最大生命值
	// 为什么需要复制：客户端需要计算血量百分比
	DOREPLIFETIME_CONDITION_NOTIFY(USG_AttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	
	// 复制攻击力
	// 为什么需要复制：客户端可能需要显示伤害数字
	DOREPLIFETIME_CONDITION_NOTIFY(USG_AttributeSet, AttackDamage, COND_None, REPNOTIFY_Always);
	
	// 复制移动速度
	// 为什么需要复制：影响角色移动表现
	DOREPLIFETIME_CONDITION_NOTIFY(USG_AttributeSet, MoveSpeed, COND_None, REPNOTIFY_Always);
	
	// 复制攻击速度
	// 为什么需要复制：影响攻击动画播放速度
	DOREPLIFETIME_CONDITION_NOTIFY(USG_AttributeSet, AttackSpeed, COND_None, REPNOTIFY_Always);
	
	// 复制攻击范围
	// 为什么需要复制：客户端需要显示攻击范围指示器
	DOREPLIFETIME_CONDITION_NOTIFY(USG_AttributeSet, AttackRange, COND_None, REPNOTIFY_Always);
}

// 属性变化前的处理
// 在属性即将被修改之前调用，可以Clamp值
void USG_AttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	// 调用父类实现
	Super::PreAttributeChange(Attribute, NewValue);

	// 处理生命值变化
	// Clamp生命值，确保不超过最大值，不低于0
	if (Attribute == GetHealthAttribute())
	{
		// FMath::Clamp 限制值在指定范围内
		// 参数：(要限制的值, 最小值, 最大值)
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
	}
	// 处理最大生命值变化
	// 确保最大生命值不低于1（避免除零错误）
	else if (Attribute == GetMaxHealthAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.0f);
	}
	// 处理移动速度变化
	// 确保移动速度不为负数
	else if (Attribute == GetMoveSpeedAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0f);
	}
	// 处理攻击速度变化
	// 确保攻击速度不低于0.1（避免除零错误和过慢的攻击）
	else if (Attribute == GetAttackSpeedAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.1f);
	}
	// 处理攻击范围变化
	// 确保攻击范围不为负数
	else if (Attribute == GetAttackRangeAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0f);
	}
}

// GameplayEffect执行后的处理
// 在GameplayEffect修改属性后调用，处理副作用
void USG_AttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	// 调用父类实现
	Super::PostGameplayEffectExecute(Data);

	// 处理即将受到的伤害
	// IncomingDamage是一个Meta属性，用于传递伤害值
	if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		// 获取伤害值
		const float LocalIncomingDamage = GetIncomingDamage();
		
		// 清空IncomingDamage（它只是临时存储）
		// 为什么要清空：避免重复应用伤害
		SetIncomingDamage(0.0f);

		// 只处理正数伤害
		if (LocalIncomingDamage > 0.0f)
		{
			// 计算新的生命值
			// 当前生命值 - 伤害值
			const float NewHealth = GetHealth() - LocalIncomingDamage;
			
			// 设置生命值，并Clamp在有效范围内
			SetHealth(FMath::Clamp(NewHealth, 0.0f, GetMaxHealth()));

			// 如果生命值归零，角色死亡
			// 注意：这里只是检测，实际的死亡处理在蓝图中
			// 为什么在蓝图中处理：
			// - 死亡逻辑可能涉及动画、特效、掉落等
			// - 蓝图更容易调整和测试
			// - 不同角色可能有不同的死亡表现
			if (GetHealth() <= 0.0f)
			{
				// 可以在这里广播死亡事件
				// 蓝图可以监听Health属性变化来检测死亡
			}
		}
	}
	// 确保Health不超过MaxHealth
	// 例如：治疗效果可能会使Health超过上限
	else if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));
	}
}

// ========== OnRep函数实现 ==========
// 这些函数在客户端接收到属性更新时调用

// 生命值复制回调
void USG_AttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth)
{
	// 通知GAS系统属性已更新
	// 这个宏会触发相关的委托和事件
	GAMEPLAYATTRIBUTE_REPNOTIFY(USG_AttributeSet, Health, OldHealth);
	
	// 可以在这里添加客户端特定的逻辑
	// 例如：更新UI、播放受伤音效等
	// 但通常这些逻辑在蓝图中处理更灵活
}

// 最大生命值复制回调
void USG_AttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USG_AttributeSet, MaxHealth, OldMaxHealth);
}

// 攻击力复制回调
void USG_AttributeSet::OnRep_AttackDamage(const FGameplayAttributeData& OldAttackDamage)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USG_AttributeSet, AttackDamage, OldAttackDamage);
}

// 移动速度复制回调
void USG_AttributeSet::OnRep_MoveSpeed(const FGameplayAttributeData& OldMoveSpeed)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USG_AttributeSet, MoveSpeed, OldMoveSpeed);
	
	// 可以在这里更新角色的移动组件速度
	// 例如：
	// if (GetOwningActor())
	// {
	//     if (UCharacterMovementComponent* MoveComp = GetOwningActor()->FindComponentByClass<UCharacterMovementComponent>())
	//     {
	//         MoveComp->MaxWalkSpeed = GetMoveSpeed();
	//     }
	// }
}

// 攻击速度复制回调
void USG_AttributeSet::OnRep_AttackSpeed(const FGameplayAttributeData& OldAttackSpeed)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USG_AttributeSet, AttackSpeed, OldAttackSpeed);
}

// 攻击范围复制回调
void USG_AttributeSet::OnRep_AttackRange(const FGameplayAttributeData& OldAttackRange)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USG_AttributeSet, AttackRange, OldAttackRange);
}