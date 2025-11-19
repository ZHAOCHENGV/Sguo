// Fill out your copyright notice in the Description page of Project Settings.


#include "Units/SG_UnitsBase.h"

#include "Debug/SG_LogCategories.h"
#include "AbilitySystem/SG_AbilitySystemComponent.h"
#include "AbilitySystem/SG_AttributeSet.h"
#include "GameFramework/CharacterMovementComponent.h"  // 必须包含
#include "Components/CapsuleComponent.h"                // 必须包含
#include "Kismet/GameplayStatics.h"
// ✨ 新增 - DataTable 相关头文件
#include "Data/Type/SG_UnitDataTable.h"
#include "Engine/DataTable.h"
// ✨ 新增 - Gameplay Ability 相关头文件
#include "AbilitySystemGlobals.h"
#include "Abilities/GameplayAbility.h"     

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
	
	// ✨ 新增 - 从 DataTable 加载配置
	// 如果启用了 DataTable 配置，在初始化前加载数据
	if (bUseDataTable && UnitDataTable && !UnitDataRowName.IsNone())
	{
		LoadUnitDataFromTable();
	}
	
	// ✨ 新增 - 授予攻击能力
	// 在初始化后授予攻击能力
	GrantAttackAbility();
	
	// ✨ 新增 - 自动生成AI控制器
	if (bUseAIController && !Controller && AIControllerClass)
	{
		// 使用SpawnDefaultController会调用AIControllerClass
		AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
		SpawnDefaultController();
		UE_LOG(LogSGGameplay, Log, TEXT("✅ 自动生成AI控制器：%s"), *AIControllerClass->GetName());
	}
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

// ========== ✨ 新增 - DataTable 相关函数实现 ==========

/**
 * @brief 从 DataTable 加载单位配置
 * @details
 * 功能说明：
 * - 从 DataTable 读取指定行的数据
 * - 应用属性到 BaseHealth、BaseAttackDamage 等
 * - 应用攻击配置（攻击动画、投射物类等）
 * 详细流程：
 * 1. 检查 DataTable 和行名称是否有效
 * 2. 从 DataTable 查找指定行
 * 3. 读取属性值并覆盖基础属性
 * 4. 读取攻击配置
 * 注意事项：
 * - 在 InitializeCharacter() 之前调用
 * - 如果 bUseDataTable = false，不会执行
 */
void ASG_UnitsBase::LoadUnitDataFromTable()
{
	// ========== 步骤1：检查有效性 ==========
	// 检查 DataTable 是否有效
	if (!UnitDataTable)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("✗ %s: UnitDataTable 为空！"), *GetName());
		return;
	}
	
	// 检查行名称是否有效
	if (UnitDataRowName.IsNone())
	{
		UE_LOG(LogSGGameplay, Error, TEXT("✗ %s: UnitDataRowName 为空！"), *GetName());
		return;
	}
	
	// ========== 步骤2：查找 DataTable 行 ==========
	// 从 DataTable 查找指定行
	// FindRow 是 UDataTable 的模板函数，返回指定行的数据指针
	FSGUnitDataRow* RowData = UnitDataTable->FindRow<FSGUnitDataRow>(
		UnitDataRowName,
		TEXT("LoadUnitDataFromTable")  // 用于错误日志的上下文
	);
	
	// 检查是否找到数据
	if (!RowData)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("✗ %s: 在 DataTable 中找不到行 '%s'！"), 
			*GetName(), *UnitDataRowName.ToString());
		return;
	}
	
	// 输出日志
	UE_LOG(LogSGGameplay, Log, TEXT("========== 从 DataTable 加载单位配置 =========="));
	UE_LOG(LogSGGameplay, Log, TEXT("  单位：%s"), *GetName());
	UE_LOG(LogSGGameplay, Log, TEXT("  数据行：%s"), *UnitDataRowName.ToString());
	UE_LOG(LogSGGameplay, Log, TEXT("  单位名称：%s"), *RowData->UnitName.ToString());
	
	// ========== 步骤3：应用属性值 ==========
	// 从 DataTable 读取的值会覆盖 Blueprint 中设置的 Base 值
	BaseHealth = RowData->BaseHealth;
	BaseAttackDamage = RowData->BaseAttackDamage;
	BaseMoveSpeed = RowData->BaseMoveSpeed;
	BaseAttackSpeed = RowData->BaseAttackSpeed;
	BaseAttackRange = RowData->BaseAttackRange;
	
	// 输出日志
	UE_LOG(LogSGGameplay, Log, TEXT("  属性配置："));
	UE_LOG(LogSGGameplay, Log, TEXT("    生命值：%.0f"), BaseHealth);
	UE_LOG(LogSGGameplay, Log, TEXT("    攻击力：%.0f"), BaseAttackDamage);
	UE_LOG(LogSGGameplay, Log, TEXT("    移动速度：%.0f"), BaseMoveSpeed);
	UE_LOG(LogSGGameplay, Log, TEXT("    攻击速度：%.2f"), BaseAttackSpeed);
	UE_LOG(LogSGGameplay, Log, TEXT("    攻击范围：%.0f"), BaseAttackRange);
	
	// ========== 步骤4：应用攻击配置 ==========
	// 应用单位类型标签
	if (RowData->UnitTypeTag.IsValid())
	{
		UnitTypeTag = RowData->UnitTypeTag;
		UE_LOG(LogSGGameplay, Log, TEXT("  单位类型：%s"), *UnitTypeTag.ToString());
	}
	
	// 应用攻击动画
	if (RowData->AttackMontage)
	{
		AttackMontage = RowData->AttackMontage;
		UE_LOG(LogSGGameplay, Log, TEXT("  攻击动画：%s"), *AttackMontage->GetName());
	}
	
	// 应用投射物类（仅远程单位）
	if (RowData->AttackType != ESGUnitAttackType::Melee && RowData->ProjectileClass)
	{
		ProjectileClass = RowData->ProjectileClass;
		UE_LOG(LogSGGameplay, Log, TEXT("  投射物类：%s"), *ProjectileClass->GetName());
	}
	
	// 输出日志
	UE_LOG(LogSGGameplay, Log, TEXT("✓ 单位配置加载完成"));
	UE_LOG(LogSGGameplay, Log, TEXT("==============================================="));
}

// ========== ✨ 新增 - 攻击系统函数实现 ==========

/**
 * @brief 授予攻击能力
 * @details
 * 功能说明：
 * - 根据单位类型授予对应的攻击 Gameplay Ability
 * - 近战单位使用 GA_Attack_Melee
 * - 远程单位使用 GA_Attack_Ranged
 * 详细流程：
 * 1. 检查 ASC 是否有效
 * 2. 根据 UnitTypeTag 确定攻击类型
 * 3. 创建 Ability Spec 并授予能力
 * 4. 缓存 Ability Handle 供后续使用
 * 注意事项：
 * - 在 BeginPlay 中自动调用
 * - 需要先配置 UnitTypeTag
 */
void ASG_UnitsBase::GrantAttackAbility()
{
	// ========== 步骤1：检查 ASC 是否有效 ==========
	if (!AbilitySystemComponent)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("✗ %s: AbilitySystemComponent 为空，无法授予攻击能力！"), *GetName());
		return;
	}
	
	// ========== 步骤2：确定攻击能力类 ==========
	// ✨ 新增 - 支持多种方式配置攻击能力类
	// 优先级：
	// 1. AttackAbilityClass（Blueprint 中直接配置）
	// 2. 根据 UnitTypeTag 自动选择（默认行为）
	TSubclassOf<UGameplayAbility> AbilityClassToGrant = AttackAbilityClass;
	
	// 如果没有在 Blueprint 中配置，则根据单位类型自动选择
	if (!AbilityClassToGrant)
	{
		UE_LOG(LogSGGameplay, Log, TEXT("  %s: 未配置 AttackAbilityClass，根据 UnitTypeTag 自动选择"), *GetName());
		
		// 🔧 修改 - 使用可选的 GameplayTag（避免未配置时报错）
		FGameplayTag InfantryTag = FGameplayTag::RequestGameplayTag(FName("Unit.Type.Infantry"), false);
		FGameplayTag CavalryTag = FGameplayTag::RequestGameplayTag(FName("Unit.Type.Cavalry"), false);
		FGameplayTag ArcherTag = FGameplayTag::RequestGameplayTag(FName("Unit.Type.Archer"), false);
		FGameplayTag CrossbowTag = FGameplayTag::RequestGameplayTag(FName("Unit.Type.Crossbow"), false);
		
		if ((InfantryTag.IsValid() && UnitTypeTag.MatchesTag(InfantryTag)) ||
			(CavalryTag.IsValid() && UnitTypeTag.MatchesTag(CavalryTag)))
		{
			// 近战单位 - 尝试加载默认近战攻击能力
			UE_LOG(LogSGGameplay, Log, TEXT("  %s 为近战单位，尝试加载默认 GA_Attack_Melee"), *GetName());
			
			AbilityClassToGrant = LoadClass<UGameplayAbility>(
				nullptr,
				TEXT("/Game/Blueprints/GAS/Abilities/GA_Attack_Melee.GA_Attack_Melee_C")
			);
			
			if (!AbilityClassToGrant)
			{
				UE_LOG(LogSGGameplay, Warning, TEXT("⚠️ %s: 默认 GA_Attack_Melee 不存在，请在 Blueprint 中手动配置 AttackAbilityClass"), *GetName());
			}
		}
		else if ((ArcherTag.IsValid() && UnitTypeTag.MatchesTag(ArcherTag)) ||
				 (CrossbowTag.IsValid() && UnitTypeTag.MatchesTag(CrossbowTag)))
		{
			// 远程单位 - 尝试加载默认远程攻击能力
			UE_LOG(LogSGGameplay, Log, TEXT("  %s 为远程单位，尝试加载默认 GA_Attack_Ranged"), *GetName());
			
			AbilityClassToGrant = LoadClass<UGameplayAbility>(
				nullptr,
				TEXT("/Game/Blueprints/GAS/Abilities/GA_Attack_Ranged.GA_Attack_Ranged_C")
			);
			
			if (!AbilityClassToGrant)
			{
				UE_LOG(LogSGGameplay, Warning, TEXT("⚠️ %s: 默认 GA_Attack_Ranged 不存在，请在 Blueprint 中手动配置 AttackAbilityClass"), *GetName());
			}
		}
		else
		{
			UE_LOG(LogSGGameplay, Warning, TEXT("⚠️ %s: 未知的单位类型 '%s'，且未配置 AttackAbilityClass"), 
				*GetName(), *UnitTypeTag.ToString());
		}
	}
	else
	{
		// 使用 Blueprint 中配置的攻击能力类
		UE_LOG(LogSGGameplay, Log, TEXT("  %s: 使用 Blueprint 配置的 AttackAbilityClass: %s"), 
			*GetName(), *AbilityClassToGrant->GetName());
	}
	
	// ========== 步骤3：授予能力 ==========
	if (AbilityClassToGrant)
	{
		// 创建 Ability Spec
		FGameplayAbilitySpec AbilitySpec(
			AbilityClassToGrant,  // 能力类
			1,                    // 能力等级
			INDEX_NONE,           // 输入ID（不使用输入绑定）
			this                  // 能力的 Source Object
		);
		
		// 授予能力并缓存 Handle
		GrantedAttackAbilityHandle = AbilitySystemComponent->GiveAbility(AbilitySpec);
		
		// 输出日志
		UE_LOG(LogSGGameplay, Log, TEXT("✓ %s: 授予攻击能力成功 (类: %s, Handle: %s)"), 
			*GetName(), *AbilityClassToGrant->GetName(), *GrantedAttackAbilityHandle.ToString());
	}
	else
	{
		UE_LOG(LogSGGameplay, Warning, TEXT("⚠️ %s: 无法确定攻击能力类，跳过授予"), *GetName());
		UE_LOG(LogSGGameplay, Warning, TEXT("  提示：请在单位 Blueprint 中配置 'Attack Config → 攻击能力类'"));
	}
}

/**
 * @brief 执行攻击
 * @details
 * 功能说明：
 * - 触发已授予的攻击能力
 * - 供 AI 或玩家输入调用
 * 详细流程：
 * 1. 检查 ASC 和攻击能力是否有效
 * 2. 检查能力是否可以激活（冷却、成本等）
 * 3. 激活攻击能力
 * 注意事项：
 * - 在 StateTree AI 中调用
 * - 需要先调用 GrantAttackAbility()
 * @return 是否成功触发攻击
 */
bool ASG_UnitsBase::PerformAttack()
{
	// ========== 步骤1：检查 ASC 是否有效 ==========
	if (!AbilitySystemComponent)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("✗ %s: AbilitySystemComponent 为空，无法执行攻击！"), *GetName());
		return false;
	}
	
	// ========== 步骤2：检查攻击能力是否已授予 ==========
	if (!GrantedAttackAbilityHandle.IsValid())
	{
		UE_LOG(LogSGGameplay, Warning, TEXT("⚠️ %s: 攻击能力未授予，无法执行攻击！"), *GetName());
		return false;
	}
	
	// ========== 步骤3：检查能力是否可以激活 ==========
	// FindAbilitySpecFromHandle 查找能力规格
	FGameplayAbilitySpec* AbilitySpec = AbilitySystemComponent->FindAbilitySpecFromHandle(GrantedAttackAbilityHandle);
	if (!AbilitySpec)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("✗ %s: 找不到攻击能力 Spec！"), *GetName());
		return false;
	}
	
	// 检查能力是否可以激活（检查冷却、成本、Tag 限制等）
	if (!AbilitySpec->Ability->CanActivateAbility(
		GrantedAttackAbilityHandle,
		AbilitySystemComponent->AbilityActorInfo.Get()))
	{
		UE_LOG(LogSGGameplay, Verbose, TEXT("⚠️ %s: 攻击能力无法激活（可能在冷却中）"), *GetName());
		return false;
	}
	
	// ========== 步骤4：激活攻击能力 ==========
	// TryActivateAbility 尝试激活能力
	bool bSuccess = AbilitySystemComponent->TryActivateAbility(GrantedAttackAbilityHandle);
	
	// 输出日志
	if (bSuccess)
	{
		UE_LOG(LogSGGameplay, Verbose, TEXT("✓ %s: 触发攻击"), *GetName());
	}
	else
	{
		UE_LOG(LogSGGameplay, Warning, TEXT("⚠️ %s: 触发攻击失败"), *GetName());
	}
	
	return bSuccess;
}

/**
 * @brief 检查当前目标是否有效
 * @details
 * 功能说明：
 * - 检查目标是否存在、是否存活、是否在范围内
 * 详细流程：
 * 1. 检查 CurrentTarget 是否为空
 * 2. 检查目标是否已死亡
 * 3. 检查目标是否仍在攻击范围内
 * 注意事项：
 * - 在 AI 中每帧检查
 * - 如果无效，需要重新查找目标
 * @return 目标是否有效
 */
bool ASG_UnitsBase::IsTargetValid() const
{
	// ========== 步骤1：检查目标是否为空 ==========
	if (!CurrentTarget)
	{
		return false;
	}
	
	// ========== 步骤2：检查目标是否已死亡 ==========
	// 尝试转换为 ASG_UnitsBase
	const ASG_UnitsBase* TargetUnit = Cast<ASG_UnitsBase>(CurrentTarget);
	if (TargetUnit)
	{
		// 如果目标已死亡，返回 false
		if (TargetUnit->bIsDead)
		{
			return false;
		}
		
		// 如果目标生命值 <= 0，返回 false
		if (TargetUnit->AttributeSet && TargetUnit->AttributeSet->GetHealth() <= 0.0f)
		{
			return false;
		}
	}
	
	// ========== 步骤3：检查目标是否在攻击范围内 ==========
	// 计算与目标的距离
	float DistanceToTarget = FVector::Dist(GetActorLocation(), CurrentTarget->GetActorLocation());
	
	// 获取攻击范围（从 AttributeSet 获取）
	float AttackRange = BaseAttackRange;
	if (AttributeSet)
	{
		AttackRange = AttributeSet->GetAttackRange();
	}
	
	// 添加一些容差（避免边界抖动）
	float RangeTolerance = 50.0f;
	
	// 如果距离超出攻击范围 + 容差，返回 false
	if (DistanceToTarget > AttackRange + RangeTolerance)
	{
		return false;
	}
	
	// ========== 所有检查通过，目标有效 ==========
	return true;
}