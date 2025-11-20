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

/**
 * @brief 构造函数
 * @details
 * 功能说明：
 * - 创建主城的所有组件
 * - ✨ 简化：直接使用 BoxComponent 的原生属性
 */
ASG_MainCityBase::ASG_MainCityBase()
{
	// 禁用 Tick
	PrimaryActorTick.bCanEverTick = false;

	// ========== 创建主城网格体作为根组件 ==========
	CityMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CityMesh"));
	RootComponent = CityMesh;
	
	// 主城网格体不影响导航
	CityMesh->SetCollisionProfileName(TEXT("NoCollision"));
	CityMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CityMesh->SetCanEverAffectNavigation(false);
	CityMesh->SetMobility(EComponentMobility::Static);

	// ========== 创建攻击检测盒 ==========
	AttackDetectionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("AttackDetectionBox"));
	AttackDetectionBox->SetupAttachment(CityMesh);
	
	// ✨ 简化 - 设置默认尺寸（可在编辑器中直接修改 Box Extent）
	AttackDetectionBox->SetBoxExtent(FVector(800.0f, 800.0f, 500.0f));
	
	// ✨ 简化 - 设置默认偏移（可在编辑器中直接修改 Location）
	AttackDetectionBox->SetRelativeLocation(FVector(0.0f, 0.0f, 500.0f));
	
	// 碰撞设置
	AttackDetectionBox->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	AttackDetectionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	AttackDetectionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	AttackDetectionBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	AttackDetectionBox->SetCanEverAffectNavigation(false);
	AttackDetectionBox->SetGenerateOverlapEvents(true);
	AttackDetectionBox->SetMobility(EComponentMobility::Static);
	
	// ✨ 在编辑器中显示碰撞盒（橙色线框）
	AttackDetectionBox->SetHiddenInGame(false);
	AttackDetectionBox->SetVisibility(true);
	AttackDetectionBox->ShapeColor = FColor::Orange;


	// ========== 创建 GAS 组件 ==========
	AbilitySystemComponent = CreateDefaultSubobject<USG_AbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSet = CreateDefaultSubobject<USG_BuildingAttributeSet>(TEXT("AttributeSet"));
	
	// 设置默认阵营
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
	UE_LOG(LogSGGameplay, Log, TEXT("  位置：%s"), *GetActorLocation().ToString());
	
	// ✨ 简化 - 输出检测盒信息（使用原生属性）
	if (AttackDetectionBox)
	{
		FVector BoxExtent = AttackDetectionBox->GetScaledBoxExtent();
		FVector BoxLocation = AttackDetectionBox->GetRelativeLocation();
		FVector WorldLocation = AttackDetectionBox->GetComponentLocation();
		
		UE_LOG(LogSGGameplay, Log, TEXT("  攻击检测盒："));
		UE_LOG(LogSGGameplay, Log, TEXT("    尺寸：%s"), *BoxExtent.ToString());
		UE_LOG(LogSGGameplay, Log, TEXT("    相对位置：%s"), *BoxLocation.ToString());
		UE_LOG(LogSGGameplay, Log, TEXT("    世界位置：%s"), *WorldLocation.ToString());
	}
	
	// 初始化 ASC
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
		UE_LOG(LogSGGameplay, Log, TEXT("  ✓ ASC 初始化完成"));
	}
	
	// 初始化主城
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
	if (bIsDestroyed)
	{
		return;
	}
	
	float NewHealth = Data.NewValue;
	float MaxHealth = AttributeSet->GetMaxHealth();
	
	UE_LOG(LogSGGameplay, Log, TEXT("%s 生命值变化：%.0f / %.0f（%.1f%%）"), 
		*GetName(), NewHealth, MaxHealth, (NewHealth / MaxHealth) * 100.0f);

	// 检测主城被摧毁
	if (NewHealth <= 0.0f && Data.OldValue > 0.0f)
	{
		UE_LOG(LogSGGameplay, Warning, TEXT("✗ %s 被摧毁！"), *GetName());
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

	SetLifeSpan(5.0f);
	UE_LOG(LogSGGameplay, Log, TEXT("  将在 5 秒后销毁"));
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
