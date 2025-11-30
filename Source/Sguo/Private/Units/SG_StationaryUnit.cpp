// 📄 文件：Source/Sguo/Private/Units/SG_StationaryUnit.cpp
// 🔧 修改 - 添加火矢计相关功能实现

#include "Units/SG_StationaryUnit.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Debug/SG_LogCategories.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Actors/SG_Projectile.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/SG_AttributeSet.h"
#include "Data/Type/SG_UnitDataTable.h"

ASG_StationaryUnit::ASG_StationaryUnit()
{
	bEnableHover = false;
	HoverHeight = 0;
	bDisableGravity = true;
	bCanBeTargeted = true;
	bDisableMovement = true;
}

void ASG_StationaryUnit::BeginPlay()
{
	Super::BeginPlay();
	ApplyStationarySettings();

	UE_LOG(LogSGUnit, Log, TEXT("[站桩单位] %s 初始化完成 | 浮空:%s | 高度:%.1f | 可被选中:%s | 禁用移动:%s"),
		*GetName(),
		bEnableHover ? TEXT("是") : TEXT("否"),
		HoverHeight,
		bCanBeTargeted ? TEXT("是") : TEXT("否"),
		bDisableMovement ? TEXT("是") : TEXT("否")
	);
}

bool ASG_StationaryUnit::CanBeTargeted() const
{
	return bCanBeTargeted;
}

void ASG_StationaryUnit::ApplyStationarySettings()
{
	if (bDisableMovement)
	{
		DisableMovementCapability();
	}

	if (bEnableHover)
	{
		ApplyHoverEffect();
	}
}

void ASG_StationaryUnit::DisableMovementCapability()
{
	UCharacterMovementComponent* MovementComp = GetCharacterMovement();
    
	if (!MovementComp)
	{
		UE_LOG(LogSGUnit, Warning, TEXT("[站桩单位] %s 的 CharacterMovement 组件无效，无法禁用移动"), *GetName());
		return;
	}

	MovementComp->MaxWalkSpeed = 0.0f;
	MovementComp->MaxAcceleration = 0.0f;
	
	if (bEnableHover || bDisableGravity)
	{
		MovementComp->SetMovementMode(MOVE_Flying);
		MovementComp->GravityScale = 0.0f;
	}
	else
	{
		MovementComp->SetMovementMode(MOVE_Walking);
	}
    
	MovementComp->bUseRVOAvoidance = false;

	UE_LOG(LogSGUnit, Verbose, TEXT("[站桩单位] %s 移动能力已禁用（速度=0，模式=%s）"), 
		*GetName(),
		(bEnableHover || bDisableGravity) ? TEXT("Flying") : TEXT("Walking"));
}

void ASG_StationaryUnit::ApplyHoverEffect()
{
	FVector CurrentLocation = GetActorLocation();
	FVector NewLocation = CurrentLocation;
	NewLocation.Z += HoverHeight;
	
	SetActorLocation(NewLocation, false, nullptr, ETeleportType::TeleportPhysics);
	
	if (bDisableGravity)
	{
		UCharacterMovementComponent* MovementComp = GetCharacterMovement();
		
		if (MovementComp)
		{
			MovementComp->GravityScale = 0.0f;
			MovementComp->SetMovementMode(MOVE_Flying);
		}
	}

	UE_LOG(LogSGUnit, Verbose, TEXT("[站桩单位] %s 浮空效果已应用 | 原始高度:%.1f | 新高度:%.1f | 偏移:%.1f"),
		*GetName(),
		CurrentLocation.Z,
		NewLocation.Z,
		HoverHeight
	);
}

// ========== ✨ 新增 - 火矢计相关实现 ==========

void ASG_StationaryUnit::StartFireArrowSkill()
{
	UE_LOG(LogSGUnit, Log, TEXT("[站桩单位] %s 开始火矢技能"), *GetName());

	// 打断当前攻击
	if (bIsAttacking)
	{
		// 停止当前攻击动画
		if (USkeletalMeshComponent* MeshComp = GetMesh())
		{
			if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
			{
				AnimInstance->Montage_Stop(0.2f);
			}
		}
		bIsAttacking = false;
		
		UE_LOG(LogSGUnit, Log, TEXT("  ✓ 打断了当前普通攻击"));
	}

	// 设置火矢技能状态
	bIsExecutingFireArrow = true;

	// 缓存原始投射物类（如果有的话）
	// 从当前攻击配置中获取
	if (CachedAttackAbilities.Num() > 0)
	{
		CachedOriginalProjectileClass = CachedAttackAbilities[CurrentAttackIndex].ProjectileClass;
		UE_LOG(LogSGUnit, Verbose, TEXT("  缓存原始投射物类：%s"), 
			CachedOriginalProjectileClass ? *CachedOriginalProjectileClass->GetName() : TEXT("无"));
	}
}

void ASG_StationaryUnit::EndFireArrowSkill()
{
	UE_LOG(LogSGUnit, Log, TEXT("[站桩单位] %s 结束火矢技能"), *GetName());

	// 清除火矢技能状态
	bIsExecutingFireArrow = false;

	// 恢复原始投射物类
	if (CachedOriginalProjectileClass && CachedAttackAbilities.Num() > 0)
	{
		CachedAttackAbilities[CurrentAttackIndex].ProjectileClass = CachedOriginalProjectileClass;
		UE_LOG(LogSGUnit, Verbose, TEXT("  恢复原始投射物类：%s"), 
			*CachedOriginalProjectileClass->GetName());
	}
	
	CachedOriginalProjectileClass = nullptr;
}

AActor* ASG_StationaryUnit::FireArrow(const FVector& TargetLocation, TSubclassOf<AActor> ProjectileClassOverride)
{
	// 确定使用的投射物类
	TSubclassOf<AActor> ProjectileClass = ProjectileClassOverride;
	if (!ProjectileClass)
	{
		ProjectileClass = GetFireArrowProjectileClass();
	}
	if (!ProjectileClass)
	{
		ProjectileClass = ASG_Projectile::StaticClass();
	}

	// 播放火矢攻击动画
	if (FireArrowMontage)
	{
		if (USkeletalMeshComponent* MeshComp = GetMesh())
		{
			if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
			{
				// 获取攻击速度作为播放速率
				float PlayRate = 1.0f;
				if (AttributeSet)
				{
					PlayRate = AttributeSet->GetAttackSpeed();
				}
				
				AnimInstance->Montage_Play(FireArrowMontage, PlayRate);
				
				UE_LOG(LogSGUnit, Verbose, TEXT("  播放火矢动画：%s (速率: %.2f)"), 
					*FireArrowMontage->GetName(), PlayRate);
			}
		}
	}

	// 获取发射位置
	FVector SpawnLocation = GetActorLocation();
	
	// 计算发射方向
	FVector ToTarget = TargetLocation - SpawnLocation;
	FRotator SpawnRotation = ToTarget.Rotation();

	// 生成投射物
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* SpawnedActor = GetWorld()->SpawnActor<AActor>(
		ProjectileClass,
		SpawnLocation,
		SpawnRotation,
		SpawnParams
	);

	// 初始化投射物
	if (ASG_Projectile* Projectile = Cast<ASG_Projectile>(SpawnedActor))
	{
		// 获取 ASC
		UAbilitySystemComponent* MyASC = GetAbilitySystemComponent();

		// 初始化投射物（目标为位置）
		Projectile->InitializeProjectileToLocation(
			MyASC,
			FactionTag,
			TargetLocation,
			-1.0f  // 使用默认弧度
		);

		// 设置投射物飞向地面
		Projectile->TargetMode = ESGProjectileTargetMode::TargetLocation;

		UE_LOG(LogSGUnit, Verbose, TEXT("  发射火矢 -> %s"), *TargetLocation.ToString());
	}

	return SpawnedActor;
}

TSubclassOf<AActor> ASG_StationaryUnit::GetFireArrowProjectileClass() const
{
	// 优先使用专用的火矢投射物类
	if (FireArrowProjectileClass)
	{
		return FireArrowProjectileClass;
	}

	// 其次使用当前攻击配置的投射物类
	if (CachedAttackAbilities.Num() > 0 && CachedAttackAbilities[CurrentAttackIndex].ProjectileClass)
	{
		return CachedAttackAbilities[CurrentAttackIndex].ProjectileClass;
	}

	// 最后使用默认投射物类
	return ASG_Projectile::StaticClass();
}
