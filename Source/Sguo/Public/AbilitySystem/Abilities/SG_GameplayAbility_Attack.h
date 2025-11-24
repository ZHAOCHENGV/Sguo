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
};
