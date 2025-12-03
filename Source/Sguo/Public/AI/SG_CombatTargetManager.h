// 📄 文件：Source/Sguo/Public/AI/SG_CombatTargetManager.h
// 🔧 修改 - 添加调试可视化和标签过滤功能

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayTagContainer.h"
#include "Units/SG_UnitsBase.h"
#include "SG_CombatTargetManager.generated.h"

class ASG_UnitsBase;
class ASG_MainCityBase;

// ✨ 新增 - 槽位状态枚举（用于可视化）
/**
 * @brief 攻击槽位状态
 * @details
 * - Free: 空闲状态（绿色）
 * - Reserved: 已预约但未到达（蓝色）
 * - Occupied: 已到达正在攻击（红色）
 */
UENUM(BlueprintType)
enum class ESGSlotStatus : uint8
{
    Free        UMETA(DisplayName = "空闲"),
    Reserved    UMETA(DisplayName = "已预约"),
    Occupied    UMETA(DisplayName = "已占用")
};

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

    // ✨ 新增 - 槽位状态
    UPROPERTY()
    ESGSlotStatus Status = ESGSlotStatus::Free;

    // 槽位是否被占据
    bool IsOccupied() const
    {
        return OccupyingUnit.IsValid() && !OccupyingUnit->bIsDead;
    }

    // ✨ 新增 - 获取槽位状态
    ESGSlotStatus GetStatus() const
    {
        if (!OccupyingUnit.IsValid() || OccupyingUnit->bIsDead)
        {
            return ESGSlotStatus::Free;
        }
        return Status;
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
 * - ✨ 新增：调试可视化系统
 * - ✨ 新增：基于标签的槽位占用控制
 */
UCLASS()
class SGUO_API USG_CombatTargetManager : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }

    // ✨ 新增 - Tick 用于调试绘制
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;

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

    // ✨ 新增 - 更新槽位状态（单位到达槽位时调用）
    /**
     * @brief 更新槽位状态为已占用
     * @param Attacker 攻击者
     * @param Target 目标
     * @details 当单位到达攻击位置时调用，将状态从 Reserved 改为 Occupied
     */
    UFUNCTION(BlueprintCallable, Category = "Combat", meta = (DisplayName = "标记槽位已占用"))
    void MarkSlotAsOccupied(ASG_UnitsBase* Attacker, AActor* Target);

    // ✨ 新增 - 检查单位是否需要占用槽位
    /**
     * @brief 检查单位是否需要占用攻击槽位
     * @param Unit 单位
     * @return 是否需要占用槽位
     * @details
     * 功能说明：
     * - 检查单位的 UnitTypeTag 是否在 SlotRequiredTags 中
     * - 远程单位通常不需要占用槽位
     */
    UFUNCTION(BlueprintPure, Category = "Combat", meta = (DisplayName = "是否需要占用槽位"))
    bool DoesUnitRequireSlot(ASG_UnitsBase* Unit) const;

    // ========== 配置 ==========

    /** 普通单位的攻击槽位数量 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config", meta = (DisplayName = "单位槽位数量"))
    int32 UnitSlotCount = 8;

    /** 槽位距离目标的距离（攻击范围内） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config", meta = (DisplayName = "槽位距离"))
    float SlotDistance = 120.0f;

    // ✨ 新增 - 需要占用槽位的单位类型标签
    /**
     * @brief 需要占用攻击槽位的单位类型标签
     * @details
     * 功能说明：
     * - 只有拥有这些标签的单位才需要占用攻击槽位
     * - 远程单位（如弓箭手）不在此列表中，则不占用槽位
     * 使用示例：
     * - Unit.Type.Infantry（步兵）
     * - Unit.Type.Cavalry（骑兵）
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config", meta = (DisplayName = "需要槽位的单位标签"))
    FGameplayTagContainer SlotRequiredTags;

    // ========== ✨ 新增 - 调试可视化配置 ==========

    /** 是否启用槽位调试可视化 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", meta = (DisplayName = "显示攻击槽位"))
    bool bShowAttackSlots = false;

    /** 空闲槽位颜色（绿色） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", meta = (DisplayName = "空闲槽位颜色"))
    FColor SlotFreeColor = FColor::Green;

    /** 已预约槽位颜色（蓝色） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", meta = (DisplayName = "预约槽位颜色"))
    FColor SlotReservedColor = FColor::Blue;

    /** 已占用槽位颜色（红色） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", meta = (DisplayName = "占用槽位颜色"))
    FColor SlotOccupiedColor = FColor::Red;

    /** 槽位调试球体半径 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", meta = (DisplayName = "槽位显示半径"))
    float SlotDebugRadius = 30.0f;

    /** 是否显示槽位连线（从单位到槽位） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", meta = (DisplayName = "显示槽位连线"))
    bool bShowSlotConnections = true;

    /** 是否显示槽位文字信息 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", meta = (DisplayName = "显示槽位文字"))
    bool bShowSlotText = true;

    // ✨ 新增 - 调试开关函数
    /**
     * @brief 切换槽位可视化显示
     */
    UFUNCTION(BlueprintCallable, Category = "Debug", meta = (DisplayName = "切换槽位显示"))
    void ToggleSlotVisualization();

    /**
     * @brief 设置槽位可视化显示
     * @param bEnable 是否启用
     */
    UFUNCTION(BlueprintCallable, Category = "Debug", meta = (DisplayName = "设置槽位显示"))
    void SetSlotVisualization(bool bEnable);

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

    // ✨ 新增 - 绘制调试信息
    /**
     * @brief 绘制所有槽位的调试可视化
     */
    void DrawDebugSlots();

    /**
     * @brief 绘制单个目标的槽位
     * @param Target 目标
     * @param CombatInfo 战斗信息
     */
    void DrawDebugSlotsForTarget(AActor* Target, const FSGTargetCombatInfo& CombatInfo);

private:
    // 目标 -> 战斗信息 映射
    UPROPERTY()
    TMap<TWeakObjectPtr<AActor>, FSGTargetCombatInfo> TargetCombatInfoMap;

    // 清理计时器
    FTimerHandle CleanupTimerHandle;
};
