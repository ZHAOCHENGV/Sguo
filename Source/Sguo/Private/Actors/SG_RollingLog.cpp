// 📄 文件：Source/Sguo/Private/Actors/SG_RollingLog.cpp
// 🔧 修改 - 完整文件，修复编译错误

#include "Actors/SG_RollingLog.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/AudioComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"  // ✨ 新增 - 包含 Niagara 系统头文件
#include "NiagaraFunctionLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameplayEffect.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Units/SG_UnitsBase.h"
#include "AbilitySystem/SG_AttributeSet.h"  // ✨ 新增 - 包含属性集头文件
#include "Debug/SG_LogCategories.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"  // ✨ 新增 - 包含音效基类头文件

/**
 * @brief 滚木默认配置命名空间
 */
namespace RollingLogDefaults
{
    /** 默认胶囊体半径（厘米）- 滚木粗细 */
    constexpr float CapsuleRadius = 50.0f;
    
    /** 默认胶囊体半高（厘米）- 滚木长度的一半 */
    constexpr float CapsuleHalfHeight = 150.0f;
}

/**
 * @brief 构造函数
 * 
 * @details
 * **功能说明：**
 * - 创建并配置所有组件
 * - 设置碰撞响应
 * - 绑定碰撞事件
 */
ASG_RollingLog::ASG_RollingLog()
{
    // 启用 Tick
    PrimaryActorTick.bCanEverTick = true;

    // ========== 创建场景根组件 ==========
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    RootComponent = SceneRoot;

    // ========== 创建碰撞胶囊体 ==========
    CollisionCapsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CollisionCapsule"));
    CollisionCapsule->SetupAttachment(RootComponent);
    
    // 设置胶囊体尺寸（横向放置，模拟滚木形状）
    CollisionCapsule->SetCapsuleRadius(RollingLogDefaults::CapsuleRadius);
    CollisionCapsule->SetCapsuleHalfHeight(RollingLogDefaults::CapsuleHalfHeight);
    
    // 旋转胶囊体使其横向（Y轴方向）
    CollisionCapsule->SetRelativeRotation(FRotator(0.0f, 0.0f, 90.0f));
    
    // 碰撞设置
    CollisionCapsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    CollisionCapsule->SetCollisionObjectType(ECC_WorldDynamic);
    CollisionCapsule->SetCollisionResponseToAllChannels(ECR_Ignore);
    CollisionCapsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    CollisionCapsule->SetGenerateOverlapEvents(true);
    
    // 绑定碰撞事件
    CollisionCapsule->OnComponentBeginOverlap.AddDynamic(this, &ASG_RollingLog::OnCapsuleOverlap);

    // ========== 创建网格体组件 ==========
    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
    MeshComponent->SetupAttachment(RootComponent);
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // ========== 初始化指针为 nullptr ==========
    // 🔧 修改 - 显式初始化原始指针
    BreakParticleSystem = nullptr;
    RollDustParticleSystem = nullptr;
    BreakSound = nullptr;
    RollSound = nullptr;
    SourceASC = nullptr;

    // 启用网络复制
    bReplicates = true;
}

/**
 * @brief BeginPlay 生命周期函数
 */
void ASG_RollingLog::BeginPlay()
{
    Super::BeginPlay();

    // 记录起始位置
    StartLocation = GetActorLocation();

    // 🔧 修改 - 使用重命名后的变量
    // 设置生存时间
    SetLifeSpan(LogLifeSpan);

    // 启动滚动特效
    StartRollingEffects();

    UE_LOG(LogSGGameplay, Log, TEXT("滚木生成：%s"), *GetName());
    UE_LOG(LogSGGameplay, Log, TEXT("  位置：%s"), *StartLocation.ToString());
    UE_LOG(LogSGGameplay, Log, TEXT("  方向：%s"), *RollDirection.ToString());
}

/**
 * @brief Tick 函数
 * @param DeltaTime 帧间隔时间
 */
void ASG_RollingLog::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 未初始化或已击中目标则不更新
    if (!bIsInitialized || bHasHitTarget || bIsDestroying)
    {
        return;
    }

    // 更新滚动位置
    UpdateRolling(DeltaTime);

    // 更新视觉旋转
    UpdateVisualRotation(DeltaTime);

    // 检查是否超出最大距离
    if (RolledDistance >= MaxRollDistance)
    {
        UE_LOG(LogSGGameplay, Log, TEXT("滚木超出最大距离，销毁：%s"), *GetName());
        K2_OnLogOutOfRange();
        Destroy();
    }
}

/**
 * @brief EndPlay 生命周期函数
 * @param EndPlayReason 结束原因
 */
void ASG_RollingLog::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 停止滚动特效
    StopRollingEffects();

    // 广播销毁事件
    OnLogDestroyed.Broadcast(this);

    Super::EndPlay(EndPlayReason);
}

/**
 * @brief 初始化滚木
 * @param InSourceASC 攻击者 ASC
 * @param InFactionTag 攻击者阵营
 * @param InRollDirection 滚动方向
 */
void ASG_RollingLog::InitializeRollingLog(
    UAbilitySystemComponent* InSourceASC,
    FGameplayTag InFactionTag,
    FVector InRollDirection
)
{
    UE_LOG(LogSGGameplay, Log, TEXT("========== 初始化滚木 =========="));

    // 🔧 修改 - 使用重命名后的变量
    // 保存攻击者信息
    SourceASC = InSourceASC;
    SourceFactionTag = InFactionTag;

    // 设置滚动方向（归一化，并确保在水平面上）
    InRollDirection.Z = 0.0f;
    RollDirection = InRollDirection.GetSafeNormal();

    // 如果方向为零，默认向前
    if (RollDirection.IsNearlyZero())
    {
        RollDirection = FVector::ForwardVector;
    }

    // 设置 Actor 朝向滚动方向
    SetActorRotation(RollDirection.Rotation());

    // 标记已初始化
    bIsInitialized = true;

    // 🔧 修改 - 使用重命名后的变量
    UE_LOG(LogSGGameplay, Log, TEXT("  攻击者阵营：%s"), *SourceFactionTag.ToString());
    UE_LOG(LogSGGameplay, Log, TEXT("  滚动方向：%s"), *RollDirection.ToString());
    UE_LOG(LogSGGameplay, Log, TEXT("  滚动速度：%.1f"), RollSpeed);
    UE_LOG(LogSGGameplay, Log, TEXT("  最大距离：%.1f"), MaxRollDistance);
    UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
}

/**
 * @brief 设置滚动方向
 * @param NewDirection 新的滚动方向
 */
void ASG_RollingLog::SetRollDirection(FVector NewDirection)
{
    // 确保方向在水平面上
    NewDirection.Z = 0.0f;
    RollDirection = NewDirection.GetSafeNormal();

    // 如果方向为零，保持原方向
    if (!RollDirection.IsNearlyZero())
    {
        SetActorRotation(RollDirection.Rotation());
    }
}

/**
 * @brief 手动销毁滚木
 */
void ASG_RollingLog::BreakAndDestroy()
{
    // 防止重复调用
    if (bIsDestroying)
    {
        return;
    }

    bIsDestroying = true;

    // 播放破碎特效
    PlayBreakEffects();

    // 调用蓝图事件
    K2_OnLogBreak(GetActorLocation());

    // 延迟销毁（等待特效播放）
    SetLifeSpan(0.5f);
}

/**
 * @brief 更新滚动位置
 * @param DeltaTime 帧间隔
 */
void ASG_RollingLog::UpdateRolling(float DeltaTime)
{
    // 计算本帧移动距离
    float MoveDistance = RollSpeed * DeltaTime;

    // 计算新位置
    FVector CurrentLocation = GetActorLocation();
    FVector NewLocation = CurrentLocation + RollDirection * MoveDistance;

    // 更新位置
    SetActorLocation(NewLocation);

    // 累计已滚动距离
    RolledDistance += MoveDistance;
}

/**
 * @brief 更新视觉旋转
 * @param DeltaTime 帧间隔
 */
void ASG_RollingLog::UpdateVisualRotation(float DeltaTime)
{
    // 只旋转网格体，不旋转整个 Actor
    if (MeshComponent)
    {
        // 计算本帧旋转角度
        float RotationThisFrame = RotationSpeed * DeltaTime;

        // 绕滚动垂直轴旋转（模拟滚动效果）
        // 滚木向前滚动时，应该绕 Y 轴（局部）旋转
        FRotator CurrentRotation = MeshComponent->GetRelativeRotation();
        CurrentRotation.Pitch += RotationThisFrame;
        MeshComponent->SetRelativeRotation(CurrentRotation);
    }
}

/**
 * @brief 碰撞检测回调
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
    // 如果已击中目标或正在销毁，忽略后续碰撞
    if (bHasHitTarget || bIsDestroying || !bIsInitialized)
    {
        return;
    }

    // 忽略空 Actor
    if (!OtherActor)
    {
        return;
    }

    // 忽略自己
    if (OtherActor == this)
    {
        return;
    }

    // 检查是否是单位
    ASG_UnitsBase* OtherUnit = Cast<ASG_UnitsBase>(OtherActor);
    if (!OtherUnit)
    {
        return;
    }

    // 🔧 修改 - 使用重命名后的变量
    // 忽略友方单位
    if (OtherUnit->FactionTag == SourceFactionTag)
    {
        return;
    }

    // 忽略已死亡的单位
    if (OtherUnit->bIsDead)
    {
        return;
    }

    // 处理击中目标
    FVector HitLocation = SweepResult.ImpactPoint.IsNearlyZero() ? 
        OtherActor->GetActorLocation() : FVector(SweepResult.ImpactPoint);
    
    HandleHitTarget(OtherActor, HitLocation);
}

/**
 * @brief 处理击中目标
 * @param HitActor 被击中的 Actor
 * @param HitLocation 击中位置
 */
void ASG_RollingLog::HandleHitTarget(AActor* HitActor, const FVector& HitLocation)
{
    UE_LOG(LogSGGameplay, Log, TEXT("========== 滚木击中目标 =========="));
    UE_LOG(LogSGGameplay, Log, TEXT("  滚木：%s"), *GetName());
    UE_LOG(LogSGGameplay, Log, TEXT("  目标：%s"), *HitActor->GetName());
    UE_LOG(LogSGGameplay, Log, TEXT("  击中位置：%s"), *HitLocation.ToString());

    // 标记已击中
    bHasHitTarget = true;

    // 计算击退方向（滚动方向）
    FVector KnockbackDir = RollDirection;
    KnockbackDir.Z = 0.0f;
    KnockbackDir.Normalize();

    // 构建击中信息
    FSGRollingLogHitInfo HitInfo;
    HitInfo.HitActor = HitActor;
    HitInfo.HitLocation = HitLocation;
    HitInfo.KnockbackDirection = KnockbackDir;
    HitInfo.RollDirection = RollDirection;

    // 应用伤害
    ApplyDamageToTarget(HitActor);

    // 应用击退
    ApplyKnockbackToTarget(HitActor, KnockbackDir);

    // 广播事件
    OnLogHitTarget.Broadcast(HitInfo);

    // 调用蓝图事件
    K2_OnHitTarget(HitInfo);

    UE_LOG(LogSGGameplay, Log, TEXT("========================================"));

    // 破碎销毁
    BreakAndDestroy();
}

/**
 * @brief 应用伤害到目标
 * @param Target 目标 Actor
 */
void ASG_RollingLog::ApplyDamageToTarget(AActor* Target)
{
    // 检查目标有效性
    if (!Target)
    {
        UE_LOG(LogSGGameplay, Error, TEXT("ApplyDamageToTarget 失败：目标为空"));
        return;
    }

    // 获取目标的 ASC
    UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
    if (!TargetASC)
    {
        UE_LOG(LogSGGameplay, Error, TEXT("ApplyDamageToTarget 失败：目标没有 ASC"));
        return;
    }

    // 检查伤害效果类
    if (!DamageEffectClass)
    {
        UE_LOG(LogSGGameplay, Warning, TEXT("ApplyDamageToTarget：伤害 GE 未设置，使用直接伤害"));
        
        // 如果没有配置 GE，直接修改生命值（不推荐，但作为后备方案）
        ASG_UnitsBase* TargetUnit = Cast<ASG_UnitsBase>(Target);
        if (TargetUnit && TargetUnit->AttributeSet)
        {
            float CurrentHealth = TargetUnit->AttributeSet->GetHealth();
            float NewHealth = FMath::Max(0.0f, CurrentHealth - DamageAmount);
            TargetUnit->AttributeSet->SetHealth(NewHealth);
            UE_LOG(LogSGGameplay, Log, TEXT("  直接伤害：%.0f -> %.0f"), CurrentHealth, NewHealth);
        }
        return;
    }

    // 🔧 修改 - 修复类型歧义问题
    // 使用 ASC 创建效果上下文
    UAbilitySystemComponent* EffectSourceASC = SourceASC ? SourceASC : TargetASC;
    
    FGameplayEffectContextHandle EffectContext = EffectSourceASC->MakeEffectContext();
    EffectContext.AddInstigator(GetOwner(), this);

    // 创建效果规格
    FGameplayEffectSpecHandle SpecHandle = EffectSourceASC->MakeOutgoingSpec(DamageEffectClass, 1.0f, EffectContext);

    if (!SpecHandle.IsValid())
    {
        UE_LOG(LogSGGameplay, Error, TEXT("ApplyDamageToTarget 失败：创建 EffectSpec 失败"));
        return;
    }

    // 设置伤害值（通过 SetByCaller）
    FGameplayTag DamageTag = FGameplayTag::RequestGameplayTag(FName("Data.Damage"));
    if (DamageTag.IsValid())
    {
        // 使用 DamageAmount 作为倍率（假设基础伤害为 1）
        // 或者如果 GE 配置了执行计算，这里设置伤害倍率
        SpecHandle.Data->SetSetByCallerMagnitude(DamageTag, DamageAmount);
    }

    // 应用效果到目标
    FActiveGameplayEffectHandle ActiveHandle = EffectSourceASC->ApplyGameplayEffectSpecToTarget(
        *SpecHandle.Data.Get(), 
        TargetASC
    );

    if (ActiveHandle.IsValid() || SpecHandle.IsValid())
    {
        UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 滚木伤害应用成功（伤害值：%.0f）"), DamageAmount);
    }
    else
    {
        UE_LOG(LogSGGameplay, Error, TEXT("  ❌ 滚木伤害应用失败"));
    }
}

/**
 * @brief 应用击退效果
 * @param Target 目标 Actor
 * @param KnockbackDir 击退方向
 */
void ASG_RollingLog::ApplyKnockbackToTarget(AActor* Target, const FVector& KnockbackDir)
{
    // 检查目标有效性
    if (!Target)
    {
        return;
    }

    // 获取目标的角色移动组件
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

    // 计算击退速度
    // 使用 LaunchCharacter 实现击退
    FVector LaunchVelocity = KnockbackDir * (KnockbackDistance / KnockbackDuration);
    
    // 添加一点向上的速度，让击退看起来更自然
    LaunchVelocity.Z = 100.0f;

    // 执行击退
    TargetUnit->LaunchCharacter(LaunchVelocity, true, true);

    UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 击退应用成功"));
    UE_LOG(LogSGGameplay, Log, TEXT("    方向：%s"), *KnockbackDir.ToString());
    UE_LOG(LogSGGameplay, Log, TEXT("    距离：%.0f"), KnockbackDistance);
    UE_LOG(LogSGGameplay, Log, TEXT("    速度：%s"), *LaunchVelocity.ToString());
}

/**
 * @brief 播放破碎特效
 */
void ASG_RollingLog::PlayBreakEffects()
{
    FVector BreakLocation = GetActorLocation();

    // 播放破碎粒子特效
    if (BreakParticleSystem)
    {
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            GetWorld(),
            BreakParticleSystem,
            BreakLocation,
            GetActorRotation()
        );
    }

    // 播放破碎音效
    if (BreakSound)
    {
        UGameplayStatics::PlaySoundAtLocation(
            this,
            BreakSound,
            BreakLocation
        );
    }

    // 隐藏网格体
    if (MeshComponent)
    {
        MeshComponent->SetVisibility(false);
    }

    // 禁用碰撞
    if (CollisionCapsule)
    {
        CollisionCapsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    UE_LOG(LogSGGameplay, Log, TEXT("  播放破碎特效：%s"), *BreakLocation.ToString());
}

/**
 * @brief 启动滚动特效
 */
void ASG_RollingLog::StartRollingEffects()
{
    // 生成滚动尘土特效
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

    // 播放滚动音效（循环）
    if (RollSound)
    {
        RollAudioComponent = UGameplayStatics::SpawnSoundAttached(
            RollSound,
            RootComponent,
            NAME_None,
            FVector::ZeroVector,
            EAttachLocation::KeepRelativeOffset,
            false,
            1.0f,
            1.0f,
            0.0f,
            nullptr,
            nullptr,
            true  // bAutoDestroy = true
        );
    }
}

/**
 * @brief 停止滚动特效
 */
void ASG_RollingLog::StopRollingEffects()
{
    // 停止尘土特效
    if (DustEffectComponent)
    {
        DustEffectComponent->DestroyComponent();
        DustEffectComponent = nullptr;
    }

    // 停止滚动音效
    if (RollAudioComponent)
    {
        RollAudioComponent->Stop();
        RollAudioComponent = nullptr;
    }
}
