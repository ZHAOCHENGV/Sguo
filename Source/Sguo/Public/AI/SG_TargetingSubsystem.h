// 📄 文件：Source/Sguo/Public/AI/SG_TargetingSubsystem.h
// ✨ 新增 - 目标管理子系统

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayTagContainer.h"
#include "SG_TargetingSubsystem.generated.h"

class ASG_UnitsBase;

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

    UPROPERTY()
    TWeakObjectPtr<AActor> Target;

    UPROPERTY()
    float Distance = 0.0f;

    UPROPERTY()
    int32 AttackerCount = 0;

    UPROPERTY()
    float Score = 0.0f;

    UPROPERTY()
    bool bIsReachable = true;
};

/**
 * @brief 目标管理子系统
 * @details
 * 功能说明：
 * - 使用场景查询高效获取范围内目标
 * - 管理目标的拥挤度（被多少单位攻击）
 * - 提供智能目标选择算法
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

    // ========== 目标查询 ==========

  
    /**
     * @brief 查找最佳目标（增加忽略列表支持）
     * @param Querier 查询者单位
     * @param SearchRadius 搜索半径
     * @param OutCandidates 输出：候选目标列表
     * @param IgnoredActors (新增) 需要忽略的 Actor 列表（默认为空）
     * @return 最佳目标 Actor
     */
    // 🔧 修改 - 增加 IgnoredActors 参数
   
    AActor* FindBestTarget(ASG_UnitsBase* Querier,float SearchRadius,TArray<FSGTargetCandidate>& OutCandidates,const TSet<TWeakObjectPtr<AActor>>& IgnoredActors);

   
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
     * 评分 = 基础分 / (1 + 攻击者数 * CrowdingPenalty)
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

private:
    // 目标 -> 攻击者信息 映射
    UPROPERTY()
    TMap<TWeakObjectPtr<AActor>, FSGTargetAttackerInfo> TargetAttackerMap;

    // 定期清理无效数据的计时器
    FTimerHandle CleanupTimerHandle;

    // 清理无效数据
    void CleanupInvalidData();
};
