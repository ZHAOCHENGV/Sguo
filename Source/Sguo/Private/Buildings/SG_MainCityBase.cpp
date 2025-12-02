// 📄 文件：Source/Sguo/Private/Buildings/SG_MainCityBase.cpp
// 🔧 修改 - 添加击飞站桩单位功能实现（完整文件）

/**
 * @file SG_MainCityBase.cpp
 * @brief 主城基类实现
 */

#include "Buildings/SG_MainCityBase.h"
#include "AbilitySystem/SG_AbilitySystemComponent.h"
#include "Buildings/SG_BuildingAttributeSet.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Debug/SG_LogCategories.h"
#include "Units/SG_UnitsBase.h"
#include "Units/SG_StationaryUnit.h"  // ✨ 新增
#include "Actors/SG_EnemySpawner.h"
#include "AI/SG_AIControllerBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"

/**
 * @brief 构造函数
 * @details
 * 功能说明：
 * - 创建主城的所有组件
 */
ASG_MainCityBase::ASG_MainCityBase()
{
	// 禁用 Tick
	PrimaryActorTick.bCanEverTick = false;
	
	// ========== 创建主城网格体作为根组件 ==========
	CityMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CityMesh"));
	RootComponent = CityMesh;
	
	// 主城网格体碰撞设置
	CityMesh->SetCollisionProfileName(TEXT("BlockAll"));
	CityMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CityMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	CityMesh->SetCanEverAffectNavigation(true);
	CityMesh->SetMobility(EComponentMobility::Static);

	// ========== 创建攻击检测盒 ==========
	AttackDetectionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("AttackDetectionBox"));
	
	// 确保正确附加到根组件
	AttackDetectionBox->SetupAttachment(RootComponent);
	
	// 设置为 Stationary
	AttackDetectionBox->SetMobility(EComponentMobility::Stationary);
	
	// 设置默认尺寸
	AttackDetectionBox->SetBoxExtent(FVector(800.0f, 800.0f, 500.0f));
	
	// 使用相对位置
	AttackDetectionBox->SetRelativeLocation(FVector(0.0f, 0.0f, 500.0f));
	
	// 碰撞设置
	AttackDetectionBox->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	AttackDetectionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	AttackDetectionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	AttackDetectionBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	AttackDetectionBox->SetCanEverAffectNavigation(false);
	AttackDetectionBox->SetGenerateOverlapEvents(true);
	
	// 在编辑器和游戏中都显示碰撞盒
	AttackDetectionBox->SetHiddenInGame(false);
	AttackDetectionBox->SetVisibility(true);
	AttackDetectionBox->ShapeColor = FColor::Orange;
	
	// 设置为自动激活
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
	
	// ========== 验证主城位置 ==========
	FVector ActorLocation = GetActorLocation();
	UE_LOG(LogSGGameplay, Log, TEXT("  主城位置：%s"), *ActorLocation.ToString());
	
	// ========== 验证检测盒位置 ==========
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
		
		// 检查检测盒是否在世界原点
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
			}
		}
		else
		{
			UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 检测盒位置正确"));
		}
		
		// 验证检测盒是否正确附加
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
	// 已摧毁的主城不再处理生命值变化
	if (bIsDestroyed)
	{
		return;
	}
	
	float NewHealth = Data.NewValue;
	float OldHealth = Data.OldValue;
	float MaxHealth = AttributeSet->GetMaxHealth();
	float Damage = OldHealth - NewHealth;
	
	// 详细伤害日志
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
 * @details
 * 功能说明：
 * - 设置摧毁标记
 * - 禁用碰撞
 * - ✨ 新增：击飞同阵营站桩单位
 * - 停止全场逻辑
 */
void ASG_MainCityBase::OnMainCityDestroyed_Implementation()
{
	// 防止重复执行
	if (bIsDestroyed) return;
	
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

	// 禁用碰撞（防止继续被攻击）
	if (AttackDetectionBox)
	{
		AttackDetectionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 禁用攻击检测盒碰撞"));
	}

	// ✨ 新增：击飞同阵营的站桩单位
	if (bEnableDestructionBlast)
	{
		BlastStationaryUnits();
	}

	// ========== 停止全场逻辑 ==========
	
	UWorld* World = GetWorld();
	if (World)
	{
		// A. 停止所有敌方生成器
		TArray<AActor*> AllSpawners;
		UGameplayStatics::GetAllActorsOfClass(World, ASG_EnemySpawner::StaticClass(), AllSpawners);
		
		for (AActor* Actor : AllSpawners)
		{
			if (ASG_EnemySpawner* Spawner = Cast<ASG_EnemySpawner>(Actor))
			{
				Spawner->StopSpawning();
				UE_LOG(LogSGGameplay, Verbose, TEXT("  已停止生成器：%s"), *Spawner->GetName());
			}
		}

		// B. 冻结所有单位 (包括敌我双方)
		TArray<AActor*> AllUnits;
		UGameplayStatics::GetAllActorsOfClass(World, ASG_UnitsBase::StaticClass(), AllUnits);
		
		for (AActor* Actor : AllUnits)
		{
			ASG_UnitsBase* Unit = Cast<ASG_UnitsBase>(Actor);
			if (Unit)
			{
				// 跳过已死亡的单位（包括刚被击飞的站桩单位）
				if (Unit->bIsDead)
				{
					continue;
				}
				
				// 1. 冻结 AI
				if (ASG_AIControllerBase* AICon = Cast<ASG_AIControllerBase>(Unit->GetController()))
				{
					AICon->FreezeAI();
				}
				
				// 2. 强制重置攻击状态
				Unit->bIsAttacking = false;
				
				// 3. 停止移动组件
				if (Unit->GetCharacterMovement())
				{
					Unit->GetCharacterMovement()->StopMovementImmediately();
					Unit->GetCharacterMovement()->DisableMovement();
				}
			}
		}
		
		UE_LOG(LogSGGameplay, Warning, TEXT("🛑 游戏结束：已停止 %d 个生成器和 %d 个单位"), AllSpawners.Num(), AllUnits.Num());
	}
	
	UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
}

// ========== ✨ 新增 - 击飞站桩单位功能实现 ==========

/**
 * @brief 击飞同阵营的站桩单位
 * @details
 * 功能说明：
 * - 查找所有同阵营的 SG_StationaryUnit
 * - 杀死它们并启用布娃娃
 * - 施加冲击波力使其被击飞
 * 详细流程：
 * 1. 获取冲击波原点（主城位置）
 * 2. 获取所有 SG_StationaryUnit
 * 3. 过滤同阵营单位
 * 4. 根据配置过滤范围
 * 5. 对每个单位执行击飞逻辑
 */
void ASG_MainCityBase::BlastStationaryUnits()
{
	UE_LOG(LogSGGameplay, Log, TEXT("========== 执行主城摧毁冲击波 =========="));
	
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("  ❌ World 为空"));
		return;
	}
	
	// 获取冲击波原点（主城位置）
	FVector BlastOrigin = GetActorLocation();
	UE_LOG(LogSGGameplay, Log, TEXT("  冲击波原点：%s"), *BlastOrigin.ToString());
	UE_LOG(LogSGGameplay, Log, TEXT("  冲击波范围：%.0f cm"), BlastRadius);
	UE_LOG(LogSGGameplay, Log, TEXT("  冲击波力度：%.0f"), BlastForce);
	UE_LOG(LogSGGameplay, Log, TEXT("  向上力度比例：%.2f"), BlastUpwardRatio);
	UE_LOG(LogSGGameplay, Log, TEXT("  影响所有站桩单位：%s"), bBlastAllStationaryUnits ? TEXT("是") : TEXT("否"));
	
	// 获取所有站桩单位
	TArray<AActor*> AllActors;
	UGameplayStatics::GetAllActorsOfClass(World, ASG_StationaryUnit::StaticClass(), AllActors);
	
	int32 AffectedCount = 0;
	
	for (AActor* Actor : AllActors)
	{
		ASG_StationaryUnit* StationaryUnit = Cast<ASG_StationaryUnit>(Actor);
		if (!StationaryUnit)
		{
			continue;
		}
		
		// 检查是否同阵营
		if (StationaryUnit->FactionTag != FactionTag)
		{
			UE_LOG(LogSGGameplay, Verbose, TEXT("  跳过不同阵营单位：%s（%s）"), 
				*StationaryUnit->GetName(), *StationaryUnit->FactionTag.ToString());
			continue;
		}
		
		// 检查是否已死亡
		if (StationaryUnit->bIsDead)
		{
			UE_LOG(LogSGGameplay, Verbose, TEXT("  跳过已死亡单位：%s"), *StationaryUnit->GetName());
			continue;
		}
		
		// 检查距离（如果不是影响所有）
		if (!bBlastAllStationaryUnits)
		{
			float Distance = FVector::Dist(BlastOrigin, StationaryUnit->GetActorLocation());
			if (Distance > BlastRadius)
			{
				UE_LOG(LogSGGameplay, Verbose, TEXT("  跳过超出范围单位：%s（距离：%.0f）"), 
					*StationaryUnit->GetName(), Distance);
				continue;
			}
		}
		
		// 执行击飞
		BlastSingleUnit(StationaryUnit, BlastOrigin);
		AffectedCount++;
	}
	
	UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 共击飞 %d 个站桩单位"), AffectedCount);
	UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
}

/**
 * @brief 对单个站桩单位执行击飞
 * @param Unit 目标单位
 * @param BlastOrigin 冲击波原点
 * @details
 * 功能说明：
 * - 设置单位死亡
 * - 停止 AI 和移动
 * - 禁用胶囊体碰撞
 * - 启用骨骼网格体布娃娃物理
 * - 施加径向冲击力
 * - 设置延迟销毁
 * 详细流程：
 * 1. 标记单位死亡
 * 2. 停止所有行为
 * 3. 禁用胶囊体碰撞
 * 4. 启用布娃娃物理
 * 5. 计算冲击力方向和大小
 * 6. 施加冲击力
 * 7. 设置延迟销毁
 */
void ASG_MainCityBase::BlastSingleUnit(ASG_StationaryUnit* Unit, const FVector& BlastOrigin)
{
	if (!Unit)
	{
		return;
	}
	
	UE_LOG(LogSGGameplay, Log, TEXT("  💥 击飞站桩单位：%s"), *Unit->GetName());
	
	// ========== 步骤1：标记死亡 ==========
	Unit->bIsDead = true;
	
	// ========== 步骤2：停止所有行为 ==========
	// 停止 AI
	if (AController* Controller = Unit->GetController())
	{
		if (ASG_AIControllerBase* AICon = Cast<ASG_AIControllerBase>(Controller))
		{
			AICon->FreezeAI();
		}
		Controller->UnPossess();
	}
	
	// 停止所有动画
	if (USkeletalMeshComponent* MeshComp = Unit->GetMesh())
	{
		if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
		{
			AnimInstance->StopAllMontages(0.0f);
		}
	}
	
	// 取消所有能力
	if (Unit->AbilitySystemComponent)
	{
		Unit->AbilitySystemComponent->CancelAllAbilities();
	}
	
	// ========== 步骤3：禁用胶囊体碰撞 ==========
	if (UCapsuleComponent* Capsule = Unit->GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	
	// 停止移动组件
	if (UCharacterMovementComponent* MoveComp = Unit->GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
		MoveComp->DisableMovement();
		MoveComp->SetComponentTickEnabled(false);
	}
	
	// ========== 步骤4：启用布娃娃物理 ==========
	USkeletalMeshComponent* MeshComp = Unit->GetMesh();
	if (MeshComp)
	{
		// 设置碰撞预设为 Ragdoll
		MeshComp->SetCollisionProfileName(TEXT("Ragdoll"));
		MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		
		// 启用物理模拟
		MeshComp->SetAllBodiesSimulatePhysics(true);
		MeshComp->SetSimulatePhysics(true);
		MeshComp->WakeAllRigidBodies();
		
		// ========== 步骤5：计算冲击力 ==========
		FVector UnitLocation = Unit->GetActorLocation();
		FVector BlastDirection = UnitLocation - BlastOrigin;
		float Distance = BlastDirection.Size();
		
		// 归一化方向
		if (Distance > KINDA_SMALL_NUMBER)
		{
			BlastDirection.Normalize();
		}
		else
		{
			// 如果距离太近，使用随机方向
			BlastDirection = FVector(FMath::FRandRange(-1.0f, 1.0f), FMath::FRandRange(-1.0f, 1.0f), 0.0f).GetSafeNormal();
		}
		
		// 计算力的大小（根据距离衰减，但最小保留 50%）
		float DistanceRatio = 1.0f;
		if (!bBlastAllStationaryUnits && BlastRadius > 0.0f)
		{
			DistanceRatio = FMath::Clamp(1.0f - (Distance / BlastRadius), 0.5f, 1.0f);
		}
		
		float FinalForce = BlastForce * DistanceRatio;
		
		// 添加向上的力
		FVector ImpulseDirection = BlastDirection;
		ImpulseDirection.Z = BlastUpwardRatio;
		ImpulseDirection.Normalize();
		
		FVector FinalImpulse = ImpulseDirection * FinalForce;
		
		UE_LOG(LogSGGameplay, Verbose, TEXT("    距离：%.0f cm"), Distance);
		UE_LOG(LogSGGameplay, Verbose, TEXT("    衰减比例：%.2f"), DistanceRatio);
		UE_LOG(LogSGGameplay, Verbose, TEXT("    最终力度：%.0f"), FinalForce);
		UE_LOG(LogSGGameplay, Verbose, TEXT("    冲击方向：%s"), *ImpulseDirection.ToString());
		
		// ========== 步骤6：施加冲击力 ==========
		// 对所有骨骼施加冲击力
		MeshComp->AddImpulse(FinalImpulse, NAME_None, true);
		
		// 额外对骨盆/根骨骼施加力（确保整体被推动）
		FName PelvisBone = TEXT("pelvis");  // 常见的骨盆骨骼名称
		if (MeshComp->GetBoneIndex(PelvisBone) != INDEX_NONE)
		{
			MeshComp->AddImpulse(FinalImpulse * 0.5f, PelvisBone, true);
		}
		
		// 尝试其他常见的根骨骼名称
		TArray<FName> RootBoneNames = { TEXT("root"), TEXT("Hips"), TEXT("spine_01"), TEXT("Spine") };
		for (const FName& BoneName : RootBoneNames)
		{
			if (MeshComp->GetBoneIndex(BoneName) != INDEX_NONE)
			{
				MeshComp->AddImpulse(FinalImpulse * 0.3f, BoneName, true);
				break;
			}
		}
	}
	else
	{
		UE_LOG(LogSGGameplay, Warning, TEXT("    ⚠️ 单位没有骨骼网格体，无法启用布娃娃"));
	}
	
	// ========== 步骤7：广播死亡事件 ==========
	Unit->OnUnitDeathEvent.Broadcast(Unit);
	
	// ========== 步骤8：设置延迟销毁 ==========
	Unit->SetLifeSpan(BlastDestroyDelay);
	
	UE_LOG(LogSGGameplay, Log, TEXT("    ✓ 将在 %.1f 秒后销毁"), BlastDestroyDelay);
}

/**
 * @brief 检查主城是否存活
 * @return 是否存活
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
