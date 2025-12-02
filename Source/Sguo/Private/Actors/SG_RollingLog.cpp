// 📄 文件：Source/Sguo/Private/Actors/SG_RollingLog.cpp
// 🔧 修改 - 修复击退方向，简化胶囊体配置

#include "Actors/SG_RollingLog.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/AudioComponent.h"
#include "NiagaraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"  // ✨ 新增 - 需要访问控制器来停止移动
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

    // 🔧 调试 - 记录设置物理前的旋转
    FRotator RotationBeforePhysics = GetActorRotation();
    UE_LOG(LogSGGameplay, Log, TEXT("  设置物理参数..."));
    UE_LOG(LogSGGameplay, Log, TEXT("    物理设置前旋转：%s"), *RotationBeforePhysics.ToString());

    // 启用物理模拟
    MeshComponent->SetSimulatePhysics(true);
    
    // 设置质量
    MeshComponent->SetMassOverrideInKg(NAME_None, LogMass, true);
    
    // 设置阻尼
    MeshComponent->SetLinearDamping(LinearDamping);
    MeshComponent->SetAngularDamping(AngularDamping);
    
    // 启用 CCD 防止穿透
    MeshComponent->BodyInstance.bUseCCD = true;

    // 🔧 关键 - 不锁定旋转轴，让物理自然滚动
    // 但要确保这不会重置旋转
    MeshComponent->BodyInstance.bLockXRotation = false;
    MeshComponent->BodyInstance.bLockYRotation = false;
    MeshComponent->BodyInstance.bLockZRotation = true;

    // 🔧 调试 - 记录设置物理后的旋转
    FRotator RotationAfterPhysics = GetActorRotation();
    UE_LOG(LogSGGameplay, Log, TEXT("    物理设置后旋转：%s"), *RotationAfterPhysics.ToString());
    
    if (!RotationBeforePhysics.Equals(RotationAfterPhysics, 0.1f))
    {
        UE_LOG(LogSGGameplay, Warning, TEXT("    ⚠️ 物理设置改变了旋转！"));
    }

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

    // 🔧 调试 - 记录施加速度前的旋转
    FRotator RotationBeforeVelocity = GetActorRotation();
    UE_LOG(LogSGGameplay, Log, TEXT("  施加初始速度..."));
    UE_LOG(LogSGGameplay, Log, TEXT("    施加前旋转：%s"), *RotationBeforeVelocity.ToString());

    // 设置线性速度
    FVector LinearVelocity = RollDirection * InitialRollSpeed;
    MeshComponent->SetPhysicsLinearVelocity(LinearVelocity);

    // 设置角速度
    // 🔧 关键 - 角速度应该让木桩绕其长轴旋转
    // 木桩的长轴取决于当前旋转
    FVector LogLongAxis = GetActorRightVector();  // 假设木桩长轴是 Right 方向
    
    float AngularSpeedRadians = FMath::DegreesToRadians(InitialAngularSpeed);
    
    // 滚动时，角速度方向应该垂直于滚动方向和木桩长轴
    // 简单起见，让木桩绕其长轴旋转
    FVector AngularVelocity = LogLongAxis * AngularSpeedRadians;
    
    MeshComponent->SetPhysicsAngularVelocityInRadians(AngularVelocity);

    // 🔧 调试 - 记录施加速度后的旋转
    FRotator RotationAfterVelocity = GetActorRotation();
    UE_LOG(LogSGGameplay, Log, TEXT("    施加后旋转：%s"), *RotationAfterVelocity.ToString());
    
    if (!RotationBeforeVelocity.Equals(RotationAfterVelocity, 0.1f))
    {
        UE_LOG(LogSGGameplay, Warning, TEXT("    ⚠️ 施加速度改变了旋转！"));
    }

    UE_LOG(LogSGGameplay, Log, TEXT("    线性速度：%s (%.0f cm/s)"), *LinearVelocity.ToString(), LinearVelocity.Size());
    UE_LOG(LogSGGameplay, Log, TEXT("    木桩长轴：%s"), *LogLongAxis.ToString());
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
 * - 停止控制器的移动请求（防止 AI 顶风作案）
 * - 停止移动组件的当前速度/加速度
 * - 强制设为下落模式（消除地面摩擦力干扰）
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

    // ========== 🔧 核心修复开始 ==========

    // 1. 停止 AI/玩家 的移动请求 (清除 Input Acceleration)
    // 这一步至关重要，防止 AI 在击退过程中继续尝试“走回来”
    if (AController* UnitController = TargetUnit->GetController())
    {
        UnitController->StopMovement();
    }

    // 2. 清除物理动量 (清除 Velocity & Acceleration)
    // 确保击退是从零速度开始，而不是与之前的移动速度叠加
    MovementComp->StopMovementImmediately();

    // 3. 强制切换到下落状态 (Detach from Ground)
    // 即使 KnockbackUpwardForce 很小，也要强制离地，避免第一帧就被 GroundFriction 刹停
    if (MovementComp->IsMovingOnGround())
    {
        MovementComp->SetMovementMode(MOVE_Falling);
    }

    // ========== 🔧 核心修复结束 ==========

    // 计算击退速度
    // 水平方向速度 = 击退距离 / 击退时间
    // 注意：这假设的是无阻力的理想运动，实际距离会因 AirDrag 略微缩短，但已足够稳定
    float HorizontalSpeed = KnockbackDistance / KnockbackDuration;
    
    // 构建击退速度向量
    FVector LaunchVelocity = KnockbackDir * HorizontalSpeed;
    LaunchVelocity.Z = KnockbackUpwardForce;

    // 执行击退 (XYOverride=true, ZOverride=true)
    // 覆盖当前所有速度
    TargetUnit->LaunchCharacter(LaunchVelocity, true, true);

    UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 击退应用成功（已重置目标状态）"));
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
   UE_LOG(LogSGGameplay, Log, TEXT(""));
    UE_LOG(LogSGGameplay, Log, TEXT("╔══════════════════════════════════════╗"));
    UE_LOG(LogSGGameplay, Log, TEXT("║       初始化滚木                      ║"));
    UE_LOG(LogSGGameplay, Log, TEXT("╚══════════════════════════════════════╝"));
    UE_LOG(LogSGGameplay, Log, TEXT("  滚木：%s"), *GetName());
    
    // 🔧 关键调试 - 追踪旋转变化
    FRotator Step0_SpawnRotation = GetActorRotation();
    UE_LOG(LogSGGameplay, Log, TEXT("  [Step 0] 生成时旋转：%s"), *Step0_SpawnRotation.ToString());
    UE_LOG(LogSGGameplay, Log, TEXT("  [Step 0] bKeepCurrentRotation = %s"), bKeepCurrentRotation ? TEXT("TRUE") : TEXT("FALSE"));

    // 保存攻击者信息
    SourceASC = InSourceASC;
    SourceFactionTag = InFactionTag;

    // 设置滚动方向
    InRollDirection.Z = 0.0f;
    RollDirection = InRollDirection.GetSafeNormal();
    if (RollDirection.IsNearlyZero())
    {
        RollDirection = FVector::ForwardVector;
    }
    UE_LOG(LogSGGameplay, Log, TEXT("  [Step 1] 滚动方向：%s"), *RollDirection.ToString());

    // 🔧 关键 - 是否覆盖旋转
    if (!bKeepCurrentRotation)
    {
        FRotator NewRotation = RollDirection.Rotation();
        SetActorRotation(NewRotation);
        UE_LOG(LogSGGameplay, Log, TEXT("  [Step 2] 用滚动方向覆盖旋转：%s"), *NewRotation.ToString());
    }
    else
    {
        UE_LOG(LogSGGameplay, Log, TEXT("  [Step 2] 保持当前旋转，跳过"));
    }
    
    FRotator Step2_Rotation = GetActorRotation();
    UE_LOG(LogSGGameplay, Log, TEXT("  [Step 2] 当前旋转：%s"), *Step2_Rotation.ToString());

    // 设置物理
    if (bEnablePhysicsRolling)
    {
        UE_LOG(LogSGGameplay, Log, TEXT("  [Step 3] 开始设置物理..."));
        SetupPhysics();
        
        FRotator Step3_Rotation = GetActorRotation();
        UE_LOG(LogSGGameplay, Log, TEXT("  [Step 3] 物理设置后旋转：%s"), *Step3_Rotation.ToString());
        
        PhysicsWarmupTimer = PhysicsWarmupDuration;
        
        UE_LOG(LogSGGameplay, Log, TEXT("  [Step 4] 开始施加速度..."));
        ApplyInitialVelocity();
        
        FRotator Step4_Rotation = GetActorRotation();
        UE_LOG(LogSGGameplay, Log, TEXT("  [Step 4] 施加速度后旋转：%s"), *Step4_Rotation.ToString());
    }

    bIsInitialized = true;

    // 最终检查
    FRotator FinalRotation = GetActorRotation();
    UE_LOG(LogSGGameplay, Log, TEXT(""));
    UE_LOG(LogSGGameplay, Log, TEXT("  ┌─────────────────────────────────┐"));
    UE_LOG(LogSGGameplay, Log, TEXT("  │ 旋转追踪结果                     │"));
    UE_LOG(LogSGGameplay, Log, TEXT("  ├─────────────────────────────────┤"));
    UE_LOG(LogSGGameplay, Log, TEXT("  │ 生成时：%s"), *Step0_SpawnRotation.ToString());
    UE_LOG(LogSGGameplay, Log, TEXT("  │ 最终：  %s"), *FinalRotation.ToString());
    UE_LOG(LogSGGameplay, Log, TEXT("  │ 是否改变：%s"), 
        Step0_SpawnRotation.Equals(FinalRotation, 0.1f) ? TEXT("否 ✓") : TEXT("是 ✗"));
    UE_LOG(LogSGGameplay, Log, TEXT("  └─────────────────────────────────┘"));
    UE_LOG(LogSGGameplay, Log, TEXT(""));
}
/**
 * @brief 强制设置滚木旋转
 * @param NewRotation 新的旋转
 * @details
 * **功能说明：**
 * - 重置 MeshComponent 的相对旋转为零
 * - 然后设置 Actor 的世界旋转
 * - 这样确保最终旋转就是我们想要的
 */
void ASG_RollingLog::ForceSetRotation(FRotator NewRotation)
{
    UE_LOG(LogSGGameplay, Log, TEXT("  ForceSetRotation: %s"), *NewRotation.ToString());
    
    // 🔧 关键 - 先重置 MeshComponent 的相对旋转
    if (MeshComponent)
    {
        FRotator OldRelativeRot = MeshComponent->GetRelativeRotation();
        MeshComponent->SetRelativeRotation(FRotator::ZeroRotator);
        UE_LOG(LogSGGameplay, Log, TEXT("    MeshComponent 相对旋转：%s -> ZeroRotator"), *OldRelativeRot.ToString());
    }
    
    // 设置 Actor 旋转
    SetActorRotation(NewRotation);
    
    // 验证
    UE_LOG(LogSGGameplay, Log, TEXT("    最终 Actor 旋转：%s"), *GetActorRotation().ToString());
    
    if (MeshComponent)
    {
        UE_LOG(LogSGGameplay, Log, TEXT("    最终 MeshComponent 世界旋转：%s"), *MeshComponent->GetComponentRotation().ToString());
    }
}
