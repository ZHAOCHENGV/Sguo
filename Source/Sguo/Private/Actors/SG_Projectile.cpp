// 📄 文件：Source/Sguo/Private/Actors/SG_Projectile.cpp

#include "Actors/SG_Projectile.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Units/SG_UnitsBase.h"
#include "Buildings/SG_MainCityBase.h"
#include "Debug/SG_LogCategories.h"
#include "GameplayEffect.h"
#include "GameplayCueManager.h"
#include "DrawDebugHelpers.h"

/**
 * @brief 构造函数
 * @details 
 * 功能说明：
 * - 创建并配置所有组件
 * - 设置碰撞响应
 * - 绑定碰撞事件
 */
ASG_Projectile::ASG_Projectile()
{
	// 启用 Tick
	PrimaryActorTick.bCanEverTick = true;

	// ========== 创建场景根组件 ==========
	// 作为根组件，允许其他组件自由调整位置和旋转
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	// ========== 创建胶囊体碰撞组件 ==========
	// 不作为根组件，可自由调整方向
	CollisionCapsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CollisionCapsule"));
	// 附加到根组件
	CollisionCapsule->SetupAttachment(RootComponent);
	
	// 设置胶囊体尺寸
	CollisionCapsule->SetCapsuleRadius(CapsuleRadius);
	CollisionCapsule->SetCapsuleHalfHeight(CapsuleHalfHeight);
	// 设置碰撞体旋转偏移
	CollisionCapsule->SetRelativeRotation(CollisionRotationOffset);
	
	// 碰撞设置
	// 仅查询，不进行物理模拟
	CollisionCapsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	// 设置为世界动态对象
	CollisionCapsule->SetCollisionObjectType(ECC_WorldDynamic);
	// 默认忽略所有通道
	CollisionCapsule->SetCollisionResponseToAllChannels(ECR_Ignore);
	// 与 Pawn 重叠
	CollisionCapsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	// 与世界静态物体阻挡（用于检测地面）
	CollisionCapsule->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	// 与世界动态物体重叠
	CollisionCapsule->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	// 确保能检测到 Overlap 事件
	CollisionCapsule->SetGenerateOverlapEvents(true);
	
	// 绑定碰撞事件
	CollisionCapsule->OnComponentBeginOverlap.AddDynamic(this, &ASG_Projectile::OnCapsuleOverlap);
	CollisionCapsule->OnComponentHit.AddDynamic(this, &ASG_Projectile::OnCapsuleHit);

	// ========== 创建网格体组件 ==========
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	// 附加到根组件
	MeshComponent->SetupAttachment(RootComponent);
	// 网格体不参与碰撞
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 启用网络复制
	bReplicates = true;
}

/**
 * @brief BeginPlay
 * @details 
 * 功能说明：
 * - 设置生存时间
 * - 更新碰撞体配置
 * - 激活飞行特效
 */
void ASG_Projectile::BeginPlay()
{
	Super::BeginPlay();

	// 设置生存时间
	SetLifeSpan(LifeSpan);

	// 更新胶囊体尺寸和旋转
	if (CollisionCapsule)
	{
		CollisionCapsule->SetCapsuleRadius(CapsuleRadius);
		CollisionCapsule->SetCapsuleHalfHeight(CapsuleHalfHeight);
		CollisionCapsule->SetRelativeRotation(CollisionRotationOffset);
        
		// ✨ 新增 - 忽略施放者和施放者的友方主城
		AActor* OwnerActor = GetOwner();
		APawn* InstigatorPawn = GetInstigator();
        
		if (OwnerActor)
		{
			CollisionCapsule->IgnoreActorWhenMoving(OwnerActor, true);
		}
        
		if (InstigatorPawn)
		{
			CollisionCapsule->IgnoreActorWhenMoving(InstigatorPawn, true);
            
			// 🔧 关键修复 - 忽略施放者同阵营的主城
			ASG_UnitsBase* InstigatorUnit = Cast<ASG_UnitsBase>(InstigatorPawn);
			if (InstigatorUnit)
			{
				TArray<AActor*> AllMainCities;
				UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASG_MainCityBase::StaticClass(), AllMainCities);
                
				for (AActor* CityActor : AllMainCities)
				{
					ASG_MainCityBase* City = Cast<ASG_MainCityBase>(CityActor);
					if (City && City->FactionTag == InstigatorUnit->FactionTag)
					{
						CollisionCapsule->IgnoreActorWhenMoving(City, true);
						UE_LOG(LogSGGameplay, Verbose, TEXT("投射物忽略友方主城：%s"), *City->GetName());
					}
				}
			}
		}
	}

	// 激活飞行 GC
	ActivateTrailGameplayCue();

	// 输出日志
	UE_LOG(LogSGGameplay, Verbose, TEXT("投射物生成：%s"), *GetName());
	UE_LOG(LogSGGameplay, Verbose, TEXT("  飞行模式：%s"), 
		FlightMode == ESGProjectileFlightMode::Linear ? TEXT("直线") :
		FlightMode == ESGProjectileFlightMode::Parabolic ? TEXT("抛物线") : TEXT("归航"));
	UE_LOG(LogSGGameplay, Verbose, TEXT("  目标模式：%s"),
		TargetMode == ESGProjectileTargetMode::TargetActor ? TEXT("目标Actor") :
		TargetMode == ESGProjectileTargetMode::TargetLocation ? TEXT("指定位置") :
		TargetMode == ESGProjectileTargetMode::AreaCenter ? TEXT("区域中心") :
		TargetMode == ESGProjectileTargetMode::AreaRandom ? TEXT("区域随机点") : TEXT("目标周围随机点"));
	UE_LOG(LogSGGameplay, Verbose, TEXT("  飞行速度：%.1f"), FlightSpeed);
	UE_LOG(LogSGGameplay, Verbose, TEXT("  弧度高度：%.1f"), ArcHeight);
}

/**
 * @brief EndPlay
 * @details 
 * 功能说明：
 * - 移除飞行特效
 * - 执行销毁特效
 * - 广播销毁事件
 */
void ASG_Projectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 移除飞行 GC
	RemoveTrailGameplayCue();
	// 执行销毁 GC
	ExecuteDestroyGameplayCue();
	
	// 调用蓝图事件
	K2_OnProjectileDestroyed(GetActorLocation());
	
	// 构建销毁信息
	FSGProjectileHitInfo DestroyInfo;
	DestroyInfo.HitLocation = GetActorLocation();
	DestroyInfo.ProjectileDirection = CurrentVelocity.GetSafeNormal();
	DestroyInfo.ProjectileSpeed = CurrentVelocity.Size();
	// 广播销毁事件
	OnProjectileDestroyed.Broadcast(DestroyInfo);

	Super::EndPlay(EndPlayReason);
}

/**
 * @brief Tick
 * @param DeltaTime 帧间隔时间
 * @details 
 * 功能说明：
 * - 根据飞行模式更新位置
 * - 更新投射物旋转
 * - 检查目标有效性（抛物线模式）
 * - 绘制调试信息
 */
void ASG_Projectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 未初始化则不处理
	if (!bIsInitialized)
	{
		return;
	}

	// 已落地则不再更新位置
	if (bHasLanded)
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
		// 检查目标是否仍然有效（仅当目标是 Actor 且未丢失时）
		if (TargetMode == ESGProjectileTargetMode::TargetActor && !bTargetLost && !IsTargetValid())
		{
			// 目标丢失，切换到地面落点模式
			HandleTargetLost();
		}
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
		// 绘制速度方向
		DrawDebugLine(GetWorld(), GetActorLocation(), GetActorLocation() + CurrentVelocity.GetSafeNormal() * 100.0f, FColor::Red, false, -1.0f, 0, 2.0f);
		
		// 绘制抛物线轨迹（仅抛物线模式）
		if (FlightMode == ESGProjectileFlightMode::Parabolic)
		{
			for (float t = 0.0f; t < 1.0f; t += 0.05f)
			{
				FVector P1, P2;
				if (bFlyToGround)
				{
					P1 = CalculateParabolicPositionToGround(t);
					P2 = CalculateParabolicPositionToGround(t + 0.05f);
				}
				else
				{
					P1 = CalculateParabolicPosition(t);
					P2 = CalculateParabolicPosition(t + 0.05f);
				}
				DrawDebugLine(GetWorld(), P1, P2, FColor::Green, false, 0.1f, 0, 1.0f);
			}
		}
	}

	if (bDrawDebugGroundImpact)
	{
		// 绘制目标位置
		DrawDebugSphere(GetWorld(), TargetLocation, 20.0f, 8, FColor::Yellow, false, -1.0f, 0, 2.0f);
		// 绘制地面落点
		DrawDebugSphere(GetWorld(), GroundImpactLocation, 30.0f, 12, FColor::Orange, false, -1.0f, 0, 2.0f);
	}

	// ✨ 新增 - 绘制区域范围
	if (bDrawDebugArea && (TargetMode == ESGProjectileTargetMode::AreaCenter || 
		TargetMode == ESGProjectileTargetMode::AreaRandom || 
		TargetMode == ESGProjectileTargetMode::TargetAreaRandom))
	{
		switch (AreaShape)
		{
		case ESGProjectileAreaShape::Circle:
			// 绘制圆形区域
			DrawDebugCircle(GetWorld(), AreaCenterLocation, AreaRadius, 32, FColor::Cyan, false, -1.0f, 0, 2.0f, FVector::ForwardVector, FVector::RightVector, false);
			if (AreaInnerRadius > 0.0f)
			{
				DrawDebugCircle(GetWorld(), AreaCenterLocation, AreaInnerRadius, 32, FColor::Blue, false, -1.0f, 0, 2.0f, FVector::ForwardVector, FVector::RightVector, false);
			}
			break;

		case ESGProjectileAreaShape::Rectangle:
			{
				// 绘制矩形区域
				FVector Forward = AreaRotation.RotateVector(FVector::ForwardVector);
				FVector Right = AreaRotation.RotateVector(FVector::RightVector);
				FVector HalfExtent = FVector(AreaSize.X * 0.5f, AreaSize.Y * 0.5f, 0.0f);
				
				FVector Corners[4];
				Corners[0] = AreaCenterLocation + Forward * HalfExtent.X + Right * HalfExtent.Y;
				Corners[1] = AreaCenterLocation + Forward * HalfExtent.X - Right * HalfExtent.Y;
				Corners[2] = AreaCenterLocation - Forward * HalfExtent.X - Right * HalfExtent.Y;
				Corners[3] = AreaCenterLocation - Forward * HalfExtent.X + Right * HalfExtent.Y;
				
				for (int32 i = 0; i < 4; ++i)
				{
					DrawDebugLine(GetWorld(), Corners[i], Corners[(i + 1) % 4], FColor::Cyan, false, -1.0f, 0, 2.0f);
				}
			}
			break;

		case ESGProjectileAreaShape::Sector:
			{
				// 绘制扇形区域
				FVector Forward = AreaRotation.RotateVector(FVector::ForwardVector);
				float HalfAngle = FMath::DegreesToRadians(SectorAngle * 0.5f);
				
				// 绘制两条边
				FVector LeftEdge = Forward.RotateAngleAxis(-SectorAngle * 0.5f, FVector::UpVector) * AreaRadius;
				FVector RightEdge = Forward.RotateAngleAxis(SectorAngle * 0.5f, FVector::UpVector) * AreaRadius;
				
				DrawDebugLine(GetWorld(), AreaCenterLocation, AreaCenterLocation + LeftEdge, FColor::Cyan, false, -1.0f, 0, 2.0f);
				DrawDebugLine(GetWorld(), AreaCenterLocation, AreaCenterLocation + RightEdge, FColor::Cyan, false, -1.0f, 0, 2.0f);
				
				// 绘制弧线
				int32 NumSegments = FMath::Max(8, FMath::CeilToInt(SectorAngle / 10.0f));
				float AngleStep = SectorAngle / NumSegments;
				for (int32 i = 0; i < NumSegments; ++i)
				{
					float Angle1 = -SectorAngle * 0.5f + AngleStep * i;
					float Angle2 = -SectorAngle * 0.5f + AngleStep * (i + 1);
					FVector P1 = AreaCenterLocation + Forward.RotateAngleAxis(Angle1, FVector::UpVector) * AreaRadius;
					FVector P2 = AreaCenterLocation + Forward.RotateAngleAxis(Angle2, FVector::UpVector) * AreaRadius;
					DrawDebugLine(GetWorld(), P1, P2, FColor::Cyan, false, -1.0f, 0, 2.0f);
				}
			}
			break;
		}
	}
#endif
}

/**
 * @brief 初始化投射物（目标为 Actor）
 * @param InInstigatorASC 攻击者 ASC
 * @param InFactionTag 攻击者阵营
 * @param InTarget 目标 Actor
 * @param InArcHeight 弧度高度（覆盖默认值，-1 表示使用默认）
 * @details 
 * 功能说明：
 * - 根据 TargetMode 决定目标位置
 * - TargetActor: 飞向目标中心
 * - TargetAreaRandom: 飞向目标周围随机点
 * 
 * 详细流程：
 * 1. 保存攻击者信息
 * 2. 记录起始位置
 * 3. 根据目标模式计算目标位置
 * 4. 计算地面落点
 * 5. 初始化飞行参数
 */
void ASG_Projectile::InitializeProjectile(
	UAbilitySystemComponent* InInstigatorASC,
	FGameplayTag InFactionTag,
	AActor* InTarget,
	float InArcHeight
)
{
	// 保存攻击者 ASC
	InstigatorASC = InInstigatorASC;
	// 保存攻击者阵营
	InstigatorFactionTag = InFactionTag;
	// 保存目标 Actor
	CurrentTarget = InTarget;

	// 重置状态标记
	bTargetLost = false;
	bHasLanded = false;
	bFlyToGround = false;

	// 记录起始位置
	StartLocation = GetActorLocation();

	// 应用弧度高度覆盖
	if (InArcHeight >= 0.0f)
	{
		ArcHeight = InArcHeight;
	}

	// ✨ 新增 - 根据目标模式计算目标位置
	if (InTarget)
	{
		switch (TargetMode)
		{
		case ESGProjectileTargetMode::TargetActor:
			// 飞向目标中心
			TargetLocation = CalculateTargetLocation(InTarget);
			AreaCenterLocation = InTarget->GetActorLocation();
			AreaRotation = InTarget->GetActorRotation();
			bFlyToGround = false;
			break;

		case ESGProjectileTargetMode::TargetAreaRandom:
			// 飞向目标周围随机点
			AreaCenterLocation = InTarget->GetActorLocation();
			AreaRotation = InTarget->GetActorRotation();
			// 生成随机点
			TargetLocation = GenerateRandomPointInArea(AreaCenterLocation, AreaRotation);
			bFlyToGround = true;
			break;

		default:
			// 其他模式使用目标位置
			TargetLocation = CalculateTargetLocation(InTarget);
			AreaCenterLocation = InTarget->GetActorLocation();
			AreaRotation = InTarget->GetActorRotation();
			bFlyToGround = false;
			break;
		}
	}
	else
	{
		// 如果没有目标，向前飞行
		TargetLocation = StartLocation + GetActorForwardVector() * 5000.0f;
		AreaCenterLocation = TargetLocation;
		AreaRotation = GetActorRotation();
	}

	// 计算地面落点
	GroundImpactLocation = CalculateGroundImpactLocation(TargetLocation);

	// 计算飞行距离
	if (bFlyToGround)
	{
		// 飞向地面模式
		TotalFlightDistance = FVector::Dist(StartLocation, GroundImpactLocation);
		TotalFlightDistanceToGround = TotalFlightDistance;
	}
	else
	{
		// 飞向目标模式
		TotalFlightDistance = FVector::Dist(StartLocation, TargetLocation);
		TotalFlightDistanceToGround = FVector::Dist(StartLocation, GroundImpactLocation);
	}

	// 初始化速度向量
	FVector FinalTarget = bFlyToGround ? GroundImpactLocation : TargetLocation;
	FVector Direction = (FinalTarget - StartLocation).GetSafeNormal();
	CurrentVelocity = Direction * FlightSpeed;

	// 重置飞行进度
	FlightProgress = 0.0f;

	// 标记为已初始化
	bIsInitialized = true;

	// 输出日志
	UE_LOG(LogSGGameplay, Log, TEXT("========== 初始化投射物（Actor目标） =========="));
	UE_LOG(LogSGGameplay, Log, TEXT("  目标：%s"), InTarget ? *InTarget->GetName() : TEXT("无"));
	UE_LOG(LogSGGameplay, Log, TEXT("  目标模式：%s"),
		TargetMode == ESGProjectileTargetMode::TargetActor ? TEXT("目标Actor") :
		TargetMode == ESGProjectileTargetMode::TargetAreaRandom ? TEXT("目标周围随机点") : TEXT("其他"));
	UE_LOG(LogSGGameplay, Log, TEXT("  起点：%s"), *StartLocation.ToString());
	UE_LOG(LogSGGameplay, Log, TEXT("  目标位置：%s"), *TargetLocation.ToString());
	UE_LOG(LogSGGameplay, Log, TEXT("  地面落点：%s"), *GroundImpactLocation.ToString());
	UE_LOG(LogSGGameplay, Log, TEXT("  飞向地面：%s"), bFlyToGround ? TEXT("是") : TEXT("否"));
	UE_LOG(LogSGGameplay, Log, TEXT("  距离：%.1f"), TotalFlightDistance);
	UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
}

/**
 * @brief 初始化投射物（目标为位置）
 * @param InInstigatorASC 攻击者 ASC
 * @param InFactionTag 攻击者阵营
 * @param InTargetLocation 目标位置
 * @param InArcHeight 弧度高度（覆盖默认值，-1 表示使用默认）
 * @details 
 * 功能说明：
 * - 根据 TargetMode 决定目标位置
 * - TargetLocation: 飞向指定位置
 * - AreaCenter: 飞向区域中心地面
 * - AreaRandom: 飞向区域内随机地面点
 */
void ASG_Projectile::InitializeProjectileToLocation(
	UAbilitySystemComponent* InInstigatorASC,
	FGameplayTag InFactionTag,
	FVector InTargetLocation,
	float InArcHeight
)
{
	// 保存攻击者 ASC
	InstigatorASC = InInstigatorASC;
	// 保存攻击者阵营
	InstigatorFactionTag = InFactionTag;
	// 清空目标 Actor
	CurrentTarget = nullptr;

	// 重置状态标记
	bTargetLost = false;
	bHasLanded = false;

	// 记录起始位置
	StartLocation = GetActorLocation();

	// 应用弧度高度覆盖
	if (InArcHeight >= 0.0f)
	{
		ArcHeight = InArcHeight;
	}

	// 设置区域中心和朝向
	AreaCenterLocation = InTargetLocation;
	AreaRotation = GetActorRotation();

	// ✨ 新增 - 根据目标模式计算目标位置
	switch (TargetMode)
	{
	case ESGProjectileTargetMode::TargetLocation:
		// 飞向指定位置
		TargetLocation = InTargetLocation + (bUseWorldSpaceOffset ? TargetLocationOffset : GetActorRotation().RotateVector(TargetLocationOffset));
		bFlyToGround = false;
		break;

	case ESGProjectileTargetMode::AreaCenter:
		// 飞向区域中心地面
		TargetLocation = InTargetLocation;
		bFlyToGround = true;
		break;

	case ESGProjectileTargetMode::AreaRandom:
		// 飞向区域内随机地面点
		TargetLocation = GenerateRandomPointInArea(InTargetLocation, AreaRotation);
		bFlyToGround = true;
		break;

	default:
		// 默认飞向指定位置
		TargetLocation = InTargetLocation;
		bFlyToGround = false;
		break;
	}

	// 计算地面落点
	GroundImpactLocation = CalculateGroundImpactLocation(TargetLocation);

	// 计算飞行距离
	if (bFlyToGround)
	{
		TotalFlightDistance = FVector::Dist(StartLocation, GroundImpactLocation);
		TotalFlightDistanceToGround = TotalFlightDistance;
	}
	else
	{
		TotalFlightDistance = FVector::Dist(StartLocation, TargetLocation);
		TotalFlightDistanceToGround = FVector::Dist(StartLocation, GroundImpactLocation);
	}

	// 初始化速度向量
	FVector FinalTarget = bFlyToGround ? GroundImpactLocation : TargetLocation;
	FVector Direction = (FinalTarget - StartLocation).GetSafeNormal();
	CurrentVelocity = Direction * FlightSpeed;

	// 重置飞行进度
	FlightProgress = 0.0f;

	// 标记为已初始化
	bIsInitialized = true;

	// 输出日志
	UE_LOG(LogSGGameplay, Log, TEXT("========== 初始化投射物（位置目标） =========="));
	UE_LOG(LogSGGameplay, Log, TEXT("  目标模式：%s"),
		TargetMode == ESGProjectileTargetMode::TargetLocation ? TEXT("指定位置") :
		TargetMode == ESGProjectileTargetMode::AreaCenter ? TEXT("区域中心") :
		TargetMode == ESGProjectileTargetMode::AreaRandom ? TEXT("区域随机点") : TEXT("其他"));
	UE_LOG(LogSGGameplay, Log, TEXT("  起点：%s"), *StartLocation.ToString());
	UE_LOG(LogSGGameplay, Log, TEXT("  目标位置：%s"), *TargetLocation.ToString());
	UE_LOG(LogSGGameplay, Log, TEXT("  地面落点：%s"), *GroundImpactLocation.ToString());
	UE_LOG(LogSGGameplay, Log, TEXT("  飞向地面：%s"), bFlyToGround ? TEXT("是") : TEXT("否"));
	UE_LOG(LogSGGameplay, Log, TEXT("  距离：%.1f"), TotalFlightDistance);
	UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
}

// ✨ 新增 - 初始化投射物（目标为区域）
/**
 * @brief 初始化投射物（目标为区域）
 * @param InInstigatorASC 攻击者 ASC
 * @param InFactionTag 攻击者阵营
 * @param InAreaCenter 区域中心位置
 * @param InAreaRotation 区域朝向
 * @param InArcHeight 弧度高度（覆盖默认值，-1 表示使用默认）
 * @details 
 * 功能说明：
 * - 用于区域攻击
 * - 根据 TargetMode 决定飞向区域中心还是随机点
 * - 区域朝向用于扇形和矩形区域
 */
void ASG_Projectile::InitializeProjectileToArea(
	UAbilitySystemComponent* InInstigatorASC,
	FGameplayTag InFactionTag,
	FVector InAreaCenter,
	FRotator InAreaRotation,
	float InArcHeight
)
{
	// 保存攻击者 ASC
	InstigatorASC = InInstigatorASC;
	// 保存攻击者阵营
	InstigatorFactionTag = InFactionTag;
	// 清空目标 Actor
	CurrentTarget = nullptr;

	// 重置状态标记
	bTargetLost = false;
	bHasLanded = false;
	// 区域模式始终飞向地面
	bFlyToGround = true;

	// 记录起始位置
	StartLocation = GetActorLocation();

	// 应用弧度高度覆盖
	if (InArcHeight >= 0.0f)
	{
		ArcHeight = InArcHeight;
	}

	// 设置区域中心和朝向
	AreaCenterLocation = InAreaCenter;
	AreaRotation = InAreaRotation;

	// 根据目标模式计算目标位置
	switch (TargetMode)
	{
	case ESGProjectileTargetMode::AreaCenter:
		// 飞向区域中心
		TargetLocation = InAreaCenter;
		break;

	case ESGProjectileTargetMode::AreaRandom:
	case ESGProjectileTargetMode::TargetAreaRandom:
		// 飞向区域内随机点
		TargetLocation = GenerateRandomPointInArea(InAreaCenter, InAreaRotation);
		break;

	default:
		// 默认飞向区域中心
		TargetLocation = InAreaCenter;
		break;
	}

	// 计算地面落点
	GroundImpactLocation = CalculateGroundImpactLocation(TargetLocation);

	// 区域模式使用地面落点计算距离
	TotalFlightDistance = FVector::Dist(StartLocation, GroundImpactLocation);
	TotalFlightDistanceToGround = TotalFlightDistance;

	// 初始化速度向量
	FVector Direction = (GroundImpactLocation - StartLocation).GetSafeNormal();
	CurrentVelocity = Direction * FlightSpeed;

	// 重置飞行进度
	FlightProgress = 0.0f;

	// 标记为已初始化
	bIsInitialized = true;

	// 输出日志
	UE_LOG(LogSGGameplay, Log, TEXT("========== 初始化投射物（区域目标） =========="));
	UE_LOG(LogSGGameplay, Log, TEXT("  区域形状：%s"),
		AreaShape == ESGProjectileAreaShape::Circle ? TEXT("圆形") :
		AreaShape == ESGProjectileAreaShape::Rectangle ? TEXT("矩形") : TEXT("扇形"));
	UE_LOG(LogSGGameplay, Log, TEXT("  区域中心：%s"), *InAreaCenter.ToString());
	UE_LOG(LogSGGameplay, Log, TEXT("  区域朝向：%s"), *InAreaRotation.ToString());
	UE_LOG(LogSGGameplay, Log, TEXT("  目标位置：%s"), *TargetLocation.ToString());
	UE_LOG(LogSGGameplay, Log, TEXT("  地面落点：%s"), *GroundImpactLocation.ToString());
	UE_LOG(LogSGGameplay, Log, TEXT("  距离：%.1f"), TotalFlightDistance);
	UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
}

/**
 * @brief 设置飞行速度（运行时）
 * @param NewSpeed 新的飞行速度
 */
void ASG_Projectile::SetFlightSpeed(float NewSpeed)
{
	// 限制最小速度为 100
	FlightSpeed = FMath::Max(100.0f, NewSpeed);
	
	// 更新当前速度向量的大小
	if (!CurrentVelocity.IsNearlyZero())
	{
		CurrentVelocity = CurrentVelocity.GetSafeNormal() * FlightSpeed;
	}
}

/**
 * @brief 设置目标位置偏移（运行时）
 * @param NewOffset 新的偏移向量
 * @param bWorldSpace 是否使用世界空间
 */
void ASG_Projectile::SetTargetLocationOffset(FVector NewOffset, bool bWorldSpace)
{
	TargetLocationOffset = NewOffset;
	bUseWorldSpaceOffset = bWorldSpace;
}

// ✨ 新增 - 设置区域参数
/**
 * @brief 设置区域参数（运行时）
 * @param InShape 区域形状
 * @param InRadius 区域半径（圆形/扇形）
 * @param InInnerRadius 区域内半径
 * @param InSize 区域尺寸（矩形）
 * @param InSectorAngle 扇形角度
 */
void ASG_Projectile::SetAreaParameters(
	ESGProjectileAreaShape InShape,
	float InRadius,
	float InInnerRadius,
	FVector2D InSize,
	float InSectorAngle
)
{
	AreaShape = InShape;
	AreaRadius = InRadius;
	AreaInnerRadius = InInnerRadius;
	AreaSize = InSize;
	SectorAngle = InSectorAngle;
}

/**
 * @brief 更新直线飞行
 * @param DeltaTime 帧间隔时间
 * @details 
 * 功能说明：
 * - 直接飞向目标位置
 * - 持续追踪目标（如果有）
 */
void ASG_Projectile::UpdateLinearFlight(float DeltaTime)
{
	// 如果有目标 Actor，动态更新目标位置
	if (CurrentTarget.IsValid() && TargetMode == ESGProjectileTargetMode::TargetActor)
	{
		TargetLocation = CalculateTargetLocation(CurrentTarget.Get());
	}

	// 计算移动距离
	float MoveDistance = FlightSpeed * DeltaTime;
	
	// 计算方向（持续指向目标）
	FVector CurrentLocation = GetActorLocation();
	FVector FinalTarget = bFlyToGround ? GroundImpactLocation : TargetLocation;
	FVector ToTarget = FinalTarget - CurrentLocation;
	
	if (ToTarget.Size() <= MoveDistance)
	{
		// 已到达目标位置
		SetActorLocation(FinalTarget);
		CurrentVelocity = ToTarget.GetSafeNormal() * FlightSpeed;
		
		// 如果飞向地面，触发落地
		if (bFlyToGround)
		{
			HandleGroundImpact();
		}
	}
	else
	{
		// 继续飞行
		FVector Direction = ToTarget.GetSafeNormal();
		CurrentVelocity = Direction * FlightSpeed;
		SetActorLocation(CurrentLocation + CurrentVelocity * DeltaTime);
	}
}

/**
 * @brief 更新抛物线飞行
 * @param DeltaTime 帧间隔时间
 * @details 
 * 功能说明：
 * - 沿抛物线轨迹飞行
 * - 根据 bFlyToGround 决定飞向目标中心还是地面落点
 */
void ASG_Projectile::UpdateParabolicFlight(float DeltaTime)
{
	// 选择使用的飞行距离
	float EffectiveFlightDistance = bFlyToGround ? TotalFlightDistanceToGround : TotalFlightDistance;

	// 防止除零
	if (EffectiveFlightDistance < KINDA_SMALL_NUMBER)
	{
		HandleGroundImpact();
		return;
	}

	// 更新飞行进度
	float DistanceThisFrame = FlightSpeed * DeltaTime;
	FlightProgress += DistanceThisFrame / EffectiveFlightDistance;
	FlightProgress = FMath::Clamp(FlightProgress, 0.0f, 1.0f);

	// 计算当前位置
	FVector NewLocation;
	if (bFlyToGround)
	{
		// 飞向地面落点
		NewLocation = CalculateParabolicPositionToGround(FlightProgress);
	}
	else
	{
		// 飞向目标中心
		NewLocation = CalculateParabolicPosition(FlightProgress);
	}
	
	// 计算速度向量（用于旋转）
	FVector PreviousLocation = GetActorLocation();
	if (DeltaTime > KINDA_SMALL_NUMBER)
	{
		CurrentVelocity = (NewLocation - PreviousLocation) / DeltaTime;
	}
	
	// 如果速度过小，使用方向估算
	if (CurrentVelocity.Size() < 1.0f)
	{
		float NextProgress = FMath::Clamp(FlightProgress + 0.01f, 0.0f, 1.0f);
		FVector NextLocation;
		if (bFlyToGround)
		{
			NextLocation = CalculateParabolicPositionToGround(NextProgress);
		}
		else
		{
			NextLocation = CalculateParabolicPosition(NextProgress);
		}
		CurrentVelocity = (NextLocation - NewLocation).GetSafeNormal() * FlightSpeed;
	}

	// 更新位置
	SetActorLocation(NewLocation);

	// 检查是否到达终点
	if (FlightProgress >= 1.0f)
	{
		if (bFlyToGround)
		{
			// 飞向地面模式，触发落地
			HandleGroundImpact();
		}
	}

	// 如果目标还活着且未丢失，动态更新目标位置
	if (!bFlyToGround && !bTargetLost && CurrentTarget.IsValid())
	{
		AActor* Target = CurrentTarget.Get();
		
		// 计算新的目标位置
		FVector NewTargetLocation = CalculateTargetLocation(Target);
		
		// 平滑更新目标位置（避免抖动）
		TargetLocation = FMath::VInterpTo(TargetLocation, NewTargetLocation, DeltaTime, 5.0f);
		
		// 更新总飞行距离
		TotalFlightDistance = FVector::Dist(StartLocation, TargetLocation);
		
		// 同时更新地面落点
		GroundImpactLocation = CalculateGroundImpactLocation(TargetLocation);
		TotalFlightDistanceToGround = FVector::Dist(StartLocation, GroundImpactLocation);
	}
}

/**
 * @brief 计算抛物线位置（飞向目标中心）
 * @param Progress 飞行进度（0-1）
 * @return 当前位置
 */
FVector ASG_Projectile::CalculateParabolicPosition(float Progress) const
{
	// 线性插值基础位置
	FVector LinearPosition = FMath::Lerp(StartLocation, TargetLocation, Progress);

	// 计算抛物线高度偏移
	float HeightOffset = FMath::Sin(Progress * PI) * ArcHeight;

	// 应用高度偏移
	return LinearPosition + FVector(0.0f, 0.0f, HeightOffset);
}

/**
 * @brief 计算到地面落点的抛物线位置
 * @param Progress 飞行进度（0-1）
 * @return 当前位置
 */
FVector ASG_Projectile::CalculateParabolicPositionToGround(float Progress) const
{
	// 线性插值基础位置（使用地面落点）
	FVector LinearPosition = FMath::Lerp(StartLocation, GroundImpactLocation, Progress);

	// 计算抛物线高度偏移
	float HeightOffset = FMath::Sin(Progress * PI) * ArcHeight;

	// 应用高度偏移
	return LinearPosition + FVector(0.0f, 0.0f, HeightOffset);
}

/**
 * @brief 更新归航飞行
 * @param DeltaTime 帧间隔时间
 */
void ASG_Projectile::UpdateHomingFlight(float DeltaTime)
{
	// 更新目标位置
	if (CurrentTarget.IsValid() && TargetMode == ESGProjectileTargetMode::TargetActor)
	{
		TargetLocation = CalculateTargetLocation(CurrentTarget.Get());
	}

	// 计算当前方向和目标方向
	FVector CurrentDirection = CurrentVelocity.GetSafeNormal();
	FVector FinalTarget = bFlyToGround ? GroundImpactLocation : TargetLocation;
	FVector DesiredDirection = (FinalTarget - GetActorLocation()).GetSafeNormal();

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

/**
 * @brief 更新旋转（朝向速度方向）
 */
void ASG_Projectile::UpdateRotation()
{
	if (!CurrentVelocity.IsNearlyZero())
	{
		FRotator NewRotation = CurrentVelocity.Rotation();
		SetActorRotation(NewRotation);
	}
}

/**
 * @brief 计算目标位置（应用偏移）
 * @param InTarget 目标 Actor
 * @return 计算后的目标位置
 */
FVector ASG_Projectile::CalculateTargetLocation(AActor* InTarget) const
{
	if (!InTarget)
	{
		return GetActorLocation() + GetActorForwardVector() * 5000.0f;
	}

	// 获取目标基础位置
	FVector BaseLocation = InTarget->GetActorLocation();

	// 瞄准目标中心
	if (ASG_UnitsBase* TargetUnit = Cast<ASG_UnitsBase>(InTarget))
	{
		if (UCapsuleComponent* Capsule = TargetUnit->GetCapsuleComponent())
		{
			BaseLocation.Z += Capsule->GetScaledCapsuleHalfHeight() * 0.5f;
		}
	}
	else if (ASG_MainCityBase* MainCity = Cast<ASG_MainCityBase>(InTarget))
	{
		if (UBoxComponent* DetectionBox = MainCity->GetAttackDetectionBox())
		{
			BaseLocation = DetectionBox->GetComponentLocation();
		}
	}

	// 应用位置偏移
	FVector FinalLocation = BaseLocation;
	if (!TargetLocationOffset.IsNearlyZero())
	{
		if (bUseWorldSpaceOffset)
		{
			FinalLocation += TargetLocationOffset;
		}
		else
		{
			FRotator TargetRotation = InTarget->GetActorRotation();
			FinalLocation += TargetRotation.RotateVector(TargetLocationOffset);
		}
	}

	return FinalLocation;
}

/**
 * @brief 计算地面落点位置
 * @param InTargetLocation 目标位置
 * @return 地面落点位置
 */
FVector ASG_Projectile::CalculateGroundImpactLocation(const FVector& InTargetLocation) const
{
	// 射线检测起点（目标位置上方）
	FVector TraceStart = InTargetLocation + FVector(0.0f, 0.0f, 100.0f);
	// 射线检测终点（向下检测）
	FVector TraceEnd = InTargetLocation - FVector(0.0f, 0.0f, GroundTraceDistance);

	// 设置查询参数
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	if (CurrentTarget.IsValid())
	{
		QueryParams.AddIgnoredActor(CurrentTarget.Get());
	}

	// 执行射线检测
	FHitResult HitResult;
	bool bHit = GetWorld()->LineTraceSingleByChannel(
		HitResult,
		TraceStart,
		TraceEnd,
		GroundTraceChannel,
		QueryParams
	);

	if (bHit)
	{
		return HitResult.ImpactPoint;
	}
	else
	{
		// 未检测到地面，使用目标位置的 XY 坐标，Z 轴使用起点 Z 坐标
		return FVector(InTargetLocation.X, InTargetLocation.Y, StartLocation.Z);
	}
}

/**
 * @brief 检查目标是否仍然有效
 * @return 目标是否有效
 */
bool ASG_Projectile::IsTargetValid() const
{
	if (!CurrentTarget.IsValid())
	{
		return false;
	}

	AActor* Target = CurrentTarget.Get();

	if (ASG_UnitsBase* TargetUnit = Cast<ASG_UnitsBase>(Target))
	{
		return !TargetUnit->bIsDead;
	}

	if (ASG_MainCityBase* TargetMainCity = Cast<ASG_MainCityBase>(Target))
	{
		return TargetMainCity->IsAlive();
	}

	return true;
}

/**
 * @brief 处理目标丢失（切换到地面落点模式）
 */
void ASG_Projectile::HandleTargetLost()
{
	// 标记目标丢失
	bTargetLost = true;
	// 切换到飞向地面模式
	bFlyToGround = true;

	// 重新计算地面落点
	GroundImpactLocation = CalculateGroundImpactLocation(TargetLocation);
	TotalFlightDistanceToGround = FVector::Dist(StartLocation, GroundImpactLocation);

	UE_LOG(LogSGGameplay, Log, TEXT("投射物目标丢失，切换到地面落点模式"));
	UE_LOG(LogSGGameplay, Log, TEXT("  当前位置：%s"), *GetActorLocation().ToString());
	UE_LOG(LogSGGameplay, Log, TEXT("  地面落点：%s"), *GroundImpactLocation.ToString());
}

/**
 * @brief 处理投射物落地
 */
void ASG_Projectile::HandleGroundImpact()
{
	// 防止重复处理
	if (bHasLanded)
	{
		return;
	}

	bHasLanded = true;

	UE_LOG(LogSGGameplay, Log, TEXT("投射物落地：%s"), *GroundImpactLocation.ToString());

	// 执行落地 GameplayCue
	ExecuteGroundImpactGameplayCue(GroundImpactLocation);

	// 构建落地信息
	FSGProjectileHitInfo GroundHitInfo;
	GroundHitInfo.HitLocation = GroundImpactLocation;
	GroundHitInfo.HitNormal = FVector::UpVector;
	GroundHitInfo.ProjectileDirection = CurrentVelocity.GetSafeNormal();
	GroundHitInfo.ProjectileSpeed = CurrentVelocity.Size();

	// 调用蓝图事件
	K2_OnGroundImpact(GroundImpactLocation);

	// 广播落地事件
	OnProjectileGroundImpact.Broadcast(GroundHitInfo);

	// 销毁投射物
	Destroy();
}

// ✨ 新增 - 区域随机点生成函数
// ========== 区域随机点计算 ==========

/**
 * @brief 在区域内生成随机点
 * @param InCenter 区域中心
 * @param InRotation 区域朝向
 * @return 随机点位置（世界坐标）
 */
FVector ASG_Projectile::GenerateRandomPointInArea(const FVector& InCenter, const FRotator& InRotation) const
{
	switch (AreaShape)
	{
	case ESGProjectileAreaShape::Circle:
		return GenerateRandomPointInCircle(InCenter);

	case ESGProjectileAreaShape::Rectangle:
		return GenerateRandomPointInRectangle(InCenter, InRotation);

	case ESGProjectileAreaShape::Sector:
		return GenerateRandomPointInSector(InCenter, InRotation);

	default:
		return InCenter;
	}
}

/**
 * @brief 在圆形区域内生成随机点
 * @param InCenter 区域中心
 * @return 随机点位置
 * @details 
 * 功能说明：
 * - 支持内半径（生成环形区域）
 * - 使用均匀分布确保点分布均匀
 */
FVector ASG_Projectile::GenerateRandomPointInCircle(const FVector& InCenter) const
{
	// 计算有效半径范围
	float MinRadius = AreaInnerRadius;
	float MaxRadius = AreaRadius;

	// 确保 MinRadius < MaxRadius
	if (MinRadius >= MaxRadius)
	{
		MinRadius = 0.0f;
	}

	// 使用均匀分布生成随机半径
	// 为了确保点在圆内均匀分布，需要对半径进行平方根变换
	float RandomValue = FMath::FRand();
	float MinRadiusSq = MinRadius * MinRadius;
	float MaxRadiusSq = MaxRadius * MaxRadius;
	float RandomRadiusSq = FMath::Lerp(MinRadiusSq, MaxRadiusSq, RandomValue);
	float RandomRadius = FMath::Sqrt(RandomRadiusSq);

	// 生成随机角度
	float RandomAngle = FMath::FRandRange(0.0f, 360.0f);

	// 计算偏移
	FVector Offset;
	Offset.X = RandomRadius * FMath::Cos(FMath::DegreesToRadians(RandomAngle));
	Offset.Y = RandomRadius * FMath::Sin(FMath::DegreesToRadians(RandomAngle));
	Offset.Z = 0.0f;

	return InCenter + Offset;
}

/**
 * @brief 在矩形区域内生成随机点
 * @param InCenter 区域中心
 * @param InRotation 区域朝向
 * @return 随机点位置
 */
FVector ASG_Projectile::GenerateRandomPointInRectangle(const FVector& InCenter, const FRotator& InRotation) const
{
	// 生成局部坐标的随机点
	float RandomX = FMath::FRandRange(-AreaSize.X * 0.5f, AreaSize.X * 0.5f);
	float RandomY = FMath::FRandRange(-AreaSize.Y * 0.5f, AreaSize.Y * 0.5f);

	// 创建局部偏移
	FVector LocalOffset(RandomX, RandomY, 0.0f);

	// 旋转到世界坐标
	FVector WorldOffset = InRotation.RotateVector(LocalOffset);

	return InCenter + WorldOffset;
}

/**
 * @brief 在扇形区域内生成随机点
 * @param InCenter 区域中心
 * @param InRotation 区域朝向
 * @return 随机点位置
 * @details 
 * 功能说明：
 * - 支持内半径（生成扇形环区域）
 * - 支持扇形朝向偏移
 */
FVector ASG_Projectile::GenerateRandomPointInSector(const FVector& InCenter, const FRotator& InRotation) const
{
	// 计算有效半径范围
	float MinRadius = AreaInnerRadius;
	float MaxRadius = AreaRadius;

	if (MinRadius >= MaxRadius)
	{
		MinRadius = 0.0f;
	}

	// 使用均匀分布生成随机半径
	float RandomValue = FMath::FRand();
	float MinRadiusSq = MinRadius * MinRadius;
	float MaxRadiusSq = MaxRadius * MaxRadius;
	float RandomRadiusSq = FMath::Lerp(MinRadiusSq, MaxRadiusSq, RandomValue);
	float RandomRadius = FMath::Sqrt(RandomRadiusSq);

	// 生成扇形范围内的随机角度
	float HalfAngle = SectorAngle * 0.5f;
	float RandomAngle = FMath::FRandRange(-HalfAngle, HalfAngle);

	// 应用扇形朝向偏移
	RandomAngle += SectorDirectionOffset;

	// 获取区域朝向的前向量
	FVector Forward = InRotation.RotateVector(FVector::ForwardVector);

	// 计算最终方向
	FVector Direction = Forward.RotateAngleAxis(RandomAngle, FVector::UpVector);

	// 计算偏移
	FVector Offset = Direction * RandomRadius;
	Offset.Z = 0.0f;

	return InCenter + Offset;
}

/**
 * @brief 胶囊体 Overlap 事件
 */
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

/**
 * @brief 胶囊体 Hit 事件
 */
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

/**
 * @brief 碰撞处理
 * @param OtherActor 其他 Actor
 * @param Hit 击中结果
 */
void ASG_Projectile::HandleProjectileImpact(AActor* OtherActor, const FHitResult& Hit)
{
	UE_LOG(LogSGGameplay, Verbose, TEXT("投射物碰撞：%s"), OtherActor ? *OtherActor->GetName() : TEXT("None"));

	// 忽略无效碰撞
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
		// 撞墙或地面
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

/**
 * @brief 应用伤害
 * @param Target 目标 Actor
 */
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

/**
 * @brief 执行击中 GameplayCue
 * @param HitInfo 击中信息
 */
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

/**
 * @brief 激活飞行 GameplayCue
 */
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

/**
 * @brief 移除飞行 GameplayCue
 */
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

/**
 * @brief 执行销毁 GameplayCue
 */
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

/**
 * @brief 执行落地 GameplayCue
 * @param ImpactLocation 落地位置
 */
void ASG_Projectile::ExecuteGroundImpactGameplayCue(const FVector& ImpactLocation)
{
	if (!GroundImpactGameplayCueTag.IsValid())
	{
		return;
	}

	FGameplayCueParameters CueParams;
	CueParams.Location = ImpactLocation;
	CueParams.Normal = FVector::UpVector;
	CueParams.Instigator = GetInstigator();
	CueParams.EffectCauser = this;
	CueParams.SourceObject = this;

	if (InstigatorASC)
	{
		InstigatorASC->ExecuteGameplayCue(GroundImpactGameplayCueTag, CueParams);
	}
	else
	{
		if (UGameplayCueManager* CueManager = UAbilitySystemGlobals::Get().GetGameplayCueManager())
		{
			CueManager->HandleGameplayCue(nullptr, GroundImpactGameplayCueTag, EGameplayCueEvent::Executed, CueParams);
		}
	}
}
