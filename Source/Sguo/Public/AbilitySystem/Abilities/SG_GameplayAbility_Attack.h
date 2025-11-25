// 📄 文件：Source/Sguo/Public/AbilitySystem/Abilities/SG_GameplayAbility_Attack.h

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "SG_GameplayAbility_Attack.generated.h"

// 前向声明
class UAnimMontage;
class UGameplayEffect;
struct FSGUnitAttackDefinition; // ✨ 新增 - 前向声明

/**
 * @brief 攻击类型枚举
 */
UENUM(BlueprintType)
enum class ESGAttackAbilityType : uint8
{
    Melee   UMETA(DisplayName = "近战"),
    Ranged  UMETA(DisplayName = "远程"),
    Skill   UMETA(DisplayName = "技能")
};

/**
 * @brief 攻击能力基类（支持动态配置）
 */
UCLASS()
class SGUO_API USG_GameplayAbility_Attack : public UGameplayAbility
{
    GENERATED_BODY()

public:
    USG_GameplayAbility_Attack();

    // ========== 攻击配置（运行时动态设置）==========
    
    UPROPERTY(BlueprintReadWrite, Category = "Attack Config", meta = (DisplayName = "攻击类型"))
    ESGAttackAbilityType AttackType = ESGAttackAbilityType::Melee;

    UPROPERTY(BlueprintReadWrite, Category = "Attack Config", meta = (DisplayName = "攻击动画"))
    TObjectPtr<UAnimMontage> AttackMontage;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack Config", meta = (DisplayName = "伤害效果"))
    TSubclassOf<UGameplayEffect> DamageEffectClass;

    UPROPERTY(BlueprintReadWrite, Category = "Attack Config", meta = (DisplayName = "伤害倍率"))
    float DamageMultiplier = 1.0f;
 
    // ========== 能力接口 ==========
    
    virtual void ActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData
    ) override;

    virtual void EndAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        bool bReplicateEndAbility,
        bool bWasCancelled
    ) override;

protected:
    /**
  * @brief 投射物类（运行时设置，仅远程单位）
  */
    UPROPERTY(BlueprintReadWrite, Category = "Attack Config", meta = (DisplayName = "投射物类"))
    TSubclassOf<AActor> ProjectileClass;

    /**
     * @brief 投射物生成偏移（运行时设置，仅远程单位）
     */
    UPROPERTY(BlueprintReadWrite, Category = "Attack Config", meta = (DisplayName = "投射物生成偏移"))
    FVector ProjectileSpawnOffset = FVector(50.0f, 0.0f, 80.0f);

    // ✨ 新增 - 发射投射物到目标
    /**
     * @brief 发射投射物攻击目标
     * @param Target 目标 Actor
     * @details
     * 功能说明：
     * - 计算从当前位置到目标的抛物线弹道
     * - 生成 ProjectileClass 实例
     * - 初始化投射物参数
     */
    void SpawnProjectileToTarget(AActor* Target, const FVector* OverrideSpawnLocation = nullptr);

    // ✨ 新增 - 接收 Notify 发送的生成事件
    UFUNCTION()
    void OnSpawnProjectileEvent(FGameplayEventData Payload);
    
    // ✨ 新增 - 带完整参数的投射物生成函数
    /**
     * @brief 使用完整参数发射投射物
     * @param Target 目标 Actor
     * @param SpawnLocation 发射位置（世界空间）
     * @param SpawnRotation 发射旋转（世界空间）
     * @param OverrideSpeed 覆盖速度（0 = 使用默认）
     * @param GravityScale 重力缩放
     * @details
     * 功能说明：
     * - 使用 AnimNotify 提供的精确发射参数
     * - 支持覆盖投射物的速度和重力
     * - 计算到目标的弹道
     */
    void SpawnProjectileToTargetWithParams(
        AActor* Target,
        const FVector& SpawnLocation,
        const FRotator& SpawnRotation,
        float OverrideSpeed,
        float GravityScale
    );
    
    // ========== ✨ 新增 - 命中事件处理 ==========
	
    /**
     * @brief 处理攻击命中事件（从 AnimNotifyState 发送）
     * @param Payload 事件数据（包含目标和伤害倍率）
     */
    UFUNCTION()
    void OnAttackHitEvent(FGameplayEventData Payload);
    
    // ========== ✨ 新增 - 从单位加载攻击配置 ==========
    
    void LoadAttackConfigFromUnit();

    // ========== 攻击逻辑 ==========
    
    UFUNCTION()
    void OnMontageNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointPayload);

    UFUNCTION(BlueprintCallable, Category = "Attack")
    void PerformAttack();

    UFUNCTION(BlueprintCallable, Category = "Attack")
    virtual int32 FindTargetsInRange(TArray<AActor*>& OutTargets);

    UFUNCTION(BlueprintCallable, Category = "Attack")
    void ApplyDamageToTarget(AActor* Target);

    UFUNCTION(BlueprintPure, Category = "Attack")
    float GetAttackRange() const;



    // ========== 蓝图事件 ==========
    
    UFUNCTION(BlueprintImplementableEvent, Category = "Attack", meta = (DisplayName = "攻击命中时"))
    void OnAttackHit(const TArray<AActor*>& Targets);

    // ✨ 新增 - 带弧度参数的投射物生成函数
    /**
     * @brief 使用完整参数发射投射物（包含弧度控制）
     * @param Target 目标 Actor
     * @param SpawnLocation 发射位置
     * @param SpawnRotation 发射旋转
     * @param OverrideSpeed 覆盖速度（0 = 使用默认）
     * @param GravityScale 重力缩放
     * @param ArcParam 弧度参数（0-1，控制抛物线高度）
     */
    void SpawnProjectileWithArc(
        AActor* Target,
        const FVector& SpawnLocation,
        const FRotator& SpawnRotation,
        float OverrideSpeed,
        float GravityScale,
        float ArcParam
    );
};
