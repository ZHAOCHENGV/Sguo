// 📄 文件：Source/Sguo/Public/Actors/SG_RollingLog.h
// 🔧 修改 - 完整文件，修复编译错误

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
class UNiagaraSystem;  // ✨ 新增 - 前置声明 Niagara 系统
class UAudioComponent;
class USoundBase;      // ✨ 新增 - 前置声明音效类

/**
 * @brief 滚木击中信息结构体
 * @details 包含滚木击中目标时的所有相关信息
 */
USTRUCT(BlueprintType)
struct FSGRollingLogHitInfo
{
    GENERATED_BODY()

    /** 被击中的 Actor */
    UPROPERTY(BlueprintReadOnly, Category = "Hit Info", meta = (DisplayName = "击中目标"))
    AActor* HitActor = nullptr;

    /** 击中的世界位置 */
    UPROPERTY(BlueprintReadOnly, Category = "Hit Info", meta = (DisplayName = "击中位置"))
    FVector HitLocation = FVector::ZeroVector;

    /** 击退方向 */
    UPROPERTY(BlueprintReadOnly, Category = "Hit Info", meta = (DisplayName = "击退方向"))
    FVector KnockbackDirection = FVector::ZeroVector;

    /** 滚木滚动方向 */
    UPROPERTY(BlueprintReadOnly, Category = "Hit Info", meta = (DisplayName = "滚动方向"))
    FVector RollDirection = FVector::ForwardVector;
};

// 前置声明本类（用于委托）
class ASG_RollingLog;

/** 滚木击中事件委托 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSGRollingLogHitSignature, const FSGRollingLogHitInfo&, HitInfo);

/** 滚木销毁事件委托 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSGRollingLogDestroyedSignature, ASG_RollingLog*, DestroyedLog);

/**
 * @brief 滚木Actor类
 * 
 * @details
 * **功能说明：**
 * - 流木计生成的滚动木桩
 * - 沿指定方向滚动
 * - 击中敌人时造成伤害并击退
 * - 击中一个目标后破碎销毁
 * 
 * **详细流程：**
 * 1. 由 SG_StrategyEffect_RollingLog 生成
 * 2. 沿指定方向滚动
 * 3. 检测与敌方单位的碰撞
 * 4. 击中时应用伤害和击退效果
 * 5. 击中后播放破碎特效并销毁
 * 
 * **注意事项：**
 * - 只击中敌方单位
 * - 击中一个目标后立即破碎
 * - 超出范围或超时后自动销毁
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

    /** 击中目标事件 */
    UPROPERTY(BlueprintAssignable, Category = "Rolling Log Events", meta = (DisplayName = "击中目标事件"))
    FSGRollingLogHitSignature OnLogHitTarget;

    /** 滚木销毁事件 */
    UPROPERTY(BlueprintAssignable, Category = "Rolling Log Events", meta = (DisplayName = "滚木销毁事件"))
    FSGRollingLogDestroyedSignature OnLogDestroyed;

    // ==================== 组件 ====================

    /**
     * @brief 场景根组件
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "场景根"))
    TObjectPtr<USceneComponent> SceneRoot;

    /**
     * @brief 碰撞胶囊体组件
     * @details 用于检测与单位的碰撞，横向放置模拟滚木形状
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "碰撞胶囊体"))
    TObjectPtr<UCapsuleComponent> CollisionCapsule;

    /**
     * @brief 网格体组件
     * @details 显示滚木的视觉效果
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "网格体"))
    TObjectPtr<UStaticMeshComponent> MeshComponent;

    // ==================== 滚动配置 ====================

    /**
     * @brief 滚动速度（厘米/秒）
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Rolling Config", meta = (DisplayName = "滚动速度", ClampMin = "100.0", UIMin = "100.0", UIMax = "3000.0"))
    float RollSpeed = 800.0f;

    /**
     * @brief 滚动旋转速度（度/秒）
     * @details 滚木视觉上的旋转速度
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Rolling Config", meta = (DisplayName = "旋转速度", ClampMin = "0.0", UIMin = "0.0", UIMax = "1080.0"))
    float RotationSpeed = 360.0f;

    /**
     * @brief 最大滚动距离（厘米）
     * @details 超过此距离后自动销毁
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Rolling Config", meta = (DisplayName = "最大滚动距离", ClampMin = "100.0", UIMin = "100.0", UIMax = "10000.0"))
    float MaxRollDistance = 3000.0f;

    /**
     * @brief 生存时间（秒）
     * @details 超过此时间后自动销毁
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Rolling Config", meta = (DisplayName = "生存时间", ClampMin = "1.0", UIMin = "1.0", UIMax = "30.0"))
    float LogLifeSpan = 10.0f;  // 🔧 修改 - 重命名避免与 AActor::LifeSpan 冲突

    // ==================== 伤害配置 ====================

    /**
     * @brief 伤害值
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Damage Config", meta = (DisplayName = "伤害值", ClampMin = "0.0", UIMin = "0.0"))
    float DamageAmount = 100.0f;

    /**
     * @brief 伤害效果类
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage Config", meta = (DisplayName = "伤害效果"))
    TSubclassOf<UGameplayEffect> DamageEffectClass;

    // ==================== 击退配置 ====================

    /**
     * @brief 击退距离（厘米）
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Knockback Config", meta = (DisplayName = "击退距离", ClampMin = "0.0", UIMin = "0.0", UIMax = "1000.0"))
    float KnockbackDistance = 300.0f;

    /**
     * @brief 击退持续时间（秒）
     * @details 击退效果的持续时间
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Knockback Config", meta = (DisplayName = "击退持续时间", ClampMin = "0.1", UIMin = "0.1", UIMax = "2.0"))
    float KnockbackDuration = 0.3f;

    /**
     * @brief 击退曲线
     * @details 控制击退速度随时间的变化，如果为空则使用线性插值
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Knockback Config", meta = (DisplayName = "击退曲线"))
    TObjectPtr<UCurveFloat> KnockbackCurve;

    // ==================== 视觉效果配置 ====================
    // 🔧 修改 - 使用原始指针而非 TObjectPtr，避免 Niagara 类型问题

    /**
     * @brief 破碎特效
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Visual Effects", meta = (DisplayName = "破碎粒子特效"))
    UNiagaraSystem* BreakParticleSystem;

    /**
     * @brief 滚动尘土特效
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Visual Effects", meta = (DisplayName = "滚动尘土特效"))
    UNiagaraSystem* RollDustParticleSystem;

    /**
     * @brief 破碎音效
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Visual Effects", meta = (DisplayName = "破碎音效"))
    USoundBase* BreakSound;

    /**
     * @brief 滚动音效
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Visual Effects", meta = (DisplayName = "滚动音效"))
    USoundBase* RollSound;

    // ==================== 运行时数据 ====================

    /**
     * @brief 攻击者的能力系统组件
     */
    UPROPERTY(Transient, BlueprintReadOnly, Category = "Runtime", meta = (DisplayName = "攻击者ASC"))
    UAbilitySystemComponent* SourceASC;  // 🔧 修改 - 使用原始指针并重命名

    /**
     * @brief 攻击者的阵营标签
     */
    UPROPERTY(Transient, BlueprintReadOnly, Category = "Runtime", meta = (DisplayName = "攻击者阵营"))
    FGameplayTag SourceFactionTag;  // 🔧 修改 - 重命名

public:
    // ==================== 初始化接口 ====================

    /**
     * @brief 初始化滚木
     * @param InSourceASC 攻击者 ASC
     * @param InFactionTag 攻击者阵营
     * @param InRollDirection 滚动方向（世界空间）
     * 
     * @details
     * **功能说明：**
     * - 设置滚木的攻击者信息
     * - 设置滚动方向
     * - 初始化移动参数
     */
    UFUNCTION(BlueprintCallable, Category = "Rolling Log", meta = (DisplayName = "初始化滚木"))
    void InitializeRollingLog(
        UAbilitySystemComponent* InSourceASC,
        FGameplayTag InFactionTag,
        FVector InRollDirection
    );

    /**
     * @brief 设置滚动方向
     * @param NewDirection 新的滚动方向
     */
    UFUNCTION(BlueprintCallable, Category = "Rolling Log", meta = (DisplayName = "设置滚动方向"))
    void SetRollDirection(FVector NewDirection);

    /**
     * @brief 获取滚动方向
     * @return 当前滚动方向
     */
    UFUNCTION(BlueprintPure, Category = "Rolling Log", meta = (DisplayName = "获取滚动方向"))
    FVector GetRollDirection() const { return RollDirection; }

    /**
     * @brief 获取已滚动距离
     * @return 已滚动的距离
     */
    UFUNCTION(BlueprintPure, Category = "Rolling Log", meta = (DisplayName = "获取已滚动距离"))
    float GetRolledDistance() const { return RolledDistance; }

    /**
     * @brief 手动销毁滚木（播放破碎效果）
     */
    UFUNCTION(BlueprintCallable, Category = "Rolling Log", meta = (DisplayName = "破碎销毁"))
    void BreakAndDestroy();

protected:
    // ==================== 内部状态 ====================

    /** 滚动方向（归一化） */
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

    /** 滚动尘土特效组件 */
    UPROPERTY()
    TObjectPtr<UNiagaraComponent> DustEffectComponent;

    /** 滚动音效组件 */
    UPROPERTY()
    TObjectPtr<UAudioComponent> RollAudioComponent;

protected:
    // ==================== 内部函数 ====================

    /**
     * @brief 更新滚动位置
     * @param DeltaTime 帧间隔
     */
    void UpdateRolling(float DeltaTime);

    /**
     * @brief 更新视觉旋转
     * @param DeltaTime 帧间隔
     */
    void UpdateVisualRotation(float DeltaTime);

    /**
     * @brief 碰撞检测回调
     */
    UFUNCTION()
    void OnCapsuleOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult
    );

    /**
     * @brief 处理击中目标
     * @param HitActor 被击中的 Actor
     * @param HitLocation 击中位置
     */
    void HandleHitTarget(AActor* HitActor, const FVector& HitLocation);

    /**
     * @brief 应用伤害到目标
     * @param Target 目标 Actor
     */
    void ApplyDamageToTarget(AActor* Target);

    /**
     * @brief 应用击退效果
     * @param Target 目标 Actor
     * @param KnockbackDir 击退方向
     */
    void ApplyKnockbackToTarget(AActor* Target, const FVector& KnockbackDir);

    /**
     * @brief 播放破碎特效
     */
    void PlayBreakEffects();

    /**
     * @brief 启动滚动特效
     */
    void StartRollingEffects();

    /**
     * @brief 停止滚动特效
     */
    void StopRollingEffects();

public:
    // ==================== 蓝图事件 ====================

    /**
     * @brief 击中目标蓝图事件
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "Rolling Log", meta = (DisplayName = "On Hit Target (BP)"))
    void K2_OnHitTarget(const FSGRollingLogHitInfo& HitInfo);

    /**
     * @brief 滚木破碎蓝图事件
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "Rolling Log", meta = (DisplayName = "On Log Break (BP)"))
    void K2_OnLogBreak(FVector BreakLocation);

    /**
     * @brief 滚木超出范围蓝图事件
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "Rolling Log", meta = (DisplayName = "On Log Out Of Range (BP)"))
    void K2_OnLogOutOfRange();
};
