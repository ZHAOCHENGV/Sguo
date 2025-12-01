// 📄 文件：Source/Sguo/Private/Actors/SG_RollingLogSpawner.cpp
// ✨ 新增 - 完整文件

#include "Actors/SG_RollingLogSpawner.h"
#include "Actors/SG_RollingLog.h"
#include "Data/SG_RollingLogCardData.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "AbilitySystemComponent.h"
#include "Debug/SG_LogCategories.h"
#include "Kismet/GameplayStatics.h"

ASG_RollingLogSpawner::ASG_RollingLogSpawner()
{
    // 启用 Tick
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
    // 箭头方向 = Actor 的 Forward 方向
    DirectionArrow->SetRelativeRotation(FRotator::ZeroRotator);

#if WITH_EDITORONLY_DATA
    // 仅在编辑器中显示
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

    UE_LOG(LogSGGameplay, Log, TEXT("流木计生成器初始化：%s"), *GetName());
    UE_LOG(LogSGGameplay, Log, TEXT("  位置：%s"), *GetActorLocation().ToString());
    UE_LOG(LogSGGameplay, Log, TEXT("  滚动方向：%s"), *GetRollDirection().ToString());
    UE_LOG(LogSGGameplay, Log, TEXT("  阵营：%s"), *FactionTag.ToString());
}

void ASG_RollingLogSpawner::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    switch (CurrentState)
    {
    case ESGSpawnerState::Active:
        {
            // 更新生成计时器
            SpawnTimer += DeltaTime;
            RemainingDuration -= DeltaTime;

            // 检查是否到达生成间隔
            if (ActiveCardData && SpawnTimer >= ActiveCardData->SpawnInterval)
            {
                SpawnTimer -= ActiveCardData->SpawnInterval;
                SpawnRollingLogs();
            }

            // 检查是否结束
            if (RemainingDuration <= 0.0f)
            {
                UE_LOG(LogSGGameplay, Log, TEXT("流木计生成器持续时间结束：%s"), *GetName());
                Deactivate();
            }
        }
        break;

    case ESGSpawnerState::Cooldown:
        {
            // 更新冷却计时
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
        // Idle 状态不做任何事
        break;
    }
}

#if WITH_EDITOR
void ASG_RollingLogSpawner::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    // 更新生成区域可视化
    UpdateSpawnAreaVisualization();
}
#endif

FVector ASG_RollingLogSpawner::GetRollDirection() const
{
    // 使用 Actor 的 Forward 方向作为滚动方向
    FVector Direction = GetActorForwardVector();
    Direction.Z = 0.0f;
    Direction.Normalize();
    return Direction;
}

bool ASG_RollingLogSpawner::Activate(USG_RollingLogCardData* CardData, UAbilitySystemComponent* InSourceASC)
{
    UE_LOG(LogSGGameplay, Log, TEXT("========== 激活流木计生成器 =========="));
    UE_LOG(LogSGGameplay, Log, TEXT("  生成器：%s"), *GetName());

    // 检查状态
    if (CurrentState != ESGSpawnerState::Idle)
    {
        UE_LOG(LogSGGameplay, Warning, TEXT("  ⚠️ 生成器不在待机状态，无法激活"));
        return false;
    }

    // 检查卡牌数据
    if (!CardData)
    {
        UE_LOG(LogSGGameplay, Error, TEXT("  ❌ 卡牌数据为空"));
        return false;
    }

    // 保存数据
    ActiveCardData = CardData;
    SourceASC = InSourceASC;

    // 初始化计时器
    SpawnTimer = 0.0f;
    RemainingDuration = CardData->SpawnDuration;

    // 设置状态
    CurrentState = ESGSpawnerState::Active;

    // 立即生成第一波
    SpawnRollingLogs();

    // 广播事件
    OnSpawnerActivated.Broadcast(this);
    K2_OnActivated();

    UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 生成器已激活"));
    UE_LOG(LogSGGameplay, Log, TEXT("    持续时间：%.1f 秒"), CardData->SpawnDuration);
    UE_LOG(LogSGGameplay, Log, TEXT("    生成间隔：%.1f 秒"), CardData->SpawnInterval);
    UE_LOG(LogSGGameplay, Log, TEXT("    每次数量：%d"), CardData->SpawnCountPerInterval);
    UE_LOG(LogSGGameplay, Log, TEXT("    伤害值：%.0f"), CardData->DamageAmount);
    UE_LOG(LogSGGameplay, Log, TEXT("========================================"));

    return true;
}

void ASG_RollingLogSpawner::Deactivate()
{
    UE_LOG(LogSGGameplay, Log, TEXT("流木计生成器停止：%s"), *GetName());

    // 广播事件
    OnSpawnerDeactivated.Broadcast(this);
    K2_OnDeactivated();

    // 清理数据
    ActiveCardData = nullptr;
    SourceASC = nullptr;
    SpawnTimer = 0.0f;
    RemainingDuration = 0.0f;

    // 进入冷却或待机
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
 * 功能说明：
 * - 从卡牌数据读取滚木类
 * - 在生成区域内随机位置生成滚木
 * - 设置滚木参数并初始化
 */
void ASG_RollingLogSpawner::SpawnRollingLogs()
{
   // 检查卡牌数据
    if (!ActiveCardData)
    {
        return;
    }

    // 获取世界
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    // ========== 确定滚木类 ==========
    // 🔧 修改 - 使用 RollingLogClassToSpawn 避免与全局 LogClass 冲突
    TSubclassOf<ASG_RollingLog> RollingLogClassToSpawn = nullptr;
    
    // 优先使用卡牌数据中的类
    if (ActiveCardData->RollingLogClass)
    {
        RollingLogClassToSpawn = TSubclassOf<ASG_RollingLog>(ActiveCardData->RollingLogClass);
    }
    // 备用：使用生成器默认类
    else if (DefaultRollingLogClass)
    {
        RollingLogClassToSpawn = DefaultRollingLogClass;
    }

    // 检查滚木类是否有效
    if (!RollingLogClassToSpawn)
    {
        UE_LOG(LogSGGameplay, Error, TEXT("  ❌ 滚木类未配置！"));
        return;
    }

    // ========== 获取滚动方向 ==========
    FVector RollDirection = GetRollDirection();
    FRotator SpawnRotation = RollDirection.Rotation();

    // ========== 生成滚木 ==========
    for (int32 i = 0; i < ActiveCardData->SpawnCountPerInterval; ++i)
    {
        // 计算随机生成位置
        FVector SpawnLocation = CalculateRandomSpawnLocation();

        // 生成参数
        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = this;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        // 生成滚木 Actor
        ASG_RollingLog* NewLog = World->SpawnActor<ASG_RollingLog>(
            RollingLogClassToSpawn,
            SpawnLocation,
            SpawnRotation,
            SpawnParams
        );

        if (NewLog)
        {
            // 从卡牌数据设置滚木参数
            NewLog->DamageAmount = ActiveCardData->DamageAmount;
            NewLog->DamageEffectClass = ActiveCardData->LogDamageEffectClass;
            NewLog->KnockbackDistance = ActiveCardData->KnockbackDistance;
            NewLog->KnockbackDuration = ActiveCardData->KnockbackDuration;
            NewLog->RollSpeed = ActiveCardData->RollSpeed;
            NewLog->MaxRollDistance = ActiveCardData->MaxRollDistance;
            NewLog->LogLifeSpan = ActiveCardData->LogLifeSpan;
            NewLog->RotationSpeed = ActiveCardData->RotationSpeed;

            // 初始化滚木
            NewLog->InitializeRollingLog(
                SourceASC,
                FactionTag,
                RollDirection
            );

            // 绑定销毁事件
            NewLog->OnLogDestroyed.AddDynamic(this, &ASG_RollingLogSpawner::OnRollingLogDestroyed);

            // 添加到列表
            SpawnedLogs.Add(NewLog);

            // 调用蓝图事件
            K2_OnLogSpawned(NewLog);

            UE_LOG(LogSGGameplay, Verbose, TEXT("  生成滚木：%s 位置：%s"), 
                *NewLog->GetName(), *SpawnLocation.ToString());
        }
        else
        {
            UE_LOG(LogSGGameplay, Error, TEXT("  ❌ 滚木生成失败！位置：%s"), *SpawnLocation.ToString());
        }
    }
}

FVector ASG_RollingLogSpawner::CalculateRandomSpawnLocation() const
{
    // 获取生成器位置
    FVector BaseLocation = GetActorLocation();

    // 计算垂直于滚动方向的向量（横向）
    FVector RollDirection = GetRollDirection();
    FVector RightVector = FVector::CrossProduct(RollDirection, FVector::UpVector);
    RightVector.Normalize();

    // 在宽度范围内随机
    float RandomWidth = FMath::FRandRange(-SpawnAreaWidth * 0.5f, SpawnAreaWidth * 0.5f);

    // 额外随机偏移（如果卡牌数据有配置）
    float RandomOffset = 0.0f;
    if (ActiveCardData && ActiveCardData->SpawnRandomOffset > 0.0f)
    {
        RandomOffset = FMath::FRandRange(-ActiveCardData->SpawnRandomOffset, ActiveCardData->SpawnRandomOffset);
    }

    // 计算最终位置
    FVector SpawnLocation = BaseLocation;
    SpawnLocation += RightVector * RandomWidth;
    SpawnLocation += RollDirection * RandomOffset;  // 沿滚动方向的随机偏移
    SpawnLocation.Z += SpawnHeightOffset;

    return SpawnLocation;
}

void ASG_RollingLogSpawner::OnRollingLogDestroyed(ASG_RollingLog* DestroyedLog)
{
    // 从列表中移除
    SpawnedLogs.RemoveAll([DestroyedLog](const TWeakObjectPtr<ASG_RollingLog>& LogPtr)
    {
        return !LogPtr.IsValid() || LogPtr.Get() == DestroyedLog;
    });
}

void ASG_RollingLogSpawner::UpdateSpawnAreaVisualization()
{
    if (SpawnAreaBox)
    {
        // 更新生成区域大小
        SpawnAreaBox->SetBoxExtent(FVector(50.0f, SpawnAreaWidth * 0.5f, 50.0f));
    }
}
