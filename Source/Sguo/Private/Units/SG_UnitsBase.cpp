// Fill out your copyright notice in the Description page of Project Settings.


#include "Units/SG_UnitsBase.h"

#include "Debug/SG_LogCategories.h"
#include "AbilitySystem/SG_AbilitySystemComponent.h"
#include "AbilitySystem/SG_AttributeSet.h"
#include "GameFramework/CharacterMovementComponent.h"  // 必须包含
#include "Components/CapsuleComponent.h"                // 必须包含
#include "Kismet/GameplayStatics.h"     

// 构造函数
ASG_UnitsBase::ASG_UnitsBase()
{
	// 启用 Tick（如果需要的话）
	PrimaryActorTick.bCanEverTick = true;

	// 创建 Ability System Component
	// 为什么在构造函数创建：组件必须在构造时创建
	AbilitySystemComponent = CreateDefaultSubobject<USG_AbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	// 设置复制模式（单人游戏可以不设置，但为了扩展性还是设置）
	AbilitySystemComponent->SetIsReplicated(true);
	// 设置复制模式为 Mixed（适合大多数情况）
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	// 创建 Attribute Set
	// 为什么用 CreateDefaultSubobject：确保在构造时创建，支持网络复制
	AttributeSet = CreateDefaultSubobject<USG_AttributeSet>(TEXT("AttributeSet"));
}

// 获取 AbilitySystemComponent（GAS 接口）
UAbilitySystemComponent* ASG_UnitsBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

// BeginPlay
void ASG_UnitsBase::BeginPlay()
{
	Super::BeginPlay();
	
	UE_LOG(LogTemp, Log, TEXT("角色生成：%s"), *GetName());
}

// 被控制时调用
void ASG_UnitsBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	
	// 初始化 ASC（服务器端）
	if (AbilitySystemComponent)
	{
		// 设置 ASC 的 Owner 和 Avatar
		// Owner：拥有此 ASC 的 Actor（通常是 PlayerState 或 Character）
		// Avatar：ASC 作用的 Actor（通常是 Character）
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
		
		UE_LOG(LogTemp, Log, TEXT("✓ ASC 初始化完成：%s"), *GetName());
	}
}

/**
 * @brief 初始化角色
 * @param InFactionTag 阵营标签
 * @param HealthMultiplier 生命值倍率
 * @param DamageMultiplier 伤害倍率
 * @param SpeedMultiplier 速度倍率
 * @details
 * 功能说明：
 * - 设置阵营标签
 * - 初始化属性值
 * - 绑定属性变化委托
 * 详细流程：
 * 1. 保存阵营标签
 * 2. 初始化属性（在绑定委托之前）
 * 3. 绑定属性变化委托
 * 注意事项：
 * - 🔧 MODIFIED - 先初始化属性，再绑定委托，避免触发误判
 */
void ASG_UnitsBase::InitializeCharacter(
	FGameplayTag InFactionTag,
	float HealthMultiplier,
	float DamageMultiplier,
	float SpeedMultiplier)
{
	// 记录初始化开始
	UE_LOG(LogSGGameplay, Log, TEXT("========== 初始化角色：%s =========="), *GetName());
    
	// 设置阵营标签
	FactionTag = InFactionTag;
	UE_LOG(LogSGGameplay, Log, TEXT("  阵营：%s"), *FactionTag.ToString());
    
	// 🔧 MODIFIED - 先初始化属性
	InitializeAttributes(HealthMultiplier, DamageMultiplier, SpeedMultiplier);
    
	// 🔧 MODIFIED - 再绑定委托（此时属性已经是正确值）
	BindAttributeDelegates();
    
	// 记录初始化完成
	UE_LOG(LogSGGameplay, Log, TEXT("✓ 角色初始化完成"));
	UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
}

// 初始化属性
void ASG_UnitsBase::InitializeAttributes(float HealthMult, float DamageMult, float SpeedMult)
{
	// 检查 AttributeSet 是否有效
	if (!AttributeSet)
	{
		UE_LOG(LogTemp, Error, TEXT("✗ AttributeSet 为空，无法初始化属性！"));
		return;
	}

	// 计算最终属性值
	float FinalMaxHealth = BaseHealth * HealthMult;
	float FinalDamage = BaseAttackDamage * DamageMult;
	float FinalMoveSpeed = BaseMoveSpeed * SpeedMult;
	float FinalAttackSpeed = BaseAttackSpeed * SpeedMult;
	UE_LOG(LogTemp, Log, TEXT("============AttributeSet初始化属性开始============"));
	UE_LOG(LogTemp, Log, TEXT("  最大生命值：%.0f (基础: %.0f, 倍率: %.2f)"), FinalMaxHealth, BaseHealth, HealthMult);
	UE_LOG(LogTemp, Log, TEXT("  攻击力：%.0f (基础: %.0f, 倍率: %.2f)"), FinalDamage, BaseAttackDamage, DamageMult);
	UE_LOG(LogTemp, Log, TEXT("  移动速度：%.0f (基础: %.0f, 倍率: %.2f)"), FinalMoveSpeed, BaseMoveSpeed, SpeedMult);
	UE_LOG(LogTemp, Log, TEXT("  攻击速度：%.2f (基础: %.2f, 倍率: %.2f)"), FinalAttackSpeed, BaseAttackSpeed, SpeedMult);

	// 设置属性值
	// 注意：直接设置属性值，不使用 GameplayEffect（简化版本）	
	AttributeSet->SetMaxHealth(FinalMaxHealth);
	AttributeSet->SetHealth(FinalMaxHealth); // 初始满血
	AttributeSet->SetAttackDamage(FinalDamage);
	AttributeSet->SetMoveSpeed(FinalMoveSpeed);
	AttributeSet->SetAttackSpeed(FinalAttackSpeed);
	AttributeSet->SetAttackRange(BaseAttackRange);
	// 同步移动速度到 CharacterMovement 组件
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->MaxWalkSpeed = FinalMoveSpeed;
		UE_LOG(LogTemp, Verbose, TEXT("  ✓ 同步移动速度到 CharacterMovement"));
	}
	UE_LOG(LogTemp, Log, TEXT("============AttributeSet初始化属性结束============"));
}

// 绑定属性变化委托
void ASG_UnitsBase::BindAttributeDelegates()
{
	// 检查 ASC 和 AttributeSet 是否有效
	if (!AbilitySystemComponent || !AttributeSet)
	{
		UE_LOG(LogTemp, Warning, TEXT("⚠️ 无法绑定属性委托：ASC 或 AttributeSet 为空"));
		return;
	}

	// 监听生命值变化
	// GetGameplayAttributeValueChangeDelegate 返回一个委托，当属性变化时触发
	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(AttributeSet->GetHealthAttribute())
		.AddUObject(this, &ASG_UnitsBase::OnHealthChanged);
	
	UE_LOG(LogTemp, Verbose, TEXT("✓ 已绑定生命值变化委托"));
}

/**
 * @brief 生命值变化回调
 * @param Data 属性变化数据
 * @details
 * 功能说明：
 * - 监听生命值变化
 * - 检测单位死亡
 * 详细流程：
 * 1. 获取新旧生命值
 * 2. 输出日志
 * 3. 检测死亡条件
 * 注意事项：
 * - 🔧 MODIFIED - 增加初始化检测，避免误判
 */
void ASG_UnitsBase::OnHealthChanged(const FOnAttributeChangeData& Data)
{
	// 🔧 MODIFIED - 如果已经死亡，直接返回
	if (bIsDead)
	{
		return;
	}
	// 获取新的生命值
	float NewHealth = Data.NewValue;
	// 获取最大生命值
	float MaxHealth = AttributeSet->GetMaxHealth();
    
	// 输出生命值变化日志
	UE_LOG(LogSGGameplay, Verbose, TEXT("%s 生命值变化：%.0f / %.0f (旧值: %.0f)"), 
		*GetName(), NewHealth, MaxHealth, Data.OldValue);

	// 🔧 MODIFIED - 增强死亡判断
	// 条件1：新生命值 <= 0
	// 条件2：旧生命值 > 0（避免初始化时误判）
	// 条件3：最大生命值 > 0（确保已初始化）
	// 条件4：不是从 0 变为满血（初始化情况）
	bool bIsDeath = (NewHealth <= 0.0f) && 
					(Data.OldValue > 0.0f) && 
					(MaxHealth > 0.0f) &&
					!(Data.OldValue == 0.0f && NewHealth == MaxHealth);
    
	// 检测死亡
	if (bIsDeath)
	{
		// 输出死亡日志
		UE_LOG(LogSGGameplay, Warning, TEXT("✗ %s 死亡"), *GetName());
		// 调用死亡处理
		OnDeath();
	}
}

// 死亡处理
void ASG_UnitsBase::OnDeath_Implementation()
{
	// 🔧 MODIFIED - 设置死亡标记
	bIsDead = true;
	
	// ✨ 新增 - 广播死亡事件（在最开始）
	UE_LOG(LogSGGameplay, Log, TEXT("📢 广播单位死亡事件：%s"), *GetName());
	OnUnitDeathEvent.Broadcast(this);
	
	// 输出死亡日志
	UE_LOG(LogSGGameplay, Log, TEXT("========== %s 执行死亡逻辑 =========="), *GetName());
    
	// 禁用碰撞
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		// 禁用碰撞
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		// 输出日志
		UE_LOG(LogSGGameplay, Verbose, TEXT("  ✓ 禁用碰撞"));
	}

	// 禁用输入（如果是玩家控制）
	if (AController* Ctrl = GetController())
	{
		// 禁用输入
		DisableInput(Cast<APlayerController>(Ctrl));
		// 输出日志
		UE_LOG(LogSGGameplay, Verbose, TEXT("  ✓ 禁用输入"));
	}

	// TODO: 播放死亡动画
	// TODO: 播放死亡音效
	// TODO: 生成掉落物

	// 延迟销毁（给动画播放时间）
	SetLifeSpan(5.0f);
	// 输出日志
	UE_LOG(LogSGGameplay, Log, TEXT("  将在 5 秒后销毁"));
}

// 查找最近的目标
AActor* ASG_UnitsBase::FindNearestTarget()
{
	// 获取所有角色
	TArray<AActor*> AllCharacters;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASG_UnitsBase::StaticClass(), AllCharacters);

	AActor* NearestEnemy = nullptr;
	float MinDistance = FLT_MAX; // 最大浮点数

	// 遍历所有角色
	for (AActor* Actor : AllCharacters)
	{
		// 排除自己
		if (Actor == this)
		{
			continue;
		}

		// 转换为角色类型
		ASG_UnitsBase* OtherCharacter = Cast<ASG_UnitsBase>(Actor);
		if (!OtherCharacter)
		{
			continue;
		}

		// 检查阵营（不同阵营才是敌人）
		if (OtherCharacter->FactionTag != this->FactionTag)
		{
			// 计算距离
			float Distance = FVector::Dist(GetActorLocation(), OtherCharacter->GetActorLocation());
			
			// 更新最近敌人
			if (Distance < MinDistance)
			{
				MinDistance = Distance;
				NearestEnemy = OtherCharacter;
			}
		}
	}

	// 如果找到敌人，返回
	if (NearestEnemy)
	{
		UE_LOG(LogTemp, Verbose, TEXT("%s 找到最近的敌人：%s (距离: %.0f)"), 
			*GetName(), *NearestEnemy->GetName(), MinDistance);
		return NearestEnemy;
	}

	// 如果没有敌人，查找敌方主城
	// TODO: 实现查找主城逻辑
	UE_LOG(LogTemp, Verbose, TEXT("%s 未找到敌人，尝试查找敌方主城"), *GetName());
	
	return nullptr;
}

// 设置目标
void ASG_UnitsBase::SetTarget(AActor* NewTarget)
{
	if (NewTarget != CurrentTarget)
	{
		CurrentTarget = NewTarget;
		
		if (CurrentTarget)
		{
			UE_LOG(LogTemp, Log, TEXT("%s 切换目标：%s"), *GetName(), *CurrentTarget->GetName());
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("%s 清空目标"), *GetName());
		}
	}
}