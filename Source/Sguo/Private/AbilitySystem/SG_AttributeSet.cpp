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
#include "Debug/SG_LogCategories.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Units/SG_UnitsBase.h"


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

/**
 * @brief 属性变化前的处理
 * @param Attribute 变化的属性
 * @param NewValue 新值（可修改）
 * @details
 * 🔧 核心修改：
 * - 在这里同步移动速度到 CharacterMovementComponent
 * - 这个函数对所有类型的 GE 都会调用（Instant、Duration、Infinite）
 */
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
	// 🔧 修改 - 处理移动速度变化
	else if (Attribute == GetMoveSpeedAttribute())
	{
		// Clamp 值
		NewValue = FMath::Max(NewValue, 0.0f);
        
		// ✨ 新增 - 立即同步到 CharacterMovementComponent
		// 这确保无论 GE 是什么类型，速度都会被正确应用
		AActor* OwningActor = GetOwningActor();
		if (OwningActor)
		{
			if (ACharacter* OwningChar = Cast<ACharacter>(OwningActor))
			{
				if (UCharacterMovementComponent* MoveComp = OwningChar->GetCharacterMovement())
				{
					MoveComp->MaxWalkSpeed = NewValue;
					UE_LOG(LogSGGameplay, Log, TEXT("🚀 %s 移动速度同步：%.1f"), 
						*OwningActor->GetName(), NewValue);
				}
			}
		}
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
	Super::PostGameplayEffectExecute(Data);

	// 获取上下文信息（用于后续可能的逻辑扩展）
	FGameplayEffectContextHandle Context = Data.EffectSpec.GetContext();
	UAbilitySystemComponent* Source = Context.GetOriginalInstigatorAbilitySystemComponent();
	AActor* TargetActor = nullptr;
	AController* TargetController = nullptr;
	ASG_UnitsBase* TargetUnit = nullptr; // 🔧 修正：使用 ASG_UnitsBase

	if (Data.Target.AbilityActorInfo.IsValid() && Data.Target.AbilityActorInfo->AvatarActor.IsValid())
	{
		TargetActor = Data.Target.AbilityActorInfo->AvatarActor.Get();
		TargetController = Data.Target.AbilityActorInfo->PlayerController.Get();
		TargetUnit = Cast<ASG_UnitsBase>(TargetActor); // 🔧 修正：Cast 到 ASG_UnitsBase
	}

	// ========== 1. 处理移动速度变化 (服务器端同步) ==========
	// ✨ 新增逻辑：当 MoveSpeed 属性改变时，直接修改 CharacterMovementComponent
	if (Data.EvaluatedData.Attribute == GetMoveSpeedAttribute())
	{
		if (TargetActor)
		{
			// 尝试获取 CharacterMovementComponent
			UCharacterMovementComponent* MoveComp = nullptr;
			
			// 优先通过 ACharacter 接口获取（最快）
			if (ACharacter* Char = Cast<ACharacter>(TargetActor))
			{
				MoveComp = Char->GetCharacterMovement();
			}
			// 备选方案：直接查找组件
			else
			{
				MoveComp = TargetActor->FindComponentByClass<UCharacterMovementComponent>();
			}

			// 应用速度
			if (MoveComp)
			{
				MoveComp->MaxWalkSpeed = GetMoveSpeed();
				// UE_LOG(LogSGGameplay, Verbose, TEXT("🚀 移速同步 (Server): %.1f"), GetMoveSpeed());
			}
		}
	}
	

	// 处理即将受到的伤害
	if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		// ✨ 新增 - 输出详细调试信息
		UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
		UE_LOG(LogSGGameplay, Log, TEXT("💥 伤害处理：%s"), *GetOwningActor()->GetName());
		
		// 获取伤害值（修改前）
		const float IncomingDamageBefore = GetIncomingDamage();
		UE_LOG(LogSGGameplay, Log, TEXT("  IncomingDamage（修改前）：%.2f"), IncomingDamageBefore);
		
		// 获取当前生命值
		const float HealthBefore = GetHealth();
		UE_LOG(LogSGGameplay, Log, TEXT("  生命值（修改前）：%.2f / %.2f"), HealthBefore, GetMaxHealth());
		
		// 保存伤害值到局部变量
		const float LocalIncomingDamage = IncomingDamageBefore;
		
		// ✅ 关键：立即清空 IncomingDamage
		SetIncomingDamage(0.0f);
		
		// ✨ 新增 - 验证是否清空成功
		const float IncomingDamageAfter = GetIncomingDamage();
		UE_LOG(LogSGGameplay, Log, TEXT("  IncomingDamage（清空后）：%.2f"), IncomingDamageAfter);
		
		if (IncomingDamageAfter != 0.0f)
		{
			UE_LOG(LogSGGameplay, Error, TEXT("  ❌ IncomingDamage 清空失败！"));
		}

		// 只处理正数伤害
		if (LocalIncomingDamage > 0.0f)
		{
			// 计算新的生命值
			const float NewHealth = HealthBefore - LocalIncomingDamage;
			
			// 设置生命值
			SetHealth(FMath::Clamp(NewHealth, 0.0f, GetMaxHealth()));
			
			// 获取修改后的生命值
			const float HealthAfter = GetHealth();
			
			// 输出日志
			UE_LOG(LogSGGameplay, Log, TEXT("  实际伤害：%.2f"), LocalIncomingDamage);
			UE_LOG(LogSGGameplay, Log, TEXT("  生命值（修改后）：%.2f / %.2f"), HealthAfter, GetMaxHealth());
			UE_LOG(LogSGGameplay, Log, TEXT("  生命值变化：%.2f → %.2f (-%0.2f)"), 
				HealthBefore, HealthAfter, HealthBefore - HealthAfter);

			// 检测死亡
			if (HealthAfter <= 0.0f && HealthBefore > 0.0f)
			{
				UE_LOG(LogSGGameplay, Warning, TEXT("  ✗ 单位死亡"));
				// ✅ 死亡检测在 ASG_UnitsBase::OnHealthChanged 中处理
			}
		}
		else
		{
			UE_LOG(LogSGGameplay, Warning, TEXT("  ⚠️ 伤害值为 0，跳过处理"));
		}
		
		UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
	}
	// 确保Health不超过MaxHealth
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
	

	// 🔧 修改 - 保持 OnRep 用于客户端预测修正
	// 虽然 PostGameplayEffectExecute 处理了服务器，但客户端通过 RepNotify 获取最新值
	// 可以保证在网络同步时客户端的 CMC 也能被正确修正
	if (GetOwningActor())
	{
		// 优先使用 Cast<ACharacter> 获取 CMC，比 FindComponentByClass 更快
		if (ACharacter* OwningChar = Cast<ACharacter>(GetOwningActor()))
		{
			if (UCharacterMovementComponent* MoveComp = OwningChar->GetCharacterMovement())
			{
				MoveComp->MaxWalkSpeed = GetMoveSpeed();
			}
		}
		else if (UCharacterMovementComponent* MoveComp = GetOwningActor()->FindComponentByClass<UCharacterMovementComponent>())
		{
			MoveComp->MaxWalkSpeed = GetMoveSpeed();
		}
	}
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