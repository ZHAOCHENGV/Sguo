// 📄 文件：Source/Sguo/Public/AbilitySystem/Abilities/SG_GameplayAbility_SummonGroup.h
// 🔧 修改 - 增加数据表自动获取蒙太奇、事件等待、位置朝向配置

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "SG_GameplayAbility_SummonGroup.generated.h"

class ASG_UnitsBase;

/**
 * @brief 召唤单位朝向类型
 */
UENUM(BlueprintType)
enum class ESGSummonRotationType : uint8
{
    SameAsOwner     UMETA(DisplayName = "跟随施法者"),
    FaceOutwards    UMETA(DisplayName = "背向中心(朝外)"),
    Random          UMETA(DisplayName = "随机朝向"),
    FaceTarget      UMETA(DisplayName = "朝向当前目标")
};

/**
 * @brief 召唤中心位置类型
 */
UENUM(BlueprintType)
enum class ESGSummonLocationType : uint8
{
    BehindOwner     UMETA(DisplayName = "施法者后方"),
    InFrontOfOwner  UMETA(DisplayName = "施法者前方"),
    AtTargetLocation UMETA(DisplayName = "目标位置"),
    AroundOwner     UMETA(DisplayName = "施法者周围")
};

USTRUCT(BlueprintType)
struct FSGSummonUnitOption
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "单位类"))
    TSubclassOf<ASG_UnitsBase> UnitClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "权重"))
    float RandomWeight = 1.0f;
};

/**
 * @brief 通用兵团召唤能力
 */
UCLASS()
class SGUO_API USG_GameplayAbility_SummonGroup : public UGameplayAbility
{
    GENERATED_BODY()

public:
    USG_GameplayAbility_SummonGroup();

    // ========== 触发配置 (自动获取蒙太奇) ==========

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Trigger", meta = (DisplayName = "触发事件标签", ToolTip = "蒙太奇中 SendGameplayEvent 发送的 Tag"))
    FGameplayTag TriggerEventTag;

    // ========== 召唤配置 ==========

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Summon Config", meta = (DisplayName = "可能的兵种列表"))
    TArray<FSGSummonUnitOption> PossibleUnits;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Summon Config", meta = (DisplayName = "召唤总数量"))
    int32 SpawnCount = 6;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Summon Config", meta = (DisplayName = "方阵每行人数", ToolTip = "0表示单行"))
    int32 UnitsPerRow = 3;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Summon Config", meta = (DisplayName = "单位间距"))
    float UnitSpacing = 150.0f;

    // ✨ 新增 - 位置与朝向控制
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Summon Config|Location", meta = (DisplayName = "生成位置类型"))
    ESGSummonLocationType LocationType = ESGSummonLocationType::BehindOwner;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Summon Config|Location", meta = (DisplayName = "生成距离偏移", ToolTip = "基于位置类型的距离偏移（如后方300米）"))
    float SpawnDistanceOffset = 300.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Summon Config|Location", meta = (DisplayName = "生成随机范围", ToolTip = "在最终计算的位置上增加随机偏移半径"))
    float SpawnRandomRange = 50.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Summon Config|Rotation", meta = (DisplayName = "单位朝向类型"))
    ESGSummonRotationType RotationType = ESGSummonRotationType::SameAsOwner;

    // ========== 能力接口 ==========

    virtual void ActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData
    ) override;

protected:
    // 等待事件的回调
    UFUNCTION()
    void OnSpawnEventReceived(FGameplayEventData Payload);

    UFUNCTION()
    void OnMontageCompleted();

    UFUNCTION()
    void OnMontageCancelled();
    
    // 执行召唤逻辑
    void ExecuteSpawn();

    // 辅助：从 UnitDataTable 获取蒙太奇
    UAnimMontage* FindMontageFromUnitData() const;

    TSubclassOf<ASG_UnitsBase> GetRandomUnitClass() const;
    FVector CalculateSpawnLocation(int32 Index, const FVector& CenterLocation, const FRotator& BaseRotation) const;
    FRotator CalculateSpawnRotation(const FVector& SpawnLocation, const FVector& CenterLocation, const FRotator& OwnerRotation) const;
};