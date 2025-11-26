// ✨ 新增文件 - 敌方单位生成器
// Copyright notice placeholder
/**
 * @file SG_EnemySpawner.h
 * @brief 敌方单位生成器
 * @details
 * 功能说明：
 * - 根据 DeckConfig 配置的卡池生成敌方单位
 * - 支持权重随机和保底机制
 * - 支持区域随机生成和定点生成
 * - 支持兵团阵型生成
 */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "CardsAndUnits/SG_CardRuntimeTypes.h" // 复用 FSGCardDrawSlot
#include "SG_EnemySpawner.generated.h"

// 前向声明
class UBoxComponent;
class USG_DeckConfig;
class USG_CardDataBase;
class ASG_UnitsBase;
class UBillboardComponent;
class ASG_MainCityBase;

/**
 * @brief 生成间隔模式
 */
UENUM(BlueprintType)
enum class ESGSpawnIntervalMethod : uint8
{
    // 使用卡组配置的冷却时间 (DeckConfig -> DrawCDSeconds)
    UseDeckCooldown     UMETA(DisplayName = "使用卡组冷却"),
    
    // 使用固定的时间间隔
    FixedInterval       UMETA(DisplayName = "固定间隔"),
    
    // 在最小和最大值之间随机
    RandomInterval      UMETA(DisplayName = "随机间隔")
};

/**
 * @brief 生成位置模式
 */
UENUM(BlueprintType)
enum class ESGSpawnLocationMode : uint8
{
    // 在区域内随机位置生成
    RandomInArea        UMETA(DisplayName = "区域内随机"),
    
    // 在区域中心生成
    CenterOfArea        UMETA(DisplayName = "区域中心")
};

UCLASS(BlueprintType, Blueprintable)
class SGUO_API ASG_EnemySpawner : public AActor
{
    GENERATED_BODY()

public:
    ASG_EnemySpawner();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // ========== 组件 ==========
    
    /**
     * @brief 生成区域
     * @details 可视化的盒体组件，用于定义生成范围
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UBoxComponent> SpawnAreaBox;

    /**
     * @brief 编辑器图标
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UBillboardComponent> Billboard;

public:
    // ========== 配置参数 ==========

    /**
     * @brief 敌方卡组配置
     * @details 定义了可以生成的单位池、权重和保底规则
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner Config", meta = (DisplayName = "卡组配置"))
    TObjectPtr<USG_DeckConfig> DeckConfig;

    /**
     * @brief 是否自动开始生成
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner Config", meta = (DisplayName = "自动开始"))
    bool bAutoStart = true;

    /**
     * @brief 生成单位的阵营标签
     * @details 默认为 Unit.Faction.Enemy
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner Config", meta = (DisplayName = "阵营标签", Categories = "Unit.Faction"))
    FGameplayTag FactionTag;

    /**
     * @brief 开始生成的延迟时间（秒）
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner Timing", meta = (DisplayName = "开始延迟(秒)", ClampMin = "0.0"))
    float StartDelay = 1.0f;

    /**
     * @brief 生成间隔模式
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner Timing", meta = (DisplayName = "间隔模式"))
    ESGSpawnIntervalMethod IntervalMethod = ESGSpawnIntervalMethod::UseDeckCooldown;

    /**
     * @brief 固定生成间隔（秒）
     * @details 仅当 IntervalMethod 为 FixedInterval 时生效
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner Timing", 
        meta = (DisplayName = "固定间隔(秒)", EditCondition = "IntervalMethod == ESGSpawnIntervalMethod::FixedInterval", EditConditionHides))
    float FixedSpawnInterval = 5.0f;

    /**
     * @brief 最小随机间隔（秒）
     * @details 仅当 IntervalMethod 为 RandomInterval 时生效
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner Timing", 
        meta = (DisplayName = "最小间隔(秒)", EditCondition = "IntervalMethod == ESGSpawnIntervalMethod::RandomInterval", EditConditionHides))
    float MinSpawnInterval = 3.0f;

    /**
     * @brief 最大随机间隔（秒）
     * @details 仅当 IntervalMethod 为 RandomInterval 时生效
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner Timing", 
        meta = (DisplayName = "最大间隔(秒)", EditCondition = "IntervalMethod == ESGSpawnIntervalMethod::RandomInterval", EditConditionHides))
    float MaxSpawnInterval = 8.0f;

    /**
     * @brief 最大生成数量
     * @details 0 表示无限生成
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner Limits", meta = (DisplayName = "最大生成数量(0=无限)"))
    int32 MaxSpawnCount = 0;

    /**
     * @brief 生成位置模式
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner Location", meta = (DisplayName = "位置模式"))
    ESGSpawnLocationMode LocationMode = ESGSpawnLocationMode::RandomInArea;

    /**
     * @brief 生成单位的固定朝向
     * @details 如果不想使用固定朝向，可以在蓝图中覆盖计算逻辑
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner Location", meta = (DisplayName = "生成朝向"))
    FRotator SpawnRotation = FRotator(0.0f, 180.0f, 0.0f);

    // ========== 控制接口 ==========

    /**
     * @brief 开始生成流程
     */
    UFUNCTION(BlueprintCallable, Category = "Spawner Control")
    void StartSpawning();

    /**
     * @brief 停止生成流程
     */
    UFUNCTION(BlueprintCallable, Category = "Spawner Control")
    void StopSpawning();

    /**
     * @brief 立即执行一次生成
     * @return 是否成功生成
     */
    UFUNCTION(BlueprintCallable, Category = "Spawner Control")
    bool SpawnNextWave();

protected:
    // ========== 内部逻辑 ==========

    /**
     * @brief 初始化生成池
     * @details 基于 DeckConfig 构建带权重的运行时卡池
     */
    void InitializeSpawnPool();

    /**
     * @brief 抽取一张卡牌
     * @details 使用权重算法和保底机制
     */
    USG_CardDataBase* DrawCardFromPool();

    /**
     * @brief 执行生成逻辑
     */
    void HandleSpawnTimer();

    /**
     * @brief 计算下一次生成的时间间隔
     */
    float GetNextSpawnInterval() const;

    /**
     * @brief 在指定位置生成单位（支持兵团）
     */
    void SpawnUnit(USG_CardDataBase* CardData, const FVector& Location);

    /**
     * @brief 获取随机生成点
     */
    FVector GetRandomSpawnLocation() const;

    // ✨ 新增 - 查找关联主城
    void FindRelatedMainCity();

private:
    // 运行时生成池
    UPROPERTY(Transient)
    TArray<FSGCardDrawSlot> SpawnPool;

    // 已使用的唯一卡牌 ID
    UPROPERTY(Transient)
    TSet<FPrimaryAssetId> ConsumedUniqueCards;

    // 随机流
    FRandomStream RandomStream;

    // 生成计时器
    FTimerHandle SpawnTimerHandle;

    // 当前已生成数量
    int32 CurrentSpawnCount = 0;

    // 是否正在运行
    bool bIsSpawning = false;

    // ✨ 新增 - 关联的主城（用于检查存活状态）
    UPROPERTY(Transient)
    TWeakObjectPtr<ASG_MainCityBase> RelatedMainCity;
};