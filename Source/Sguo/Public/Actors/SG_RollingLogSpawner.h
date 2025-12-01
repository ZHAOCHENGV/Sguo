// 📄 文件：Source/Sguo/Public/Actors/SG_RollingLogSpawner.h
// ✨ 新增 - 完整文件

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "SG_RollingLogSpawner.generated.h"

// 前置声明
class ASG_RollingLog;
class USG_RollingLogCardData;
class UAbilitySystemComponent;
class UArrowComponent;
class UBoxComponent;

/**
 * @brief 流木计生成器状态枚举
 */
UENUM(BlueprintType)
enum class ESGSpawnerState : uint8
{
    /** 待机状态 - 等待激活 */
    Idle        UMETA(DisplayName = "待机"),
    
    /** 激活状态 - 正在生成滚木 */
    Active      UMETA(DisplayName = "激活"),
    
    /** 冷却状态 - 等待下次可用 */
    Cooldown    UMETA(DisplayName = "冷却")
};

/** 生成器激活事件 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSGSpawnerActivatedSignature, ASG_RollingLogSpawner*, Spawner);

/** 生成器停止事件 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSGSpawnerDeactivatedSignature, ASG_RollingLogSpawner*, Spawner);

/**
 * @brief 流木计生成器
 * 
 * @details
 * **功能说明：**
 * - 预先放置在场景中
 * - 方向由 Actor 的朝向决定（箭头指向滚动方向）
 * - 当玩家使用流木计卡牌时被激活
 * - 从数据资产读取生成参数
 * 
 * **使用方式：**
 * 1. 在场景中放置此 Actor
 * 2. 调整 Actor 的旋转来设置滚动方向
 * 3. 设置阵营标签（决定对谁造成伤害）
 * 4. 玩家使用卡牌时调用 Activate()
 * 
 * **注意事项：**
 * - 箭头方向 = 滚木滚动方向
 * - 可以放置多个生成器，使用卡牌时全部激活
 * - 参数从卡牌数据资产读取
 */
UCLASS(BlueprintType, Blueprintable)
class SGUO_API ASG_RollingLogSpawner : public AActor
{
    GENERATED_BODY()

public:
    ASG_RollingLogSpawner();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

#if WITH_EDITOR
    virtual void OnConstruction(const FTransform& Transform) override;
#endif

public:
    // ==================== 事件委托 ====================

    /** 生成器激活事件 */
    UPROPERTY(BlueprintAssignable, Category = "Spawner Events", meta = (DisplayName = "激活事件"))
    FSGSpawnerActivatedSignature OnSpawnerActivated;

    /** 生成器停止事件 */
    UPROPERTY(BlueprintAssignable, Category = "Spawner Events", meta = (DisplayName = "停止事件"))
    FSGSpawnerDeactivatedSignature OnSpawnerDeactivated;

    // ==================== 组件 ====================

    /**
     * @brief 场景根组件
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "场景根"))
    TObjectPtr<USceneComponent> SceneRoot;

    /**
     * @brief 方向指示箭头（编辑器可见）
     * @details 箭头方向即滚木滚动方向
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "方向箭头"))
    TObjectPtr<UArrowComponent> DirectionArrow;

    /**
     * @brief 生成区域可视化（编辑器可见）
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "生成区域"))
    TObjectPtr<UBoxComponent> SpawnAreaBox;

    // ==================== 配置 ====================

    /**
     * @brief 生成器所属阵营
     * @details 决定生成的滚木对谁造成伤害
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner Config", 
        meta = (DisplayName = "所属阵营", Categories = "Unit.Faction"))
    FGameplayTag FactionTag;

    /**
     * @brief 默认滚木类（备用）
     * @details 如果卡牌数据资产没有指定滚木类，使用此类
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Spawner Config", 
        meta = (DisplayName = "默认滚木类"))
    TSubclassOf<ASG_RollingLog> DefaultRollingLogClass;

    /**
     * @brief 生成区域宽度（厘米）
     * @details 垂直于滚动方向的生成范围
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner Config", 
        meta = (DisplayName = "生成区域宽度", ClampMin = "0.0", UIMin = "0.0", UIMax = "2000.0"))
    float SpawnAreaWidth = 800.0f;

    /**
     * @brief 生成高度偏移（厘米）
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner Config", 
        meta = (DisplayName = "生成高度偏移", ClampMin = "0.0", UIMin = "0.0", UIMax = "500.0"))
    float SpawnHeightOffset = 50.0f;

    /**
     * @brief 激活后冷却时间（秒）
     * @details 一次激活完成后，需要等待多久才能再次激活（0 = 无冷却）
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner Config", 
        meta = (DisplayName = "冷却时间", ClampMin = "0.0", UIMin = "0.0", UIMax = "60.0"))
    float CooldownTime = 0.0f;

public:
    // ==================== 核心接口 ====================

    /**
     * @brief 激活生成器
     * @param CardData 流木计卡牌数据
     * @param InSourceASC 施放者的 ASC（可选）
     * @return 是否成功激活
     * 
     * @details
     * **功能说明：**
     * - 从卡牌数据读取所有参数
     * - 开始定时生成滚木
     * - 持续时间结束后自动停止
     */
    UFUNCTION(BlueprintCallable, Category = "Spawner", meta = (DisplayName = "激活生成器"))
    bool Activate(USG_RollingLogCardData* CardData, UAbilitySystemComponent* InSourceASC = nullptr);

    /**
     * @brief 停止生成器
     * @details 立即停止生成滚木
     */
    UFUNCTION(BlueprintCallable, Category = "Spawner", meta = (DisplayName = "停止生成器"))
    void Deactivate();

    /**
     * @brief 获取当前状态
     */
    UFUNCTION(BlueprintPure, Category = "Spawner", meta = (DisplayName = "获取状态"))
    ESGSpawnerState GetCurrentState() const { return CurrentState; }

    /**
     * @brief 检查是否可以激活
     */
    UFUNCTION(BlueprintPure, Category = "Spawner", meta = (DisplayName = "是否可激活"))
    bool CanActivate() const { return CurrentState == ESGSpawnerState::Idle; }

    /**
     * @brief 获取滚动方向（世界空间）
     */
    UFUNCTION(BlueprintPure, Category = "Spawner", meta = (DisplayName = "获取滚动方向"))
    FVector GetRollDirection() const;

    /**
     * @brief 获取剩余激活时间
     */
    UFUNCTION(BlueprintPure, Category = "Spawner", meta = (DisplayName = "获取剩余时间"))
    float GetRemainingTime() const { return RemainingDuration; }

    /**
     * @brief 获取冷却剩余时间
     */
    UFUNCTION(BlueprintPure, Category = "Spawner", meta = (DisplayName = "获取冷却剩余时间"))
    float GetCooldownRemainingTime() const { return CooldownRemainingTime; }

protected:
    // ==================== 内部状态 ====================

    /** 当前状态 */
    UPROPERTY(BlueprintReadOnly, Category = "Spawner State", meta = (DisplayName = "当前状态"))
    ESGSpawnerState CurrentState = ESGSpawnerState::Idle;

    /** 当前使用的卡牌数据 */
    UPROPERTY(Transient)
    TObjectPtr<USG_RollingLogCardData> ActiveCardData;

    /** 施放者 ASC */
    UPROPERTY(Transient)
    TObjectPtr<UAbilitySystemComponent> SourceASC;

    /** 生成计时器 */
    float SpawnTimer = 0.0f;

    /** 剩余持续时间 */
    float RemainingDuration = 0.0f;

    /** 冷却剩余时间 */
    float CooldownRemainingTime = 0.0f;

    /** 已生成的滚木列表 */
    UPROPERTY(Transient)
    TArray<TWeakObjectPtr<ASG_RollingLog>> SpawnedLogs;

protected:
    // ==================== 内部函数 ====================

    /**
     * @brief 生成滚木
     */
    void SpawnRollingLogs();

    /**
     * @brief 计算随机生成位置
     */
    FVector CalculateRandomSpawnLocation() const;

    /**
     * @brief 滚木销毁回调
     */
    UFUNCTION()
    void OnRollingLogDestroyed(ASG_RollingLog* DestroyedLog);

    /**
     * @brief 进入冷却状态
     */
    void EnterCooldown();

    /**
     * @brief 更新生成区域可视化
     */
    void UpdateSpawnAreaVisualization();

public:
    // ==================== 蓝图事件 ====================

    /**
     * @brief 滚木生成蓝图事件
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "Spawner", meta = (DisplayName = "On Log Spawned (BP)"))
    void K2_OnLogSpawned(ASG_RollingLog* SpawnedLog);

    /**
     * @brief 激活开始蓝图事件
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "Spawner", meta = (DisplayName = "On Activated (BP)"))
    void K2_OnActivated();

    /**
     * @brief 停止蓝图事件
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "Spawner", meta = (DisplayName = "On Deactivated (BP)"))
    void K2_OnDeactivated();

    /**
     * @brief 冷却结束蓝图事件
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "Spawner", meta = (DisplayName = "On Cooldown Finished (BP)"))
    void K2_OnCooldownFinished();

    // ==================== 调试配置 ====================

#if WITH_EDITORONLY_DATA
    /** 是否显示生成区域 */
    UPROPERTY(EditAnywhere, Category = "Debug", meta = (DisplayName = "显示生成区域"))
    bool bShowSpawnArea = true;

    /** 是否显示滚动方向 */
    UPROPERTY(EditAnywhere, Category = "Debug", meta = (DisplayName = "显示滚动方向"))
    bool bShowRollDirection = true;
#endif
};
