// 🔧 MODIFIED FILE - 放置预览 Actor 实现
// Copyright notice placeholder
/**
 * @file SG_PlacementPreview.cpp
 * @brief 卡牌放置预览 Actor 实现
 */
#include "Actors/SG_PlacementPreview.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DecalComponent.h"
#include "Data/SG_CardDataBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Debug/SG_LogCategories.h"
#include "Actors/SG_FrontLineManager.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"  
#include "GameFramework/Character.h"

// 构造函数
ASG_PlacementPreview::ASG_PlacementPreview()
{
	// 启用 Tick
	PrimaryActorTick.bCanEverTick = true;

	// 创建根组件
	RootComp = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	RootComponent = RootComp;

	// 创建预览网格体
	PreviewMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PreviewMesh"));
	PreviewMesh->SetupAttachment(RootComp);
	// 禁用碰撞
	PreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// 设置半透明渲染
	PreviewMesh->SetRenderCustomDepth(false);
	// 默认隐藏
	PreviewMesh->SetVisibility(false);

	// 创建区域指示器
	AreaIndicator = CreateDefaultSubobject<UDecalComponent>(TEXT("AreaIndicator"));
	AreaIndicator->SetupAttachment(RootComp);
	// 默认隐藏
	AreaIndicator->SetVisibility(false);
	// 设置默认大小
	AreaIndicator->DecalSize = FVector(100.0f, 100.0f, 100.0f);

	// 初始化运行时数据
	PreviewLocation = FVector::ZeroVector;
	PreviewRotation = FRotator::ZeroRotator;
	bCanPlace = false;
	// ✨ NEW - 初始化对象类型数组（默认只检测 Pawn）
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
}

// 生命周期开始
void ASG_PlacementPreview::BeginPlay()
{
	Super::BeginPlay();
	// 🔧 MODIFIED - 查找并缓存前线管理器
	CachedFrontLineManager = ASG_FrontLineManager::GetFrontLineManager(this);
    
	if (CachedFrontLineManager)
	{
		// 输出日志
		UE_LOG(LogSGGameplay, Log, TEXT("✓ 找到前线管理器"));
	}
	else
	{
		// 输出警告
		UE_LOG(LogSGGameplay, Warning, TEXT("⚠️ 未找到前线管理器，前线检测将被禁用"));
	}
}

// 每帧更新
void ASG_PlacementPreview::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 更新预览位置
	UpdatePreviewLocation();

	// 检测是否可以放置
	bCanPlace = CanPlaceAtCurrentLocation();

	// 更新预览颜色
	UpdatePreviewColor();
}

// 初始化预览
void ASG_PlacementPreview::InitializePreview(USG_CardDataBase* InCardData, APlayerController* InPlayerController)
{
	// 保存引用
	CardData = InCardData;
	PlayerController = InPlayerController;

	// 检查有效性
	if (!CardData || !PlayerController)
	{
		UE_LOG(LogTemp, Error, TEXT("InitializePreview 失败：CardData 或 PlayerController 为空"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("初始化放置预览 - 卡牌: %s, 放置类型: %d"), 
		*CardData->CardName.ToString(), 
		static_cast<int32>(CardData->PlacementType));

	// 根据放置类型创建预览
	switch (CardData->PlacementType)
	{
	case ESGPlacementType::Single:
		CreateSinglePointPreview();
		break;

	case ESGPlacementType::Area:
		CreateAreaPreview();
		break;

	case ESGPlacementType::Global:
		// 全局效果不需要预览
		UE_LOG(LogTemp, Warning, TEXT("全局效果卡牌不需要预览"));
		Destroy();
		break;

	default:
		UE_LOG(LogTemp, Error, TEXT("未知的放置类型"));
		Destroy();
		break;
	}
}

// 检查是否可以放置
bool ASG_PlacementPreview::CanPlaceAtCurrentLocation() const
{
	// 检查卡牌数据有效性
	if (!CardData)
	{
		return false;
	}

	// 检查是否在有效位置（射线是否命中地面）
	if (PreviewLocation.IsNearlyZero())
	{
		return false;
	}
	
	// 🔧 MODIFIED - 检查前线限制（优先检查，避免不必要的碰撞检测）
	if (CheckFrontLineViolation())
	{
		// 输出日志
		UE_LOG(LogSGGameplay, Verbose, TEXT("❌ 违反前线限制"));
		return false;
	}

	// 检查碰撞
	if (CheckCollision())
	{
		return false;
	}



	return true;
}

// 更新预览位置
void ASG_PlacementPreview::UpdatePreviewLocation()
{
	// 检查控制器有效性
	if (!PlayerController)
	{
		return;
	}

	// 获取鼠标位置
	float MouseX, MouseY;
	if (!PlayerController->GetMousePosition(MouseX, MouseY))
	{
		return;
	}

	// 从鼠标位置发射射线
	FVector WorldLocation, WorldDirection;
	if (!PlayerController->DeprojectScreenPositionToWorld(MouseX, MouseY, WorldLocation, WorldDirection))
	{
		return;
	}

	// 射线起点和终点
	FVector Start = WorldLocation;
	FVector End = Start + WorldDirection * RaycastDistance;

	// 执行射线检测
	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this); // 忽略自己

	// 检测地面
	bool bHit = GetWorld()->LineTraceSingleByChannel(
		HitResult,
		Start,
		End,
		ECC_Visibility,
		QueryParams
	);

	// 如果命中地面
	if (bHit)
	{
		// 更新预览位置（加上偏移避免 Z-Fighting）
		PreviewLocation = HitResult.Location + FVector(0.0f, 0.0f, GroundOffset);

		// 更新预览旋转（朝向玩家）
		if (PlayerController->GetPawn())
		{
			FVector DirectionToPlayer = PlayerController->GetPawn()->GetActorLocation() - PreviewLocation;
			DirectionToPlayer.Z = 0.0f; // 只考虑水平方向
			PreviewRotation = DirectionToPlayer.Rotation();
		}

		// 更新 Actor 位置
		SetActorLocation(PreviewLocation);
		SetActorRotation(PreviewRotation);

		// 调试绘制
		// DrawDebugSphere(GetWorld(), PreviewLocation, 50.0f, 12, FColor::Yellow, false, 0.0f);
	}
}
// ✨ NEW - 检查前线限制
/**
 * @brief 检查是否违反前线限制
 * @return 是否违反（true = 违反，不能放置）
 */
bool ASG_PlacementPreview::CheckFrontLineViolation() const
{
	// 检查卡牌数据是否有效
	if (!CardData)
	{
		return false;
	}
    
	// 全局效果不需要检查前线
	if (CardData->PlacementType == ESGPlacementType::Global)
	{
		return false;
	}
    
	// 如果卡牌不受前线限制，直接通过
	if (!CardData->bRespectFrontLine)
	{
		UE_LOG(LogSGGameplay, Verbose, TEXT("卡牌 [%s] 不受前线限制"), 
			*CardData->CardName.ToString());
		return false;
	}
    
	// 如果没有前线管理器，不检查前线
	if (!CachedFrontLineManager)
	{
		UE_LOG(LogSGGameplay, Verbose, TEXT("⚠️ 无前线管理器，跳过前线检查"));
		return false;
	}
    
	// 🔧 修改 - 使用新的区域查询接口
	ESGFrontLineZone Zone = CachedFrontLineManager->GetZoneAtLocation(PreviewLocation);
    
	// 玩家卡牌只能在玩家区域和中立区放置
	bool bIsValidPlacement = (Zone == ESGFrontLineZone::PlayerZone) || (Zone == ESGFrontLineZone::NeutralZone);
    
	// 输出详细日志
	UE_LOG(LogSGGameplay, Verbose, TEXT("前线检测："));
	UE_LOG(LogSGGameplay, Verbose, TEXT("  预览位置 X：%.0f"), PreviewLocation.X);
	UE_LOG(LogSGGameplay, Verbose, TEXT("  玩家前线 X：%.0f"), CachedFrontLineManager->GetPlayerFrontLineX());
	UE_LOG(LogSGGameplay, Verbose, TEXT("  敌人前线 X：%.0f"), CachedFrontLineManager->GetEnemyFrontLineX());
	UE_LOG(LogSGGameplay, Verbose, TEXT("  当前区域：%s"), 
		Zone == ESGFrontLineZone::PlayerZone ? TEXT("玩家区域") :
		Zone == ESGFrontLineZone::NeutralZone ? TEXT("中立区") : TEXT("敌人区域"));
	UE_LOG(LogSGGameplay, Verbose, TEXT("  是否可放置：%s"), bCanPlace ? TEXT("是") : TEXT("否"));
    
	// 返回是否违反（取反）
	return !bIsValidPlacement;
}


// 更新预览颜色
void ASG_PlacementPreview::UpdatePreviewColor()
{
	// 选择颜色
	FLinearColor TargetColor = bCanPlace ? ValidPlacementColor : InvalidPlacementColor;

	// 更新网格体颜色
	if (PreviewMesh && PreviewMesh->IsVisible())
	{
		// 如果还没有动态材质实例，创建一个
		if (!PreviewMaterialInstance && PreviewMesh->GetMaterial(0))
		{
			PreviewMaterialInstance = PreviewMesh->CreateDynamicMaterialInstance(0);
		}

		// 更新材质颜色
		if (PreviewMaterialInstance)
		{
			PreviewMaterialInstance->SetVectorParameterValue(TEXT("PreviewColor"), TargetColor);
			PreviewMaterialInstance->SetScalarParameterValue(TEXT("Opacity"), PreviewOpacity);
		}
	}

	// 更新区域指示器颜色
	if (AreaIndicator && AreaIndicator->IsVisible())
	{
		// 创建动态材质实例
		if (AreaIndicator->GetDecalMaterial())
		{
			UMaterialInstanceDynamic* DecalMaterial = AreaIndicator->CreateDynamicMaterialInstance();
			if (DecalMaterial)
			{
				DecalMaterial->SetVectorParameterValue(TEXT("Color"), TargetColor);
				DecalMaterial->SetScalarParameterValue(TEXT("Opacity"), PreviewOpacity);
			}
		}
	}
}

// ✨ NEW - 完整的碰撞检测实现（支持多种检测方式）
bool ASG_PlacementPreview::CheckCollision() const
{
	// 如果位置无效，返回有碰撞
	if (PreviewLocation.IsNearlyZero())
	{
		return true;
	}

	// 根据检测方式执行不同的检测逻辑
	switch (CollisionCheckMethod)
	{
	case ESGCollisionCheckMethod::ByChannel:
		return CheckCollisionByChannel();

	case ESGCollisionCheckMethod::ByObjectType:
		return CheckCollisionByObjectType();

	case ESGCollisionCheckMethod::ByActorClass:
		return CheckCollisionByActorClass();

	case ESGCollisionCheckMethod::ByDistance:
		return CheckCollisionByDistance();

	default:
		UE_LOG(LogTemp, Error, TEXT("未知的碰撞检测方式"));
		return true;
	}
}

// ✨ NEW - 通道查询方式
bool ASG_PlacementPreview::CheckCollisionByChannel() const
{
	TArray<FOverlapResult> OverlapResults;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	bool bHasOverlap = GetWorld()->OverlapMultiByChannel(
		OverlapResults,
		PreviewLocation,
		FQuat::Identity,
		CollisionChannel,
		FCollisionShape::MakeSphere(CollisionCheckRadius),
		QueryParams
	);

	int32 ValidOverlapCount = 0;

	for (const FOverlapResult& Result : OverlapResults)
	{
		AActor* OverlappedActor = Result.GetActor();
		
		if (!OverlappedActor || OverlappedActor == this)
		{
			continue;
		}

		if (bIgnorePreviewActors && OverlappedActor->IsA<ASG_PlacementPreview>())
		{
			continue;
		}

		if (ACharacter* Character = Cast<ACharacter>(OverlappedActor))
		{
			if (bIgnoreDeadUnits && (!IsValid(Character)  || Character->GetLifeSpan() > 0.0f))
			{
				continue;
			}
			
			ValidOverlapCount++;
			UE_LOG(LogTemp, Verbose, TEXT("  [ByChannel] 检测到单位：%s"), *Character->GetName());
		}
	}

	bool bResult = (ValidOverlapCount > 0);
	
	if (bEnableDebugDraw)
	{
		DrawDebugSphere(GetWorld(), PreviewLocation, CollisionCheckRadius, 12, 
			bResult ? FColor::Red : FColor::Green, false, 0.0f, 0, 2.0f);
	}

	return bResult;
}

// ✨ NEW - 对象类型查询方式
bool ASG_PlacementPreview::CheckCollisionByObjectType() const
{
	// 检查是否设置了对象类型
	if (ObjectTypes.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("⚠️ 未设置对象类型，默认不检测碰撞"));
		return false;
	}

	TArray<FOverlapResult> OverlapResults;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	FCollisionObjectQueryParams ObjectQueryParams;
	for (TEnumAsByte<EObjectTypeQuery> ObjectType : ObjectTypes)
	{
		ObjectQueryParams.AddObjectTypesToQuery(UEngineTypes::ConvertToCollisionChannel(ObjectType));
	}

	bool bHasOverlap = GetWorld()->OverlapMultiByObjectType(
		OverlapResults,
		PreviewLocation,
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(CollisionCheckRadius),
		QueryParams
	);

	int32 ValidOverlapCount = 0;

	for (const FOverlapResult& Result : OverlapResults)
	{
		AActor* OverlappedActor = Result.GetActor();
		
		if (!OverlappedActor || OverlappedActor == this)
		{
			continue;
		}

		if (bIgnorePreviewActors && OverlappedActor->IsA<ASG_PlacementPreview>())
		{
			continue;
		}

		if (ACharacter* Character = Cast<ACharacter>(OverlappedActor))
		{
			if (bIgnoreDeadUnits && (!IsValid(Character) || Character->GetLifeSpan() > 0.0f))
			{
				continue;
			}
			
			ValidOverlapCount++;
			UE_LOG(LogTemp, Verbose, TEXT("  [ByObjectType] 检测到单位：%s"), *Character->GetName());
		}
	}

	bool bResult = (ValidOverlapCount > 0);
	
	if (bEnableDebugDraw)
	{
		DrawDebugSphere(GetWorld(), PreviewLocation, CollisionCheckRadius, 12, 
			bResult ? FColor::Red : FColor::Green, false, 0.0f, 0, 2.0f);
	}

	return bResult;
}

// ✨ NEW - Actor 类查询方式
bool ASG_PlacementPreview::CheckCollisionByActorClass() const
{
	// 检查是否设置了 Actor 类
	if (ActorClassesToCheck.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("⚠️ 未设置 Actor 类，默认不检测碰撞"));
		return false;
	}

	int32 ValidOverlapCount = 0;

	// 遍历每个要检测的 Actor 类
	for (TSubclassOf<AActor> ActorClass : ActorClassesToCheck)
	{
		if (!ActorClass)
		{
			continue;
		}

		// 获取场景中所有该类的 Actor
		TArray<AActor*> FoundActors;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), ActorClass, FoundActors);

		// 检查距离
		for (AActor* Actor : FoundActors)
		{
			if (!Actor || Actor == this)
			{
				continue;
			}

			if (bIgnorePreviewActors && Actor->IsA<ASG_PlacementPreview>())
			{
				continue;
			}

			if (bIgnoreDeadUnits)
			{
				if (ACharacter* Character = Cast<ACharacter>(Actor))
				{
					if (!IsValid(Character) || Character->GetLifeSpan() > 0.0f)
					{
						continue;
					}
				}
			}

			// 计算距离
			float Distance = FVector::Dist(PreviewLocation, Actor->GetActorLocation());
			
			if (Distance < CollisionCheckRadius)
			{
				ValidOverlapCount++;
				UE_LOG(LogTemp, Verbose, TEXT("  [ByActorClass] 检测到单位：%s (距离: %.0f)"), 
					*Actor->GetName(), Distance);
			}
		}
	}

	bool bResult = (ValidOverlapCount > 0);
	
	if (bEnableDebugDraw)
	{
		DrawDebugSphere(GetWorld(), PreviewLocation, CollisionCheckRadius, 12, 
			bResult ? FColor::Red : FColor::Green, false, 0.0f, 0, 2.0f);
	}

	return bResult;
}

// ✨ NEW - 距离查询方式（最简单）
bool ASG_PlacementPreview::CheckCollisionByDistance() const
{
	// 获取场景中所有 Character
	TArray<AActor*> AllCharacters;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACharacter::StaticClass(), AllCharacters);

	int32 ValidOverlapCount = 0;

	for (AActor* Actor : AllCharacters)
	{
		if (!Actor || Actor == this)
		{
			continue;
		}

		if (bIgnorePreviewActors && Actor->IsA<ASG_PlacementPreview>())
		{
			continue;
		}

		if (bIgnoreDeadUnits)
		{
			if (ACharacter* Character = Cast<ACharacter>(Actor))
			{
				if (!IsValid(Character) || Character->GetLifeSpan() > 0.0f)
				{
					continue;
				}
			}
		}

		// 计算距离
		float Distance = FVector::Dist(PreviewLocation, Actor->GetActorLocation());
		
		if (Distance < CollisionCheckRadius)
		{
			ValidOverlapCount++;
			UE_LOG(LogTemp, Verbose, TEXT("  [ByDistance] 检测到单位：%s (距离: %.0f)"), 
				*Actor->GetName(), Distance);
		}
	}

	bool bResult = (ValidOverlapCount > 0);
	
	if (bEnableDebugDraw)
	{
		DrawDebugSphere(GetWorld(), PreviewLocation, CollisionCheckRadius, 12, 
			bResult ? FColor::Red : FColor::Green, false, 0.0f, 0, 2.0f);
	}

	return bResult;
}


// 创建单点预览
void ASG_PlacementPreview::CreateSinglePointPreview()
{
	UE_LOG(LogTemp, Log, TEXT("创建单点预览"));

	// 显示预览网格体
	PreviewMesh->SetVisibility(true);

	// TODO: 根据卡牌类型设置不同的预览网格体
	// 这里需要在蓝图中设置预览网格体
	// 或者从卡牌数据中获取预览网格体

	// 设置默认网格体（如果没有在蓝图中设置）
	if (!PreviewMesh->GetStaticMesh())
	{
		// 使用默认的球体网格
		static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere"));
		if (SphereMesh.Succeeded())
		{
			PreviewMesh->SetStaticMesh(SphereMesh.Object);
			PreviewMesh->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.0f));
		}
	}

	// 隐藏区域指示器
	AreaIndicator->SetVisibility(false);
}



// 创建区域预览
void ASG_PlacementPreview::CreateAreaPreview()
{
	UE_LOG(LogTemp, Log, TEXT("创建区域预览"));

	// 隐藏预览网格体
	PreviewMesh->SetVisibility(false);

	// 显示区域指示器
	AreaIndicator->SetVisibility(true);

	// 设置区域大小
	if (CardData)
	{
		FVector2D AreaSize = CardData->PlacementAreaSize;
		// Decal 的 X 是厚度，Y 和 Z 是宽度和长度
		AreaIndicator->DecalSize = FVector(100.0f, AreaSize.X / 2.0f, AreaSize.Y / 2.0f);

		UE_LOG(LogTemp, Log, TEXT("区域大小: %.0f x %.0f"), AreaSize.X, AreaSize.Y);
	}

	// TODO: 设置区域指示器材质
	// 需要在蓝图中设置或从资源加载
}
