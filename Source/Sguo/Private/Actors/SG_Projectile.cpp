// 📄 文件：Source/Sguo/Private/Actors/SG_Projectile.cpp

#include "Actors/SG_Projectile.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"  // ✨ 新增 - 修复 UBoxComponent 未定义
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Units/SG_UnitsBase.h"
#include "Buildings/SG_MainCityBase.h"
#include "Debug/SG_LogCategories.h"
#include "GameplayEffect.h"
#include "GameplayCueManager.h"
#include "DrawDebugHelpers.h"

// ========== 构造函数 ==========
ASG_Projectile::ASG_Projectile()
{
	PrimaryActorTick.bCanEverTick = true;

	// ========== 创建场景根组件 ==========
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	// ========== 创建胶囊体碰撞组件 ==========
	CollisionCapsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CollisionCapsule"));
	CollisionCapsule->SetupAttachment(RootComponent);
	
	CollisionCapsule->SetCapsuleRadius(CapsuleRadius);
	CollisionCapsule->SetCapsuleHalfHeight(CapsuleHalfHeight);
	CollisionCapsule->SetRelativeRotation(CollisionRotationOffset);
	
	// 🔧 修改 - 碰撞设置
	CollisionCapsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionCapsule->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionCapsule->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionCapsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	CollisionCapsule->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	CollisionCapsule->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	// ✨ 新增 - 确保能检测到 OverlapAllDynamic 类型的碰撞体
	CollisionCapsule->SetGenerateOverlapEvents(true);
	
	// 绑定碰撞事件
	CollisionCapsule->OnComponentBeginOverlap.AddDynamic(this, &ASG_Projectile::OnCapsuleOverlap);
	CollisionCapsule->OnComponentHit.AddDynamic(this, &ASG_Projectile::OnCapsuleHit);

	// ========== 创建网格体组件 ==========
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(RootComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	bReplicates = true;
}

// ========== BeginPlay ==========
void ASG_Projectile::BeginPlay()
{
	Super::BeginPlay();

	SetLifeSpan(LifeSpan);

	// 更新胶囊体尺寸和旋转
	if (CollisionCapsule)
	{
		CollisionCapsule->SetCapsuleRadius(CapsuleRadius);
		CollisionCapsule->SetCapsuleHalfHeight(CapsuleHalfHeight);
		CollisionCapsule->SetRelativeRotation(CollisionRotationOffset);
	}

	// 激活飞行 GC
	ActivateTrailGameplayCue();

	UE_LOG(LogSGGameplay, Verbose, TEXT("投射物生成：%s"), *GetName());
	UE_LOG(LogSGGameplay, Verbose, TEXT("  飞行模式：%s"), 
		FlightMode == ESGProjectileFlightMode::Linear ? TEXT("直线") :
		FlightMode == ESGProjectileFlightMode::Parabolic ? TEXT("抛物线") : TEXT("归航"));
	UE_LOG(LogSGGameplay, Verbose, TEXT("  飞行速度：%.1f"), FlightSpeed);
	UE_LOG(LogSGGameplay, Verbose, TEXT("  弧度高度：%.1f"), ArcHeight);
}

// ========== EndPlay ==========
void ASG_Projectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RemoveTrailGameplayCue();
	ExecuteDestroyGameplayCue();
	
	K2_OnProjectileDestroyed(GetActorLocation());
	
	FSGProjectileHitInfo DestroyInfo;
	DestroyInfo.HitLocation = GetActorLocation();
	DestroyInfo.ProjectileDirection = CurrentVelocity.GetSafeNormal();
	DestroyInfo.ProjectileSpeed = CurrentVelocity.Size();
	OnProjectileDestroyed.Broadcast(DestroyInfo);

	Super::EndPlay(EndPlayReason);
}

// ========== Tick ==========
void ASG_Projectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bIsInitialized)
	{
		return;
	}

	// 根据飞行模式更新位置
	switch (FlightMode)
	{
	case ESGProjectileFlightMode::Linear:
		UpdateLinearFlight(DeltaTime);
		break;

	case ESGProjectileFlightMode::Parabolic:
		UpdateParabolicFlight(DeltaTime);
		break;

	case ESGProjectileFlightMode::Homing:
		UpdateHomingFlight(DeltaTime);
		break;
	}

	// 更新旋转
	UpdateRotation();

#if WITH_EDITOR
	// 调试绘制
	if (bDrawDebugTrajectory)
	{
		DrawDebugLine(GetWorld(), GetActorLocation(), GetActorLocation() + CurrentVelocity.GetSafeNormal() * 100.0f, FColor::Red, false, -1.0f, 0, 2.0f);
	}
#endif
}

// ========== 初始化投射物 ==========
void ASG_Projectile::InitializeProjectile(
	UAbilitySystemComponent* InInstigatorASC,
	FGameplayTag InFactionTag,
	AActor* InTarget,
	float InArcHeight
)
{
	InstigatorASC = InInstigatorASC;
	InstigatorFactionTag = InFactionTag;
	CurrentTarget = InTarget;

	// 记录起始位置
	StartLocation = GetActorLocation();

	// 计算目标位置
	if (InTarget)
	{
		TargetLocation = InTarget->GetActorLocation();

		// 瞄准目标中心
		if (ASG_UnitsBase* TargetUnit = Cast<ASG_UnitsBase>(InTarget))
		{
			if (UCapsuleComponent* Capsule = TargetUnit->GetCapsuleComponent())
			{
				TargetLocation.Z += Capsule->GetScaledCapsuleHalfHeight() * 0.5f;
			}
		}
		else if (ASG_MainCityBase* MainCity = Cast<ASG_MainCityBase>(InTarget))
		{
			if (UBoxComponent* DetectionBox = MainCity->GetAttackDetectionBox())
			{
				TargetLocation = DetectionBox->GetComponentLocation();
			}
		}
	}
	else
	{
		// 如果没有目标，向前飞行
		TargetLocation = StartLocation + GetActorForwardVector() * 5000.0f;
	}

	// 应用弧度高度覆盖
	if (InArcHeight >= 0.0f)
	{
		ArcHeight = InArcHeight;
	}

	// 计算飞行距离
	TotalFlightDistance = FVector::Dist(StartLocation, TargetLocation);

	// 初始化速度向量
	FVector Direction = (TargetLocation - StartLocation).GetSafeNormal();
	CurrentVelocity = Direction * FlightSpeed;

	// 重置飞行进度
	FlightProgress = 0.0f;

	bIsInitialized = true;

	UE_LOG(LogSGGameplay, Log, TEXT("========== 初始化投射物 =========="));
	UE_LOG(LogSGGameplay, Log, TEXT("  目标：%s"), InTarget ? *InTarget->GetName() : TEXT("无"));
	UE_LOG(LogSGGameplay, Log, TEXT("  起点：%s"), *StartLocation.ToString());
	UE_LOG(LogSGGameplay, Log, TEXT("  终点：%s"), *TargetLocation.ToString());
	UE_LOG(LogSGGameplay, Log, TEXT("  距离：%.1f"), TotalFlightDistance);
	UE_LOG(LogSGGameplay, Log, TEXT("  速度：%.1f"), FlightSpeed);
	UE_LOG(LogSGGameplay, Log, TEXT("  弧度：%.1f"), ArcHeight);
	UE_LOG(LogSGGameplay, Log, TEXT("  模式：%s"), 
		FlightMode == ESGProjectileFlightMode::Linear ? TEXT("直线") :
		FlightMode == ESGProjectileFlightMode::Parabolic ? TEXT("抛物线") : TEXT("归航"));
	UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
}

// ========== 设置飞行速度 ==========
void ASG_Projectile::SetFlightSpeed(float NewSpeed)
{
	FlightSpeed = FMath::Max(100.0f, NewSpeed);
	
	// 更新当前速度向量的大小
	if (!CurrentVelocity.IsNearlyZero())
	{
		CurrentVelocity = CurrentVelocity.GetSafeNormal() * FlightSpeed;
	}
}

// ========== 直线飞行 ==========
void ASG_Projectile::UpdateLinearFlight(float DeltaTime)
{
	// 计算移动距离
	float MoveDistance = FlightSpeed * DeltaTime;
	
	// 计算方向（持续指向目标）
	FVector CurrentLocation = GetActorLocation();
	FVector ToTarget = TargetLocation - CurrentLocation;
	
	if (ToTarget.Size() <= MoveDistance)
	{
		// 已到达目标位置
		SetActorLocation(TargetLocation);
		CurrentVelocity = ToTarget.GetSafeNormal() * FlightSpeed;
	}
	else
	{
		// 继续飞行
		FVector Direction = ToTarget.GetSafeNormal();
		CurrentVelocity = Direction * FlightSpeed;
		SetActorLocation(CurrentLocation + CurrentVelocity * DeltaTime);
	}
}

// ========== 抛物线飞行 ==========
void ASG_Projectile::UpdateParabolicFlight(float DeltaTime)
{
	// 更新飞行进度
	float DistanceThisFrame = FlightSpeed * DeltaTime;
	FlightProgress += DistanceThisFrame / TotalFlightDistance;
	FlightProgress = FMath::Clamp(FlightProgress, 0.0f, 1.0f);

	// 计算当前位置
	FVector NewLocation = CalculateParabolicPosition(FlightProgress);
	
	// 计算速度向量（用于旋转）
	FVector PreviousLocation = GetActorLocation();
	CurrentVelocity = (NewLocation - PreviousLocation) / DeltaTime;
	
	// 如果速度过小，使用方向估算
	if (CurrentVelocity.Size() < 1.0f)
	{
		// 使用下一帧的位置估算方向
		float NextProgress = FMath::Clamp(FlightProgress + 0.01f, 0.0f, 1.0f);
		FVector NextLocation = CalculateParabolicPosition(NextProgress);
		CurrentVelocity = (NextLocation - NewLocation).GetSafeNormal() * FlightSpeed;
	}

	// 更新位置
	SetActorLocation(NewLocation);

	// 如果目标还活着，动态更新目标位置
	if (CurrentTarget.IsValid())
	{
		AActor* Target = CurrentTarget.Get();
		FVector NewTargetLocation = Target->GetActorLocation();
		
		// 瞄准目标中心
		if (ASG_UnitsBase* TargetUnit = Cast<ASG_UnitsBase>(Target))
		{
			if (UCapsuleComponent* Capsule = TargetUnit->GetCapsuleComponent())
			{
				NewTargetLocation.Z += Capsule->GetScaledCapsuleHalfHeight() * 0.5f;
			}
		}
		
		// 平滑更新目标位置（避免抖动）
		TargetLocation = FMath::VInterpTo(TargetLocation, NewTargetLocation, DeltaTime, 5.0f);
	}

#if WITH_EDITOR
	// 调试绘制抛物线
	if (bDrawDebugTrajectory)
	{
		for (float t = 0.0f; t < 1.0f; t += 0.05f)
		{
			FVector P1 = CalculateParabolicPosition(t);
			FVector P2 = CalculateParabolicPosition(t + 0.05f);
			DrawDebugLine(GetWorld(), P1, P2, FColor::Green, false, 0.1f, 0, 1.0f);
		}
	}
#endif
}

// ========== 计算抛物线位置 ==========
FVector ASG_Projectile::CalculateParabolicPosition(float Progress) const
{
	// 线性插值基础位置
	FVector LinearPosition = FMath::Lerp(StartLocation, TargetLocation, Progress);

	// 计算抛物线高度偏移
	// 使用 sin 曲线：在 Progress=0.5 时达到最大高度
	float HeightOffset = FMath::Sin(Progress * PI) * ArcHeight;

	// 应用高度偏移
	return LinearPosition + FVector(0.0f, 0.0f, HeightOffset);
}

// ========== 归航飞行 ==========
void ASG_Projectile::UpdateHomingFlight(float DeltaTime)
{
	// 更新目标位置
	if (CurrentTarget.IsValid())
	{
		AActor* Target = CurrentTarget.Get();
		TargetLocation = Target->GetActorLocation();
		
		// 瞄准目标中心
		if (ASG_UnitsBase* TargetUnit = Cast<ASG_UnitsBase>(Target))
		{
			if (UCapsuleComponent* Capsule = TargetUnit->GetCapsuleComponent())
			{
				TargetLocation.Z += Capsule->GetScaledCapsuleHalfHeight() * 0.5f;
			}
		}
	}

	// 计算当前方向和目标方向
	FVector CurrentDirection = CurrentVelocity.GetSafeNormal();
	FVector DesiredDirection = (TargetLocation - GetActorLocation()).GetSafeNormal();

	// 计算最大转向角度
	float MaxTurnAngle = HomingStrength * DeltaTime;

	// 插值转向
	FVector NewDirection = FMath::VInterpNormalRotationTo(
		CurrentDirection,
		DesiredDirection,
		DeltaTime,
		HomingStrength
	);

	// 更新速度向量
	CurrentVelocity = NewDirection * FlightSpeed;

	// 更新位置
	SetActorLocation(GetActorLocation() + CurrentVelocity * DeltaTime);
}

// ========== 更新旋转 ==========
void ASG_Projectile::UpdateRotation()
{
	if (!CurrentVelocity.IsNearlyZero())
	{
		FRotator NewRotation = CurrentVelocity.Rotation();
		SetActorRotation(NewRotation);
	}
}

// ========== 胶囊体 Overlap 事件 ==========
void ASG_Projectile::OnCapsuleOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult
)
{
	HandleProjectileImpact(OtherActor, SweepResult);
}

// ========== 胶囊体 Hit 事件 ==========
void ASG_Projectile::OnCapsuleHit(
	UPrimitiveComponent* HitComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	FVector NormalImpulse,
	const FHitResult& Hit
)
{
	HandleProjectileImpact(OtherActor, Hit);
}

// ========== 碰撞处理 ==========
void ASG_Projectile::HandleProjectileImpact(AActor* OtherActor, const FHitResult& Hit)
{
	UE_LOG(LogSGGameplay, Verbose, TEXT("投射物碰撞：%s"), OtherActor ? *OtherActor->GetName() : TEXT("None"));

	if (!OtherActor || OtherActor == GetOwner() || OtherActor == GetInstigator())
	{
		return;
	}

	// ========== 检查是否是主城 ==========
	ASG_MainCityBase* TargetMainCity = Cast<ASG_MainCityBase>(OtherActor);
	if (TargetMainCity)
	{
		if (TargetMainCity->FactionTag == InstigatorFactionTag)
		{
			UE_LOG(LogSGGameplay, Verbose, TEXT("  碰撞友方主城，忽略"));
			return;
		}

		if (HitActors.Contains(OtherActor))
		{
			return;
		}

		if (!TargetMainCity->IsAlive())
		{
			Destroy();
			return;
		}

		UE_LOG(LogSGGameplay, Log, TEXT("  🏰 击中敌方主城：%s"), *TargetMainCity->GetName());

		FSGProjectileHitInfo HitInfo;
		HitInfo.HitActor = OtherActor;
		HitInfo.HitLocation = Hit.ImpactPoint.IsNearlyZero() ? OtherActor->GetActorLocation() : FVector(Hit.ImpactPoint);
		HitInfo.HitNormal = Hit.ImpactNormal.IsNearlyZero() ? -GetActorForwardVector() : FVector(Hit.ImpactNormal);
		HitInfo.ProjectileDirection = CurrentVelocity.GetSafeNormal();
		HitInfo.ProjectileSpeed = CurrentVelocity.Size();

		ApplyDamageToTarget(OtherActor);
		HitActors.Add(OtherActor);

		ExecuteHitGameplayCue(HitInfo);
		K2_OnHitTarget(HitInfo);
		OnProjectileHitTarget.Broadcast(HitInfo);

		if (!bPenetrate || (MaxPenetrateCount > 0 && HitActors.Num() >= MaxPenetrateCount))
		{
			Destroy();
		}
		return;
	}

	// ========== 检查是否是单位 ==========
	ASG_UnitsBase* TargetUnit = Cast<ASG_UnitsBase>(OtherActor);
	if (!TargetUnit)
	{
		// 撞墙
		FSGProjectileHitInfo HitInfo;
		HitInfo.HitActor = OtherActor;
		HitInfo.HitLocation = Hit.ImpactPoint.IsNearlyZero() ? GetActorLocation() : FVector(Hit.ImpactPoint);
		HitInfo.HitNormal = Hit.ImpactNormal.IsNearlyZero() ? -GetActorForwardVector() : FVector(Hit.ImpactNormal);
		HitInfo.ProjectileDirection = CurrentVelocity.GetSafeNormal();
		HitInfo.ProjectileSpeed = CurrentVelocity.Size();
		
		ExecuteHitGameplayCue(HitInfo);
		K2_OnHitTarget(HitInfo);
		OnProjectileHitTarget.Broadcast(HitInfo);
		
		Destroy();
		return;
	}

	// 检查阵营
	if (TargetUnit->FactionTag == InstigatorFactionTag)
	{
		return;
	}

	// 检查是否已击中
	if (HitActors.Contains(OtherActor))
	{
		return;
	}

	// 检查是否已死亡
	if (TargetUnit->bIsDead)
	{
		return;
	}

	UE_LOG(LogSGGameplay, Log, TEXT("  🎯 击中敌方单位：%s"), *TargetUnit->GetName());

	FSGProjectileHitInfo HitInfo;
	HitInfo.HitActor = OtherActor;
	HitInfo.HitLocation = Hit.ImpactPoint.IsNearlyZero() ? OtherActor->GetActorLocation() : FVector(Hit.ImpactPoint);
	HitInfo.HitNormal = Hit.ImpactNormal.IsNearlyZero() ? -GetActorForwardVector() : FVector(Hit.ImpactNormal);
	HitInfo.HitBoneName = Hit.BoneName;
	HitInfo.ProjectileDirection = CurrentVelocity.GetSafeNormal();
	HitInfo.ProjectileSpeed = CurrentVelocity.Size();

	ApplyDamageToTarget(OtherActor);
	HitActors.Add(OtherActor);

	ExecuteHitGameplayCue(HitInfo);
	K2_OnHitTarget(HitInfo);
	OnProjectileHitTarget.Broadcast(HitInfo);

	if (!bPenetrate || (MaxPenetrateCount > 0 && HitActors.Num() >= MaxPenetrateCount))
	{
		Destroy();
	}
}

// ========== 应用伤害 ==========
void ASG_Projectile::ApplyDamageToTarget(AActor* Target)
{
	if (!Target)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("ApplyDamageToTarget 失败：目标为空"));
		return;
	}

	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	if (!TargetASC)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("ApplyDamageToTarget 失败：目标没有 ASC"));
		return;
	}

	if (!InstigatorASC)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("ApplyDamageToTarget 失败：攻击者 ASC 为空"));
		return;
	}

	if (!DamageEffectClass)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("ApplyDamageToTarget 失败：伤害 GE 未设置"));
		return;
	}

	FGameplayEffectContextHandle EffectContext = InstigatorASC->MakeEffectContext();
	EffectContext.AddInstigator(GetOwner(), this);

	FGameplayEffectSpecHandle SpecHandle = InstigatorASC->MakeOutgoingSpec(DamageEffectClass, 1.0f, EffectContext);

	if (!SpecHandle.IsValid())
	{
		UE_LOG(LogSGGameplay, Error, TEXT("ApplyDamageToTarget 失败：创建 EffectSpec 失败"));
		return;
	}

	FGameplayTag DamageTag = FGameplayTag::RequestGameplayTag(FName("Data.Damage"));
	SpecHandle.Data->SetSetByCallerMagnitude(DamageTag, DamageMultiplier);

	FActiveGameplayEffectHandle ActiveHandle = InstigatorASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);

	if (ActiveHandle.IsValid() || SpecHandle.IsValid())
	{
		UE_LOG(LogSGGameplay, Log, TEXT("    ✓ 投射物伤害应用成功（倍率：%.2f）"), DamageMultiplier);
	}
	else
	{
		UE_LOG(LogSGGameplay, Error, TEXT("    ❌ 投射物伤害应用失败"));
	}
}

// ========== GameplayCue 函数 ==========

void ASG_Projectile::ExecuteHitGameplayCue(const FSGProjectileHitInfo& HitInfo)
{
	if (!HitGameplayCueTag.IsValid())
	{
		return;
	}

	FGameplayCueParameters CueParams;
	CueParams.Location = HitInfo.HitLocation;
	CueParams.Normal = HitInfo.HitNormal;
	CueParams.Instigator = GetInstigator();
	CueParams.EffectCauser = this;
	CueParams.SourceObject = this;
	
	if (InstigatorASC)
	{
		InstigatorASC->ExecuteGameplayCue(HitGameplayCueTag, CueParams);
	}
	else
	{
		if (UGameplayCueManager* CueManager = UAbilitySystemGlobals::Get().GetGameplayCueManager())
		{
			CueManager->HandleGameplayCue(nullptr, HitGameplayCueTag, EGameplayCueEvent::Executed, CueParams);
		}
	}
}

void ASG_Projectile::ActivateTrailGameplayCue()
{
	if (!TrailGameplayCueTag.IsValid() || bTrailCueActive)
	{
		return;
	}

	FGameplayCueParameters CueParams;
	CueParams.Location = GetActorLocation();
	CueParams.Instigator = GetInstigator();
	CueParams.EffectCauser = this;
	CueParams.SourceObject = this;

	if (InstigatorASC)
	{
		InstigatorASC->AddGameplayCue(TrailGameplayCueTag, CueParams);
		bTrailCueActive = true;
	}
}

void ASG_Projectile::RemoveTrailGameplayCue()
{
	if (!TrailGameplayCueTag.IsValid() || !bTrailCueActive)
	{
		return;
	}

	if (InstigatorASC)
	{
		InstigatorASC->RemoveGameplayCue(TrailGameplayCueTag);
		bTrailCueActive = false;
	}
}

void ASG_Projectile::ExecuteDestroyGameplayCue()
{
	if (!DestroyGameplayCueTag.IsValid())
	{
		return;
	}

	FGameplayCueParameters CueParams;
	CueParams.Location = GetActorLocation();
	CueParams.Normal = -GetActorForwardVector();
	CueParams.Instigator = GetInstigator();
	CueParams.EffectCauser = this;

	if (InstigatorASC)
	{
		InstigatorASC->ExecuteGameplayCue(DestroyGameplayCueTag, CueParams);
	}
}
