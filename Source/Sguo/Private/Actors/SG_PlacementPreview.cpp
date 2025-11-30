// 📄 文件：Source/Sguo/Private/Actors/SG_PlacementPreview.cpp
// 🔧 修改 - 完整修复编译错误，删除废弃函数，包含性能优化

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
#include "GameFramework/Pawn.h"

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
    // 🔧 修改 - 默认让 Decal 向下投射 (Pitch -90)，防止侧面拉伸
    AreaIndicator->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));

    // 初始化
    PreviewLocation = FVector::ZeroVector;
    PreviewRotation = FRotator::ZeroRotator;
    bCanPlace = false;

    // 🔧 优化 - 默认配置
    // 移除旧的 GroundTraceIgnoredClasses.Add(...)
    
    // 默认使用 Pawn 作为碰撞检测类型
    CollisionObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
    
    // 默认开启高性能地面检测 (只检测 WorldStatic)
    bOnlyTraceWorldStatic = true;
}

void ASG_PlacementPreview::BeginPlay()
{
    Super::BeginPlay();
    
    // 查找前线管理器
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

    // ✨ 优化 - 基础查询参数（移除 build ignore list）
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);
    if (PlayerController->GetPawn())
    {
        QueryParams.AddIgnoredActor(PlayerController->GetPawn());
    }

    // 执行射线检测
    FHitResult HitResult;
    bool bHit = false;

    // ✨ 核心优化逻辑：区分“强制静态检测”和“自定义检测”
    if (bOnlyTraceWorldStatic)
    {
        // 🚀 高性能模式：只检测 Static，自动忽略所有 Character/Pawn (通常是 Dynamic)
        FCollisionObjectQueryParams ObjectParams;
        ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
        // 如果你的地貌是单独的类型，在这里添加，例如:
        // ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic); 
        
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
        // 传统模式：混合检测
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
    }

    // 更新位置
    if (bHit)
    {
        // ✨ 根据法线偏移，确保贴合
        PreviewLocation = HitResult.Location + HitResult.ImpactNormal * GroundOffset;
        SetActorLocation(PreviewLocation);

        // 调试绘制
        if (bDebugGroundTrace)
        {
            DrawDebugLine(GetWorld(), Start, HitResult.Location, FColor::Green, false, 0.0f, 0, 1.0f);
            DrawDebugSphere(GetWorld(), HitResult.Location, 10.0f, 8, FColor::Cyan, false, 0.0f);
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
    // ✨ 针对计谋卡(Area类型)的特殊逻辑
    // 如果是计谋卡，通常允许重叠释放（或者不需要检测单位碰撞）
    // 这里保留碰撞检测逻辑，如果不需要，可以在 CardData 里加标志位控制
    if (CardData && CardData->PlacementType == ESGPlacementType::Area)
    {
        // 如果想让计谋卡无视单位碰撞（比如火矢计可以随便放），这里直接 return false
        // return false; 
    }

    if (PreviewLocation.IsNearlyZero())
    {
        return true;
    }

    // 构建查询参数
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);
    if (PlayerController && PlayerController->GetPawn())
    {
        QueryParams.AddIgnoredActor(PlayerController->GetPawn());
    }

    TArray<FOverlapResult> OverlapResults;
    bool bHasOverlap = false;

    // 优先使用对象类型查询
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
                // 如果 LifeSpan > 0，通常意味着正在倒计时销毁（已死亡）
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

bool ASG_PlacementPreview::CheckFrontLineViolation() const
{
    if (!CardData)
    {
        return false;
    }
    
    // 全局和无限制卡牌
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
        // 假设 X 是半径
        float Radius = CardData->PlacementAreaSize.X;
        
        // Decal Size: X=厚度(深度), Y=宽, Z=高
        // 由于我们设置了 Pitch -90，X轴垂直地面
        // 我们给 400 的厚度确保能覆盖斜坡
        AreaIndicator->DecalSize = FVector(400.0f, Radius, Radius);
    }
}

