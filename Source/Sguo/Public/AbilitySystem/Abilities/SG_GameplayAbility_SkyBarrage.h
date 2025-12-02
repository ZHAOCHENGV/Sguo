// 📄 文件：Source/Sguo/Public/AbilitySystem/Abilities/SG_GameplayAbility_SkyBarrage.h
// 🔧 修改 - 增加飞行参数配置

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "SG_GameplayAbility_SkyBarrage.generated.h"

class ASG_Projectile;

/**
 * @brief 通用高空打击能力（剑雨/箭雨）
 */
UCLASS()
class SGUO_API USG_GameplayAbility_SkyBarrage : public UGameplayAbility
{
    GENERATED_BODY()

public:
    USG_GameplayAbility_SkyBarrage();

    // ========== 触发配置 ==========
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Trigger")
    FGameplayTag TriggerEventTag;

    // ========== 打击配置 ==========

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Barrage Config", meta = (DisplayName = "投射物类"))
    TSubclassOf<ASG_Projectile> ProjectileClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Barrage Config", meta = (DisplayName = "打击中心距离"))
    float TargetDistance = 800.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Barrage Config", meta = (DisplayName = "打击区域半径"))
    float AreaRadius = 500.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Barrage Config", meta = (DisplayName = "投射物总数量"))
    int32 TotalProjectiles = 30;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Barrage Config", meta = (DisplayName = "持续时间"))
    float Duration = 2.0f;

    // 🔧 MODIFIED - 替换原本单一的 SpawnHeight，改为向量偏移，控制发射方向
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Barrage Config|Flight", meta = (DisplayName = "生成位置偏移", ToolTip = "相对于打击中心的偏移。例如 (0,0,1000) 为正上方，(500,0,500) 为侧上方斜射"))
    FVector SpawnOriginOffset = FVector(0.0f, 0.0f, 1000.0f);
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Barrage Config|Flight", meta = (DisplayName = "生成范围抖动"))
    float SpawnSourceSpread = 200.0f;

    /**
     * @brief 伤害倍率
     * @details 此技能生成的投射物的伤害倍率（例如 0.5 表示 50% 攻击力）
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Barrage Config|Damage", meta = (DisplayName = "伤害倍率", ClampMin = "0.0"))
    float DamageMultiplier = 1.0f;
    
    // ✨ NEW - 速度与朝向控制
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Barrage Config|Flight", meta = (DisplayName = "覆盖飞行速度", ToolTip = "-1 表示使用投射物默认速度"))
    float OverrideFlightSpeed = -1.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Barrage Config|Flight", meta = (DisplayName = "覆盖生成朝向", ToolTip = "投射物生成时的初始朝向，通常朝向下落方向"))
    FRotator OverrideSpawnRotation = FRotator(-90.0f, 0.0f, 0.0f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Barrage Config|Flight", meta = (DisplayName = "自动朝向目标", ToolTip = "如果为true，将忽略 OverrideSpawnRotation，自动计算从生成点到落点的朝向"))
    bool bAutoRotateToTarget = true;

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
    UFUNCTION()
    void OnStartBarrageEvent(FGameplayEventData Payload);

    void StartBarrageLoop();

    // ✨ 新增
    UFUNCTION()
    void OnMontageCompleted();

    UFUNCTION()
    void OnMontageCancelled();

    UFUNCTION()
    void SpawnProjectileLoop();

    UAnimMontage* FindMontageFromUnitData() const;

    FTimerHandle BarrageTimerHandle;
    int32 ProjectilesSpawned = 0;
    float IntervalPerShot = 0.1f;
    FVector CachedTargetCenter;
};