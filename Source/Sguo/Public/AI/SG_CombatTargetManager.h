// 📄 文件：Source/Sguo/Public/AI/SG_CombatTargetManager.h
// ✨ 新增 - 战斗目标管理器（带攻击槽位系统）

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayTagContainer.h"
#include "Units/SG_UnitsBase.h"
#include "SG_CombatTargetManager.generated.h"

class ASG_UnitsBase;
class ASG_MainCityBase;

/**
 * @brief 攻击槽位信息
 */
USTRUCT()
struct FSGAttackSlot
{
    GENERATED_BODY()

    // 槽位位置（相对于目标的偏移）
    UPROPERTY()
    FVector RelativePosition = FVector::ZeroVector;

    // 占据此槽位的单位
    UPROPERTY()
    TWeakObjectPtr<ASG_UnitsBase> OccupyingUnit;

    // 槽位是否被占据
    bool IsOccupied() const
    {
        return OccupyingUnit.IsValid() && !OccupyingUnit->bIsDead;
    }

    // 获取世界坐标
    FVector GetWorldPosition(AActor* Target) const
    {
        if (Target)
        {
            return Target->GetActorLocation() + RelativePosition;
        }
        return FVector::ZeroVector;
    }
};

/**
 * @brief 目标战斗信息
 */
USTRUCT()
struct FSGTargetCombatInfo
{
    GENERATED_BODY()

    // 攻击槽位列表
    UPROPERTY()
    TArray<FSGAttackSlot> AttackSlots;

    // 获取可用槽位数量
    int32 GetAvailableSlotCount() const
    {
        int32 Count = 0;
        for (const FSGAttackSlot& Slot : AttackSlots)
        {
            if (!Slot.IsOccupied())
            {
                Count++;
            }
        }
        return Count;
    }

    // 获取已占用槽位数量
    int32 GetOccupiedSlotCount() const
    {
        return AttackSlots.Num() - GetAvailableSlotCount();
    }
};

/**
 * @brief 战斗目标管理器
 * @details
 * 功能说明：
 * - 管理每个目标的攻击槽位
 * - 单位必须预约槽位才能攻击
 * - 槽位满了，单位必须选择其他目标
 */
UCLASS()
class SGUO_API USG_CombatTargetManager : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }

    // ========== 核心接口 ==========

    /**
     * @brief 为单位查找最佳目标（带槽位检查）
     * @param Querier 查询单位
     * @return 最佳目标，如果没有可用目标则返回 nullptr
     */
    UFUNCTION(BlueprintCallable, Category = "Combat", meta = (DisplayName = "查找最佳目标"))
    AActor* FindBestTargetWithSlot(ASG_UnitsBase* Querier);

    /**
     * @brief 尝试预约目标的攻击槽位
     * @param Attacker 攻击者
     * @param Target 目标
     * @param OutSlotPosition 输出：槽位世界坐标
     * @return 是否成功预约
     */
    UFUNCTION(BlueprintCallable, Category = "Combat", meta = (DisplayName = "预约攻击槽位"))
    bool TryReserveAttackSlot(ASG_UnitsBase* Attacker, AActor* Target, FVector& OutSlotPosition);

    /**
     * @brief 释放攻击槽位
     * @param Attacker 攻击者
     * @param Target 目标
     */
    UFUNCTION(BlueprintCallable, Category = "Combat", meta = (DisplayName = "释放攻击槽位"))
    void ReleaseAttackSlot(ASG_UnitsBase* Attacker, AActor* Target);

    /**
     * @brief 释放单位的所有槽位
     * @param Attacker 攻击者
     */
    UFUNCTION(BlueprintCallable, Category = "Combat", meta = (DisplayName = "释放所有槽位"))
    void ReleaseAllSlots(ASG_UnitsBase* Attacker);

    /**
     * @brief 检查目标是否有可用槽位
     * @param Target 目标
     * @return 是否有空闲槽位
     */
    UFUNCTION(BlueprintPure, Category = "Combat", meta = (DisplayName = "是否有可用槽位"))
    bool HasAvailableSlot(AActor* Target) const;

    /**
     * @brief 获取目标的已占用槽位数量
     */
    UFUNCTION(BlueprintPure, Category = "Combat", meta = (DisplayName = "获取已占用槽位数"))
    int32 GetOccupiedSlotCount(AActor* Target) const;

    /**
     * @brief 获取单位当前预约的槽位位置
     * @param Attacker 攻击者
     * @param Target 目标
     * @param OutPosition 输出：槽位位置
     * @return 是否找到
     */
    UFUNCTION(BlueprintPure, Category = "Combat")
    bool GetReservedSlotPosition(ASG_UnitsBase* Attacker, AActor* Target, FVector& OutPosition) const;

    // ========== 配置 ==========

    /** 普通单位的攻击槽位数量 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config", meta = (DisplayName = "单位槽位数量"))
    int32 UnitSlotCount = 8;

    /** 主城的攻击槽位数量 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config", meta = (DisplayName = "主城槽位数量"))
    int32 MainCitySlotCount = 20;

    /** 槽位距离目标的距离（攻击范围内） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config", meta = (DisplayName = "槽位距离"))
    float SlotDistance = 120.0f;

protected:
    /**
     * @brief 为目标初始化攻击槽位
     */
    void InitializeSlotsForTarget(AActor* Target);

    /**
     * @brief 获取或创建目标的战斗信息
     */
    FSGTargetCombatInfo& GetOrCreateCombatInfo(AActor* Target);

    /**
     * @brief 查找最近的可用槽位
     */
    int32 FindNearestAvailableSlot(AActor* Target, const FVector& AttackerLocation);

    /**
     * @brief 使用场景查询获取范围内的敌方单位
     */
    void QueryEnemiesInRange(ASG_UnitsBase* Querier, float Range, TArray<AActor*>& OutEnemies);

    /**
     * @brief 定期清理无效数据
     */
    void CleanupInvalidData();

private:
    // 目标 -> 战斗信息 映射
    UPROPERTY()
    TMap<TWeakObjectPtr<AActor>, FSGTargetCombatInfo> TargetCombatInfoMap;

    // 清理计时器
    FTimerHandle CleanupTimerHandle;
};
