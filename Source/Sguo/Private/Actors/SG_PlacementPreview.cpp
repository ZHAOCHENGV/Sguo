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

    // 创建区域指示器（计谋卡Area常用）
    AreaIndicator = CreateDefaultSubobject<UDecalComponent>(TEXT("AreaIndicator"));
    AreaIndicator->SetupAttachment(RootComp);
    AreaIndicator->SetVisibility(false);
    AreaIndicator->DecalSize = FVector(100.0f, 100.0f, 100.0f);
    // 🔧 修改 - 确保 Decal 朝下投射
    AreaIndicator->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));

    // 初始化
    PreviewLocation = FVector::ZeroVector;
    PreviewRotation = FRotator::ZeroRotator;
    bCanPlace = false;

    // 默认设置：碰撞检测使用 Pawn 对象类型
    CollisionObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
    
    // 🔧 优化 - 默认仅检测 WorldStatic，这是忽略 Pawn 最快的方法
    bOnlyTraceWorldStatic = true;
}

void ASG_PlacementPreview::BeginPlay()
{
    Super::BeginPlay();
    
    CachedFrontLineManager = ASG_FrontLineManager::GetFrontLineManager(this);
    
    if (CachedFrontLineManager)
    {
        UE_LOG(LogSGGameplay, Verbose, TEXT("✓ 找到前线管理器"));
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
        return;
    }

    switch (CardData->PlacementType)
    {
    case ESGPlacementType::Single:
        CreateSinglePointPreview();
        break;

    case ESGPlacementType::Area:
        CreateAreaPreview(); // 计谋卡通常走这里
        break;

    case ESGPlacementType::Global:
        Destroy(); // 不需要预览
        break;

    default:
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
    if (!PlayerController) return;

    // 获取鼠标位置
    float MouseX, MouseY;
    if (!PlayerController->GetMousePosition(MouseX, MouseY)) return;

    // 转换为世界射线
    FVector WorldLocation, WorldDirection;
    if (!PlayerController->DeprojectScreenPositionToWorld(MouseX, MouseY, WorldLocation, WorldDirection)) return;

    FVector Start = WorldLocation;
    FVector End = Start + WorldDirection * RaycastDistance;

    // ✨ 优化 - 基础查询参数（不包含每帧遍历！）
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this); // 忽略自己
    if (APawn* PlayerPawn = PlayerController->GetPawn())
    {
        QueryParams.AddIgnoredActor(PlayerPawn); // 忽略玩家控制的角色
    }
    
    // 执行射线检测
    FHitResult HitResult;
    bool bHit = false;

    // ✨ 核心逻辑：区分“强制静态检测”和“自定义检测”
    if (bOnlyTraceWorldStatic)
    {
        // 🚀 高性能模式：只检测 Static，完美忽略所有单位（Dynamic/Pawn）
        FCollisionObjectQueryParams ObjectParams;
        ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic); 
        // 也可以加上 Landscape 如果它是单独的类型
        
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
        // 传统的通道/类型混合模式
        if (GroundObjectTypes.Num() > 0)
        {
            FCollisionObjectQueryParams ObjectParams;
            for (auto ObjectType : GroundObjectTypes)
            {
                ObjectParams.AddObjectTypesToQuery(UEngineTypes::ConvertToCollisionChannel(ObjectType));
            }
            bHit = GetWorld()->LineTraceSingleByObjectType(HitResult, Start, End, ObjectParams, QueryParams);
        }
        else
        {
            bHit = GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, GroundTraceChannel, QueryParams);
        }
    }

    // 更新位置
    if (bHit)
    {
        // ✨ 新增 - 更加贴合地面的法线旋转（可选，如果是Decal会自动贴合，但Mesh需要旋转）
        PreviewLocation = HitResult.Location + HitResult.ImpactNormal * GroundOffset;
        
        // 如果是 Mesh，可以根据法线调整旋转
        // FRotator NewRot = FRotationMatrix::MakeFromZ(HitResult.ImpactNormal).Rotator();
        // SetActorRotation(NewRot); // 根据需求开启

        SetActorLocation(PreviewLocation);

        if (bDebugGroundTrace)
        {
            DrawDebugLine(GetWorld(), Start, HitResult.Location, FColor::Green, false, 0.0f, 0, 1.0f);
            DrawDebugSphere(GetWorld(), HitResult.Location, 10.0f, 8, FColor::Cyan, false, 0.0f);
        }
    }
    else
    {
        // 未检测到地面，投影到远端或保持原位
        if (bDebugGroundTrace)
        {
            DrawDebugLine(GetWorld(), Start, End, FColor::Red, false, 0.0f, 0, 1.0f);
        }
    }
}

bool ASG_PlacementPreview::CheckCollision() const
{
   // 如果是计谋卡（Area模式），且不需要检测碰撞（例如火矢计可以随便放），则直接返回 false（无碰撞）
    // 通常计谋卡是可以重叠释放的，除非你的设计不允许
    if (CardData && CardData->PlacementType == ESGPlacementType::Area)
    {
        // 🔧 这里假设计谋卡不需要避开单位。如果需要，请保留下方逻辑。
        // return false; 
    }
    
    if (PreviewLocation.IsNearlyZero()) return true;

    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);
    if (PlayerController && PlayerController->GetPawn())
    {
        QueryParams.AddIgnoredActor(PlayerController->GetPawn());
    }

    // 仅忽略当前已经缓存的，不每帧遍历
    // 如果有特殊需求要忽略特定单位，建议在 Start 时获取一次
    
    TArray<FOverlapResult> OverlapResults;
    bool bHasOverlap = false;

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
        bHasOverlap = GetWorld()->OverlapMultiByChannel(
            OverlapResults,
            PreviewLocation,
            FQuat::Identity,
            CollisionCheckChannel,
            FCollisionShape::MakeSphere(CollisionCheckRadius),
            QueryParams
        );
    }

    int32 ValidOverlapCount = 0;
    for (const FOverlapResult& Result : OverlapResults)
    {
        AActor* OverlappedActor = Result.GetActor();
        if (!OverlappedActor || OverlappedActor == this) continue;

        if (bIgnoreDeadUnits)
        {
            if (ACharacter* Character = Cast<ACharacter>(OverlappedActor))
            {
                // 如果是死亡单位，忽略
                // 注意：这里需要你的 UnitBase 有 IsDead 接口或者 LifeSpan
                // 假设 LifeSpan > 0 意味着正在死亡过程中
                if (Character->GetLifeSpan() > 0.0f) continue;
            }
        }
        ValidOverlapCount++;
    }

    return (ValidOverlapCount > 0);
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

    // 区域预览（如火矢计）
    PreviewMesh->SetVisibility(false);
    AreaIndicator->SetVisibility(true);

    if (CardData)
    {
        // Decal 的 Size X/Y/Z 对应：厚度 / 宽 / 高
        // CardData->PlacementAreaSize 是半径还是直径？假设是半径
        float Radius = CardData->PlacementAreaSize.X; // 假设 X 是半径
        AreaIndicator->DecalSize = FVector(200.0f, Radius, Radius); // 200厚度，确保覆盖斜坡
    }
 
}
