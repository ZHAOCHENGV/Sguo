// ✨ 新增 - 投射物Actor实现
// Copyright notice placeholder

#include "Actors/SG_Projectile.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "AbilitySystemComponent.h"
#include "Units/SG_UnitsBase.h"
#include "Debug/SG_LogCategories.h"
#include "GameplayEffect.h"

// ========== 构造函数 ==========
ASG_Projectile::ASG_Projectile()
{
	// 不需要每帧Tick
	PrimaryActorTick.bCanEverTick = false;

	// 创建碰撞组件（作为根组件）
	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	// 设置碰撞半径
	CollisionComponent->InitSphereRadius(5.0f);
	// 设置碰撞通道
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionComponent->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	// 绑定碰撞事件
	CollisionComponent->OnComponentHit.AddDynamic(this, &ASG_Projectile::OnProjectileHit);
	// 设置为根组件
	SetRootComponent(CollisionComponent);

	// 创建网格体组件
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	// 附加到碰撞组件
	MeshComponent->SetupAttachment(CollisionComponent);
	// 禁用网格体的碰撞
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 创建投射物移动组件
	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	// 设置初始速度
	ProjectileMovement->InitialSpeed = ProjectileSpeed;
	ProjectileMovement->MaxSpeed = ProjectileSpeed;
	// 不受重力影响（默认直线飞行）
	ProjectileMovement->ProjectileGravityScale = 0.0f;
	// 启用旋转跟随速度
	ProjectileMovement->bRotationFollowsVelocity = true;
	// 禁用反弹
	ProjectileMovement->bShouldBounce = false;

	// 启用网络复制
	bReplicates = true;
}

// ========== BeginPlay ==========
void ASG_Projectile::BeginPlay()
{
	Super::BeginPlay();

	// 设置生存时间
	SetLifeSpan(LifeSpan);

	// 输出日志：投射物生成
	UE_LOG(LogSGGameplay, Verbose, TEXT("投射物生成：%s"), *GetName());
}

// ========== 初始化投射物 ==========
void ASG_Projectile::InitializeProjectile(
	UAbilitySystemComponent* InInstigatorASC,
	FGameplayTag InFactionTag,
	FVector InDirection
)
{
	// 保存攻击者信息
	InstigatorASC = InInstigatorASC;
	InstigatorFactionTag = InFactionTag;

	// 输出日志：初始化投射物
	UE_LOG(LogSGGameplay, Log, TEXT("========== 初始化投射物 =========="));
	UE_LOG(LogSGGameplay, Log, TEXT("  攻击者阵营：%s"), *InstigatorFactionTag.ToString());
	UE_LOG(LogSGGameplay, Log, TEXT("  飞行方向：%s"), *InDirection.ToString());

	// 归一化方向向量
	InDirection.Normalize();

	// 设置投射物朝向
	FRotator Rotation = InDirection.Rotation();
	SetActorRotation(Rotation);

	// 设置飞行速度和方向
	ProjectileMovement->Velocity = InDirection * ProjectileSpeed;

	// 根据飞行类型配置重力
	switch (ProjectileType)
	{
	case ESGProjectileType::Linear:
		// 直线飞行：不受重力影响
		ProjectileMovement->ProjectileGravityScale = 0.0f;
		UE_LOG(LogSGGameplay, Log, TEXT("  飞行类型：直线"));
		break;

	case ESGProjectileType::Parabolic:
		// 抛物线飞行：受重力影响
		ProjectileMovement->ProjectileGravityScale = GravityScale;
		UE_LOG(LogSGGameplay, Log, TEXT("  飞行类型：抛物线（重力: %.2f)"), GravityScale);
		break;
	}

	// 输出日志：初始化完成
	UE_LOG(LogSGGameplay, Log, TEXT("  飞行速度：%.1f"), ProjectileSpeed);
	UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
}

// ========== 碰撞事件 ==========
void ASG_Projectile::OnProjectileHit(
	UPrimitiveComponent* HitComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	FVector NormalImpulse,
	const FHitResult& Hit
)
{
	// 输出日志：碰撞事件
	UE_LOG(LogSGGameplay, Verbose, TEXT("投射物碰撞：%s"), OtherActor ? *OtherActor->GetName() : TEXT("None"));

	// 检查目标是否有效
	if (!OtherActor || OtherActor == GetOwner())
	{
		return;
	}

	// 检查是否是单位
	ASG_UnitsBase* TargetUnit = Cast<ASG_UnitsBase>(OtherActor);
	if (!TargetUnit)
	{
		// 不是单位，直接销毁
		UE_LOG(LogSGGameplay, Verbose, TEXT("  碰撞非单位，销毁投射物"));
		Destroy();
		return;
	}

	// 检查是否是敌方单位
	if (TargetUnit->FactionTag == InstigatorFactionTag)
	{
		// 是友方单位，忽略
		UE_LOG(LogSGGameplay, Verbose, TEXT("  碰撞友方单位，忽略"));
		return;
	}

	// 检查是否已经击中过此目标（防止重复伤害）
	if (HitActors.Contains(OtherActor))
	{
		UE_LOG(LogSGGameplay, Verbose, TEXT("  已击中过此目标，忽略"));
		return;
	}

	// 输出日志：击中敌方单位
	UE_LOG(LogSGGameplay, Log, TEXT("  🎯 击中敌方单位：%s"), *TargetUnit->GetName());

	// 应用伤害
	ApplyDamageToTarget(OtherActor);

	// 记录已击中目标
	HitActors.Add(OtherActor);

	// 触发蓝图事件
	OnHitTarget(OtherActor);

	// 如果不穿透，销毁投射物
	if (!bPenetrate)
	{
		UE_LOG(LogSGGameplay, Verbose, TEXT("  不穿透，销毁投射物"));
		Destroy();
	}
	else
	{
		// 检查是否达到最大穿透数量
		if (MaxPenetrateCount > 0 && HitActors.Num() >= MaxPenetrateCount)
		{
			UE_LOG(LogSGGameplay, Log, TEXT("  达到最大穿透数量，销毁投射物"));
			Destroy();
		}
		else
		{
			UE_LOG(LogSGGameplay, Verbose, TEXT("  穿透继续飞行（已击中: %d/%d）"), 
				HitActors.Num(), MaxPenetrateCount);
		}
	}
}

// ========== 应用伤害到目标 ==========
void ASG_Projectile::ApplyDamageToTarget(AActor* Target)
{
	// 检查目标是否有效
	if (!Target)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("ApplyDamageToTarget 失败：目标为空"));
		return;
	}

	// 获取目标的 AbilitySystemComponent
	// 🔧 修改 - UE 5.6 API 变更：使用 UAbilitySystemGlobals::GetAbilitySystemComponentFromActor
	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	if (!TargetASC)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("ApplyDamageToTarget 失败：目标没有 ASC"));
		return;
	}

	// 检查攻击者 ASC 是否有效
	if (!InstigatorASC)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("ApplyDamageToTarget 失败：攻击者 ASC 为空"));
		return;
	}

	// 检查伤害 GE 是否有效
	if (!DamageEffectClass)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("ApplyDamageToTarget 失败：伤害 GE 未设置"));
		return;
	}

	// 创建 EffectContext
	FGameplayEffectContextHandle EffectContext = InstigatorASC->MakeEffectContext();
	EffectContext.AddInstigator(GetOwner(), this);

	// 创建 EffectSpec
	FGameplayEffectSpecHandle SpecHandle = InstigatorASC->MakeOutgoingSpec(
		DamageEffectClass,
		1.0f, // Level
		EffectContext
	);

	// 检查 SpecHandle 是否有效
	if (!SpecHandle.IsValid())
	{
		UE_LOG(LogSGGameplay, Error, TEXT("ApplyDamageToTarget 失败：创建 EffectSpec 失败"));
		return;
	}

	// 设置伤害倍率（SetByCaller）
	FGameplayTag DamageTag = FGameplayTag::RequestGameplayTag(FName("Data.Damage"));
	SpecHandle.Data->SetSetByCallerMagnitude(DamageTag, DamageMultiplier);

	// 输出日志：应用伤害
	UE_LOG(LogSGGameplay, Verbose, TEXT("    投射物伤害倍率：%.2f"), DamageMultiplier);

	// 应用 GameplayEffect 到目标
	FActiveGameplayEffectHandle ActiveHandle = InstigatorASC->ApplyGameplayEffectSpecToTarget(
		*SpecHandle.Data.Get(),
		TargetASC
	);

	// 检查是否应用成功
	if (ActiveHandle.IsValid())
	{
		UE_LOG(LogSGGameplay, Verbose, TEXT("    ✓ 投射物伤害应用成功"));
	}
	else
	{
		UE_LOG(LogSGGameplay, Error, TEXT("    ❌ 投射物伤害应用失败"));
	}
}
