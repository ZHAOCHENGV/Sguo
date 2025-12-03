// 📄 文件：Source/Sguo/Public/AI/SG_CombatTargetManager.h
// 🔧 修改 - 添加三色调试可视化和基于标签的槽位占用控制
// ✅ 这是完整文件

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayTagContainer.h"
// ✨ 新增 - Tickable 接口，用于每帧绘制调试信息
#include "Tickable.h"
#include "SG_CombatTargetManager.generated.h"

// 前置声明
class ASG_UnitsBase;
class ASG_MainCityBase;

// ✨ 新增 - 槽位状态枚举
/**
 * @brief 攻击槽位状态枚举
 * @details
 * 功能说明：
 * - 用于区分槽位的三种状态，便于调试可视化
 * - Available: 空闲状态，显示为绿色
 * - Reserved: 已预约状态（攻击者正在移动中），显示为蓝色
 * - Occupied: 已到达状态（攻击者已在槽位附近），显示为红色
 */
UENUM(BlueprintType)
enum class ESGAttackSlotState : uint8
{
    Available   UMETA(DisplayName = "空闲"),      // 绿色 - 没有被占用
    Reserved    UMETA(DisplayName = "已预约"),    // 蓝色 - 被预约但攻击者还没到达
    Occupied    UMETA(DisplayName = "已到达")     // 红色 - 攻击者已经到达槽位
};

/**
 * @brief 攻击槽位信息结构体
 * @details
 * 功能说明：
 * - 记录单个槽位的角度和占用单位
 * - 提供槽位状态计算和世界坐标转换功能
 */
USTRUCT()
struct FSGAttackSlot
{
    GENERATED_BODY()

    // 槽位角度（相对于目标的角度，0-360度）
    UPROPERTY()
    float Angle = 0.0f;

    // 占据此槽位的单位（使用弱引用防止循环引用）
    UPROPERTY()
    TWeakObjectPtr<ASG_UnitsBase> OccupyingUnit;

    /**
     * @brief 检查槽位是否被占据
     * @return 是否有有效单位占据此槽位
     */
    bool IsOccupied() const;

    // ✨ 新增 - 获取槽位状态（用于调试可视化）
    /**
     * @brief 获取槽位当前状态
     * @param Target 目标 Actor
     * @param TargetRadius 目标碰撞半径
     * @param ArrivalThreshold 到达判定阈值（距离小于此值认为已到达）
     * @return 槽位状态枚举值
     * @details
     * 功能说明：
     * - 未被占用返回 Available
     * - 被占用但攻击者距离槽位超过阈值返回 Reserved
     * - 被占用且攻击者已到达槽位返回 Occupied
     */
    ESGAttackSlotState GetSlotState(AActor* Target, float TargetRadius, float ArrivalThreshold = 100.0f) const;

    /**
     * @brief 获取槽位的世界坐标
     * @param Target 目标 Actor
     * @param AttackerAttackRange 攻击者的攻击范围
     * @param TargetRadius 目标碰撞半径
     * @return 槽位在世界空间中的位置
     */
    FVector GetWorldPosition(AActor* Target, float AttackerAttackRange, float TargetRadius) const;

    // ✨ 新增 - 使用默认攻击范围获取槽位位置（用于调试绘制）
    /**
     * @brief 获取槽位的世界坐标（使用默认攻击范围）
     * @param Target 目标 Actor
     * @param TargetRadius 目标碰撞半径
     * @param DefaultAttackRange 默认攻击范围
     * @return 槽位在世界空间中的位置
     */
    FVector GetWorldPositionWithDefault(AActor* Target, float TargetRadius, float DefaultAttackRange = 150.0f) const;
};

/**
 * @brief 目标战斗信息结构体
 * @details
 * 功能说明：
 * - 存储某个目标的所有攻击槽位
 * - 缓存目标的碰撞半径
 */
USTRUCT()
struct FSGTargetCombatInfo
{
    GENERATED_BODY()

    // 攻击槽位列表
    UPROPERTY()
    TArray<FSGAttackSlot> AttackSlots;

    // 目标的碰撞半径（缓存，避免重复计算）
    UPROPERTY()
    float TargetRadius = 50.0f;

    /**
     * @brief 获取可用槽位数量
     * @return 未被占用的槽位数量
     */
    int32 GetAvailableSlotCount() const;

    /**
     * @brief 获取已占用槽位数量
     * @return 被占用的槽位数量
     */
    int32 GetOccupiedSlotCount() const;

    // ✨ 新增 - 获取各状态槽位数量（用于调试显示）
    /**
     * @brief 获取各状态槽位数量
     * @param Target 目标 Actor
     * @param OutAvailable 输出：空闲槽位数
     * @param OutReserved 输出：预约槽位数
     * @param OutOccupied 输出：已到达槽位数
     * @param ArrivalThreshold 到达判定阈值
     */
    void GetSlotStateCounts(AActor* Target, int32& OutAvailable, int32& OutReserved, int32& OutOccupied, float ArrivalThreshold = 100.0f) const;
};

/**
 * @brief 战斗目标管理器（World Subsystem）
 * @details
 * 功能说明：
 * - 管理每个目标周围的攻击槽位
 * - 槽位距离根据攻击者的攻击范围动态计算
 * - ✨ 新增 - 三色调试可视化系统（绿/蓝/红）
 * - ✨ 新增 - 基于 GameplayTag 的槽位占用控制
 * 使用方式：
 * - 通过 GetWorld()->GetSubsystem<USG_CombatTargetManager>() 获取
 * 注意事项：
 * - 远程单位可配置为不占用槽位
 * - 调试可视化可通过控制台命令或蓝图切换
 */
UCLASS()
class SGUO_API USG_CombatTargetManager : public UWorldSubsystem, public FTickableGameObject
{
    GENERATED_BODY()

public:
    // ========== 生命周期 ==========
    
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }

    // ========== ✨ 新增 - FTickableGameObject 接口实现 ==========
    
    /**
     * @brief 每帧 Tick（用于绘制调试信息）
     * @param DeltaTime 帧间隔时间
     */
    virtual void Tick(float DeltaTime) override;
    
    /**
     * @brief 获取统计 ID（性能分析用）
     */
    virtual TStatId GetStatId() const override
    {
        RETURN_QUICK_DECLARE_CYCLE_STAT(USG_CombatTargetManager, STATGROUP_Tickables);
    }
    
    /**
     * @brief 是否可以 Tick（只有开启调试可视化时才 Tick）
     */
    virtual bool IsTickable() const override { return bShowDebugVisualization; }
    
    /**
     * @brief 暂停时是否 Tick
     */
    virtual bool IsTickableWhenPaused() const override { return false; }
    
    /**
     * @brief 编辑器中是否 Tick
     */
    virtual bool IsTickableInEditor() const override { return false; }
    
    /**
     * @brief 获取 Tickable 所在的 World
     */
    virtual UWorld* GetTickableGameObjectWorld() const override { return GetWorld(); }

    // ========== 核心接口 ==========

    /**
     * @brief 为单位查找最佳目标（带槽位检查）
     * @param Querier 查询单位
     * @return 最佳目标 Actor，如果没有可用目标则返回 nullptr
     */
    UFUNCTION(BlueprintCallable, Category = "Combat", meta = (DisplayName = "查找最佳目标"))
    AActor* FindBestTargetWithSlot(ASG_UnitsBase* Querier);

    /**
     * @brief 尝试预约目标的攻击槽位
     * @param Attacker 攻击单位
     * @param Target 目标 Actor
     * @param OutSlotPosition 输出：预约成功后的槽位世界坐标
     * @return 是否预约成功
     */
    UFUNCTION(BlueprintCallable, Category = "Combat", meta = (DisplayName = "预约攻击槽位"))
    bool TryReserveAttackSlot(ASG_UnitsBase* Attacker, AActor* Target, FVector& OutSlotPosition);

    /**
     * @brief 释放攻击槽位
     * @param Attacker 攻击单位
     * @param Target 目标 Actor
     */
    UFUNCTION(BlueprintCallable, Category = "Combat", meta = (DisplayName = "释放攻击槽位"))
    void ReleaseAttackSlot(ASG_UnitsBase* Attacker, AActor* Target);

    /**
     * @brief 释放单位的所有槽位
     * @param Attacker 攻击单位
     */
    UFUNCTION(BlueprintCallable, Category = "Combat", meta = (DisplayName = "释放所有槽位"))
    void ReleaseAllSlots(ASG_UnitsBase* Attacker);

    /**
     * @brief 检查目标是否有可用槽位
     * @param Target 目标 Actor
     * @return 是否有可用槽位
     */
    UFUNCTION(BlueprintPure, Category = "Combat", meta = (DisplayName = "是否有可用槽位"))
    bool HasAvailableSlot(AActor* Target) const;

    /**
     * @brief 获取目标的已占用槽位数量
     * @param Target 目标 Actor
     * @return 已占用的槽位数量
     */
    UFUNCTION(BlueprintPure, Category = "Combat", meta = (DisplayName = "获取已占用槽位数"))
    int32 GetOccupiedSlotCount(AActor* Target) const;

    /**
     * @brief 获取单位当前预约的槽位位置
     * @param Attacker 攻击单位
     * @param Target 目标 Actor
     * @param OutPosition 输出：槽位世界坐标
     * @return 是否找到预约的槽位
     */
    UFUNCTION(BlueprintPure, Category = "Combat")
    bool GetReservedSlotPosition(ASG_UnitsBase* Attacker, AActor* Target, FVector& OutPosition) const;

    /**
     * @brief 使用球形检测获取范围内的敌方单位
     * @param Querier 查询单位
     * @param Range 检测范围
     * @param OutEnemies 输出：敌方单位列表
     */
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void QueryEnemiesInRange(ASG_UnitsBase* Querier, float Range, TArray<AActor*>& OutEnemies);

    // ========== ✨ 新增 - 槽位占用标签检查 ==========

    /**
     * @brief 检查单位是否需要占用攻击槽位
     * @param Unit 要检查的单位
     * @return 是否需要占用槽位
     * @details
     * 功能说明：
     * - 检查单位的 UnitTypeTag 是否在 SlotOccupyingUnitTypes 中
     * - 如果 SlotOccupyingUnitTypes 为空，默认所有单位都占用槽位（向后兼容）
     * - 远程单位不应该占用槽位，可以站在任意位置攻击
     * 使用场景：
     * - 近战单位（步兵、骑兵）：需要占用槽位
     * - 远程单位（弓箭手、弩手）：不需要占用槽位
     */
    UFUNCTION(BlueprintPure, Category = "Combat|Slot", meta = (DisplayName = "单位是否需要占用槽位"))
    bool ShouldUnitOccupySlot(const ASG_UnitsBase* Unit) const;

    // ========== ✨ 新增 - 调试可视化接口 ==========

    /**
     * @brief 切换调试可视化显示
     * @details 调用一次开启，再调用一次关闭
     */
    UFUNCTION(BlueprintCallable, Category = "Combat|Debug", meta = (DisplayName = "切换槽位调试显示"))
    void ToggleDebugVisualization();

    /**
     * @brief 设置调试可视化显示状态
     * @param bEnable 是否启用
     */
    UFUNCTION(BlueprintCallable, Category = "Combat|Debug", meta = (DisplayName = "设置槽位调试显示"))
    void SetDebugVisualization(bool bEnable);

    /**
     * @brief 获取调试可视化状态
     * @return 是否启用调试显示
     */
    UFUNCTION(BlueprintPure, Category = "Combat|Debug", meta = (DisplayName = "获取槽位调试显示状态"))
    bool IsDebugVisualizationEnabled() const { return bShowDebugVisualization; }

    // ========== 槽位配置 ==========

    /** 普通单位的攻击槽位数量 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config", meta = (DisplayName = "单位槽位数量"))
    int32 UnitSlotCount = 8;

    /** 主城的攻击槽位数量 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config", meta = (DisplayName = "主城槽位数量"))
    int32 MainCitySlotCount = 20;

    /** 槽位距离系数（相对于攻击范围的比例） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config", meta = (DisplayName = "槽位距离系数", ClampMin = "0.5", ClampMax = "1.0"))
    float SlotDistanceRatio = 0.8f;

    // ========== ✨ 新增 - 槽位占用标签配置 ==========

    /**
     * @brief 需要占用攻击槽位的单位类型标签
     * @details
     * 功能说明：
     * - 只有拥有这些标签之一的单位才会占用攻击槽位
     * - 远程单位（如弓箭手、弩手）不应该占用槽位
     * - 近战单位（如步兵、骑兵）应该占用槽位
     * 使用方式：
     * - 添加需要占用槽位的单位类型标签
     * - 例如：Unit.Type.Infantry, Unit.Type.Cavalry
     * - 如果为空，默认所有单位都占用槽位（向后兼容）
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Slot Occupation", 
        meta = (DisplayName = "需要占用槽位的单位类型", Categories = "Unit.Type"))
    FGameplayTagContainer SlotOccupyingUnitTypes;

    // ========== ✨ 新增 - 调试配置 ==========

    /** 是否显示调试可视化 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", meta = (DisplayName = "显示槽位调试"))
    bool bShowDebugVisualization = false;

    /** 空闲槽位颜色 - 绿色 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug|Colors", meta = (DisplayName = "空闲槽位颜色"))
    FColor DebugColorAvailable = FColor::Green;

    /** 预约槽位颜色 - 蓝色 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug|Colors", meta = (DisplayName = "预约槽位颜色"))
    FColor DebugColorReserved = FColor::Blue;

    /** 已到达槽位颜色 - 红色 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug|Colors", meta = (DisplayName = "已到达槽位颜色"))
    FColor DebugColorOccupied = FColor::Red;

    /** 槽位已满颜色（目标周围的圆圈） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug|Colors", meta = (DisplayName = "槽位已满颜色"))
    FColor DebugColorFull = FColor::Orange;

    /** 攻击者连线颜色 - 预约中（虚线） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug|Colors", meta = (DisplayName = "预约连线颜色"))
    FColor DebugColorReservedLine = FColor::Cyan;

    /** 攻击者连线颜色 - 已到达（实线） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug|Colors", meta = (DisplayName = "到达连线颜色"))
    FColor DebugColorOccupiedLine = FColor::Yellow;

    /** 调试显示的默认攻击范围（当无法获取攻击者信息时使用） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", meta = (DisplayName = "默认显示攻击范围"))
    float DebugDefaultAttackRange = 150.0f;

    /** 槽位球体大小 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", meta = (DisplayName = "槽位球体大小"))
    float DebugSlotSphereSize = 30.0f;

    /** 到达判定阈值（距离小于此值认为已到达槽位） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", meta = (DisplayName = "到达判定阈值"))
    float DebugArrivalThreshold = 100.0f;

    /** 是否显示槽位编号 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug|Display", meta = (DisplayName = "显示槽位编号"))
    bool bShowSlotNumbers = true;

    /** 是否显示占用者名称 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug|Display", meta = (DisplayName = "显示占用者名称"))
    bool bShowOccupierNames = true;

    /** 是否显示攻击者到槽位的连线 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug|Display", meta = (DisplayName = "显示攻击者连线"))
    bool bShowAttackerLines = true;

    /** 是否显示目标状态信息 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug|Display", meta = (DisplayName = "显示目标状态"))
    bool bShowTargetStatus = true;

    /** 是否显示状态图例（屏幕左上角） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug|Display", meta = (DisplayName = "显示状态图例"))
    bool bShowLegend = true;

protected:
    /**
     * @brief 为目标初始化攻击槽位
     * @param Target 目标 Actor
     */
    void InitializeSlotsForTarget(AActor* Target);
    
    /**
     * @brief 获取或创建目标的战斗信息
     * @param Target 目标 Actor
     * @return 目标的战斗信息引用
     */
    FSGTargetCombatInfo& GetOrCreateCombatInfo(AActor* Target);
    
    /**
     * @brief 查找最近的可用槽位
     * @param Target 目标 Actor
     * @param AttackerLocation 攻击者位置
     * @param AttackerAttackRange 攻击者攻击范围
     * @return 槽位索引，如果没有可用槽位返回 INDEX_NONE
     */
    int32 FindNearestAvailableSlot(AActor* Target, const FVector& AttackerLocation, float AttackerAttackRange);
    
    /**
     * @brief 获取目标的碰撞半径
     * @param Target 目标 Actor
     * @return 碰撞半径
     */
    float GetTargetCollisionRadius(AActor* Target) const;
    
    /**
     * @brief 清理无效数据（定期调用）
     */
    void CleanupInvalidData();

    // ✨ 新增 - 调试绘制函数
    /**
     * @brief 绘制所有目标的调试槽位
     */
    void DrawDebugSlots();
    
    /**
     * @brief 绘制单个目标的调试槽位
     * @param Target 目标 Actor
     * @param CombatInfo 目标的战斗信息
     */
    void DrawDebugSlotsForTarget(AActor* Target, const FSGTargetCombatInfo& CombatInfo);
    
    /**
     * @brief 绘制调试图例（屏幕信息）
     */
    void DrawDebugLegend();

private:
    // 目标 -> 战斗信息 映射表
    UPROPERTY()
    TMap<TWeakObjectPtr<AActor>, FSGTargetCombatInfo> TargetCombatInfoMap;

    // 清理计时器句柄
    FTimerHandle CleanupTimerHandle;
};
