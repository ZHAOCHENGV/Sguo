// 📄 文件：Source/Sguo/Public/AI/SG_TargetingSubsystem.h
// 🔧 修改 - 修复蓝图不支持的参数类型
// ✅ 这是完整文件

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayTagContainer.h"
#include "SG_TargetingSubsystem.generated.h"

// 前置声明
class ASG_UnitsBase;
class ASG_MainCityBase;

/**
 * @brief 目标攻击者信息
 * @details 记录某个目标被哪些单位攻击
 */
USTRUCT()
struct FSGTargetAttackerInfo
{
    GENERATED_BODY()

    // 正在攻击此目标的单位列表
    UPROPERTY()
    TArray<TWeakObjectPtr<ASG_UnitsBase>> Attackers;

    // 获取有效攻击者数量（清理无效引用）
    int32 GetValidAttackerCount()
    {
        // 清理无效引用
        Attackers.RemoveAll([](const TWeakObjectPtr<ASG_UnitsBase>& Attacker)
        {
            return !Attacker.IsValid();
        });
        return Attackers.Num();
    }
};

/**
 * @brief 目标候选信息
 * @details 用于目标评估
 */
USTRUCT(BlueprintType)
struct FSGTargetCandidate
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Target")
    TWeakObjectPtr<AActor> Target;

    UPROPERTY(BlueprintReadOnly, Category = "Target")
    float Distance = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Target")
    int32 AttackerCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Target")
    float Score = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Target")
    bool bIsReachable = true;

    UPROPERTY(BlueprintReadOnly, Category = "Target")
    bool bIsMainCity = false;
};

/**
 * @brief 目标查找结果类型
 * @details 用于标识查找到的目标类型
 */
UENUM(BlueprintType)
enum class ESGTargetFindResult : uint8
{
    None        UMETA(DisplayName = "未找到目标"),
    EnemyUnit   UMETA(DisplayName = "敌方单位"),
    EnemyCity   UMETA(DisplayName = "敌方主城")
};

/**
 * @brief 目标管理子系统
 * @details
 * 功能说明：
 * - 使用场景查询高效获取范围内目标
 * - 管理目标的拥挤度（被多少单位攻击）
 * - 提供智能目标选择算法
 * - 当视野内无敌方单位时自动回退到敌方主城
 * 使用方式：
 * - 通过 GetWorld()->GetSubsystem<USG_TargetingSubsystem>() 获取
 */
UCLASS()
class SGUO_API USG_TargetingSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    // ========== 生命周期 ==========
    
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }

    // ========== 目标查询（C++ 接口） ==========
    // 🔧 修改 - 这些函数不暴露给蓝图，因为参数类型不被蓝图支持

    /**
     * @brief 查找最佳目标（C++ 核心接口）
     * @param Querier 查询者单位
     * @param SearchRadius 搜索半径
     * @param OutCandidates 输出：候选目标列表
     * @param IgnoredActors 需要忽略的 Actor 列表
     * @return 最佳目标 Actor
     * @details
     * 功能说明：
     * - 优先在视野范围内查找敌方单位
     * - 如果没有敌方单位，自动回退到敌方主城
     * - 使用高效的球形场景查询
     * 注意事项：
     * - 此函数不暴露给蓝图，请使用 FindBestTargetBP
     */
    AActor* FindBestTarget(
        ASG_UnitsBase* Querier,
        float SearchRadius,
        TArray<FSGTargetCandidate>& OutCandidates,
        const TSet<TWeakObjectPtr<AActor>>& IgnoredActors
    );

    /**
     * @brief 查找最佳目标（带结果类型，C++ 接口）
     * @param Querier 查询者单位
     * @param SearchRadius 搜索半径
     * @param OutResultType 输出：结果类型（单位/主城/无）
     * @param IgnoredActors 需要忽略的 Actor 列表
     * @return 最佳目标 Actor
     */
    AActor* FindBestTargetWithType(
        ASG_UnitsBase* Querier,
        float SearchRadius,
        ESGTargetFindResult& OutResultType,
        const TSet<TWeakObjectPtr<AActor>>& IgnoredActors
    );

    /**
     * @brief 仅查找敌方单位（C++ 接口）
     * @param Querier 查询者单位
     * @param SearchRadius 搜索半径
     * @param OutCandidates 输出：候选单位列表
     * @param IgnoredActors 需要忽略的 Actor 列表
     * @return 最佳敌方单位
     */
    AActor* FindEnemyUnitsOnly(
        ASG_UnitsBase* Querier,
        float SearchRadius,
        TArray<FSGTargetCandidate>& OutCandidates,
        const TSet<TWeakObjectPtr<AActor>>& IgnoredActors
    );

    // ========== 目标查询（蓝图接口） ==========
    // ✨ 新增 - 蓝图友好的接口，不使用 TSet<TWeakObjectPtr>

    /**
     * @brief 查找最佳目标（蓝图接口）
     * @param Querier 查询者单位
     * @param SearchRadius 搜索半径
     * @return 最佳目标 Actor
     * @details
     * 功能说明：
     * - 蓝图友好版本，不需要传入忽略列表
     * - 内部会处理回退到主城的逻辑
     */
    UFUNCTION(BlueprintCallable, Category = "Targeting", meta = (DisplayName = "查找最佳目标"))
    AActor* FindBestTargetBP(ASG_UnitsBase* Querier, float SearchRadius);

    /**
     * @brief 查找最佳目标带类型（蓝图接口）
     * @param Querier 查询者单位
     * @param SearchRadius 搜索半径
     * @param OutResultType 输出：结果类型
     * @return 最佳目标 Actor
     */
    UFUNCTION(BlueprintCallable, Category = "Targeting", meta = (DisplayName = "查找最佳目标（带类型）"))
    AActor* FindBestTargetWithTypeBP(
        ASG_UnitsBase* Querier,
        float SearchRadius,
        ESGTargetFindResult& OutResultType
    );

    /**
     * @brief 仅查找敌方单位（蓝图接口）
     * @param Querier 查询者单位
     * @param SearchRadius 搜索半径
     * @return 最佳敌方单位
     */
    UFUNCTION(BlueprintCallable, Category = "Targeting", meta = (DisplayName = "查找敌方单位"))
    AActor* FindEnemyUnitsOnlyBP(ASG_UnitsBase* Querier, float SearchRadius);

    /**
     * @brief 查找敌方主城
     * @param Querier 查询者单位
     * @return 最近的敌方主城
     * @details
     * 功能说明：
     * - 查找所有敌方主城
     * - 返回最近的存活主城
     * - 用于视野内无敌方单位时的回退目标
     */
    UFUNCTION(BlueprintCallable, Category = "Targeting", meta = (DisplayName = "查找敌方主城"))
    ASG_MainCityBase* FindEnemyMainCity(ASG_UnitsBase* Querier);

    // ========== 拥挤度管理 ==========

    /**
     * @brief 注册攻击者
     * @param Attacker 攻击者
     * @param Target 目标
     * @details 当单位开始攻击某目标时调用
     */
    UFUNCTION(BlueprintCallable, Category = "Targeting", meta = (DisplayName = "注册攻击者"))
    void RegisterAttacker(ASG_UnitsBase* Attacker, AActor* Target);

    /**
     * @brief 注销攻击者
     * @param Attacker 攻击者
     * @param Target 目标
     * @details 当单位停止攻击某目标时调用
     */
    UFUNCTION(BlueprintCallable, Category = "Targeting", meta = (DisplayName = "注销攻击者"))
    void UnregisterAttacker(ASG_UnitsBase* Attacker, AActor* Target);

    /**
     * @brief 获取目标的攻击者数量
     * @param Target 目标
     * @return 正在攻击该目标的单位数量
     */
    UFUNCTION(BlueprintPure, Category = "Targeting", meta = (DisplayName = "获取攻击者数量"))
    int32 GetAttackerCount(AActor* Target) const;

    /**
     * @brief 检查目标是否已满（达到最大攻击者数量）
     * @param Target 目标
     * @param MaxAttackers 最大攻击者数量
     * @return 是否已满
     */
    UFUNCTION(BlueprintPure, Category = "Targeting", meta = (DisplayName = "目标是否已满"))
    bool IsTargetFull(AActor* Target, int32 MaxAttackers = 6) const;

    // ========== 配置参数 ==========

    /**
     * @brief 拥挤度惩罚系数
     * @details 每增加一个攻击者，目标评分降低的比例
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting Config", meta = (DisplayName = "拥挤惩罚系数"))
    float CrowdingPenalty = 0.3f;

    /**
     * @brief 距离权重
     * @details 距离对评分的影响程度
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting Config", meta = (DisplayName = "距离权重"))
    float DistanceWeight = 1.0f;

    /**
     * @brief 默认最大攻击者数量
     * @details 每个目标最多被多少单位同时攻击
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting Config", meta = (DisplayName = "默认最大攻击者"))
    int32 DefaultMaxAttackers = 6;

    /**
     * @brief 场景查询碰撞通道
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting Config", meta = (DisplayName = "查询碰撞通道"))
    TEnumAsByte<ECollisionChannel> QueryChannel = ECC_Pawn;

    /**
     * @brief 主城缓存刷新间隔（秒）
     * @details 为了避免每次查询都遍历所有主城，使用缓存
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting Config|Performance", 
        meta = (DisplayName = "主城缓存刷新间隔", ClampMin = "0.5", UIMin = "0.5", UIMax = "5.0"))
    float MainCityCacheRefreshInterval = 1.0f;

protected:
    /**
     * @brief 使用 OverlapSphere 进行场景查询
     * @param Center 查询中心点
     * @param Radius 查询半径
     * @param OutActors 输出：查询到的 Actor 列表
     */
    void PerformSphereQuery(const FVector& Center, float Radius, TArray<AActor*>& OutActors);

    /**
     * @brief 计算目标评分
     * @param Querier 查询者
     * @param Target 目标
     * @param Distance 距离
     * @param AttackerCount 攻击者数量
     * @return 评分（越高越好）
     */
    float CalculateTargetScore(
        ASG_UnitsBase* Querier,
        AActor* Target,
        float Distance,
        int32 AttackerCount
    ) const;

    /**
     * @brief 刷新主城缓存
     * @details 定期更新主城列表，避免每次查询都遍历
     */
    void RefreshMainCityCache();

    /**
     * @brief 获取目标的碰撞半径
     * @param Target 目标 Actor
     * @return 碰撞半径
     */
    float GetTargetCollisionRadius(AActor* Target) const;

private:
    // 目标 -> 攻击者信息 映射
    UPROPERTY()
    TMap<TWeakObjectPtr<AActor>, FSGTargetAttackerInfo> TargetAttackerMap;

    // 定期清理无效数据的计时器
    FTimerHandle CleanupTimerHandle;

    // 清理无效数据
    void CleanupInvalidData();

    // 主城缓存
    UPROPERTY()
    TArray<TWeakObjectPtr<ASG_MainCityBase>> CachedMainCities;

    // 主城缓存刷新计时器
    FTimerHandle MainCityCacheTimerHandle;

    // 主城缓存是否有效
    bool bMainCityCacheValid = false;
};