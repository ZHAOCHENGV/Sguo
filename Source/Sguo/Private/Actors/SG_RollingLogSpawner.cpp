// 📄 文件：Source/Sguo/Private/Actors/SG_RollingLogSpawner.cpp
// 🔧 修改 - 添加滚木旋转预览可视化

#include "Actors/SG_RollingLogSpawner.h"
#include "Actors/SG_RollingLog.h"
#include "Data/SG_RollingLogCardData.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/BillboardComponent.h"
#include "Components/StaticMeshComponent.h"  // ✨ 新增
#include "AbilitySystemComponent.h"
#include "Debug/SG_LogCategories.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInstanceDynamic.h"  // ✨ 新增
#include "Engine/StaticMesh.h"  // ✨ 新增
#include "DrawDebugHelpers.h"  // ✨ 新增

ASG_RollingLogSpawner::ASG_RollingLogSpawner()
{
    PrimaryActorTick.bCanEverTick = true;

    // ========== 创建场景根组件 ==========
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    RootComponent = SceneRoot;

    // ========== 创建方向箭头 ==========
    DirectionArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("DirectionArrow"));
    DirectionArrow->SetupAttachment(RootComponent);
    DirectionArrow->SetArrowColor(FLinearColor::Red);
    DirectionArrow->ArrowSize = 2.0f;
    DirectionArrow->ArrowLength = 200.0f;
    DirectionArrow->SetRelativeRotation(FRotator::ZeroRotator);

#if WITH_EDITORONLY_DATA
    DirectionArrow->bIsEditorOnly = true;
#endif

    // ========== 创建生成区域可视化 ==========
    SpawnAreaBox = CreateDefaultSubobject<UBoxComponent>(TEXT("SpawnAreaBox"));
    SpawnAreaBox->SetupAttachment(RootComponent);
    SpawnAreaBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SpawnAreaBox->SetBoxExtent(FVector(50.0f, SpawnAreaWidth * 0.5f, 50.0f));
    SpawnAreaBox->SetLineThickness(2.0f);
    SpawnAreaBox->ShapeColor = FColor::Cyan;

#if WITH_EDITORONLY_DATA
    SpawnAreaBox->bIsEditorOnly = true;
#endif

    // ========== 创建广告牌组件 ==========
    BillboardComponent = CreateDefaultSubobject<UBillboardComponent>(TEXT("BillboardComponent"));
    BillboardComponent->SetupAttachment(RootComponent);
    BillboardComponent->SetRelativeLocation(FVector(0.0f, 0.0f, BillboardHeightOffset));
    
    static ConstructorHelpers::FObjectFinder<UTexture2D> DefaultSpriteFinder(
        TEXT("/Engine/EditorResources/S_NavLinkProxy")
    );
    if (DefaultSpriteFinder.Succeeded())
    {
        BillboardSprite = DefaultSpriteFinder.Object;
        BillboardComponent->SetSprite(BillboardSprite);
    }
    
    BillboardComponent->bIsScreenSizeScaled = true;
    BillboardComponent->ScreenSize = 0.0025f;

#if WITH_EDITORONLY_DATA
    BillboardComponent->bIsEditorOnly = true;
#endif

    // ========== ✨ 新增 - 创建滚木预览网格体 ==========
    LogPreviewMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LogPreviewMesh"));
    LogPreviewMesh->SetupAttachment(RootComponent);
    LogPreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    LogPreviewMesh->SetCastShadow(false);
    
    // 尝试加载默认圆柱体网格作为预览
    static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultMeshFinder(
        TEXT("/Engine/BasicShapes/Cylinder")
    );
    if (DefaultMeshFinder.Succeeded())
    {
        PreviewMesh = DefaultMeshFinder.Object;
        LogPreviewMesh->SetStaticMesh(PreviewMesh);
    }
    
    // 设置预览网格体的默认缩放（横向放置的木桩）
    LogPreviewMesh->SetRelativeScale3D(FVector(0.5f, 0.5f, 1.5f));  // X=粗细, Y=粗细, Z=长度
    LogPreviewMesh->SetRelativeRotation(FRotator(0.0f, 0.0f, 90.0f));  // 横向放置
    
#if WITH_EDITORONLY_DATA
    LogPreviewMesh->bIsEditorOnly = true;
#endif

    // 默认阵营为玩家
    FactionTag = FGameplayTag::RequestGameplayTag(FName("Unit.Faction.Player"), false);
}

void ASG_RollingLogSpawner::BeginPlay()
{
    Super::BeginPlay();

    // 运行时隐藏编辑器组件
    if (DirectionArrow)
    {
        DirectionArrow->SetVisibility(false);
    }
    if (SpawnAreaBox)
    {
        SpawnAreaBox->SetVisibility(false);
    }

    // 广告牌可见性
    if (BillboardComponent)
    {
        if (bShowBillboardAtRuntime)
        {
            BillboardComponent->SetVisibility(true);
            BillboardComponent->SetHiddenInGame(false);
        }
        else
        {
            BillboardComponent->SetVisibility(false);
            BillboardComponent->SetHiddenInGame(true);
        }
    }

    // ✨ 新增 - 运行时隐藏预览网格体
    if (LogPreviewMesh)
    {
        LogPreviewMesh->SetVisibility(false);
        LogPreviewMesh->SetHiddenInGame(true);
    }

    SetupBillboard();

    UE_LOG(LogSGGameplay, Log, TEXT("流木计生成器初始化：%s"), *GetName());
    UE_LOG(LogSGGameplay, Log, TEXT("  位置：%s"), *GetActorLocation().ToString());
    UE_LOG(LogSGGameplay, Log, TEXT("  滚动方向：%s"), *GetRollDirection().ToString());
    UE_LOG(LogSGGameplay, Log, TEXT("  生成旋转：%s"), *GetSpawnRotation().ToString());
    UE_LOG(LogSGGameplay, Log, TEXT("  阵营：%s"), *FactionTag.ToString());
}

void ASG_RollingLogSpawner::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    switch (CurrentState)
    {
    case ESGSpawnerState::Active:
        {
            SpawnTimer += DeltaTime;
            RemainingDuration -= DeltaTime;

            if (ActiveCardData && SpawnTimer >= ActiveCardData->SpawnInterval)
            {
                SpawnTimer -= ActiveCardData->SpawnInterval;
                SpawnRollingLogs();
            }

            if (RemainingDuration <= 0.0f)
            {
                UE_LOG(LogSGGameplay, Log, TEXT("流木计生成器持续时间结束：%s"), *GetName());
                Deactivate();
            }
        }
        break;

    case ESGSpawnerState::Cooldown:
        {
            CooldownRemainingTime -= DeltaTime;

            if (CooldownRemainingTime <= 0.0f)
            {
                CooldownRemainingTime = 0.0f;
                CurrentState = ESGSpawnerState::Idle;
                
                UE_LOG(LogSGGameplay, Log, TEXT("流木计生成器冷却结束：%s"), *GetName());
                K2_OnCooldownFinished();
            }
        }
        break;

    default:
        break;
    }

    // ✨ 新增 - 在编辑器中绘制生成旋转坐标轴
#if WITH_EDITOR
    if (bShowSpawnRotationAxis && !GetWorld()->IsGameWorld())
    {
        FVector Location = GetActorLocation();
        FRotator SpawnRot = GetSpawnRotation();
        
        // 获取旋转后的坐标轴
        FVector ForwardAxis = SpawnRot.Vector();
        FVector RightAxis = FRotationMatrix(SpawnRot).GetScaledAxis(EAxis::Y);
        FVector UpAxis = FRotationMatrix(SpawnRot).GetScaledAxis(EAxis::Z);
        
        float AxisLength = 150.0f;
        
        // X 轴（红色）- Forward
        DrawDebugDirectionalArrow(GetWorld(), Location, Location + ForwardAxis * AxisLength, 
            20.0f, FColor::Red, false, -1.0f, 0, 3.0f);
        
        // Y 轴（绿色）- Right
        DrawDebugDirectionalArrow(GetWorld(), Location, Location + RightAxis * AxisLength, 
            20.0f, FColor::Green, false, -1.0f, 0, 3.0f);
        
        // Z 轴（蓝色）- Up
        DrawDebugDirectionalArrow(GetWorld(), Location, Location + UpAxis * AxisLength, 
            20.0f, FColor::Blue, false, -1.0f, 0, 3.0f);
    }
#endif
}

#if WITH_EDITOR
void ASG_RollingLogSpawner::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    UpdateSpawnAreaVisualization();
    
    // ✨ 新增 - 更新预览网格体
    UpdatePreviewMesh();

    if (BillboardComponent)
    {
        BillboardComponent->SetRelativeLocation(FVector(0.0f, 0.0f, BillboardHeightOffset));
        
        if (BillboardSprite)
        {
            BillboardComponent->SetSprite(BillboardSprite);
        }
    }
}

/**
 * @brief 属性修改回调
 * @details 当在编辑器中修改属性时更新预览
 */
void ASG_RollingLogSpawner::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    // 获取修改的属性名称
    FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? 
        PropertyChangedEvent.Property->GetFName() : NAME_None;

    // 如果修改了旋转相关属性，更新预览
    if (PropertyName == GET_MEMBER_NAME_CHECKED(ASG_RollingLogSpawner, SpawnRotationOffset) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(ASG_RollingLogSpawner, bUseCustomSpawnRotation) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(ASG_RollingLogSpawner, PreviewMesh) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(ASG_RollingLogSpawner, PreviewMeshScale) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(ASG_RollingLogSpawner, bShowPreviewMesh) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(ASG_RollingLogSpawner, PreviewMeshOpacity))
    {
        UpdatePreviewMesh();
    }
}
#endif

FVector ASG_RollingLogSpawner::GetRollDirection() const
{
    FVector Direction = GetActorForwardVector();
    Direction.Z = 0.0f;
    Direction.Normalize();
    return Direction;
}

/**
 * @brief 获取滚木生成时的旋转
 * @return 生成旋转（世界空间）
 * @details
 * **功能说明：**
 * - 如果使用自定义旋转，返回 Actor 旋转 + 偏移
 * - 否则根据滚动方向自动计算
 */
FRotator ASG_RollingLogSpawner::GetSpawnRotation() const
{
    // 🔧 直接使用预览网格体的世界旋转
    if (LogPreviewMesh)
    {
        return LogPreviewMesh->GetComponentRotation();
    }
    
    // 备用：使用滚动方向
    return GetRollDirection().Rotation();
}

/**
 * @brief 更新预览网格体
 * @details
 * **功能说明：**
 * - 更新预览网格体的变换和材质
 * - 在编辑器中实时显示滚木的生成姿态
 */
void ASG_RollingLogSpawner::UpdatePreviewMesh()
{
    if (!LogPreviewMesh)
    {
        return;
    }

    // 设置可见性
    LogPreviewMesh->SetVisibility(bShowPreviewMesh);

    // 设置网格体
    if (PreviewMesh)
    {
        LogPreviewMesh->SetStaticMesh(PreviewMesh);
    }

    // 设置缩放
    LogPreviewMesh->SetRelativeScale3D(PreviewMeshScale);

    // 🔧 关键 - 预览网格体使用相对旋转
    // 因为它附着在生成器上，相对旋转 = SpawnRotationOffset
    // 最终世界旋转 = 生成器旋转 * SpawnRotationOffset（自动计算）
    LogPreviewMesh->SetRelativeRotation(SpawnRotationOffset);

    UE_LOG(LogSGGameplay, Verbose, TEXT("UpdatePreviewMesh:"));
    UE_LOG(LogSGGameplay, Verbose, TEXT("  SpawnRotationOffset: %s"), *SpawnRotationOffset.ToString());
    UE_LOG(LogSGGameplay, Verbose, TEXT("  预览网格相对旋转: %s"), *LogPreviewMesh->GetRelativeRotation().ToString());
    UE_LOG(LogSGGameplay, Verbose, TEXT("  预览网格世界旋转: %s"), *LogPreviewMesh->GetComponentRotation().ToString());

    // 创建/更新半透明材质
    CreatePreviewMaterial();
}

/**
 * @brief 设置预览可见性
 */
void ASG_RollingLogSpawner::SetPreviewVisibility(bool bVisible)
{
    bShowPreviewMesh = bVisible;
    
    if (LogPreviewMesh)
    {
        LogPreviewMesh->SetVisibility(bVisible);
    }
}

/**
 * @brief 创建预览材质
 * @details 创建半透明材质实例用于预览
 */
void ASG_RollingLogSpawner::CreatePreviewMaterial()
{
    if (!LogPreviewMesh)
    {
        return;
    }

    // 如果没有材质实例，创建一个
    if (!PreviewMaterialInstance)
    {
        // 获取默认材质
        UMaterialInterface* BaseMaterial = LogPreviewMesh->GetMaterial(0);
        if (BaseMaterial)
        {
            PreviewMaterialInstance = UMaterialInstanceDynamic::Create(BaseMaterial, this);
        }
    }

    // 应用材质并设置透明度
    if (PreviewMaterialInstance)
    {
        // 尝试设置透明度参数（如果材质支持）
        PreviewMaterialInstance->SetScalarParameterValue(TEXT("Opacity"), PreviewMeshOpacity);
        LogPreviewMesh->SetMaterial(0, PreviewMaterialInstance);
    }

    // 设置颜色（用于区分预览和实际物体）
    // 使用半透明的橙色
    LogPreviewMesh->SetVectorParameterValueOnMaterials(TEXT("Color"), FVector(1.0f, 0.5f, 0.0f));
}

/**
 * @brief 设置预览网格体
 */
void ASG_RollingLogSpawner::SetupPreviewMesh()
{
    if (!LogPreviewMesh)
    {
        return;
    }

    // 如果有配置的预览网格，使用它
    if (PreviewMesh)
    {
        LogPreviewMesh->SetStaticMesh(PreviewMesh);
    }

    // 更新变换
    UpdatePreviewMesh();
}

bool ASG_RollingLogSpawner::Activate(USG_RollingLogCardData* CardData, UAbilitySystemComponent* InSourceASC)
{
    UE_LOG(LogSGGameplay, Log, TEXT("========== 激活流木计生成器 =========="));
    UE_LOG(LogSGGameplay, Log, TEXT("  生成器：%s"), *GetName());

    if (CurrentState != ESGSpawnerState::Idle)
    {
        UE_LOG(LogSGGameplay, Warning, TEXT("  ⚠️ 生成器不在待机状态，无法激活"));
        return false;
    }

    if (!CardData)
    {
        UE_LOG(LogSGGameplay, Error, TEXT("  ❌ 卡牌数据为空"));
        return false;
    }

    ActiveCardData = CardData;
    SourceASC = InSourceASC;
    SpawnTimer = 0.0f;
    RemainingDuration = CardData->SpawnDuration;
    CurrentState = ESGSpawnerState::Active;

    SpawnRollingLogs();

    OnSpawnerActivated.Broadcast(this);
    K2_OnActivated();

    UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 生成器已激活"));
    UE_LOG(LogSGGameplay, Log, TEXT("    生成旋转：%s"), *GetSpawnRotation().ToString());
    UE_LOG(LogSGGameplay, Log, TEXT("    持续时间：%.1f 秒"), CardData->SpawnDuration);
    UE_LOG(LogSGGameplay, Log, TEXT("    生成间隔：%.1f 秒"), CardData->SpawnInterval);
    UE_LOG(LogSGGameplay, Log, TEXT("    每次数量：%d"), CardData->SpawnCountPerInterval);
    UE_LOG(LogSGGameplay, Log, TEXT("========================================"));

    return true;
}

void ASG_RollingLogSpawner::Deactivate()
{
    UE_LOG(LogSGGameplay, Log, TEXT("流木计生成器停止：%s"), *GetName());

    OnSpawnerDeactivated.Broadcast(this);
    K2_OnDeactivated();

    ActiveCardData = nullptr;
    SourceASC = nullptr;
    SpawnTimer = 0.0f;
    RemainingDuration = 0.0f;

    if (CooldownTime > 0.0f)
    {
        EnterCooldown();
    }
    else
    {
        CurrentState = ESGSpawnerState::Idle;
    }
}

void ASG_RollingLogSpawner::EnterCooldown()
{
    CurrentState = ESGSpawnerState::Cooldown;
    CooldownRemainingTime = CooldownTime;

    UE_LOG(LogSGGameplay, Log, TEXT("  进入冷却：%.1f 秒"), CooldownTime);
}

/**
 * @brief 生成滚木
 * @details
 * **🔧 修改：**
 * - 使用 GetSpawnRotation() 获取生成旋转
 */
void ASG_RollingLogSpawner::SpawnRollingLogs()
{
     if (!ActiveCardData)
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    // 确定滚木类
    TSubclassOf<ASG_RollingLog> RollingLogClassToSpawn = nullptr;
    
    if (ActiveCardData->RollingLogClass)
    {
        RollingLogClassToSpawn = TSubclassOf<ASG_RollingLog>(ActiveCardData->RollingLogClass);
    }
    else if (DefaultRollingLogClass)
    {
        RollingLogClassToSpawn = DefaultRollingLogClass;
    }

    if (!RollingLogClassToSpawn)
    {
        UE_LOG(LogSGGameplay, Error, TEXT("  ❌ 滚木类未配置！"));
        return;
    }

    // 获取滚动方向
    FVector RollDirection = GetRollDirection();
    
    // 获取生成旋转
    FRotator SpawnRotation;
    if (LogPreviewMesh)
    {
        SpawnRotation = LogPreviewMesh->GetComponentRotation();
        UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 使用 LogPreviewMesh 世界旋转：%s"), *SpawnRotation.ToString());
    }
    else
    {
        SpawnRotation = RollDirection.Rotation();
    }

    UE_LOG(LogSGGameplay, Log, TEXT("  ========== 生成滚木 =========="));
    UE_LOG(LogSGGameplay, Log, TEXT("    目标旋转：%s"), *SpawnRotation.ToString());

    // 生成滚木
    for (int32 i = 0; i < ActiveCardData->SpawnCountPerInterval; ++i)
    {
        FVector SpawnLocation = CalculateRandomSpawnLocation();

        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = this;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        // 生成滚木（使用默认旋转，稍后强制设置）
        ASG_RollingLog* NewLog = World->SpawnActor<ASG_RollingLog>(
            RollingLogClassToSpawn,
            SpawnLocation,
            FRotator::ZeroRotator,  // 🔧 先用零旋转生成
            SpawnParams
        );

        if (NewLog)
        {
            // 🔧 关键 - 使用 ForceSetRotation 强制设置旋转
            NewLog->ForceSetRotation(SpawnRotation);

            // 设置滚木参数
            NewLog->DamageAmount = ActiveCardData->DamageAmount;
            NewLog->DamageEffectClass = ActiveCardData->LogDamageEffectClass;
            NewLog->KnockbackDistance = ActiveCardData->KnockbackDistance;
            NewLog->KnockbackDuration = ActiveCardData->KnockbackDuration;
            NewLog->RollSpeed = ActiveCardData->RollSpeed;
            NewLog->MaxRollDistance = ActiveCardData->MaxRollDistance;
            NewLog->LogLifeSpan = ActiveCardData->LogLifeSpan;
            NewLog->RotationSpeed = ActiveCardData->RotationSpeed;

            // 初始化滚木，保持当前旋转
            NewLog->InitializeRollingLog(
                SourceASC,
                FactionTag,
                RollDirection,
                true
            );

            UE_LOG(LogSGGameplay, Log, TEXT("    [%d] 最终旋转：%s"), 
                i, *NewLog->GetActorRotation().ToString());

            NewLog->OnLogDestroyed.AddDynamic(this, &ASG_RollingLogSpawner::OnRollingLogDestroyed);
            SpawnedLogs.Add(NewLog);
            K2_OnLogSpawned(NewLog);
        }
    }
    
    UE_LOG(LogSGGameplay, Log, TEXT("  ================================"));
}

FVector ASG_RollingLogSpawner::CalculateRandomSpawnLocation() const
{
    FVector BaseLocation = GetActorLocation();

    FVector RollDirection = GetRollDirection();
    FVector RightVector = FVector::CrossProduct(RollDirection, FVector::UpVector);
    RightVector.Normalize();

    float RandomWidth = FMath::FRandRange(-SpawnAreaWidth * 0.5f, SpawnAreaWidth * 0.5f);

    float RandomOffset = 0.0f;
    if (ActiveCardData && ActiveCardData->SpawnRandomOffset > 0.0f)
    {
        RandomOffset = FMath::FRandRange(-ActiveCardData->SpawnRandomOffset, ActiveCardData->SpawnRandomOffset);
    }

    FVector SpawnLocation = BaseLocation;
    SpawnLocation += RightVector * RandomWidth;
    SpawnLocation += RollDirection * RandomOffset;
    SpawnLocation.Z += SpawnHeightOffset;

    return SpawnLocation;
}

void ASG_RollingLogSpawner::OnRollingLogDestroyed(ASG_RollingLog* DestroyedLog)
{
    SpawnedLogs.RemoveAll([DestroyedLog](const TWeakObjectPtr<ASG_RollingLog>& LogPtr)
    {
        return !LogPtr.IsValid() || LogPtr.Get() == DestroyedLog;
    });
}

void ASG_RollingLogSpawner::UpdateSpawnAreaVisualization()
{
    if (SpawnAreaBox)
    {
        SpawnAreaBox->SetBoxExtent(FVector(50.0f, SpawnAreaWidth * 0.5f, 50.0f));
    }
}

void ASG_RollingLogSpawner::SetupBillboard()
{
    if (!BillboardComponent)
    {
        return;
    }

    if (BillboardSprite)
    {
        BillboardComponent->SetSprite(BillboardSprite);
    }

    BillboardComponent->SetRelativeLocation(FVector(0.0f, 0.0f, BillboardHeightOffset));
}

void ASG_RollingLogSpawner::SetBillboardVisibility(bool bVisible)
{
    if (BillboardComponent)
    {
        BillboardComponent->SetVisibility(bVisible);
        BillboardComponent->SetHiddenInGame(!bVisible);
    }
}

void ASG_RollingLogSpawner::UpdateBillboardSprite(UTexture2D* NewSprite)
{
    if (BillboardComponent && NewSprite)
    {
        BillboardSprite = NewSprite;
        BillboardComponent->SetSprite(NewSprite);
    }
}
