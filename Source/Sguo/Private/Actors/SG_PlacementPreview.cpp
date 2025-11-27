// SG_PlacementPreview.cpp
// Fill out your copyright notice in the Description page of Project Settings.

#include "Actors/SG_PlacementPreview.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DecalComponent.h"
#include "Data/SG_CardDataBase.h"
#include "Engine/OverlapResult.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Debug/SG_LogCategories.h"
#include "Actors/SG_FrontLineManager.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"

ASG_PlacementPreview::ASG_PlacementPreview()
{
    PrimaryActorTick.bCanEverTick = true;

    // 创建根组件
    RootComp = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
    RootComponent = RootComp;

    // 创建预览网格体
    PreviewMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PreviewMesh"));
    PreviewMesh->SetupAttachment(RootComp);
    PreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PreviewMesh->SetVisibility(false);

    // 创建区域指示器
    AreaIndicator = CreateDefaultSubobject<UDecalComponent>(TEXT("AreaIndicator"));
    AreaIndicator->SetupAttachment(RootComp);
    AreaIndicator->SetVisibility(false);
    AreaIndicator->DecalSize = FVector(100.0f, 100.0f, 100.0f);

    // 初始化
    PreviewLocation = FVector::ZeroVector;
    PreviewRotation = FRotator::ZeroRotator;
    bCanPlace = false;

    // 默认设置：地面检测忽略 Character
    GroundTraceIgnoredClasses.Add(ACharacter::StaticClass());
    
    // 默认设置：碰撞检测使用 Pawn 对象类型
    CollisionObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
}

void ASG_PlacementPreview::BeginPlay()
{
    Super::BeginPlay();
    
    // 查找前线管理器
    CachedFrontLineManager = ASG_FrontLineManager::GetFrontLineManager(this);
    
    if (CachedFrontLineManager)
    {
        UE_LOG(LogSGGameplay, Log, TEXT("✓ 找到前线管理器"));
    }
    else
    {
        UE_LOG(LogSGGameplay, Warning, TEXT("⚠️ 未找到前线管理器"));
    }
}

void ASG_PlacementPreview::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 更新位置
    UpdatePreviewLocation();

    // 检测是否可放置
    bCanPlace = CanPlaceAtCurrentLocation();

    // 更新颜色
    UpdatePreviewColor();
}

void ASG_PlacementPreview::InitializePreview(USG_CardDataBase* InCardData, APlayerController* InPlayerController)
{
    CardData = InCardData;
    PlayerController = InPlayerController;

    if (!CardData || !PlayerController)
    {
        UE_LOG(LogTemp, Error, TEXT("InitializePreview 失败：参数无效"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("初始化放置预览 - 卡牌: %s"), *CardData->CardName.ToString());

    switch (CardData->PlacementType)
    {
    case ESGPlacementType::Single:
        CreateSinglePointPreview();
        break;

    case ESGPlacementType::Area:
        CreateAreaPreview();
        break;

    case ESGPlacementType::Global:
        UE_LOG(LogTemp, Warning, TEXT("全局效果卡牌不需要预览"));
        Destroy();
        break;

    default:
        UE_LOG(LogTemp, Error, TEXT("未知的放置类型"));
        Destroy();
        break;
    }
}

bool ASG_PlacementPreview::CanPlaceAtCurrentLocation() const
{
    // 检查卡牌数据
    if (!CardData)
    {
        return false;
    }

    // 检查位置是否有效
    if (PreviewLocation.IsNearlyZero())
    {
        return false;
    }
    
    // 检查前线限制
    if (CheckFrontLineViolation())
    {
        return false;
    }

    // 检查碰撞
    if (CheckCollision())
    {
        return false;
    }

    return true;
}

void ASG_PlacementPreview::UpdatePreviewLocation()
{
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

    // 转换为世界射线
    FVector WorldLocation, WorldDirection;
    if (!PlayerController->DeprojectScreenPositionToWorld(MouseX, MouseY, WorldLocation, WorldDirection))
    {
        return;
    }

    // 射线参数
    FVector Start = WorldLocation;
    FVector End = Start + WorldDirection * RaycastDistance;

    // 构建忽略列表
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);
    BuildGroundTraceIgnoreList(QueryParams);

    // 执行射线检测
    FHitResult HitResult;
    bool bHit = false;

    // 优先使用对象类型查询（如果设置了）
    if (GroundObjectTypes.Num() > 0)
    {
        FCollisionObjectQueryParams ObjectParams;
        for (auto ObjectType : GroundObjectTypes)
        {
            ObjectParams.AddObjectTypesToQuery(UEngineTypes::ConvertToCollisionChannel(ObjectType));
        }
        
        bHit = GetWorld()->LineTraceSingleByObjectType(
            HitResult,
            Start,
            End,
            ObjectParams,
            QueryParams
        );
    }
    else
    {
        // 使用通道查询
        bHit = GetWorld()->LineTraceSingleByChannel(
            HitResult,
            Start,
            End,
            GroundTraceChannel,
            QueryParams
        );
    }

    // 更新位置
    if (bHit)
    {
        PreviewLocation = HitResult.Location + FVector(0.0f, 0.0f, GroundOffset);
        SetActorLocation(PreviewLocation);

        // 调试绘制
        if (bDebugGroundTrace)
        {
            DrawDebugLine(GetWorld(), Start, HitResult.Location, FColor::Green, false, 0.0f, 0, 1.0f);
            DrawDebugSphere(GetWorld(), HitResult.Location, 10.0f, 8, FColor::Cyan, false, 0.0f);
            DrawDebugSphere(GetWorld(), PreviewLocation, 15.0f, 8, FColor::Yellow, false, 0.0f);
        }
    }
    else
    {
        if (bDebugGroundTrace)
        {
            DrawDebugLine(GetWorld(), Start, End, FColor::Red, false, 0.0f, 0, 1.0f);
        }
    }
}

bool ASG_PlacementPreview::CheckCollision() const
{
    if (PreviewLocation.IsNearlyZero())
    {
        return true;
    }

    // 构建忽略列表
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);
    BuildCollisionIgnoreList(QueryParams);

    TArray<FOverlapResult> OverlapResults;
    bool bHasOverlap = false;

    // 优先使用对象类型查询（如果设置了）
    if (CollisionObjectTypes.Num() > 0)
    {
        FCollisionObjectQueryParams ObjectParams;
        for (auto ObjectType : CollisionObjectTypes)
        {
            ObjectParams.AddObjectTypesToQuery(UEngineTypes::ConvertToCollisionChannel(ObjectType));
        }
        
        bHasOverlap = GetWorld()->OverlapMultiByObjectType(
            OverlapResults,
            PreviewLocation,
            FQuat::Identity,
            ObjectParams,
            FCollisionShape::MakeSphere(CollisionCheckRadius),
            QueryParams
        );
    }
    else
    {
        // 使用通道查询
        bHasOverlap = GetWorld()->OverlapMultiByChannel(
            OverlapResults,
            PreviewLocation,
            FQuat::Identity,
            CollisionCheckChannel,
            FCollisionShape::MakeSphere(CollisionCheckRadius),
            QueryParams
        );
    }

    // 统计有效碰撞
    int32 ValidOverlapCount = 0;
    
    for (const FOverlapResult& Result : OverlapResults)
    {
        AActor* OverlappedActor = Result.GetActor();
        
        if (!OverlappedActor || OverlappedActor == this)
        {
            continue;
        }

        // 检查是否是死亡单位
        if (bIgnoreDeadUnits)
        {
            if (ACharacter* Character = Cast<ACharacter>(OverlappedActor))
            {
                if (!IsValid(Character) || Character->GetLifeSpan() > 0.0f)
                {
                    continue;
                }
            }
        }

        ValidOverlapCount++;
        
        if (bDebugCollision)
        {
            UE_LOG(LogTemp, Log, TEXT("  碰撞检测到：%s"), *OverlappedActor->GetName());
        }
    }

    bool bResult = (ValidOverlapCount > 0);

    // 调试绘制
    if (bDebugCollision)
    {
        DrawDebugSphere(GetWorld(), PreviewLocation, CollisionCheckRadius, 16, 
            bResult ? FColor::Red : FColor::Green, false, 0.0f, 0, 2.0f);
    }

    return bResult;
}

void ASG_PlacementPreview::BuildGroundTraceIgnoreList(FCollisionQueryParams& OutParams) const
{
    // 忽略配置的类
    for (TSubclassOf<AActor> ActorClass : GroundTraceIgnoredClasses)
    {
        if (!ActorClass)
        {
            continue;
        }

        TArray<AActor*> FoundActors;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), ActorClass, FoundActors);
        
        for (AActor* Actor : FoundActors)
        {
            if (Actor)
            {
                OutParams.AddIgnoredActor(Actor);
            }
        }
    }

    // 始终忽略其他预览 Actor
    TArray<AActor*> AllPreviews;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASG_PlacementPreview::StaticClass(), AllPreviews);
    for (AActor* Preview : AllPreviews)
    {
        if (Preview)
        {
            OutParams.AddIgnoredActor(Preview);
        }
    }
}

void ASG_PlacementPreview::BuildCollisionIgnoreList(FCollisionQueryParams& OutParams) const
{
    // 忽略配置的类
    for (TSubclassOf<AActor> ActorClass : CollisionIgnoredClasses)
    {
        if (!ActorClass)
        {
            continue;
        }

        TArray<AActor*> FoundActors;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), ActorClass, FoundActors);
        
        for (AActor* Actor : FoundActors)
        {
            if (Actor)
            {
                OutParams.AddIgnoredActor(Actor);
            }
        }
    }

    // 始终忽略其他预览 Actor
    TArray<AActor*> AllPreviews;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASG_PlacementPreview::StaticClass(), AllPreviews);
    for (AActor* Preview : AllPreviews)
    {
        if (Preview)
        {
            OutParams.AddIgnoredActor(Preview);
        }
    }
}

bool ASG_PlacementPreview::CheckFrontLineViolation() const
{
    if (!CardData)
    {
        return false;
    }
    
    if (CardData->PlacementType == ESGPlacementType::Global)
    {
        return false;
    }
    
    if (!CardData->bRespectFrontLine)
    {
        return false;
    }
    
    if (!CachedFrontLineManager)
    {
        return false;
    }
    
    ESGFrontLineZone Zone = CachedFrontLineManager->GetZoneAtLocation(PreviewLocation);
    bool bIsValidPlacement = (Zone == ESGFrontLineZone::PlayerZone) || (Zone == ESGFrontLineZone::NeutralZone);
    
    return !bIsValidPlacement;
}

void ASG_PlacementPreview::UpdatePreviewColor()
{
    FLinearColor TargetColor = bCanPlace ? ValidPlacementColor : InvalidPlacementColor;

    if (PreviewMesh && PreviewMesh->IsVisible())
    {
        if (!PreviewMaterialInstance && PreviewMesh->GetMaterial(0))
        {
            PreviewMaterialInstance = PreviewMesh->CreateDynamicMaterialInstance(0);
        }

        if (PreviewMaterialInstance)
        {
            PreviewMaterialInstance->SetVectorParameterValue(TEXT("PreviewColor"), TargetColor);
            PreviewMaterialInstance->SetScalarParameterValue(TEXT("Opacity"), PreviewOpacity);
        }
    }

    if (AreaIndicator && AreaIndicator->IsVisible())
    {
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

void ASG_PlacementPreview::CreateSinglePointPreview()
{
    UE_LOG(LogTemp, Log, TEXT("创建单点预览"));

    PreviewMesh->SetVisibility(true);

    if (!PreviewMesh->GetStaticMesh())
    {
        static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere"));
        if (SphereMesh.Succeeded())
        {
            PreviewMesh->SetStaticMesh(SphereMesh.Object);
            PreviewMesh->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.0f));
        }
    }

    AreaIndicator->SetVisibility(false);
}

void ASG_PlacementPreview::CreateAreaPreview()
{
    UE_LOG(LogTemp, Log, TEXT("创建区域预览"));

    PreviewMesh->SetVisibility(false);
    AreaIndicator->SetVisibility(true);

    if (CardData)
    {
        FVector2D AreaSize = CardData->PlacementAreaSize;
        AreaIndicator->DecalSize = FVector(100.0f, AreaSize.X / 2.0f, AreaSize.Y / 2.0f);
    }
}
