// 🔧 简化 - SG_MainCityBase.cpp

/**
 * @file SG_MainCityBase.cpp
 * @brief 主城基类实现
 */

#include "Buildings/SG_MainCityBase.h"
#include "AbilitySystem/SG_AbilitySystemComponent.h"
#include "Buildings/SG_BuildingAttributeSet.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Debug/SG_LogCategories.h"
#include "Units/SG_UnitsBase.h"

/**
 * @brief 构造函数
 * @details
 * 功能说明：
 * - 创建主城的所有组件
 * - ✨ 简化：直接使用 BoxComponent 的原生属性
 */
ASG_MainCityBase::ASG_MainCityBase()
{
	// 🔧 修改 - 启用 Tick（用于调试可视化）
	PrimaryActorTick.bCanEverTick = false;
	// ========== 创建主城网格体作为根组件 ==========
	CityMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CityMesh"));
	RootComponent = CityMesh;
	
	// 🔧 修改 - 主城网格体碰撞设置
	CityMesh->SetCollisionProfileName(TEXT("BlockAll"));  // 改为 BlockAll，防止单位穿过
	CityMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CityMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);  // 阻挡 Pawn
	CityMesh->SetCanEverAffectNavigation(true);  // 影响导航（阻挡寻路）
	CityMesh->SetMobility(EComponentMobility::Static);

	// ========== 🔧 修复 - 创建攻击检测盒 ==========
	AttackDetectionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("AttackDetectionBox"));
	
	// 🔧 关键修复 1：确保正确附加到根组件
	AttackDetectionBox->SetupAttachment(RootComponent);
	
	// 🔧 关键修复 2：设置为 Stationary（允许在编辑器中移动，运行时固定）
	AttackDetectionBox->SetMobility(EComponentMobility::Stationary);
	
	// 设置默认尺寸
	AttackDetectionBox->SetBoxExtent(FVector(800.0f, 800.0f, 500.0f));
	
	// 🔧 关键修复 3：使用 SetRelativeLocation（相对于父组件）
	AttackDetectionBox->SetRelativeLocation(FVector(0.0f, 0.0f, 500.0f));
	
	// 碰撞设置
	AttackDetectionBox->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	AttackDetectionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	AttackDetectionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	AttackDetectionBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	AttackDetectionBox->SetCanEverAffectNavigation(false);
	AttackDetectionBox->SetGenerateOverlapEvents(true);
	
	// ✨ 在编辑器和游戏中都显示碰撞盒
	AttackDetectionBox->SetHiddenInGame(false);
	AttackDetectionBox->SetVisibility(true);
	AttackDetectionBox->ShapeColor = FColor::Orange;
	
	// 🔧 关键修复 4：设置为自动激活
	AttackDetectionBox->SetActive(true);
	AttackDetectionBox->bAutoActivate = true;

	// ========== 创建 GAS 组件 ==========
	AbilitySystemComponent = CreateDefaultSubobject<USG_AbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	AttributeSet = CreateDefaultSubobject<USG_BuildingAttributeSet>(TEXT("AttributeSet"));
	
	FactionTag = FGameplayTag::RequestGameplayTag(TEXT("Unit.Faction.Player"));
}

/**
 * @brief 获取 AbilitySystemComponent（GAS 接口要求）
 */
UAbilitySystemComponent* ASG_MainCityBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

/**
 * @brief BeginPlay 生命周期
 */
void ASG_MainCityBase::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogSGGameplay, Log, TEXT("========== 主城 BeginPlay：%s =========="), *GetName());
	UE_LOG(LogSGGameplay, Log, TEXT("  阵营：%s"), *FactionTag.ToString());
	
	// ========== 🔧 新增 - 验证主城位置 ==========
	FVector ActorLocation = GetActorLocation();
	UE_LOG(LogSGGameplay, Log, TEXT("  主城位置：%s"), *ActorLocation.ToString());
	
	// ========== 🔧 新增 - 验证检测盒位置 ==========
	if (AttackDetectionBox)
	{
		// 获取检测盒的世界位置
		FVector BoxWorldLocation = AttackDetectionBox->GetComponentLocation();
		FVector BoxRelativeLocation = AttackDetectionBox->GetRelativeLocation();
		FVector BoxExtent = AttackDetectionBox->GetScaledBoxExtent();
		
		UE_LOG(LogSGGameplay, Log, TEXT("  攻击检测盒："));
		UE_LOG(LogSGGameplay, Log, TEXT("    相对位置：%s"), *BoxRelativeLocation.ToString());
		UE_LOG(LogSGGameplay, Log, TEXT("    世界位置：%s"), *BoxWorldLocation.ToString());
		UE_LOG(LogSGGameplay, Log, TEXT("    尺寸：%s"), *BoxExtent.ToString());
		
		// 🔧 关键修复 - 检查检测盒是否在世界原点
		if (BoxWorldLocation.Equals(FVector::ZeroVector, 10.0f))
		{
			UE_LOG(LogSGGameplay, Error, TEXT("  ❌ 检测盒位置错误（在世界原点）！"));
			UE_LOG(LogSGGameplay, Error, TEXT("  尝试修复..."));
			
			// 尝试重新附加
			AttackDetectionBox->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
			AttackDetectionBox->AttachToComponent(
				RootComponent, 
				FAttachmentTransformRules::KeepRelativeTransform
			);
			
			// 重新设置相对位置
			AttackDetectionBox->SetRelativeLocation(FVector(0.0f, 0.0f, 500.0f));
			
			// 验证修复结果
			FVector NewWorldLocation = AttackDetectionBox->GetComponentLocation();
			UE_LOG(LogSGGameplay, Warning, TEXT("  修复后世界位置：%s"), *NewWorldLocation.ToString());
			
			if (!NewWorldLocation.Equals(FVector::ZeroVector, 10.0f))
			{
				UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 检测盒位置修复成功"));
			}
			else
			{
				UE_LOG(LogSGGameplay, Error, TEXT("  ❌ 检测盒位置修复失败！"));
				UE_LOG(LogSGGameplay, Error, TEXT("  请检查："));
				UE_LOG(LogSGGameplay, Error, TEXT("    1. 主城蓝图中是否手动设置了检测盒位置"));
				UE_LOG(LogSGGameplay, Error, TEXT("    2. 主城是否正确放置在场景中"));
				UE_LOG(LogSGGameplay, Error, TEXT("    3. RootComponent 是否为 CityMesh"));
			}
		}
		else
		{
			UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 检测盒位置正确"));
		}
		
		// 🔧 新增 - 验证检测盒是否正确附加
		USceneComponent* Parent = AttackDetectionBox->GetAttachParent();
		if (Parent == RootComponent)
		{
			UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 检测盒正确附加到根组件"));
		}
		else
		{
			UE_LOG(LogSGGameplay, Error, TEXT("  ❌ 检测盒未正确附加！"));
			UE_LOG(LogSGGameplay, Error, TEXT("    当前父组件：%s"), Parent ? *Parent->GetName() : TEXT("None"));
		}
	}
	else
	{
		UE_LOG(LogSGGameplay, Error, TEXT("  ❌ 攻击检测盒为空！"));
	}
	
	// ========== 初始化 ASC ==========
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
		UE_LOG(LogSGGameplay, Log, TEXT("  ✓ ASC 初始化完成"));
	}
	
	// ========== 初始化主城 ==========
	InitializeMainCity();
	
	UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
}

/**
 * @brief 初始化主城
 */
void ASG_MainCityBase::InitializeMainCity()
{
	UE_LOG(LogSGGameplay, Log, TEXT("========== 初始化主城：%s =========="), *GetName());
	
	if (!AttributeSet)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("❌ AttributeSet 为空"));
		return;
	}

	// 设置初始生命值
	AttributeSet->SetMaxHealth(InitialHealth);
	AttributeSet->SetHealth(InitialHealth);
	
	UE_LOG(LogSGGameplay, Log, TEXT("  初始生命值：%.0f"), InitialHealth);

	// 绑定属性变化委托
	BindAttributeDelegates();
	
	UE_LOG(LogSGGameplay, Log, TEXT("✓ 主城初始化完成"));
	UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
 }

/**
 * @brief 绑定属性变化委托
 */
void ASG_MainCityBase::BindAttributeDelegates()
{
	if (!AbilitySystemComponent || !AttributeSet)
	{
		UE_LOG(LogSGGameplay, Warning, TEXT("⚠️ 无法绑定属性委托：ASC 或 AttributeSet 为空"));
		return;
	}

	// 监听生命值变化
	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(AttributeSet->GetHealthAttribute())
		.AddUObject(this, &ASG_MainCityBase::OnHealthChanged);
	
	UE_LOG(LogSGGameplay, Verbose, TEXT("✓ 已绑定生命值变化委托"));
}

/**
 * @brief 生命值变化回调
 */
void ASG_MainCityBase::OnHealthChanged(const FOnAttributeChangeData& Data)
{
	// 🔧 修复：已摧毁的主城不再处理生命值变化
	if (bIsDestroyed)
	{
		return;
	}
	
	float NewHealth = Data.NewValue;
	float OldHealth = Data.OldValue;
	float MaxHealth = AttributeSet->GetMaxHealth();
	float Damage = OldHealth - NewHealth;
	
	// ✨ 新增 - 详细伤害日志
	if (Damage > 0.0f)
	{
		UE_LOG(LogSGGameplay, Warning, TEXT("========================================"));
		UE_LOG(LogSGGameplay, Warning, TEXT("🩸 主城受到伤害：%s"), *GetName());
		UE_LOG(LogSGGameplay, Warning, TEXT("  伤害值：%.2f"), Damage);
		UE_LOG(LogSGGameplay, Warning, TEXT("  旧生命值：%.0f"), OldHealth);
		UE_LOG(LogSGGameplay, Warning, TEXT("  新生命值：%.0f"), NewHealth);
		UE_LOG(LogSGGameplay, Warning, TEXT("  最大生命值：%.0f"), MaxHealth);
		UE_LOG(LogSGGameplay, Warning, TEXT("  剩余百分比：%.1f%%"), (NewHealth / MaxHealth) * 100.0f);
		UE_LOG(LogSGGameplay, Warning, TEXT("========================================"));
	}
	else
	{
		UE_LOG(LogSGGameplay, Log, TEXT("%s 生命值变化：%.0f / %.0f（%.1f%%）"), 
			*GetName(), NewHealth, MaxHealth, (NewHealth / MaxHealth) * 100.0f);
	}

	// 检测主城被摧毁
	if (NewHealth <= 0.0f && OldHealth > 0.0f)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("========================================"));
		UE_LOG(LogSGGameplay, Error, TEXT("💥 主城被摧毁：%s"), *GetName());
		UE_LOG(LogSGGameplay, Error, TEXT("========================================"));
		OnMainCityDestroyed();
	}
}



/**
 * @brief 主城被摧毁时调用
 */
void ASG_MainCityBase::OnMainCityDestroyed_Implementation()
{
	bIsDestroyed = true;
	
	UE_LOG(LogSGGameplay, Log, TEXT("========== %s 执行摧毁逻辑 =========="), *GetName());
	
	if (FactionTag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Unit.Faction.Player"))))
	{
		UE_LOG(LogSGGameplay, Warning, TEXT("⚠️ 玩家主城被摧毁 → 游戏失败"));
	}
	else if (FactionTag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Unit.Faction.Enemy"))))
	{
		UE_LOG(LogSGGameplay, Warning, TEXT("✓ 敌方主城被摧毁 → 游戏胜利"));
	}
	

	// ✨ 新增：禁用碰撞（防止继续被攻击）
	if (AttackDetectionBox)
	{
		AttackDetectionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 禁用攻击检测盒碰撞"));
	}
}


/**
 * @brief 检查主城是否存活
 * @return 是否存活
 * @details
 * 功能说明：
 * - 快速检查主城状态
 * - 用于 AI 和 UI 查询
 */
bool ASG_MainCityBase::IsAlive() const
{
	// 方法 1：检查摧毁标记
	if (bIsDestroyed)
	{
		return false;
	}
	
	// 方法 2：检查生命值
	if (AttributeSet)
	{
		return AttributeSet->GetHealth() > 0.0f;
	}
	
	// 如果没有 AttributeSet，假设存活
	return true;
}

/**
 * @brief 获取当前生命值
 */
float ASG_MainCityBase::GetCurrentHealth() const
{
	if (AttributeSet)
	{
		return AttributeSet->GetHealth();
	}
	return 0.0f;
}

/**
 * @brief 获取最大生命值
 */
float ASG_MainCityBase::GetMaxHealth() const
{
	if (AttributeSet)
	{
		return AttributeSet->GetMaxHealth();
	}
	return 0.0f;
}

/**
 * @brief 获取生命值百分比
 */
float ASG_MainCityBase::GetHealthPercentage() const
{
	if (AttributeSet)
	{
		float Current = AttributeSet->GetHealth();
		float Max = AttributeSet->GetMaxHealth();
		
		if (Max > 0.0f)
		{
			return (Current / Max);
		}
	}
	return 0.0f;
}



