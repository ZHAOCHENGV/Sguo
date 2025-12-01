// 📄 文件：Source/Sguo/Private/Actors/SG_RollingLog.cpp
// 🔧 修改 - 修复击退方向，简化胶囊体配置

#include "Actors/SG_RollingLog.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/AudioComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameplayEffect.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Units/SG_UnitsBase.h"
#include "AbilitySystem/SG_AttributeSet.h"
#include "Debug/SG_LogCategories.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "DrawDebugHelpers.h"

/**
 * @brief 构造函数
 * 
 * @details
 * **组件结构：**
 * - MeshComponent（根组件）：启用物理，进行真实滚动
 * - CollisionCapsule（附着）：仅 Overlap 检测敌人
 * 
 * **🔧 修改：**
 * - CollisionCapsule 设置为可在蓝图中编辑变换
 * - 不再硬编码胶囊体尺寸
 */
ASG_RollingLog::ASG_RollingLog()
{
    PrimaryActorTick.bCanEverTick = true;

    // ========== 网格体作为根组件 ==========
    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
    RootComponent = MeshComponent;
    
    // 网格体碰撞设置 - 用于物理滚动
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    MeshComponent->SetCollisionObjectType(ECC_PhysicsBody);
    MeshComponent->SetCollisionResponseToAllChannels(ECR_Block);
    MeshComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);  // 忽略 Pawn
    MeshComponent->SetNotifyRigidBodyCollision(true);
    MeshComponent->SetSimulatePhysics(false);  // 默认禁用，InitializeRollingLog 中启用

    // ========== 🔧 修改 - 胶囊体可在蓝图中自由调整 ==========
    CollisionCapsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CollisionCapsule"));
    CollisionCapsule->SetupAttachment(RootComponent);
    
    // 🔧 修改 - 设置默认尺寸（可在蓝图中覆盖）
    CollisionCapsule->SetCapsuleRadius(50.0f);
    CollisionCapsule->SetCapsuleHalfHeight(130.0f);
    
    // 🔧 关键 - 胶囊体只用于 Overlap 检测
    CollisionCapsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    CollisionCapsule->SetCollisionObjectType(ECC_WorldDynamic);
    CollisionCapsule->SetCollisionResponseToAllChannels(ECR_Ignore);
    CollisionCapsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    CollisionCapsule->SetGenerateOverlapEvents(true);
    
    // 🔧 修改 - 允许在蓝图中隐藏时仍然进行碰撞检测
    CollisionCapsule->SetHiddenInGame(false);
    
    // 绑定 Overlap 事件
    CollisionCapsule->OnComponentBeginOverlap.AddDynamic(this, &ASG_RollingLog::OnCapsuleOverlap);

    // 初始化指针
    BreakParticleSystem = nullptr;
    RollDustParticleSystem = nullptr;
    BreakSound = nullptr;
    RollSound = nullptr;
    SourceASC = nullptr;

    bReplicates = true;
}

/**
 * @brief BeginPlay 生命周期函数
 */
void ASG_RollingLog::BeginPlay()
{
    Super::BeginPlay();

    StartLocation = GetActorLocation();
    SetLifeSpan(LogLifeSpan);

    // 🔧 修改 - 不再在这里设置胶囊体尺寸，使用蓝图中配置的尺寸

    StartRollingEffects();

    // 输出胶囊体信息（调试用）
    if (CollisionCapsule)
    {
        UE_LOG(LogSGGameplay, Log, TEXT("========== 滚木生成 =========="));
        UE_LOG(LogSGGameplay, Log, TEXT("  名称：%s"), *GetName());
        UE_LOG(LogSGGameplay, Log, TEXT("  位置：%s"), *StartLocation.ToString());
        UE_LOG(LogSGGameplay, Log, TEXT("  检测胶囊体尺寸：半径=%.0f, 半长=%.0f"), 
            CollisionCapsule->GetScaledCapsuleRadius(),
            CollisionCapsule->GetScaledCapsuleHalfHeight());
        UE_LOG(LogSGGameplay, Log, TEXT("  检测胶囊体相对位置：%s"), 
            *CollisionCapsule->GetRelativeLocation().ToString());
        UE_LOG(LogSGGameplay, Log, TEXT("  检测胶囊体相对旋转：%s"), 
            *CollisionCapsule->GetRelativeRotation().ToString());
        UE_LOG(LogSGGameplay, Log, TEXT("  物理模式：%s（等待初始化）"), 
            bEnablePhysicsRolling ? TEXT("启用") : TEXT("禁用"));
        UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
    }
}

/**
 * @brief Tick 函数
 */
void ASG_RollingLog::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!bIsInitialized || bIsDestroying)
    {
        return;
    }

    if (bEnablePhysicsRolling)
    {
        UpdatePhysicsRolling(DeltaTime);
    }
    else
    {
        UpdateRolling(DeltaTime);
        UpdateVisualRotation(DeltaTime);
    }

    // 更新已滚动距离
    RolledDistance = FVector::Dist(StartLocation, GetActorLocation());

    // 检查最大距离
    if (RolledDistance >= MaxRollDistance)
    {
        UE_LOG(LogSGGameplay, Log, TEXT("滚木超出最大距离（%.0f >= %.0f），销毁：%s"), 
            RolledDistance, MaxRollDistance, *GetName());
        K2_OnLogOutOfRange();
        BreakAndDestroy();
    }

    DrawDebugInfo();
}

/**
 * @brief EndPlay 生命周期函数
 */
void ASG_RollingLog::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopRollingEffects();
    OnLogDestroyed.Broadcast(this);
    Super::EndPlay(EndPlayReason);
}


/**
 * @brief 设置滚动方向
 */
void ASG_RollingLog::SetRollDirection(FVector NewDirection)
{
    NewDirection.Z = 0.0f;
    RollDirection = NewDirection.GetSafeNormal();
    if (!RollDirection.IsNearlyZero())
    {
        SetActorRotation(RollDirection.Rotation());
    }
}

/**
 * @brief 获取当前速度
 */
FVector ASG_RollingLog::GetCurrentVelocity() const
{
    if (bEnablePhysicsRolling && MeshComponent)
    {
        return MeshComponent->GetPhysicsLinearVelocity();
    }
    return RollDirection * RollSpeed;
}

/**
 * @brief 获取当前速度大小
 */
float ASG_RollingLog::GetCurrentSpeed() const
{
    return GetCurrentVelocity().Size();
}

/**
 * @brief 手动销毁滚木
 */
void ASG_RollingLog::BreakAndDestroy()
{
    if (bIsDestroying)
    {
        return;
    }

    bIsDestroying = true;

    UE_LOG(LogSGGameplay, Log, TEXT("🔥 滚木破碎：%s"), *GetName());

    PlayBreakEffects();
    K2_OnLogBreak(GetActorLocation());
    SetLifeSpan(0.5f);
}

/**
 * @brief 设置物理参数
 */
void ASG_RollingLog::SetupPhysics()
{
    if (!MeshComponent)
    {
        UE_LOG(LogSGGameplay, Error, TEXT("SetupPhysics 失败：MeshComponent 为空"));
        return;
    }

    UE_LOG(LogSGGameplay, Log, TEXT("  设置物理参数..."));

    MeshComponent->SetSimulatePhysics(true);
    MeshComponent->SetMassOverrideInKg(NAME_None, LogMass, true);
    MeshComponent->SetLinearDamping(LinearDamping);
    MeshComponent->SetAngularDamping(AngularDamping);
    MeshComponent->BodyInstance.bUseCCD = true;

    // 只锁定 Z 旋转，防止原地打转
    MeshComponent->BodyInstance.bLockXRotation = false;
    MeshComponent->BodyInstance.bLockYRotation = false;
    MeshComponent->BodyInstance.bLockZRotation = true;

    UE_LOG(LogSGGameplay, Log, TEXT("    质量：%.1f kg"), LogMass);
    UE_LOG(LogSGGameplay, Log, TEXT("    线性阻尼：%.2f"), LinearDamping);
    UE_LOG(LogSGGameplay, Log, TEXT("    角阻尼：%.2f"), AngularDamping);
}

/**
 * @brief 施加初始速度
 */
void ASG_RollingLog::ApplyInitialVelocity()
{
    if (!MeshComponent || !bEnablePhysicsRolling)
    {
        return;
    }

    if (!MeshComponent->IsSimulatingPhysics())
    {
        UE_LOG(LogSGGameplay, Error, TEXT("ApplyInitialVelocity 失败：物理模拟未启用"));
        return;
    }

    // 设置线性速度
    FVector LinearVelocity = RollDirection * InitialRollSpeed;
    MeshComponent->SetPhysicsLinearVelocity(LinearVelocity);

    // 设置角速度（绕垂直于滚动方向的水平轴旋转）
    FVector RotationAxis = FVector::CrossProduct(FVector::UpVector, RollDirection);
    RotationAxis.Normalize();
    
    float AngularSpeedRadians = FMath::DegreesToRadians(InitialAngularSpeed);
    FVector AngularVelocity = -RotationAxis * AngularSpeedRadians;
    
    MeshComponent->SetPhysicsAngularVelocityInRadians(AngularVelocity);

    UE_LOG(LogSGGameplay, Log, TEXT("  施加初始速度："));
    UE_LOG(LogSGGameplay, Log, TEXT("    线性速度：%s (%.0f cm/s)"), *LinearVelocity.ToString(), LinearVelocity.Size());
    UE_LOG(LogSGGameplay, Log, TEXT("    角速度：%.0f deg/s"), InitialAngularSpeed);
    
    // 验证
    FVector ActualVel = MeshComponent->GetPhysicsLinearVelocity();
    UE_LOG(LogSGGameplay, Log, TEXT("    实际速度：%s (%.0f cm/s)"), *ActualVel.ToString(), ActualVel.Size());
}

/**
 * @brief 更新物理滚动状态
 */
void ASG_RollingLog::UpdatePhysicsRolling(float DeltaTime)
{
    if (PhysicsWarmupTimer > 0.0f)
    {
        PhysicsWarmupTimer -= DeltaTime;
        return;
    }

    float Speed = GetCurrentSpeed();

    if (Speed < MinVelocityThreshold)
    {
        UE_LOG(LogSGGameplay, Log, TEXT("滚木速度过低（%.1f < %.1f），销毁：%s"), 
            Speed, MinVelocityThreshold, *GetName());
        BreakAndDestroy();
    }
}

/**
 * @brief 更新滚动位置（非物理模式）
 */
void ASG_RollingLog::UpdateRolling(float DeltaTime)
{
    float MoveDistance = RollSpeed * DeltaTime;
    FVector CurrentLocation = GetActorLocation();
    FVector NewLocation = CurrentLocation + RollDirection * MoveDistance;
    SetActorLocation(NewLocation);
}

/**
 * @brief 更新视觉旋转（非物理模式）
 */
void ASG_RollingLog::UpdateVisualRotation(float DeltaTime)
{
    if (bEnablePhysicsRolling || !MeshComponent)
    {
        return;
    }

    float RotationThisFrame = RotationSpeed * DeltaTime;
    FRotator CurrentRotation = GetActorRotation();
    CurrentRotation.Roll += RotationThisFrame;
    SetActorRotation(CurrentRotation);
}

/**
 * @brief Overlap 碰撞检测回调
 */
void ASG_RollingLog::OnCapsuleOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult
)
{
    // 检查状态
    if (bIsDestroying || !bIsInitialized || bHasHitTarget)
    {
        return;
    }

    // 检查 Actor
    if (!OtherActor || OtherActor == this)
    {
        return;
    }

    // 检查是否是单位
    ASG_UnitsBase* OtherUnit = Cast<ASG_UnitsBase>(OtherActor);
    if (!OtherUnit)
    {
        return;
    }

    // 检查阵营
    if (OtherUnit->FactionTag == SourceFactionTag)
    {
        return;
    }

    // 检查是否已死亡
    if (OtherUnit->bIsDead)
    {
        return;
    }

    UE_LOG(LogSGGameplay, Log, TEXT("🎯 滚木 %s 碰撞到敌方单位：%s"), 
        *GetName(), *OtherActor->GetName());

    FVector HitLocation = SweepResult.ImpactPoint.IsNearlyZero() ? 
        OtherActor->GetActorLocation() : FVector(SweepResult.ImpactPoint);
    
    HandleHitTarget(OtherActor, HitLocation);
}

/**
 * @brief 处理击中目标
 * @details
 * **🔧 修改：**
 * - 击退方向使用 RollDirection（滚动方向），而非速度方向
 * - 击中后立即破碎
 */
void ASG_RollingLog::HandleHitTarget(AActor* HitActor, const FVector& HitLocation)
{
    UE_LOG(LogSGGameplay, Log, TEXT("========== 滚木击中目标 =========="));
    UE_LOG(LogSGGameplay, Log, TEXT("  滚木：%s"), *GetName());
    UE_LOG(LogSGGameplay, Log, TEXT("  目标：%s"), *HitActor->GetName());
    UE_LOG(LogSGGameplay, Log, TEXT("  击中位置：%s"), *HitLocation.ToString());

    // 立即标记，防止多次触发
    bHasHitTarget = true;

    // 🔧 关键修改 - 击退方向使用滚动方向
    // 确保方向在水平面上
    FVector KnockbackDir = RollDirection;
    KnockbackDir.Z = 0.0f;
    KnockbackDir.Normalize();
    
    UE_LOG(LogSGGameplay, Log, TEXT("  滚动方向：%s"), *RollDirection.ToString());
    UE_LOG(LogSGGameplay, Log, TEXT("  击退方向：%s"), *KnockbackDir.ToString());

    // 构建击中信息
    FSGRollingLogHitInfo HitInfo;
    HitInfo.HitActor = HitActor;
    HitInfo.HitLocation = HitLocation;
    HitInfo.KnockbackDirection = KnockbackDir;
    HitInfo.RollDirection = RollDirection;

    // 应用伤害
    bool bDamageApplied = ApplyDamageToTarget(HitActor);
    UE_LOG(LogSGGameplay, Log, TEXT("  伤害应用：%s（%.0f）"), 
        bDamageApplied ? TEXT("成功") : TEXT("失败"), DamageAmount);

    // 应用击退
    ApplyKnockbackToTarget(HitActor, KnockbackDir);

    // 广播事件
    OnLogHitTarget.Broadcast(HitInfo);
    K2_OnHitTarget(HitInfo);

    UE_LOG(LogSGGameplay, Log, TEXT("========================================"));

    // 击中后立即破碎
    BreakAndDestroy();
}

/**
 * @brief 应用伤害到目标
 */
bool ASG_RollingLog::ApplyDamageToTarget(AActor* Target)
{
    if (!Target)
    {
        return false;
    }

    UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
    ASG_UnitsBase* TargetUnit = Cast<ASG_UnitsBase>(Target);
    
    // 方式1：使用 GE
    if (DamageEffectClass && TargetASC)
    {
        UAbilitySystemComponent* EffectSourceASC = SourceASC ? SourceASC : TargetASC;
        
        FGameplayEffectContextHandle EffectContext = EffectSourceASC->MakeEffectContext();
        EffectContext.AddInstigator(GetOwner(), this);

        FGameplayEffectSpecHandle SpecHandle = EffectSourceASC->MakeOutgoingSpec(DamageEffectClass, 1.0f, EffectContext);
        if (!SpecHandle.IsValid())
        {
            return false;
        }

        FGameplayTag DamageTag = FGameplayTag::RequestGameplayTag(FName("Data.Damage"), false);
        if (DamageTag.IsValid())
        {
            SpecHandle.Data->SetSetByCallerMagnitude(DamageTag, DamageAmount);
        }

        FActiveGameplayEffectHandle ActiveHandle = EffectSourceASC->ApplyGameplayEffectSpecToTarget(
            *SpecHandle.Data.Get(), 
            TargetASC
        );

        if (ActiveHandle.IsValid())
        {
            return true;
        }
    }

    // 方式2：直接修改属性
    if (TargetUnit && TargetUnit->AttributeSet)
    {
        float CurrentHealth = TargetUnit->AttributeSet->GetHealth();
        float NewHealth = FMath::Max(0.0f, CurrentHealth - DamageAmount);
        TargetUnit->AttributeSet->SetHealth(NewHealth);
        return true;
    }

    return false;
}

/**
 * @brief 应用击退效果
 * @param Target 目标 Actor
 * @param KnockbackDir 击退方向（已确保是滚动方向）
 * @details
 * **🔧 修改：**
 * - 击退方向直接使用传入的 KnockbackDir（已经是 RollDirection）
 * - 添加可配置的向上分量
 */
void ASG_RollingLog::ApplyKnockbackToTarget(AActor* Target, const FVector& KnockbackDir)
{
    if (!Target)
    {
        return;
    }

    ASG_UnitsBase* TargetUnit = Cast<ASG_UnitsBase>(Target);
    if (!TargetUnit)
    {
        return;
    }

    UCharacterMovementComponent* MovementComp = TargetUnit->GetCharacterMovement();
    if (!MovementComp)
    {
        UE_LOG(LogSGGameplay, Warning, TEXT("ApplyKnockbackToTarget：目标没有移动组件"));
        return;
    }

    // 🔧 修改 - 计算击退速度
    // 水平方向速度 = 击退距离 / 击退时间
    float HorizontalSpeed = KnockbackDistance / KnockbackDuration;
    
    // 构建击退速度向量
    FVector LaunchVelocity = KnockbackDir * HorizontalSpeed;
    LaunchVelocity.Z = KnockbackUpwardForce;  // 🔧 修改 - 使用可配置的向上分量

    // 执行击退
    TargetUnit->LaunchCharacter(LaunchVelocity, true, true);

    UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 击退应用成功"));
    UE_LOG(LogSGGameplay, Log, TEXT("    击退方向：%s"), *KnockbackDir.ToString());
    UE_LOG(LogSGGameplay, Log, TEXT("    水平速度：%.0f cm/s"), HorizontalSpeed);
    UE_LOG(LogSGGameplay, Log, TEXT("    向上分量：%.0f cm/s"), KnockbackUpwardForce);
    UE_LOG(LogSGGameplay, Log, TEXT("    最终速度：%s"), *LaunchVelocity.ToString());
}

/**
 * @brief 播放破碎特效
 */
void ASG_RollingLog::PlayBreakEffects()
{
    FVector BreakLocation = GetActorLocation();

    if (BreakParticleSystem)
    {
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            GetWorld(),
            BreakParticleSystem,
            BreakLocation,
            GetActorRotation()
        );
    }

    if (BreakSound)
    {
        UGameplayStatics::PlaySoundAtLocation(this, BreakSound, BreakLocation);
    }

    // 停止物理
    if (MeshComponent)
    {
        MeshComponent->SetSimulatePhysics(false);
        MeshComponent->SetVisibility(false);
        MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    // 禁用检测
    if (CollisionCapsule)
    {
        CollisionCapsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
}

/**
 * @brief 启动滚动特效
 */
void ASG_RollingLog::StartRollingEffects()
{
    if (RollDustParticleSystem)
    {
        DustEffectComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
            RollDustParticleSystem,
            RootComponent,
            NAME_None,
            FVector::ZeroVector,
            FRotator::ZeroRotator,
            EAttachLocation::KeepRelativeOffset,
            true
        );
    }

    if (RollSound)
    {
        RollAudioComponent = UGameplayStatics::SpawnSoundAttached(
            RollSound,
            RootComponent,
            NAME_None,
            FVector::ZeroVector,
            EAttachLocation::KeepRelativeOffset,
            false, 1.0f, 1.0f, 0.0f,
            nullptr, nullptr, true
        );
    }
}

/**
 * @brief 停止滚动特效
 */
void ASG_RollingLog::StopRollingEffects()
{
    if (DustEffectComponent)
    {
        DustEffectComponent->DestroyComponent();
        DustEffectComponent = nullptr;
    }

    if (RollAudioComponent)
    {
        RollAudioComponent->Stop();
        RollAudioComponent = nullptr;
    }
}

/**
 * @brief 绘制调试信息
 */
void ASG_RollingLog::DrawDebugInfo()
{
#if ENABLE_DRAW_DEBUG
    if (!GetWorld())
    {
        return;
    }

    FVector Location = GetActorLocation();

    // 绘制检测胶囊体
    if (bShowDetectionCapsule && CollisionCapsule)
    {
        float Radius = CollisionCapsule->GetScaledCapsuleRadius();
        float HalfHeight = CollisionCapsule->GetScaledCapsuleHalfHeight();
        FVector CapsuleLocation = CollisionCapsule->GetComponentLocation();
        FQuat CapsuleRotation = CollisionCapsule->GetComponentQuat();

        DrawDebugCapsule(
            GetWorld(),
            CapsuleLocation,
            HalfHeight,
            Radius,
            CapsuleRotation,
            FColor::Green,
            false, -1.0f, 0, 2.0f
        );
    }

    // 绘制滚动方向
    if (bShowRollDirection)
    {
        DrawDebugDirectionalArrow(
            GetWorld(),
            Location,
            Location + RollDirection * 300.0f,
            60.0f,
            FColor::Red,
            false, -1.0f, 0, 4.0f
        );

        // 标注文字
        DrawDebugString(
            GetWorld(),
            Location + RollDirection * 150.0f + FVector(0, 0, 30.0f),
            TEXT("Roll Dir"),
            nullptr,
            FColor::Red,
            0.0f,
            true
        );
    }

    // 速度信息
    if (bShowVelocityDebug)
    {
        FVector Velocity = GetCurrentVelocity();
        float Speed = Velocity.Size();

        if (Speed > 10.0f)
        {
            DrawDebugDirectionalArrow(
                GetWorld(),
                Location,
                Location + Velocity.GetSafeNormal() * 150.0f,
                40.0f,
                FColor::Yellow,
                false, -1.0f, 0, 2.0f
            );
        }

        FString SpeedText = FString::Printf(TEXT("Speed: %.0f cm/s"), Speed);
        DrawDebugString(GetWorld(), Location + FVector(0, 0, 80.0f), SpeedText, nullptr, FColor::Yellow, 0.0f, true);

        if (PhysicsWarmupTimer > 0.0f)
        {
            FString WarmupText = FString::Printf(TEXT("Warmup: %.2f s"), PhysicsWarmupTimer);
            DrawDebugString(GetWorld(), Location + FVector(0, 0, 110.0f), WarmupText, nullptr, FColor::Cyan, 0.0f, true);
        }

        FString DistText = FString::Printf(TEXT("Dist: %.0f / %.0f"), RolledDistance, MaxRollDistance);
        DrawDebugString(GetWorld(), Location + FVector(0, 0, 50.0f), DistText, nullptr, FColor::White, 0.0f, true);
    }
#endif
}


/**
 * @brief 初始化滚木
 * @param InSourceASC 攻击者 ASC
 * @param InFactionTag 攻击者阵营
 * @param InRollDirection 滚动方向（世界空间）
 * @param bKeepCurrentRotation 是否保持当前旋转
 * @details
 * **🔧 修改：**
 * - 新增 bKeepCurrentRotation 参数
 * - 如果为 true，保持生成时的旋转，不用滚动方向覆盖
 */
void ASG_RollingLog::InitializeRollingLog(UAbilitySystemComponent* InSourceASC, FGameplayTag InFactionTag,
    FVector InRollDirection, bool bKeepCurrentRotation)
{
    UE_LOG(LogSGGameplay, Log, TEXT("========== 初始化滚木 =========="));
    UE_LOG(LogSGGameplay, Log, TEXT("  滚木：%s"), *GetName());
    UE_LOG(LogSGGameplay, Log, TEXT("  当前旋转：%s"), *GetActorRotation().ToString());
    UE_LOG(LogSGGameplay, Log, TEXT("  保持当前旋转：%s"), bKeepCurrentRotation ? TEXT("是") : TEXT("否"));

    SourceASC = InSourceASC;
    SourceFactionTag = InFactionTag;

    // 设置滚动方向（水平面）
    InRollDirection.Z = 0.0f;
    RollDirection = InRollDirection.GetSafeNormal();
    if (RollDirection.IsNearlyZero())
    {
        RollDirection = FVector::ForwardVector;
    }

    // 🔧 关键修改 - 根据参数决定是否覆盖旋转
    if (!bKeepCurrentRotation)
    {
        // 使用滚动方向设置旋转（原有逻辑）
        FRotator ActorRotation = RollDirection.Rotation();
        SetActorRotation(ActorRotation);
        UE_LOG(LogSGGameplay, Log, TEXT("  设置旋转为滚动方向：%s"), *ActorRotation.ToString());
    }
    else
    {
        // 保持当前旋转（生成时已设置）
        UE_LOG(LogSGGameplay, Log, TEXT("  保持生成时的旋转：%s"), *GetActorRotation().ToString());
    }

    // 设置物理
    if (bEnablePhysicsRolling)
    {
        SetupPhysics();
        PhysicsWarmupTimer = PhysicsWarmupDuration;
        ApplyInitialVelocity();
    }

    bIsInitialized = true;

    UE_LOG(LogSGGameplay, Log, TEXT("  攻击者阵营：%s"), *SourceFactionTag.ToString());
    UE_LOG(LogSGGameplay, Log, TEXT("  滚动方向：%s"), *RollDirection.ToString());
    UE_LOG(LogSGGameplay, Log, TEXT("  最终旋转：%s"), *GetActorRotation().ToString());
    UE_LOG(LogSGGameplay, Log, TEXT("  初始速度：%.0f cm/s"), InitialRollSpeed);
    UE_LOG(LogSGGameplay, Log, TEXT("========================================"));   
}
