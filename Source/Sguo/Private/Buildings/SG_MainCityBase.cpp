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
#include "Kismet/GameplayStatics.h"
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
	PrimaryActorTick.bCanEverTick = true;

	// ========== 创建主城网格体作为根组件 ==========
	CityMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CityMesh"));
	RootComponent = CityMesh;
	CityMesh->SetCollisionProfileName(TEXT("NoCollision"));
	CityMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CityMesh->SetCanEverAffectNavigation(false);
	CityMesh->SetMobility(EComponentMobility::Static);

	// ========== 创建攻击检测盒 ==========
	AttackDetectionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("AttackDetectionBox"));
	AttackDetectionBox->SetupAttachment(CityMesh);
	AttackDetectionBox->SetBoxExtent(FVector(800.0f, 800.0f, 500.0f));
	AttackDetectionBox->SetRelativeLocation(FVector(0.0f, 0.0f, 500.0f));
	AttackDetectionBox->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	AttackDetectionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	AttackDetectionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	AttackDetectionBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	AttackDetectionBox->SetCanEverAffectNavigation(false);
	AttackDetectionBox->SetGenerateOverlapEvents(true);
	AttackDetectionBox->SetMobility(EComponentMobility::Static);
	
	// ✨ 编辑器中显示碰撞盒
	AttackDetectionBox->SetHiddenInGame(false);
	AttackDetectionBox->SetVisibility(true);
	AttackDetectionBox->ShapeColor = FColor::Orange;

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
	float OldHealth = Data.OldValue;
	float MaxHealth = AttributeSet->GetMaxHealth();
	float Damage = OldHealth - NewHealth;
	
	// ✨ 新增 - 详细伤害日志
	if (bShowDamageLog && Damage > 0.0f)
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
		UE_LOG(LogSGGameplay, Warning, TEXT("✗ %s 被摧毁！"), *GetName());
		OnMainCityDestroyed();
	}
}
// ========== ✨ 新增 - Tick 函数 ==========

/**
 * @brief Tick 函数
 * @param DeltaTime 帧间隔时间
 * @details
 * 功能说明：
 * - 每帧绘制调试可视化
 */
void ASG_MainCityBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	// 绘制调试可视化
	if (bShowAttackDetectionBox || bShowHealthInfo)
	{
		DrawDebugVisualization();
	}
}

/**
 * @brief 绘制调试可视化
 * @details
 * 功能说明：
 * - 绘制攻击检测盒
 * - 绘制生命值信息
 * - 绘制单位到检测盒的距离线
 */
void ASG_MainCityBase::DrawDebugVisualization()
{
	if (!AttackDetectionBox)
	{
		return;
	}
	
	FVector BoxCenter = AttackDetectionBox->GetComponentLocation();
	FVector BoxExtent = AttackDetectionBox->GetScaledBoxExtent();
	FQuat BoxRotation = AttackDetectionBox->GetComponentQuat();
	
	// ========== 绘制攻击检测盒 ==========
	if (bShowAttackDetectionBox)
	{
		// 绘制盒体边框
		DrawDebugBox(
			GetWorld(),
			BoxCenter,
			BoxExtent,
			BoxRotation,
			DetectionBoxColor.ToFColor(true),
			false,  // 不持久
			-1.0f,  // 生命周期（一帧）
			0,      // 深度优先级
			3.0f    // 线条粗细
		);
		
		// 绘制中心点
		DrawDebugPoint(
			GetWorld(),
			BoxCenter,
			15.0f,
			FColor::Red,
			false,
			-1.0f
		);
		
		// 绘制检测盒信息文本
		FString BoxInfo = FString::Printf(
			TEXT("检测盒信息\n尺寸: %.0f x %.0f x %.0f\n半径: %.0f"),
			BoxExtent.X * 2.0f,
			BoxExtent.Y * 2.0f,
			BoxExtent.Z * 2.0f,
			FMath::Max3(BoxExtent.X, BoxExtent.Y, BoxExtent.Z)
		);
		
		DrawDebugString(
			GetWorld(),
			BoxCenter + FVector(0, 0, BoxExtent.Z + 100.0f),
			BoxInfo,
			nullptr,
			FColor::Orange,
			-1.0f,
			true,
			1.5f
		);
		
		// ✨ 新增 - 绘制周边单位到检测盒的距离线
		TArray<AActor*> AllUnits;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASG_UnitsBase::StaticClass(), AllUnits);
		
		for (AActor* Actor : AllUnits)
		{
			ASG_UnitsBase* Unit = Cast<ASG_UnitsBase>(Actor);
			if (!Unit || Unit->FactionTag == FactionTag)
			{
				continue;  // 跳过友方单位
			}
			
			FVector UnitLocation = Unit->GetActorLocation();
			float DistanceToCenter = FVector::Dist(UnitLocation, BoxCenter);
			float BoxRadius = FMath::Max3(BoxExtent.X, BoxExtent.Y, BoxExtent.Z);
			float DistanceToSurface = FMath::Max(0.0f, DistanceToCenter - BoxRadius);
			float AttackRange = Unit->GetAttackRangeForAI();
			
			// 根据距离选择颜色
			FColor LineColor;
			if (DistanceToSurface <= AttackRange)
			{
				LineColor = FColor::Red;  // 在攻击范围内
			}
			else if (DistanceToSurface <= AttackRange * 2.0f)
			{
				LineColor = FColor::Yellow;  // 接近攻击范围
			}
			else
			{
				LineColor = FColor::Green;  // 远离
			}
			
			// 绘制单位到检测盒中心的线
			DrawDebugLine(
				GetWorld(),
				UnitLocation,
				BoxCenter,
				LineColor,
				false,
				-1.0f,
				0,
				2.0f
			);
			
			// 绘制距离信息
			FString DistanceInfo = FString::Printf(
				TEXT("%.0f / %.0f"),
				DistanceToSurface,
				AttackRange
			);
			
			DrawDebugString(
				GetWorld(),
				(UnitLocation + BoxCenter) * 0.5f,
				DistanceInfo,
				nullptr,
				LineColor,
				-1.0f,
				true,
				1.2f
			);
		}
	}
	
	// ========== 绘制生命值信息 ==========
	if (bShowHealthInfo && AttributeSet)
	{
		float CurrentHealth = AttributeSet->GetHealth();
		float MaxHealth = AttributeSet->GetMaxHealth();
		float HealthPercentage = (CurrentHealth / MaxHealth) * 100.0f;
		
		// 根据生命值百分比选择颜色
		FColor TextColor;
		if (HealthPercentage > 75.0f)
		{
			TextColor = FColor::Green;
		}
		else if (HealthPercentage > 50.0f)
		{
			TextColor = FColor::Yellow;
		}
		else if (HealthPercentage > 25.0f)
		{
			TextColor = FColor::Orange;
		}
		else
		{
			TextColor = FColor::Red;
		}
		
		FString HealthInfo = FString::Printf(
			TEXT("%s\n生命值: %.0f / %.0f (%.1f%%)"),
			*GetName(),
			CurrentHealth,
			MaxHealth,
			HealthPercentage
		);
		
		DrawDebugString(
			GetWorld(),
			GetActorLocation() + FVector(0, 0, 1000.0f),
			HealthInfo,
			nullptr,
			TextColor,
			-1.0f,
			true,
			2.0f
		);
	}
}

// ========== ✨ 新增 - 调试开关函数 ==========

/**
 * @brief 切换攻击检测盒显示
 */
void ASG_MainCityBase::ToggleDetectionBoxVisualization()
{
	bShowAttackDetectionBox = !bShowAttackDetectionBox;
	UE_LOG(LogSGGameplay, Log, TEXT("%s: 攻击检测盒可视化 %s"), 
		*GetName(), bShowAttackDetectionBox ? TEXT("开启") : TEXT("关闭"));
}

/**
 * @brief 切换生命值信息显示
 */
void ASG_MainCityBase::ToggleHealthInfoVisualization()
{
	bShowHealthInfo = !bShowHealthInfo;
	UE_LOG(LogSGGameplay, Log, TEXT("%s: 生命值信息可视化 %s"), 
		*GetName(), bShowHealthInfo ? TEXT("开启") : TEXT("关闭"));
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
