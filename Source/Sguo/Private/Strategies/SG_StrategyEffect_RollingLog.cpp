// 📄 文件：Source/Sguo/Private/Strategies/SG_StrategyEffect_RollingLog.cpp
// 🔧 修改 - 完整文件，添加方向计算和修复编译错误

#include "Strategies/SG_StrategyEffect_RollingLog.h"

#include "AbilitySystemGlobals.h"
#include "Actors/SG_RollingLog.h"
#include "Data/SG_StrategyCardData.h"
#include "Buildings/SG_MainCityBase.h"  // ✨ 新增 - 包含主城头文件
#include "Components/StaticMeshComponent.h"
#include "Components/DecalComponent.h"
#include "NiagaraComponent.h"
#include "Debug/SG_LogCategories.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

/**
 * @brief 构造函数
 */
ASG_StrategyEffect_RollingLog::ASG_StrategyEffect_RollingLog()
{
    // 启用 Tick
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    // 设置默认滚动方向（向左）
    CurrentRollDirectionType = DefaultRollDirectionType;
    RollDirection = FVector::ForwardVector;

    // ✨ 新增 - 初始化原始指针为 nullptr
    DirectionArrowMesh = nullptr;
    DirectionArrowMaterial = nullptr;
    AreaPreviewDecalMaterial = nullptr;
}

/**
 * @brief BeginPlay 生命周期函数
 */
void ASG_StrategyEffect_RollingLog::BeginPlay()
{
    Super::BeginPlay();

    // ✨ 新增 - 计算主城连线方向
    CalculateMainCityDirection();

    // 根据方向类型设置初始滚动方向
    UpdateRollDirectionFromType();

    // 创建预览组件
    CreatePreviewComponents();
}

/**
 * @brief Tick 函数
 * @param DeltaTime 帧间隔时间
 */
void ASG_StrategyEffect_RollingLog::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 如果正在执行，更新生成逻辑
    if (bIsExecuting)
    {
        // 累计时间
        ElapsedTime += DeltaTime;
        SpawnTimer += DeltaTime;

        // 检查是否到达生成间隔
        if (SpawnTimer >= SpawnConfig.SpawnInterval)
        {
            SpawnTimer -= SpawnConfig.SpawnInterval;
            SpawnRollingLogs();
        }

        // 检查是否结束
        if (ElapsedTime >= RollingLogDuration)
        {
            UE_LOG(LogSGGameplay, Log, TEXT("流木计效果持续时间结束"));
            
            // 调用蓝图事件
            K2_OnEffectEnded();
            
            // 结束效果
            EndEffect();
        }
    }

    // 如果在目标选择状态，更新预览
    if (CurrentState == ESGStrategyEffectState::WaitingForTarget)
    {
        UpdatePreviewDisplay();
    }
}

/**
 * @brief EndPlay 生命周期函数
 * @param EndPlayReason 结束原因
 */
void ASG_StrategyEffect_RollingLog::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 清理已生成的滚木
    CleanupSpawnedLogs();

    Super::EndPlay(EndPlayReason);
}

/**
 * @brief 检查是否需要目标选择
 * @return true - 需要玩家选择位置和方向
 */
bool ASG_StrategyEffect_RollingLog::RequiresTargetSelection_Implementation() const
{
    // 流木计需要玩家选择位置和方向
    return true;
}

/**
 * @brief 开始目标选择
 * @return 是否成功开始
 */
bool ASG_StrategyEffect_RollingLog::StartTargetSelection_Implementation()
{
    UE_LOG(LogSGGameplay, Log, TEXT("========== 流木计：开始目标选择 =========="));

    // 调用基类实现
    if (!Super::StartTargetSelection_Implementation())
    {
        return false;
    }

    // 显示预览
    ShowPreview();

    UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 预览已显示"));
    UE_LOG(LogSGGameplay, Log, TEXT("  当前方向类型：%s"), 
        CurrentRollDirectionType == ESGRollingLogDirection::Left ? TEXT("向左") :
        CurrentRollDirectionType == ESGRollingLogDirection::Right ? TEXT("向右") :
        CurrentRollDirectionType == ESGRollingLogDirection::Forward ? TEXT("向前") : TEXT("自定义"));
    UE_LOG(LogSGGameplay, Log, TEXT("========================================"));

    return true;
}

/**
 * @brief 更新目标位置
 * @param NewLocation 新的目标位置
 */
void ASG_StrategyEffect_RollingLog::UpdateTargetLocation_Implementation(const FVector& NewLocation)
{
    // 调用基类实现
    Super::UpdateTargetLocation_Implementation(NewLocation);

    // 更新预览位置
    UpdatePreviewDisplay();
}

/**
 * @brief 确认目标
 * @return 是否成功确认
 */
bool ASG_StrategyEffect_RollingLog::ConfirmTarget_Implementation()
{
    UE_LOG(LogSGGameplay, Log, TEXT("========== 流木计：确认目标 =========="));
    UE_LOG(LogSGGameplay, Log, TEXT("  位置：%s"), *TargetLocation.ToString());
    UE_LOG(LogSGGameplay, Log, TEXT("  方向：%s"), *RollDirection.ToString());
    UE_LOG(LogSGGameplay, Log, TEXT("  方向类型：%s"), 
        CurrentRollDirectionType == ESGRollingLogDirection::Left ? TEXT("向左") :
        CurrentRollDirectionType == ESGRollingLogDirection::Right ? TEXT("向右") :
        CurrentRollDirectionType == ESGRollingLogDirection::Forward ? TEXT("向前") : TEXT("自定义"));

    // 隐藏预览
    HidePreview();

    // 调用基类确认（会触发 ExecuteEffect）
    bool bResult = Super::ConfirmTarget_Implementation();

    UE_LOG(LogSGGameplay, Log, TEXT("========================================"));

    return bResult;
}

/**
 * @brief 取消效果
 */
void ASG_StrategyEffect_RollingLog::CancelEffect_Implementation()
{
    UE_LOG(LogSGGameplay, Log, TEXT("========== 流木计：取消 =========="));

    // 隐藏预览
    HidePreview();

    // 清理滚木
    CleanupSpawnedLogs();

    // 调用基类取消
    Super::CancelEffect_Implementation();

    UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
}

/**
 * @brief 执行效果
 */
void ASG_StrategyEffect_RollingLog::ExecuteEffect_Implementation()
{
    UE_LOG(LogSGGameplay, Log, TEXT("========== 流木计：开始执行 =========="));

    // 设置状态
    SetState(ESGStrategyEffectState::Executing);

    // 重置计时器
    ElapsedTime = 0.0f;
    SpawnTimer = 0.0f;

    // 标记正在执行
    bIsExecuting = true;

    // 立即生成第一批滚木
    SpawnRollingLogs();

    // 调用蓝图事件
    K2_OnEffectStarted();

    UE_LOG(LogSGGameplay, Log, TEXT("  效果持续时间：%.1f 秒"), RollingLogDuration);
    UE_LOG(LogSGGameplay, Log, TEXT("  生成间隔：%.1f 秒"), SpawnConfig.SpawnInterval);
    UE_LOG(LogSGGameplay, Log, TEXT("  每次生成数量：%d"), SpawnConfig.SpawnCountPerInterval);
    UE_LOG(LogSGGameplay, Log, TEXT("  滚动方向：%s"), *RollDirection.ToString());
    UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
}

// ==================== ✨ 新增 - 方向控制函数 ====================

/**
 * @brief 设置滚动方向类型
 * @param NewDirectionType 新的方向类型
 */
void ASG_StrategyEffect_RollingLog::SetRollDirectionType(ESGRollingLogDirection NewDirectionType)
{
    CurrentRollDirectionType = NewDirectionType;
    
    // 更新实际滚动方向
    UpdateRollDirectionFromType();
    
    // 调用蓝图事件
    K2_OnDirectionChanged(CurrentRollDirectionType, RollDirection);
    
    // 更新预览
    UpdatePreviewDisplay();

    UE_LOG(LogSGGameplay, Log, TEXT("流木计方向已更改：%s -> %s"),
        NewDirectionType == ESGRollingLogDirection::Left ? TEXT("向左") :
        NewDirectionType == ESGRollingLogDirection::Right ? TEXT("向右") :
        NewDirectionType == ESGRollingLogDirection::Forward ? TEXT("向前") : TEXT("自定义"),
        *RollDirection.ToString());
}

/**
 * @brief 设置自定义滚动方向
 * @param NewDirection 新的滚动方向
 */
void ASG_StrategyEffect_RollingLog::SetCustomRollDirection(FVector NewDirection)
{
    // 设置为自定义模式
    CurrentRollDirectionType = ESGRollingLogDirection::Custom;
    
    // 确保方向在水平面上
    NewDirection.Z = 0.0f;
    
    // 归一化
    if (!NewDirection.IsNearlyZero())
    {
        RollDirection = NewDirection.GetSafeNormal();
    }

    // 调用蓝图事件
    K2_OnDirectionChanged(CurrentRollDirectionType, RollDirection);
    
    // 更新预览
    UpdatePreviewDisplay();
}

/**
 * @brief 切换到下一个方向
 */
void ASG_StrategyEffect_RollingLog::CycleRollDirection()
{
    switch (CurrentRollDirectionType)
    {
    case ESGRollingLogDirection::Left:
        SetRollDirectionType(ESGRollingLogDirection::Right);
        break;
    case ESGRollingLogDirection::Right:
        SetRollDirectionType(ESGRollingLogDirection::Forward);
        break;
    case ESGRollingLogDirection::Forward:
        SetRollDirectionType(ESGRollingLogDirection::Left);
        break;
    case ESGRollingLogDirection::Custom:
        // 自定义模式切换到向左
        SetRollDirectionType(ESGRollingLogDirection::Left);
        break;
    }
}

/**
 * @brief 旋转滚动方向（仅自定义模式）
 * @param DeltaYaw 旋转角度（度）
 */
void ASG_StrategyEffect_RollingLog::RotateRollDirection(float DeltaYaw)
{
    // 切换到自定义模式
    CurrentRollDirectionType = ESGRollingLogDirection::Custom;
    
    // 绕 Z 轴旋转
    FRotator CurrentRotation = RollDirection.Rotation();
    CurrentRotation.Yaw += DeltaYaw;
    RollDirection = CurrentRotation.Vector();
    RollDirection.Z = 0.0f;
    RollDirection.Normalize();

    // 更新预览
    UpdatePreviewDisplay();
}

/**
 * @brief 计算主城连线方向
 */
void ASG_StrategyEffect_RollingLog::CalculateMainCityDirection()
{
    UE_LOG(LogSGGameplay, Log, TEXT("========== 计算主城连线方向 =========="));

    // 获取我方阵营标签
    FGameplayTag PlayerFactionTag = InstigatorFactionTag;
    if (!PlayerFactionTag.IsValid())
    {
        // 默认使用玩家阵营
        PlayerFactionTag = FGameplayTag::RequestGameplayTag(FName("Unit.Faction.Player"), false);
    }

    // 查找我方主城
    ASG_MainCityBase* PlayerMainCity = FindMainCityByFaction(PlayerFactionTag);
    
    // 查找敌方主城
    FGameplayTag EnemyFactionTag = FGameplayTag::RequestGameplayTag(FName("Unit.Faction.Enemy"), false);
    ASG_MainCityBase* EnemyMainCity = FindMainCityByFaction(EnemyFactionTag);

    if (PlayerMainCity && EnemyMainCity)
    {
        // 计算从我方主城到敌方主城的方向
        FVector PlayerLocation = PlayerMainCity->GetActorLocation();
        FVector EnemyLocation = EnemyMainCity->GetActorLocation();
        
        MainCityLineDirection = (EnemyLocation - PlayerLocation);
        MainCityLineDirection.Z = 0.0f;
        MainCityLineDirection.Normalize();
        
        // 计算右方向（用于左右滚动）
        MainCityLineRight = FVector::CrossProduct(FVector::UpVector, MainCityLineDirection);
        MainCityLineRight.Normalize();
        
        bMainCityDirectionCalculated = true;

        UE_LOG(LogSGGameplay, Log, TEXT("  我方主城：%s 位置：%s"), 
            *PlayerMainCity->GetName(), *PlayerLocation.ToString());
        UE_LOG(LogSGGameplay, Log, TEXT("  敌方主城：%s 位置：%s"), 
            *EnemyMainCity->GetName(), *EnemyLocation.ToString());
        UE_LOG(LogSGGameplay, Log, TEXT("  主城连线方向：%s"), *MainCityLineDirection.ToString());
        UE_LOG(LogSGGameplay, Log, TEXT("  右方向：%s"), *MainCityLineRight.ToString());
    }
    else
    {
        // 未找到主城，使用默认方向
        MainCityLineDirection = FVector::ForwardVector;
        MainCityLineRight = FVector::RightVector;
        bMainCityDirectionCalculated = false;

        UE_LOG(LogSGGameplay, Warning, TEXT("  ⚠️ 未找到主城，使用默认方向"));
        if (!PlayerMainCity)
        {
            UE_LOG(LogSGGameplay, Warning, TEXT("    我方主城未找到（阵营：%s）"), *PlayerFactionTag.ToString());
        }
        if (!EnemyMainCity)
        {
            UE_LOG(LogSGGameplay, Warning, TEXT("    敌方主城未找到"));
        }
    }

    UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
}

/**
 * @brief 根据方向类型计算实际滚动方向
 */
void ASG_StrategyEffect_RollingLog::UpdateRollDirectionFromType()
{
    switch (CurrentRollDirectionType)
    {
    case ESGRollingLogDirection::Left:
        // 向左 = 主城连线的负右方向
        RollDirection = -MainCityLineRight;
        break;
        
    case ESGRollingLogDirection::Right:
        // 向右 = 主城连线的右方向
        RollDirection = MainCityLineRight;
        break;
        
    case ESGRollingLogDirection::Forward:
        // 向前 = 主城连线方向
        RollDirection = MainCityLineDirection;
        break;
        
    case ESGRollingLogDirection::Custom:
        // 自定义模式保持当前方向不变
        break;
    }

    // 确保方向归一化
    if (!RollDirection.IsNearlyZero())
    {
        RollDirection.Normalize();
    }
}

/**
 * @brief 查找指定阵营的主城
 * @param FactionTag 阵营标签
 * @return 主城 Actor
 */
ASG_MainCityBase* ASG_StrategyEffect_RollingLog::FindMainCityByFaction(const FGameplayTag& FactionTag) const
{
    // 获取所有主城
    TArray<AActor*> AllMainCities;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASG_MainCityBase::StaticClass(), AllMainCities);

    // 查找匹配阵营的主城
    for (AActor* Actor : AllMainCities)
    {
        ASG_MainCityBase* MainCity = Cast<ASG_MainCityBase>(Actor);
        if (MainCity && MainCity->FactionTag.MatchesTag(FactionTag))
        {
            return MainCity;
        }
    }

    return nullptr;
}

// ==================== 预览相关函数 ====================

/**
 * @brief 创建预览组件
 */
void ASG_StrategyEffect_RollingLog::CreatePreviewComponents()
{
    // 创建方向箭头网格组件
    if (DirectionArrowMesh)
    {
        ArrowMeshComponent = NewObject<UStaticMeshComponent>(this, TEXT("ArrowMeshComponent"));
        ArrowMeshComponent->SetStaticMesh(DirectionArrowMesh);
        ArrowMeshComponent->SetupAttachment(RootComponent);
        ArrowMeshComponent->RegisterComponent();
        
        if (DirectionArrowMaterial)
        {
            ArrowMeshComponent->SetMaterial(0, DirectionArrowMaterial);
        }
        
        // 初始隐藏
        ArrowMeshComponent->SetVisibility(false);
    }

    // 创建区域预览贴花
    if (AreaPreviewDecalMaterial)
    {
        AreaDecalComponent = NewObject<UDecalComponent>(this, TEXT("AreaDecalComponent"));
        AreaDecalComponent->SetDecalMaterial(AreaPreviewDecalMaterial);
        AreaDecalComponent->SetupAttachment(RootComponent);
        AreaDecalComponent->RegisterComponent();
        
        // 设置贴花大小（对应生成区域）
        AreaDecalComponent->DecalSize = FVector(
            SpawnConfig.SpawnAreaLength * 0.5f,
            SpawnConfig.SpawnAreaWidth * 0.5f,
            100.0f  // 高度
        );
        
        // 贴花朝下
        AreaDecalComponent->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
        
        // 初始隐藏
        AreaDecalComponent->SetVisibility(false);
    }
}

/**
 * @brief 更新预览显示
 */
void ASG_StrategyEffect_RollingLog::UpdatePreviewDisplay()
{
    // 更新箭头位置和方向
    if (ArrowMeshComponent)
    {
        ArrowMeshComponent->SetWorldLocation(TargetLocation + FVector(0, 0, 50.0f));
        ArrowMeshComponent->SetWorldRotation(RollDirection.Rotation());
    }

    // 更新区域贴花
    if (AreaDecalComponent)
    {
        AreaDecalComponent->SetWorldLocation(TargetLocation);
        
        // 贴花旋转：朝向滚动方向
        FRotator DecalRotation = RollDirection.Rotation();
        DecalRotation.Pitch = -90.0f;  // 贴花朝下
        AreaDecalComponent->SetWorldRotation(DecalRotation);
    }
}

/**
 * @brief 隐藏预览
 */
void ASG_StrategyEffect_RollingLog::HidePreview()
{
    if (ArrowMeshComponent)
    {
        ArrowMeshComponent->SetVisibility(false);
    }

    if (AreaDecalComponent)
    {
        AreaDecalComponent->SetVisibility(false);
    }

    if (PreviewEffectComponent)
    {
        PreviewEffectComponent->SetVisibility(false);
    }
}

/**
 * @brief 显示预览
 */
void ASG_StrategyEffect_RollingLog::ShowPreview()
{
    if (ArrowMeshComponent)
    {
        ArrowMeshComponent->SetVisibility(true);
    }

    if (AreaDecalComponent)
    {
        AreaDecalComponent->SetVisibility(true);
    }

    if (PreviewEffectComponent)
    {
        PreviewEffectComponent->SetVisibility(true);
    }
}

// ==================== 滚木生成相关函数 ====================

/**
 * @brief 生成滚木
 */
void ASG_StrategyEffect_RollingLog::SpawnRollingLogs()
{
    // 检查滚木类是否配置
    if (!SpawnConfig.RollingLogClass)
    {
        UE_LOG(LogSGGameplay, Error, TEXT("流木计：滚木类未配置！"));
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    // 🔧 修改 - 从基类获取 ASC（使用正确的成员变量）
    // 获取施放者的 ASC
    UAbilitySystemComponent* SpawnerASC = nullptr;
    if (EffectInstigator)
    {
        SpawnerASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(EffectInstigator);
    }

    // 生成指定数量的滚木
    for (int32 i = 0; i < SpawnConfig.SpawnCountPerInterval; ++i)
    {
        // 计算随机生成位置
        FVector SpawnLocation = CalculateRandomSpawnLocation();

        // 生成参数
        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = this;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        // 生成旋转（朝向滚动方向）
        FRotator SpawnRotation = RollDirection.Rotation();

        // 生成滚木
        ASG_RollingLog* NewLog = World->SpawnActor<ASG_RollingLog>(
            SpawnConfig.RollingLogClass,
            SpawnLocation,
            SpawnRotation,
            SpawnParams
        );

        if (NewLog)
        {
            // 🔧 修改 - 使用正确的变量名
            // 初始化滚木
            NewLog->InitializeRollingLog(
                SpawnerASC,
                InstigatorFactionTag,
                RollDirection
            );

            // 绑定销毁事件
            NewLog->OnLogDestroyed.AddDynamic(this, &ASG_StrategyEffect_RollingLog::OnRollingLogDestroyed);

            // 添加到列表
            SpawnedLogs.Add(NewLog);

            // 调用蓝图事件
            K2_OnLogSpawned(NewLog);

            UE_LOG(LogSGGameplay, Verbose, TEXT("  生成滚木：%s 位置：%s"), 
                *NewLog->GetName(), *SpawnLocation.ToString());
        }
        else
        {
            UE_LOG(LogSGGameplay, Error, TEXT("  滚木生成失败！"));
        }
    }
}

/**
 * @brief 计算随机生成位置
 * @return 生成位置（世界坐标）
 */
FVector ASG_StrategyEffect_RollingLog::CalculateRandomSpawnLocation() const
{
    // 计算垂直于滚动方向的向量（横向）
    FVector RightVector = FVector::CrossProduct(RollDirection, FVector::UpVector);
    RightVector.Normalize();

    // 在生成区域内随机位置
    // 横向随机（宽度方向）
    float RandomWidth = FMath::FRandRange(
        -SpawnConfig.SpawnAreaWidth * 0.5f, 
        SpawnConfig.SpawnAreaWidth * 0.5f
    );
    
    // 纵向随机（长度方向，沿滚动反方向，即生成在目标位置后方）
    float RandomLength = FMath::FRandRange(
        0.0f, 
        SpawnConfig.SpawnAreaLength
    );

    // 计算最终位置
    FVector SpawnLocation = TargetLocation;
    SpawnLocation += RightVector * RandomWidth;           // 横向偏移
    SpawnLocation -= RollDirection * RandomLength;        // 纵向偏移（后方）
    SpawnLocation.Z += SpawnConfig.SpawnHeightOffset;     // 高度偏移

    return SpawnLocation;
}

/**
 * @brief 清理所有已生成的滚木
 */
void ASG_StrategyEffect_RollingLog::CleanupSpawnedLogs()
{
    for (TWeakObjectPtr<ASG_RollingLog>& LogPtr : SpawnedLogs)
    {
        if (LogPtr.IsValid())
        {
            // 解绑事件
            LogPtr->OnLogDestroyed.RemoveDynamic(this, &ASG_StrategyEffect_RollingLog::OnRollingLogDestroyed);
            
            // 销毁
            LogPtr->Destroy();
        }
    }

    SpawnedLogs.Empty();
}

/**
 * @brief 滚木销毁回调
 * @param DestroyedLog 被销毁的滚木
 */
void ASG_StrategyEffect_RollingLog::OnRollingLogDestroyed(ASG_RollingLog* DestroyedLog)
{
    // 从列表中移除
    SpawnedLogs.RemoveAll([DestroyedLog](const TWeakObjectPtr<ASG_RollingLog>& LogPtr)
    {
        return !LogPtr.IsValid() || LogPtr.Get() == DestroyedLog;
    });

    UE_LOG(LogSGGameplay, Verbose, TEXT("滚木销毁，剩余：%d"), SpawnedLogs.Num());
}
