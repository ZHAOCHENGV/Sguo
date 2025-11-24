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
// ✨ 新增 - 调试可视化相关头文件
#include "AIController.h"
#include "DrawDebugHelpers.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Data/SG_CharacterCardData.h"
#include "Data/Type/SG_UnitDataTable.h" // ✨ 新增 - 包含完整定义
// 构造函数
ASG_UnitsBase::ASG_UnitsBase()
{
	// 🔧 修改 - 启用 Tick（用于调试可视化）
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

/**
 * @brief 设置源卡牌数据
 * @param CardData 卡牌数据
 * @details
 * 功能说明：
 * - 缓存卡牌数据引用
 * - 在生成单位后立即调用
 */
void ASG_UnitsBase::SetSourceCardData(USG_CharacterCardData* CardData)
{
	SourceCardData = CardData;
    
	if (CardData)
	{
		UE_LOG(LogSGGameplay, Log, TEXT("✓ %s: 设置源卡牌数据：%s"), 
			*GetName(), *CardData->GetName());
		UE_LOG(LogSGGameplay, Log, TEXT("  生命值倍率：%.2f"), CardData->HealthMultiplier);
		UE_LOG(LogSGGameplay, Log, TEXT("  伤害倍率：%.2f"), CardData->DamageMultiplier);
		UE_LOG(LogSGGameplay, Log, TEXT("  速度倍率：%.2f"), CardData->SpeedMultiplier);
	}
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
    
    UE_LOG(LogSGGameplay, Log, TEXT("========== 单位生成：%s =========="), *GetName());
    
    // ========== 步骤1：检查是否已初始化 ==========
    bool bNeedsInitialization = false;
    
    if (!AttributeSet)
    {
        UE_LOG(LogSGGameplay, Error, TEXT("❌ %s: AttributeSet 为空！"), *GetName());
        return;
    }
    
    if (AttributeSet->GetMaxHealth() <= 0.0f)
    {
        bNeedsInitialization = true;
        UE_LOG(LogSGGameplay, Log, TEXT("  检测到未初始化的单位"));
    }
    else
    {
        UE_LOG(LogSGGameplay, Log, TEXT("  单位已初始化（MaxHealth: %.0f）"), 
            AttributeSet->GetMaxHealth());
    }
    
    // ========== 步骤2：根据配置选择初始化方式 ==========
    if (bNeedsInitialization)
    {
        // ========== 🔧 关键修改 - 先加载 DataTable，再应用倍率 ==========
        
        if (bUseDataTable)
        {
            UE_LOG(LogSGGameplay, Log, TEXT("  使用 DataTable 初始化"));
            
            // 🔧 修改 - 先加载 DataTable 基础属性
            bool bLoadSuccess = IsLoadUnitDataFromTable();
            
            if (bLoadSuccess)
            {
                // ✨ 新增 - 从卡牌数据读取倍率
                float HealthMult = 1.0f;
                float DamageMult = 1.0f;
                float SpeedMult = 1.0f;
                
                if (SourceCardData)
                {
                    HealthMult = SourceCardData->HealthMultiplier;
                    DamageMult = SourceCardData->DamageMultiplier;
                    SpeedMult = SourceCardData->SpeedMultiplier;
                    
                    UE_LOG(LogSGGameplay, Log, TEXT("  从卡牌数据读取倍率："));
                    UE_LOG(LogSGGameplay, Log, TEXT("    卡牌：%s"), *SourceCardData->GetName());
                    UE_LOG(LogSGGameplay, Log, TEXT("    生命值倍率：%.2f"), HealthMult);
                    UE_LOG(LogSGGameplay, Log, TEXT("    伤害倍率：%.2f"), DamageMult);
                    UE_LOG(LogSGGameplay, Log, TEXT("    速度倍率：%.2f"), SpeedMult);
                }
                else
                {
                    UE_LOG(LogSGGameplay, Log, TEXT("  未设置卡牌数据，使用默认倍率（1.0）"));
                }
                
                // 🔧 关键修改 - 应用倍率到基础属性
                UE_LOG(LogSGGameplay, Log, TEXT("  应用倍率前的基础属性："));
                UE_LOG(LogSGGameplay, Log, TEXT("    BaseHealth: %.0f"), BaseHealth);
                UE_LOG(LogSGGameplay, Log, TEXT("    BaseAttackDamage: %.0f"), BaseAttackDamage);
                UE_LOG(LogSGGameplay, Log, TEXT("    BaseMoveSpeed: %.0f"), BaseMoveSpeed);
                UE_LOG(LogSGGameplay, Log, TEXT("    BaseAttackSpeed: %.2f"), BaseAttackSpeed);
                
                // 应用倍率到基础属性
                BaseHealth *= HealthMult;
                BaseAttackDamage *= DamageMult;
                BaseMoveSpeed *= SpeedMult;
                BaseAttackSpeed *= SpeedMult;
                
                UE_LOG(LogSGGameplay, Log, TEXT("  应用倍率后的基础属性："));
                UE_LOG(LogSGGameplay, Log, TEXT("    BaseHealth: %.0f"), BaseHealth);
                UE_LOG(LogSGGameplay, Log, TEXT("    BaseAttackDamage: %.0f"), BaseAttackDamage);
                UE_LOG(LogSGGameplay, Log, TEXT("    BaseMoveSpeed: %.0f"), BaseMoveSpeed);
                UE_LOG(LogSGGameplay, Log, TEXT("    BaseAttackSpeed: %.2f"), BaseAttackSpeed);
                
                // 初始化角色（倍率已经应用到 Base 属性，所以这里传 1.0）
                FGameplayTag InitFactionTag = DetermineFactionTag();
                InitializeCharacter(InitFactionTag, 1.0f, 1.0f, 1.0f);
                
                UE_LOG(LogSGGameplay, Log, TEXT("  ✓ DataTable + 倍率初始化完成"));
            }
            else
            {
                UE_LOG(LogSGGameplay, Warning, TEXT("  ⚠️ DataTable 加载失败，回退到默认初始化"));
                InitializeWithDefaults();
            }
        }
        else
        {
            UE_LOG(LogSGGameplay, Log, TEXT("  使用默认值初始化"));
            
            // ✨ 新增 - 从卡牌数据读取倍率
            float HealthMult = 1.0f;
            float DamageMult = 1.0f;
            float SpeedMult = 1.0f;
            
            if (SourceCardData)
            {
                HealthMult = SourceCardData->HealthMultiplier;
                DamageMult = SourceCardData->DamageMultiplier;
                SpeedMult = SourceCardData->SpeedMultiplier;
                
                UE_LOG(LogSGGameplay, Log, TEXT("  从卡牌数据读取倍率"));
            }
            
            // 应用倍率到基础属性
            BaseHealth *= HealthMult;
            BaseAttackDamage *= DamageMult;
            BaseMoveSpeed *= SpeedMult;
            BaseAttackSpeed *= SpeedMult;
            
            // 初始化角色（倍率已经应用到 Base 属性，所以这里传 1.0）
            FGameplayTag InitFactionTag = DetermineFactionTag();
            InitializeCharacter(InitFactionTag, 1.0f, 1.0f, 1.0f);
        }
    }
    
    // ========== 步骤3：加载攻击技能配置 ==========
    if (bUseDataTable)
    {
        LoadAttackAbilitiesFromDataTable();
    }
    
    // ========== 步骤4：授予通用攻击能力 ==========
    GrantCommonAttackAbility();
    
    UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
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
 * @param HealthMultiplier 生命值倍率（已废弃，保留兼容性）
 * @param DamageMultiplier 伤害倍率（已废弃，保留兼容性）
 * @param SpeedMultiplier 速度倍率（已废弃，保留兼容性）
 * @details
 * 功能说明：
 * - 设置阵营标签
 * - 初始化属性值（使用已应用倍率的 Base 属性）
 * - 绑定属性变化委托
 * 注意事项：
 * - 倍率应该在调用此函数之前应用到 Base 属性
 * - 此函数的倍率参数已废弃，保留是为了向后兼容
 */
void ASG_UnitsBase::InitializeCharacter(
	FGameplayTag InFactionTag,
	float HealthMultiplier,
	float DamageMultiplier,
	float SpeedMultiplier)
{
	UE_LOG(LogSGGameplay, Log, TEXT("========== 初始化角色：%s =========="), *GetName());
    
	// 设置阵营标签
	FactionTag = InFactionTag;
	UE_LOG(LogSGGameplay, Log, TEXT("  阵营：%s"), *FactionTag.ToString());
    
	// 🔧 修改 - 直接使用 Base 属性（倍率已经应用）
	InitializeAttributes(1.0f, 1.0f, 1.0f);
    
	// 绑定委托
	BindAttributeDelegates();
    
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
	SetLifeSpan(2.0f);
	// 输出日志
	UE_LOG(LogSGGameplay, Log, TEXT("  将在 2 秒后销毁"));
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
 * @brief 从 DataTable 加载攻击技能配置
 * @details
 * 功能说明：
 * - 从 DataTable 读取攻击技能列表
 * - 缓存到 CachedAttackAbilities
 * - 为后续随机选择攻击做准备
 */
void ASG_UnitsBase::LoadAttackAbilitiesFromDataTable()
{
	 // ========== 步骤1：检查有效性 ==========
    if (!UnitDataTable)
    {
        UE_LOG(LogSGGameplay, Error, TEXT("❌ %s: UnitDataTable 为空！"), *GetName());
        return;
    }
    
    if (UnitDataRowName.IsNone())
    {
        UE_LOG(LogSGGameplay, Error, TEXT("❌ %s: UnitDataRowName 为空！"), *GetName());
        return;
    }
    
    // ========== 步骤2：查找 DataTable 行 ==========
    FSGUnitDataRow* RowData = UnitDataTable->FindRow<FSGUnitDataRow>(
        UnitDataRowName,
        TEXT("LoadAttackAbilitiesFromDataTable")
    );
    
    if (!RowData)
    {
        UE_LOG(LogSGGameplay, Error, TEXT("❌ %s: 在 DataTable 中找不到行 '%s'！"), 
            *GetName(), *UnitDataRowName.ToString());
        return;
    }
    
    // ========== 步骤3：缓存攻击技能列表 ==========
    CachedAttackAbilities = RowData->Abilities;
    
    // ========== 步骤4：输出日志 ==========
    UE_LOG(LogSGGameplay, Log, TEXT("========== 加载攻击技能配置 =========="));
    UE_LOG(LogSGGameplay, Log, TEXT("  单位：%s"), *GetName());
    UE_LOG(LogSGGameplay, Log, TEXT("  攻击技能数量：%d"), CachedAttackAbilities.Num());
    
    for (int32 i = 0; i < CachedAttackAbilities.Num(); ++i)
    {
        const FSGUnitAttackDefinition& Ability = CachedAttackAbilities[i];
        
        UE_LOG(LogSGGameplay, Log, TEXT("  [%d] 攻击技能："), i);
        UE_LOG(LogSGGameplay, Log, TEXT("    动画：%s"), 
            Ability.Montage ? *Ability.Montage->GetName() : TEXT("未设置"));
        UE_LOG(LogSGGameplay, Log, TEXT("    攻击类型：%s"), 
            *UEnum::GetValueAsString(Ability.AttackType));
        UE_LOG(LogSGGameplay, Log, TEXT("    冷却时间：%.2f 秒"), Ability.Cooldown);
        
        if (Ability.SpecificAbilityClass)
        {
            UE_LOG(LogSGGameplay, Log, TEXT("    指定能力：%s"), 
                *Ability.SpecificAbilityClass->GetName());
        }
        
        if (Ability.AttackType != ESGUnitAttackType::Melee && Ability.ProjectileClass)
        {
            UE_LOG(LogSGGameplay, Log, TEXT("    投射物类：%s"), 
                *Ability.ProjectileClass->GetName());
        }
    }
    
    UE_LOG(LogSGGameplay, Log, TEXT("✓ 攻击技能配置加载完成"));
    UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
}
/**
 * @brief 授予通用攻击能力
 * @details
 * 功能说明：
 * - 根据单位类型授予通用 GA
 * - 所有攻击共享此 GA
 * - 通过传递不同的配置数据来实现不同的攻击效果
 */
void ASG_UnitsBase::GrantCommonAttackAbility()
{
		// ========== 步骤1：检查 ASC 是否有效 ==========
	if (!AbilitySystemComponent)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("❌ %s: AbilitySystemComponent 为空！"), *GetName());
		return;
	}
	
	// ========== 步骤2：确定通用攻击能力类 ==========
	TSubclassOf<UGameplayAbility> AbilityClassToGrant = CommonAttackAbilityClass;
	
	// 如果没有在 Blueprint 中配置，根据单位类型自动选择
	if (!AbilityClassToGrant)
	{
		UE_LOG(LogSGGameplay, Log, TEXT("  %s: 未配置 CommonAttackAbilityClass，根据 UnitTypeTag 自动选择"), *GetName());
		
		FGameplayTag InfantryTag = FGameplayTag::RequestGameplayTag(FName("Unit.Type.Infantry"), false);
		FGameplayTag CavalryTag = FGameplayTag::RequestGameplayTag(FName("Unit.Type.Cavalry"), false);
		FGameplayTag ArcherTag = FGameplayTag::RequestGameplayTag(FName("Unit.Type.Archer"), false);
		FGameplayTag CrossbowTag = FGameplayTag::RequestGameplayTag(FName("Unit.Type.Crossbow"), false);
		
		if ((InfantryTag.IsValid() && UnitTypeTag.MatchesTag(InfantryTag)) ||
			(CavalryTag.IsValid() && UnitTypeTag.MatchesTag(CavalryTag)))
		{
			// 近战单位 - 加载默认近战攻击能力
			AbilityClassToGrant = LoadClass<UGameplayAbility>(
				nullptr,
				TEXT("/Game/Blueprints/GAS/Abilities/GA_Attack_Melee.GA_Attack_Melee_C")
			);
			
			if (!AbilityClassToGrant)
			{
				UE_LOG(LogSGGameplay, Warning, TEXT("⚠️ %s: 默认 GA_Attack_Melee 不存在，请在 Blueprint 中手动配置 CommonAttackAbilityClass"), *GetName());
			}
		}
		else if ((ArcherTag.IsValid() && UnitTypeTag.MatchesTag(ArcherTag)) ||
				 (CrossbowTag.IsValid() && UnitTypeTag.MatchesTag(CrossbowTag)))
		{
			// 远程单位 - 加载默认远程攻击能力
			AbilityClassToGrant = LoadClass<UGameplayAbility>(
				nullptr,
				TEXT("/Game/Blueprints/GAS/Abilities/GA_Attack_Ranged.GA_Attack_Ranged_C")
			);
			
			if (!AbilityClassToGrant)
			{
				UE_LOG(LogSGGameplay, Warning, TEXT("⚠️ %s: 默认 GA_Attack_Ranged 不存在，请在 Blueprint 中手动配置 CommonAttackAbilityClass"), *GetName());
			}
		}
		else
		{
			UE_LOG(LogSGGameplay, Warning, TEXT("⚠️ %s: 未知的单位类型 '%s'，且未配置 CommonAttackAbilityClass"), 
				*GetName(), *UnitTypeTag.ToString());
		}
	}
	else
	{
		// 使用 Blueprint 中配置的攻击能力类
		UE_LOG(LogSGGameplay, Log, TEXT("  %s: 使用 Blueprint 配置的 CommonAttackAbilityClass: %s"), 
			*GetName(), *AbilityClassToGrant->GetName());
	}
	
	// ========== 步骤3：授予能力 ==========
	if (AbilityClassToGrant)
	{
		FGameplayAbilitySpec AbilitySpec(
			AbilityClassToGrant,
			1,
			INDEX_NONE,
			this
		);
		
		// 🔧 修改 - 变量名从 GrantedAttackAbilityHandle 改为 GrantedCommonAttackHandle
		GrantedCommonAttackHandle = AbilitySystemComponent->GiveAbility(AbilitySpec);
		
		UE_LOG(LogSGGameplay, Log, TEXT("✓ %s: 授予通用攻击能力成功 (类: %s)"), 
			*GetName(), *AbilityClassToGrant->GetName());
	}
	else
	{
		UE_LOG(LogSGGameplay, Warning, TEXT("⚠️ %s: 无法确定通用攻击能力类"), *GetName());
	}
}

// ========== ✨ 新增 - 攻击系统函数实现 ==========

/**
 * @brief 执行攻击（随机选择技能）
 * @return 是否成功触发攻击
 * @details
 * 功能说明：
 * - 从攻击技能列表中随机选择一个
 * - 如果指定了 SpecificAbilityClass，激活特定 GA
 * - 否则激活通用 GA 并传递配置数据
 * 详细流程：
 * 1. 检查攻击技能列表是否为空
 * 2. 随机选择一个攻击技能
 * 3. 更新当前攻击索引
 * 4. 激活对应的 GA
 */
bool ASG_UnitsBase::PerformAttack()
{
	UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
	UE_LOG(LogSGGameplay, Log, TEXT("🔫 %s 尝试执行攻击"), *GetName());
	
	// ========== ✨ 新增 - 步骤1：检查是否在冷却中 ==========
	if (bIsAttackOnCooldown)
	{
		UE_LOG(LogSGGameplay, Warning, TEXT("  ⏳ 攻击冷却中，剩余时间：%.2f 秒"), CooldownRemainingTime);
		UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
		return false;
	}
	
	// ========== 步骤2：检查攻击技能列表 ==========
	if (CachedAttackAbilities.Num() == 0)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("  ❌ 攻击技能列表为空！"));
		UE_LOG(LogSGGameplay, Error, TEXT("  提示：检查 DataTable 中是否配置了攻击技能"));
		UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
		return false;
	}
	
	// ========== 步骤3：随机选择攻击技能 ==========
	CurrentAttackIndex = FMath::RandRange(0, CachedAttackAbilities.Num() - 1);
	const FSGUnitAttackDefinition& SelectedAttack = CachedAttackAbilities[CurrentAttackIndex];
	
	UE_LOG(LogSGGameplay, Log, TEXT("  随机选择攻击技能 [%d/%d]"), 
		CurrentAttackIndex + 1, CachedAttackAbilities.Num());
	UE_LOG(LogSGGameplay, Log, TEXT("    动画：%s"), 
		SelectedAttack.Montage ? *SelectedAttack.Montage->GetName() : TEXT("未设置"));
	UE_LOG(LogSGGameplay, Log, TEXT("    攻击类型：%s"), 
		*UEnum::GetValueAsString(SelectedAttack.AttackType));
	UE_LOG(LogSGGameplay, Log, TEXT("    冷却时间：%.2f 秒"), SelectedAttack.Cooldown);
	
	// ========== 步骤4：检查 ASC 是否有效 ==========
	if (!AbilitySystemComponent)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("  ❌ AbilitySystemComponent 为空"));
		UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
		return false;
	}
	
	// ========== 步骤5：处理特定能力 ==========
	FGameplayAbilitySpecHandle AbilityHandleToActivate;
	
	if (SelectedAttack.SpecificAbilityClass)
	{
		UE_LOG(LogSGGameplay, Log, TEXT("  使用指定能力：%s"), 
			*SelectedAttack.SpecificAbilityClass->GetName());
		
		// 检查是否已授予此能力
		FGameplayAbilitySpecHandle* FoundHandle = GrantedSpecificAbilities.Find(SelectedAttack.SpecificAbilityClass);
		
		if (FoundHandle && FoundHandle->IsValid())
		{
			// 已授予，直接使用
			AbilityHandleToActivate = *FoundHandle;
			UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 能力已授予，直接激活"));
		}
		else
		{
			// 未授予，先授予能力
			UE_LOG(LogSGGameplay, Log, TEXT("  授予特定能力..."));
			
			FGameplayAbilitySpec AbilitySpec(
				SelectedAttack.SpecificAbilityClass,
				1,
				INDEX_NONE,
				this
			);
			
			FGameplayAbilitySpecHandle NewHandle = AbilitySystemComponent->GiveAbility(AbilitySpec);
			
			if (NewHandle.IsValid())
			{
				GrantedSpecificAbilities.Add(SelectedAttack.SpecificAbilityClass, NewHandle);
				AbilityHandleToActivate = NewHandle;
				UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 特定能力授予成功"));
			}
			else
			{
				UE_LOG(LogSGGameplay, Error, TEXT("  ❌ 特定能力授予失败"));
				UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
				return false;
			}
		}
	}
	else
	{
		// 使用通用攻击能力
		if (!GrantedCommonAttackHandle.IsValid())
		{
			UE_LOG(LogSGGameplay, Error, TEXT("  ❌ 通用攻击能力未授予"));
			UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
			return false;
		}
		
		AbilityHandleToActivate = GrantedCommonAttackHandle;
		UE_LOG(LogSGGameplay, Log, TEXT("  使用通用攻击能力"));
	}
	
	// ========== 步骤6：激活能力 ==========
	bool bSuccess = AbilitySystemComponent->TryActivateAbility(AbilityHandleToActivate);
	
	if (bSuccess)
	{
		UE_LOG(LogSGGameplay, Log, TEXT("  ✅ 攻击能力激活成功"));
		
		// ========== ✨ 新增 - 步骤7：开始冷却 ==========
		if (SelectedAttack.Cooldown > 0.0f)
		{
			StartAttackCooldown(SelectedAttack.Cooldown);
		}
		else
		{
			// 如果冷却时间为 0，根据攻击速度自动计算
			float AutoCooldown = 1.0f / FMath::Max(BaseAttackSpeed, 0.1f);
			StartAttackCooldown(AutoCooldown);
			UE_LOG(LogSGGameplay, Log, TEXT("  自动计算冷却时间：%.2f 秒"), AutoCooldown);
		}
	}
	else
	{
		UE_LOG(LogSGGameplay, Error, TEXT("  ❌ 攻击能力激活失败"));
	}
	
	UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
	return bSuccess;
}

// ========== ✨ 新增 - 冷却系统实现 ==========

/**
 * @brief 开始攻击冷却
 * @param Duration 冷却时间（秒）
 * @details
 * 功能说明：
 * - 设置冷却标记
 * - 启动冷却定时器
 * - 更新冷却剩余时间
 */
void ASG_UnitsBase::StartAttackCooldown(float Duration)
{
	// ========== 步骤1：设置冷却标记 ==========
	bIsAttackOnCooldown = true;
	CooldownRemainingTime = Duration;
	
	UE_LOG(LogSGGameplay, Verbose, TEXT("  ⏳ 开始攻击冷却：%.2f 秒"), Duration);
	
	// ========== 步骤2：清除旧的定时器（如果存在）==========
	if (GetWorldTimerManager().IsTimerActive(AttackCooldownTimerHandle))
	{
		GetWorldTimerManager().ClearTimer(AttackCooldownTimerHandle);
	}
	
	// ========== 步骤3：启动冷却定时器 ==========
	GetWorldTimerManager().SetTimer(
		AttackCooldownTimerHandle,
		this,
		&ASG_UnitsBase::OnAttackCooldownEnd,
		Duration,
		false // 不循环
	);
}

/**
 * @brief 冷却结束回调
 * @details
 * 功能说明：
 * - 重置冷却标记
 * - 清空冷却剩余时间
 */
void ASG_UnitsBase::OnAttackCooldownEnd()
{
	// ========== 步骤1：重置冷却标记 ==========
	bIsAttackOnCooldown = false;
	CooldownRemainingTime = 0.0f;
	
	UE_LOG(LogSGGameplay, Verbose, TEXT("  ✅ %s 攻击冷却结束"), *GetName());
}


/**
 * @brief 获取当前攻击配置
 * @return 当前攻击技能定义
 * @details
 * 功能说明：
 * - 返回当前正在使用的攻击配置
 * - 供 GA 使用，获取动画、伤害倍率等信息
 */
FSGUnitAttackDefinition ASG_UnitsBase::GetCurrentAttackDefinition() const
{
	// 检查索引有效性
	if (CachedAttackAbilities.IsValidIndex(CurrentAttackIndex))
	{
		return CachedAttackAbilities[CurrentAttackIndex];
	}
    
	// 返回默认值
	UE_LOG(LogSGGameplay, Warning, TEXT("⚠️ %s: CurrentAttackIndex 无效，返回默认配置"), *GetName());
	return FSGUnitAttackDefinition();
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

// ========== ✨ 新增 - 调试可视化系统实现 ==========

/**
 * @brief Tick 函数
 * @param DeltaTime 帧间隔时间
 * @details
 * 功能说明：
 * - 每帧绘制攻击范围和视野范围的可视化
 * 详细流程：
 * 1. 检查是否启用可视化
 * 2. 绘制攻击范围圆圈
 * 3. 绘制视野范围圆圈
 * 注意事项：
 * - 使用 DrawDebugCircle 绘制水平圆圈
 * - 仅在开启相应开关时绘制
 */
void ASG_UnitsBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// ========== ✨ 新增 - 更新冷却剩余时间 ==========
	if (bIsAttackOnCooldown)
	{
		CooldownRemainingTime = GetWorldTimerManager().GetTimerRemaining(AttackCooldownTimerHandle);
		
		// 确保不会出现负数
		if (CooldownRemainingTime < 0.0f)
		{
			CooldownRemainingTime = 0.0f;
		}
	}
	// 获取角色位置
	FVector ActorLocation = GetActorLocation();

	// ========== 绘制攻击范围 ==========
	if (bShowAttackRange && AttributeSet)
	{
		// 获取当前攻击范围
		float CurrentAttackRange = AttributeSet->GetAttackRange();

		// 绘制攻击范围圆圈
		// DrawDebugCircle 参数说明：
		// - GetWorld()：世界对象
		// - ActorLocation：圆心位置
		// - CurrentAttackRange：半径
		// - 32：圆的分段数（越大越圆滑）
		// - AttackRangeColor.ToFColor(true)：颜色
		// - false：不持久绘制（每帧重绘）
		// - -1.0f：生命周期（-1表示一帧）
		// - 0：深度优先级
		// - 3.0f：线条粗细
		DrawDebugCircle(
			GetWorld(),
			ActorLocation,
			CurrentAttackRange,
			32,
			AttackRangeColor.ToFColor(true),
			false,
			-1.0f,
			0,
			3.0f,
			FVector(0, 1, 0),  // Y轴（用于旋转圆圈）
			FVector(1, 0, 0),  // X轴（用于旋转圆圈）
			false
		);
		// ✨ 新增 - 显示冷却信息
		if (bIsAttackOnCooldown)
		{
			FString CooldownText = FString::Printf(TEXT("冷却中：%.1f 秒"), CooldownRemainingTime);
			DrawDebugString(
				GetWorld(),
				ActorLocation + FVector(0, 0, 150.0f),
				CooldownText,
				nullptr,
				FColor::Yellow,
				0.0f, // 一帧
				true  // 绘制阴影
			);
		}
	}

	// ========== 绘制视野范围 ==========
	if (bShowVisionRange)
	{
		// 绘制视野范围圆圈
		DrawDebugCircle(
			GetWorld(),
			ActorLocation,
			VisionRange,
			48,  // 视野范围更大，使用更多分段
			VisionRangeColor.ToFColor(true),
			false,
			-1.0f,
			0,
			2.0f,  // 视野范围线条稍细
			FVector(0, 1, 0),
			FVector(1, 0, 0),
			false
		);
	}

}

/**
 * @brief 切换攻击范围显示
 * @details
 * 功能说明：
 * - 开关攻击范围的可视化显示
 * 详细流程：
 * 1. 反转 bShowAttackRange 标志
 * 2. 输出日志
 * 注意事项：
 * - 可在蓝图中调用
 * - 可通过控制台命令调用
 */
void ASG_UnitsBase::ToggleAttackRangeVisualization()
{
	bShowAttackRange = !bShowAttackRange;
	UE_LOG(LogSGGameplay, Log, TEXT("%s: 攻击范围可视化 %s"), 
		*GetName(), bShowAttackRange ? TEXT("开启") : TEXT("关闭"));
}

/**
 * @brief 切换视野范围显示
 * @details
 * 功能说明：
 * - 开关视野范围的可视化显示
 * 详细流程：
 * 1. 反转 bShowVisionRange 标志
 * 2. 输出日志
 * 注意事项：
 * - 可在蓝图中调用
 * - 可通过控制台命令调用
 */
void ASG_UnitsBase::ToggleVisionRangeVisualization()
{
	bShowVisionRange = !bShowVisionRange;
	UE_LOG(LogSGGameplay, Log, TEXT("%s: 视野范围可视化 %s"), 
		*GetName(), bShowVisionRange ? TEXT("开启") : TEXT("关闭"));
}


/**
 * @brief 确定单位的阵营标签
 * @return 阵营标签
 * @details
 * 功能说明：
 * - 优先使用已设置的 FactionTag
 * - 如果未设置，使用默认阵营标签
 * 默认阵营优先级：
 * 1. FactionTag（如果已在 Blueprint 中设置）
 * 2. Unit.Faction.Player（默认玩家阵营）
 */
FGameplayTag ASG_UnitsBase::DetermineFactionTag() const
{
	// 如果已经设置了阵营标签，直接使用
	if (FactionTag.IsValid())
	{
		UE_LOG(LogSGGameplay, Log, TEXT("  使用已配置的阵营标签：%s"), *FactionTag.ToString());
		return FactionTag;
	}
	
	// 否则使用默认阵营标签（玩家阵营）
	FGameplayTag DefaultFactionTag = FGameplayTag::RequestGameplayTag(
		FName("Unit.Faction.Player"), 
		false  // 不报错
	);
	
	if (DefaultFactionTag.IsValid())
	{
		UE_LOG(LogSGGameplay, Log, TEXT("  使用默认阵营标签：%s"), *DefaultFactionTag.ToString());
		return DefaultFactionTag;
	}
	else
	{
		UE_LOG(LogSGGameplay, Warning, TEXT("  ⚠️ 默认阵营标签 'Unit.Faction.Player' 未配置"));
		UE_LOG(LogSGGameplay, Warning, TEXT("  请在 Config/DefaultGameplayTags.ini 中添加此标签"));
		return FGameplayTag();
	}
}

/**
 * @brief 使用默认值初始化单位
 * @details
 * 功能说明：
 * - 使用 Blueprint 中配置的 Base 属性
 * - 使用确定的阵营标签
 * - 所有倍率为 1.0（不进行缩放）
 */
void ASG_UnitsBase::InitializeWithDefaults()
{
	// 获取阵营标签
	FGameplayTag InitFactionTag = DetermineFactionTag();
	
	// ✨ 新增 - 从 CardData 读取倍率
	float HealthMult = 1.0f;
	float DamageMult = 1.0f;
	float SpeedMult = 1.0f;
	
	if (SourceCardData)
	{
		HealthMult = SourceCardData->HealthMultiplier;
		DamageMult = SourceCardData->DamageMultiplier;
		SpeedMult = SourceCardData->SpeedMultiplier;
		
		UE_LOG(LogSGGameplay, Log, TEXT("  应用卡牌倍率"));
	}
	
	// 使用倍率初始化
	InitializeCharacter(
		InitFactionTag,
		HealthMult,
		DamageMult,
		SpeedMult
	);
	
	UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 默认值初始化完成"));
	UE_LOG(LogSGGameplay, Log, TEXT("    生命值：%.0f (基础: %.0f, 倍率: %.2f)"), 
		BaseHealth * HealthMult, BaseHealth, HealthMult);
	UE_LOG(LogSGGameplay, Log, TEXT("    攻击力：%.0f (基础: %.0f, 倍率: %.2f)"), 
		BaseAttackDamage * DamageMult, BaseAttackDamage, DamageMult);
	UE_LOG(LogSGGameplay, Log, TEXT("    移动速度：%.0f (基础: %.0f, 倍率: %.2f)"), 
		BaseMoveSpeed * SpeedMult, BaseMoveSpeed, SpeedMult);
	UE_LOG(LogSGGameplay, Log, TEXT("    视野范围：%.0f"), VisionRange);
}

/**
 * @brief 从 DataTable 加载单位配置
 * @return 是否加载成功
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
 * 5.🔧 新增：缓存 AI 配置（寻敌范围、追击范围）
 * 注意事项：
 * - 在 BeginPlay 中调用
 * - 如果 bUseDataTable = false，不会执行
 */
bool ASG_UnitsBase::IsLoadUnitDataFromTable()
{
 // ========== 步骤1：检查有效性 ==========
    if (!UnitDataTable)
    {
        UE_LOG(LogSGGameplay, Error, TEXT("  ❌ UnitDataTable 为空！"));
        return false;
    }
    
    if (UnitDataRowName.IsNone())
    {
        UE_LOG(LogSGGameplay, Error, TEXT("  ❌ UnitDataRowName 为空！"));
        return false;
    }
    
    // ========== 步骤2：查找 DataTable 行 ==========
    FSGUnitDataRow* RowData = UnitDataTable->FindRow<FSGUnitDataRow>(
        UnitDataRowName,
        TEXT("LoadUnitDataFromTable")
    );
    
    if (!RowData)
    {
        UE_LOG(LogSGGameplay, Error, TEXT("  ❌ 在 DataTable 中找不到行 '%s'！"), 
            *UnitDataRowName.ToString());
        return false;
    }
    
    // 输出日志
    UE_LOG(LogSGGameplay, Log, TEXT("  从 DataTable 加载配置"));
    UE_LOG(LogSGGameplay, Log, TEXT("    数据行：%s"), *UnitDataRowName.ToString());
    UE_LOG(LogSGGameplay, Log, TEXT("    单位名称：%s"), *RowData->UnitName.ToString());
    
    // ========== 步骤3：应用属性值 ==========
    BaseHealth = RowData->BaseHealth;
    BaseAttackDamage = RowData->BaseAttackDamage;
    BaseMoveSpeed = RowData->BaseMoveSpeed;
    BaseAttackSpeed = RowData->BaseAttackSpeed;
    BaseAttackRange = RowData->BaseAttackRange;
    
    // ✨ 新增 - 缓存 AI 配置
    CachedDetectionRange = RowData->DetectionRange;
    CachedChaseRange = RowData->ChaseRange;
    
    // ✨ 新增 - 同步 VisionRange（用于调试可视化）
    VisionRange = RowData->DetectionRange;
    
    UE_LOG(LogSGGameplay, Log, TEXT("    属性配置："));
    UE_LOG(LogSGGameplay, Log, TEXT("      生命值：%.0f"), BaseHealth);
    UE_LOG(LogSGGameplay, Log, TEXT("      攻击力：%.0f"), BaseAttackDamage);
    UE_LOG(LogSGGameplay, Log, TEXT("      移动速度：%.0f"), BaseMoveSpeed);
    UE_LOG(LogSGGameplay, Log, TEXT("      攻击速度：%.2f"), BaseAttackSpeed);
    UE_LOG(LogSGGameplay, Log, TEXT("      攻击范围：%.0f"), BaseAttackRange);
    UE_LOG(LogSGGameplay, Log, TEXT("    AI 配置："));
    UE_LOG(LogSGGameplay, Log, TEXT("      寻敌范围：%.0f"), CachedDetectionRange);
    UE_LOG(LogSGGameplay, Log, TEXT("      追击范围：%.0f"), CachedChaseRange);
    
    // ========== 步骤4：应用单位类型标签 ==========
    if (RowData->UnitTypeTag.IsValid())
    {
        UnitTypeTag = RowData->UnitTypeTag;
        UE_LOG(LogSGGameplay, Log, TEXT("    单位类型：%s"), *UnitTypeTag.ToString());
    }
	
	return true;
}



// ========== ✨ 新增 - AI 配置接口实现 ==========

/**
 * @brief 获取寻敌范围
 * @return 寻敌范围（厘米）
 * @details
 * 功能说明：
 * - 从 DataTable 读取寻敌范围
 * - 如果未使用 DataTable，使用 VisionRange
 * - AI 用此值查找目标
 */
float ASG_UnitsBase::GetDetectionRange() const
{
	// 如果使用 DataTable，返回缓存的值
	if (bUseDataTable)
	{
		return CachedDetectionRange;
	}
	
	// 否则使用 VisionRange
	return VisionRange;
}

/**
 * @brief 获取追击范围
 * @return 追击范围（厘米）
 * @details
 * 功能说明：
 * - 从 DataTable 读取追击范围
 * - 如果未使用 DataTable，使用 VisionRange * 1.5
 * - AI 用此值决定是否放弃追击
 */
float ASG_UnitsBase::GetChaseRange() const
{
	// 如果使用 DataTable，返回缓存的值
	if (bUseDataTable)
	{
		return CachedChaseRange;
	}
	
	// 否则使用 VisionRange * 1.5
	return VisionRange * 1.5f;
}

/**
 * @brief 获取攻击范围
 * @return 攻击范围（厘米）
 * @details
 * 功能说明：
 * - 从 AttributeSet 读取攻击范围
 * - 如果 AttributeSet 无效，使用 BaseAttackRange
 * - AI 用此值决定是否可以攻击
 */
float ASG_UnitsBase::GetAttackRangeForAI() const
{
	// 优先从 AttributeSet 读取
	if (AttributeSet)
	{
		return AttributeSet->GetAttackRange();
	}
	
	// 否则使用基础攻击范围
	return BaseAttackRange;
}