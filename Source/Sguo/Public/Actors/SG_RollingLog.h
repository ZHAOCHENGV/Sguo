// 📄 文件：Source/Sguo/Public/Actors/SG_RollingLog.h
// 🔧 修改 - 简化胶囊体配置，使用组件本身尺寸

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "SG_RollingLog.generated.h"

// 前置声明
class UCapsuleComponent;
class UStaticMeshComponent;
class UGameplayEffect;
class UAbilitySystemComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class UAudioComponent;
class USoundBase;

/**
 * @brief 滚木击中信息结构体
 */
USTRUCT(BlueprintType)
struct FSGRollingLogHitInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Hit Info", meta = (DisplayName = "击中目标"))
    AActor* HitActor = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "Hit Info", meta = (DisplayName = "击中位置"))
    FVector HitLocation = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "Hit Info", meta = (DisplayName = "击退方向"))
    FVector KnockbackDirection = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "Hit Info", meta = (DisplayName = "滚动方向"))
    FVector RollDirection = FVector::ForwardVector;
};

class ASG_RollingLog;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSGRollingLogHitSignature, const FSGRollingLogHitInfo&, HitInfo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSGRollingLogDestroyedSignature, ASG_RollingLog*, DestroyedLog);

/**
 * @brief 滚木Actor类
 * 
 * @details
 * **组件结构：**
 * - MeshComponent（根组件）：启用物理模拟，真实滚动
 * - CollisionCapsule（附着）：仅用于 Overlap 检测敌方单位
 * 
 * **使用说明：**
 * - 在蓝图中可以直接调整 CollisionCapsule 的变换（位置、旋转、缩放）
 * - 胶囊体尺寸直接使用组件本身设置，无需额外配置
 * - 击中敌人后立即破碎
 */
UCLASS()
class SGUO_API ASG_RollingLog : public AActor
{
    GENERATED_BODY()

public:
    ASG_RollingLog();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    // ==================== 事件委托 ====================

    UPROPERTY(BlueprintAssignable, Category = "Rolling Log Events", meta = (DisplayName = "击中目标事件"))
    FSGRollingLogHitSignature OnLogHitTarget;

    UPROPERTY(BlueprintAssignable, Category = "Rolling Log Events", meta = (DisplayName = "滚木销毁事件"))
    FSGRollingLogDestroyedSignature OnLogDestroyed;

    // ==================== 组件 ====================

    /**
     * @brief 网格体组件（根组件）
     * @details 
     * - 作为根组件，启用物理模拟
     * - 显示木桩外观并进行物理滚动
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "网格体"))
    TObjectPtr<UStaticMeshComponent> MeshComponent;

    /**
     * @brief 碰撞胶囊体组件
     * @details 
     * - 🔧 修改 - 可在蓝图视口中自由调整变换
     * - 仅用于 Overlap 检测敌方单位
     * - 尺寸直接使用组件本身设置
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Components", meta = (DisplayName = "检测胶囊体"))
    TObjectPtr<UCapsuleComponent> CollisionCapsule;

    // ==================== 物理配置 ====================

    /**
     * @brief 是否启用物理滚动
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Physics Config", meta = (DisplayName = "启用物理滚动"))
    bool bEnablePhysicsRolling = true;

    /**
     * @brief 初始滚动速度（厘米/秒）
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Physics Config", meta = (DisplayName = "初始滚动速度", ClampMin = "0.0", UIMin = "0.0", UIMax = "5000.0"))
    float InitialRollSpeed = 1500.0f;

    /**
     * @brief 初始角速度（度/秒）
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Physics Config", meta = (DisplayName = "初始角速度", ClampMin = "0.0", UIMin = "0.0", UIMax = "1080.0"))
    float InitialAngularSpeed = 400.0f;

    /**
     * @brief 滚木质量（千克）
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Physics Config", meta = (DisplayName = "滚木质量", ClampMin = "1.0", UIMin = "1.0", UIMax = "500.0"))
    float LogMass = 50.0f;

    /**
     * @brief 线性阻尼
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Physics Config", meta = (DisplayName = "线性阻尼", ClampMin = "0.0", UIMin = "0.0", UIMax = "5.0"))
    float LinearDamping = 0.2f;

    /**
     * @brief 角阻尼
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Physics Config", meta = (DisplayName = "角阻尼", ClampMin = "0.0", UIMin = "0.0", UIMax = "5.0"))
    float AngularDamping = 0.1f;

    /**
     * @brief 最小速度阈值
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Physics Config", meta = (DisplayName = "最小速度阈值", ClampMin = "0.0", UIMin = "0.0", UIMax = "200.0"))
    float MinVelocityThreshold = 30.0f;

    /**
     * @brief 物理预热时间（秒）
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Physics Config", meta = (DisplayName = "物理预热时间", ClampMin = "0.1", UIMin = "0.1", UIMax = "2.0"))
    float PhysicsWarmupDuration = 0.5f;

    // ==================== 滚动配置（非物理模式）====================

    /**
     * @brief 滚动速度（厘米/秒）- 非物理模式
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Rolling Config", meta = (DisplayName = "滚动速度", ClampMin = "100.0", UIMin = "100.0", UIMax = "3000.0", EditCondition = "!bEnablePhysicsRolling"))
    float RollSpeed = 800.0f;

    /**
     * @brief 旋转速度（度/秒）- 非物理模式
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Rolling Config", meta = (DisplayName = "旋转速度", ClampMin = "0.0", UIMin = "0.0", UIMax = "1080.0", EditCondition = "!bEnablePhysicsRolling"))
    float RotationSpeed = 360.0f;

    /**
     * @brief 最大滚动距离（厘米）
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Rolling Config", meta = (DisplayName = "最大滚动距离", ClampMin = "100.0", UIMin = "100.0", UIMax = "10000.0"))
    float MaxRollDistance = 3000.0f;

    /**
     * @brief 生存时间（秒）
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Rolling Config", meta = (DisplayName = "生存时间", ClampMin = "1.0", UIMin = "1.0", UIMax = "30.0"))
    float LogLifeSpan = 10.0f;

    // ==================== 伤害配置 ====================

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Damage Config", meta = (DisplayName = "伤害值", ClampMin = "0.0", UIMin = "0.0"))
    float DamageAmount = 100.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage Config", meta = (DisplayName = "伤害效果"))
    TSubclassOf<UGameplayEffect> DamageEffectClass;

    // ==================== 击退配置 ====================

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Knockback Config", meta = (DisplayName = "击退距离", ClampMin = "0.0", UIMin = "0.0", UIMax = "1000.0"))
    float KnockbackDistance = 300.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Knockback Config", meta = (DisplayName = "击退持续时间", ClampMin = "0.1", UIMin = "0.1", UIMax = "2.0"))
    float KnockbackDuration = 0.3f;

    /**
     * @brief 击退向上分量
     * @details 击退时添加的垂直速度，让目标被弹起
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Knockback Config", meta = (DisplayName = "击退向上分量", ClampMin = "0.0", UIMin = "0.0", UIMax = "500.0"))
    float KnockbackUpwardForce = 150.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Knockback Config", meta = (DisplayName = "击退曲线"))
    TObjectPtr<UCurveFloat> KnockbackCurve;

    // ==================== 视觉效果配置 ====================

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Visual Effects", meta = (DisplayName = "破碎粒子特效"))
    UNiagaraSystem* BreakParticleSystem;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Visual Effects", meta = (DisplayName = "滚动尘土特效"))
    UNiagaraSystem* RollDustParticleSystem;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Visual Effects", meta = (DisplayName = "破碎音效"))
    USoundBase* BreakSound;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Visual Effects", meta = (DisplayName = "滚动音效"))
    USoundBase* RollSound;

    // ==================== 运行时数据 ====================

    UPROPERTY(Transient, BlueprintReadOnly, Category = "Runtime", meta = (DisplayName = "攻击者ASC"))
    UAbilitySystemComponent* SourceASC;

    UPROPERTY(Transient, BlueprintReadOnly, Category = "Runtime", meta = (DisplayName = "攻击者阵营"))
    FGameplayTag SourceFactionTag;

    // ==================== 调试配置 ====================

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Debug", meta = (DisplayName = "显示检测胶囊体"))
    bool bShowDetectionCapsule = false;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Debug", meta = (DisplayName = "显示速度信息"))
    bool bShowVelocityDebug = false;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Debug", meta = (DisplayName = "显示滚动方向"))
    bool bShowRollDirection = false;

public:
    // ==================== 公共接口 ====================


    UFUNCTION(BlueprintCallable, Category = "Rolling Log", meta = (DisplayName = "设置滚动方向"))
    void SetRollDirection(FVector NewDirection);

    UFUNCTION(BlueprintPure, Category = "Rolling Log", meta = (DisplayName = "获取滚动方向"))
    FVector GetRollDirection() const { return RollDirection; }

    UFUNCTION(BlueprintPure, Category = "Rolling Log", meta = (DisplayName = "获取已滚动距离"))
    float GetRolledDistance() const { return RolledDistance; }

    UFUNCTION(BlueprintPure, Category = "Rolling Log", meta = (DisplayName = "获取当前速度"))
    FVector GetCurrentVelocity() const;

    UFUNCTION(BlueprintPure, Category = "Rolling Log", meta = (DisplayName = "获取当前速度大小"))
    float GetCurrentSpeed() const;

    UFUNCTION(BlueprintCallable, Category = "Rolling Log", meta = (DisplayName = "破碎销毁"))
    void BreakAndDestroy();

protected:
    // ==================== 内部状态 ====================

    /** 滚动方向（世界空间，归一化） */
    FVector RollDirection = FVector::ForwardVector;

    /** 起始位置 */
    FVector StartLocation;

    /** 已滚动距离 */
    float RolledDistance = 0.0f;

    /** 是否已初始化 */
    bool bIsInitialized = false;

    /** 是否已击中目标 */
    bool bHasHitTarget = false;

    /** 是否正在销毁 */
    bool bIsDestroying = false;

    /** 物理预热计时器 */
    float PhysicsWarmupTimer = 0.0f;

    UPROPERTY()
    TObjectPtr<UNiagaraComponent> DustEffectComponent;

    UPROPERTY()
    TObjectPtr<UAudioComponent> RollAudioComponent;

protected:
    // ==================== 内部函数 ====================

    void SetupPhysics();
    void ApplyInitialVelocity();
    void UpdatePhysicsRolling(float DeltaTime);
    void UpdateRolling(float DeltaTime);
    void UpdateVisualRotation(float DeltaTime);

    UFUNCTION()
    void OnCapsuleOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult
    );

    void HandleHitTarget(AActor* HitActor, const FVector& HitLocation);
    bool ApplyDamageToTarget(AActor* Target);
    void ApplyKnockbackToTarget(AActor* Target, const FVector& KnockbackDir);
    void PlayBreakEffects();
    void StartRollingEffects();
    void StopRollingEffects();
    void DrawDebugInfo();

public:
    // ==================== 蓝图事件 ====================

    UFUNCTION(BlueprintImplementableEvent, Category = "Rolling Log", meta = (DisplayName = "On Hit Target (BP)"))
    void K2_OnHitTarget(const FSGRollingLogHitInfo& HitInfo);

    UFUNCTION(BlueprintImplementableEvent, Category = "Rolling Log", meta = (DisplayName = "On Log Break (BP)"))
    void K2_OnLogBreak(FVector BreakLocation);

    UFUNCTION(BlueprintImplementableEvent, Category = "Rolling Log", meta = (DisplayName = "On Log Out Of Range (BP)"))
    void K2_OnLogOutOfRange();

    /**
    * @brief 初始化滚木
    * @param InSourceASC 攻击者 ASC
    * @param InFactionTag 攻击者阵营
    * @param InRollDirection 滚动方向（世界空间）
    * @param bKeepCurrentRotation 是否保持当前旋转（不被滚动方向覆盖）
    */
    UFUNCTION(BlueprintCallable, Category = "Rolling Log", meta = (DisplayName = "初始化滚木"))
    void InitializeRollingLog(
        UAbilitySystemComponent* InSourceASC,
        FGameplayTag InFactionTag,
        FVector InRollDirection,
        bool bKeepCurrentRotation = false  // ✨ 新增参数
    );
};
