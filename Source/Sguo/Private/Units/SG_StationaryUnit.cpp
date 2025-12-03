// 📄 文件：Source/Sguo/Private/Units/SG_StationaryUnit.cpp
// 🔧 修改 - 实现计谋技能执行系统
// ✅ 这是完整文件

#include "Units/SG_StationaryUnit.h"
#include "AI/SG_StationaryAIController.h"
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

    // 设置默认 AI 控制器类
    AIControllerClass = ASG_StationaryAIController::StaticClass();
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

    // ✨ 新增 - 启用 Tick
    PrimaryActorTick.bCanEverTick = true;
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

// ✨ 新增 - Tick 函数
void ASG_StationaryUnit::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 更新计谋技能
    if (StrategySkillState == ESGStrategySkillState::Executing)
    {
        UpdateStrategySkill(DeltaTime);
    }
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
}

// ========== ✨ 新增 - 计谋技能系统实现 ==========

/**
 * @brief 开始执行计谋技能
 */
void ASG_StationaryUnit::StartStrategySkill(
    const FVector& TargetLocation,
    float AreaRadius,
    float Duration,
    float FireInterval,
    int32 ArrowsPerRound,
    TSubclassOf<AActor> ProjectileClass,
    UAnimMontage* AttackMontage)
{
    UE_LOG(LogSGUnit, Log, TEXT("[站桩单位] %s 开始计谋技能"), *GetName());
    UE_LOG(LogSGUnit, Log, TEXT("  目标位置: %s"), *TargetLocation.ToString());
    UE_LOG(LogSGUnit, Log, TEXT("  区域半径: %.0f"), AreaRadius);
    UE_LOG(LogSGUnit, Log, TEXT("  持续时间: %.1f 秒"), Duration);
    UE_LOG(LogSGUnit, Log, TEXT("  射击间隔: %.2f 秒"), FireInterval);
    UE_LOG(LogSGUnit, Log, TEXT("  每轮数量: %d"), ArrowsPerRound);

    // 打断当前普通攻击
    if (bIsAttacking)
    {
        if (USkeletalMeshComponent* MeshComp = GetMesh())
        {
            if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
            {
                AnimInstance->Montage_Stop(0.1f);
            }
        }
        bIsAttacking = false;
    }

    // 设置计谋技能参数
    StrategySkillState = ESGStrategySkillState::Executing;
    StrategySkillRemainingTime = Duration;
    StrategySkillFireTimer = 0.0f;  // 立即开始第一次射击
    CurrentFireInterval = FireInterval;
    StrategySkillTargetLocation = TargetLocation;
    StrategySkillAreaRadius = AreaRadius;
    StrategySkillArrowsPerRound = ArrowsPerRound;

    // 设置投射物类（优先使用传入的，其次使用 DataTable 配置）
    if (ProjectileClass)
    {
        CurrentProjectileClass = ProjectileClass;
    }
    else
    {
        CurrentProjectileClass = GetDataTableProjectileClass();
    }

    // 设置攻击蒙太奇（优先使用传入的，其次使用 DataTable 配置）
    if (AttackMontage)
    {
        CurrentAttackMontage = AttackMontage;
    }
    else
    {
        CurrentAttackMontage = GetDataTableAttackMontage();
    }

    UE_LOG(LogSGUnit, Log, TEXT("  投射物类: %s"), 
        CurrentProjectileClass ? *CurrentProjectileClass->GetName() : TEXT("默认"));
    UE_LOG(LogSGUnit, Log, TEXT("  攻击蒙太奇: %s"), 
        CurrentAttackMontage ? *CurrentAttackMontage->GetName() : TEXT("无"));

    // 兼容旧代码
    bIsExecutingFireArrow = true;
}

/**
 * @brief 停止计谋技能
 */
void ASG_StationaryUnit::StopStrategySkill()
{
    UE_LOG(LogSGUnit, Log, TEXT("[站桩单位] %s 停止计谋技能"), *GetName());

    StrategySkillState = ESGStrategySkillState::None;
    StrategySkillRemainingTime = 0.0f;
    StrategySkillFireTimer = 0.0f;
    CurrentFireInterval = 0.0f;
    StrategySkillTargetLocation = FVector::ZeroVector;
    StrategySkillAreaRadius = 0.0f;
    StrategySkillArrowsPerRound = 1;
    CurrentProjectileClass = nullptr;
    CurrentAttackMontage = nullptr;

    // 兼容旧代码
    bIsExecutingFireArrow = false;
}

/**
 * @brief 更新计谋技能逻辑
 * @param DeltaTime 帧间隔
 */
void ASG_StationaryUnit::UpdateStrategySkill(float DeltaTime)
{
    // 更新剩余时间
    StrategySkillRemainingTime -= DeltaTime;

    // 检查是否结束
    if (StrategySkillRemainingTime <= 0.0f)
    {
        StopStrategySkill();
        return;
    }

    // 更新射击计时器
    StrategySkillFireTimer += DeltaTime;

    // 检查是否到达射击间隔
    if (StrategySkillFireTimer >= CurrentFireInterval)
    {
        StrategySkillFireTimer = 0.0f;
        ExecuteStrategyFire();
    }
}

/**
 * @brief 执行一次计谋技能射击
 */
void ASG_StationaryUnit::ExecuteStrategyFire()
{
    if (StrategySkillState != ESGStrategySkillState::Executing)
    {
        return;
    }

    UE_LOG(LogSGUnit, Verbose, TEXT("[站桩单位] %s 执行计谋射击 x%d"), 
        *GetName(), StrategySkillArrowsPerRound);

    // ========== 播放攻击蒙太奇 ==========
    if (CurrentAttackMontage)
    {
        if (USkeletalMeshComponent* MeshComp = GetMesh())
        {
            if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
            {
                // 🔧 核心：根据射击间隔计算播放速率
                // 播放速率 = 蒙太奇时长 / 射击间隔
                // 确保蒙太奇在下一次射击前播放完毕
                float MontageLength = CurrentAttackMontage->GetPlayLength();
                float PlayRate = 1.0f;
                
                if (CurrentFireInterval > 0.0f && MontageLength > 0.0f)
                {
                    // 计算需要的播放速率
                    // 如果蒙太奇时长大于射击间隔，需要加速播放
                    PlayRate = MontageLength / CurrentFireInterval;
                    
                    // 限制最大播放速率，避免动画过快
                    PlayRate = FMath::Clamp(PlayRate, 0.5f, 5.0f);
                    
                    // 同时考虑攻击速度属性
                    if (AttributeSet)
                    {
                        PlayRate *= AttributeSet->GetAttackSpeed();
                    }
                }

                AnimInstance->Montage_Play(CurrentAttackMontage, PlayRate);

                UE_LOG(LogSGUnit, Verbose, TEXT("  播放蒙太奇: %s (速率: %.2f, 间隔: %.2f, 时长: %.2f)"),
                    *CurrentAttackMontage->GetName(),
                    PlayRate,
                    CurrentFireInterval,
                    MontageLength);
            }
        }
    }

    // ========== 发射投射物 ==========
    for (int32 i = 0; i < StrategySkillArrowsPerRound; ++i)
    {
        // 在区域内随机位置
        FVector RandomOffset = FVector(
            FMath::FRandRange(-StrategySkillAreaRadius, StrategySkillAreaRadius),
            FMath::FRandRange(-StrategySkillAreaRadius, StrategySkillAreaRadius),
            0.0f
        );
        
        // 确保随机点在圆形区域内
        while (RandomOffset.Size2D() > StrategySkillAreaRadius)
        {
            RandomOffset = FVector(
                FMath::FRandRange(-StrategySkillAreaRadius, StrategySkillAreaRadius),
                FMath::FRandRange(-StrategySkillAreaRadius, StrategySkillAreaRadius),
                0.0f
            );
        }

        FVector TargetPos = StrategySkillTargetLocation + RandomOffset;

        // 发射火矢
        FireArrow(TargetPos, CurrentProjectileClass);
    }
}

/**
 * @brief 获取 DataTable 中配置的攻击蒙太奇
 */
UAnimMontage* ASG_StationaryUnit::GetDataTableAttackMontage(int32 AbilityIndex) const
{
    if (CachedAttackAbilities.IsValidIndex(AbilityIndex))
    {
        return CachedAttackAbilities[AbilityIndex].Montage;
    }
    return nullptr;
}

/**
 * @brief 获取 DataTable 中配置的投射物类
 */
TSubclassOf<AActor> ASG_StationaryUnit::GetDataTableProjectileClass(int32 AbilityIndex) const
{
    if (CachedAttackAbilities.IsValidIndex(AbilityIndex))
    {
        return CachedAttackAbilities[AbilityIndex].ProjectileClass;
    }
    return nullptr;
}

// ========== 旧版火矢接口（保持兼容） ==========

void ASG_StationaryUnit::StartFireArrowSkill()
{
    UE_LOG(LogSGUnit, Log, TEXT("[站桩单位] %s 开始火矢技能（旧接口）"), *GetName());

    if (bIsAttacking)
    {
        if (USkeletalMeshComponent* MeshComp = GetMesh())
        {
            if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
            {
                AnimInstance->Montage_Stop(0.2f);
            }
        }
        bIsAttacking = false;
    }

    bIsExecutingFireArrow = true;

    if (CachedAttackAbilities.Num() > 0)
    {
        CachedOriginalProjectileClass = CachedAttackAbilities[CurrentAttackIndex].ProjectileClass;
    }
}

void ASG_StationaryUnit::EndFireArrowSkill()
{
    UE_LOG(LogSGUnit, Log, TEXT("[站桩单位] %s 结束火矢技能（旧接口）"), *GetName());

    bIsExecutingFireArrow = false;

    if (CachedOriginalProjectileClass && CachedAttackAbilities.Num() > 0)
    {
        CachedAttackAbilities[CurrentAttackIndex].ProjectileClass = CachedOriginalProjectileClass;
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
        UAbilitySystemComponent* MyASC = GetAbilitySystemComponent();

        Projectile->InitializeProjectileToLocation(
            MyASC,
            FactionTag,
            TargetLocation,
            -1.0f
        );

        Projectile->TargetMode = ESGProjectileTargetMode::TargetLocation;
    }

    return SpawnedActor;
}

TSubclassOf<AActor> ASG_StationaryUnit::GetFireArrowProjectileClass() const
{
    if (FireArrowProjectileClass)
    {
        return FireArrowProjectileClass;
    }

    if (CachedAttackAbilities.Num() > 0 && CachedAttackAbilities[CurrentAttackIndex].ProjectileClass)
    {
        return CachedAttackAbilities[CurrentAttackIndex].ProjectileClass;
    }

    return ASG_Projectile::StaticClass();
}
