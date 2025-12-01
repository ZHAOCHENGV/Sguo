// 📄 文件：Source/Sguo/Public/Strategies/SG_StrategyEffect_RollingLog.h
// 🔧 修改 - 简化版，只负责激活场景中的生成器

#pragma once

#include "CoreMinimal.h"
#include "Strategies/SG_StrategyEffectBase.h"
#include "SG_StrategyEffect_RollingLog.generated.h"

// 前置声明
class ASG_RollingLogSpawner;
class USG_RollingLogCardData;

/**
 * @brief 流木计效果类（简化版）
 * 
 * @details
 * **功能说明：**
 * - 查找场景中所有匹配阵营的生成器
 * - 激活这些生成器
 * - 生成器自己负责生成滚木
 * 
 * **使用方式：**
 * 1. 在场景中预先放置 ASG_RollingLogSpawner
 * 2. 玩家使用流木计卡牌
 * 3. 此效果类激活所有匹配的生成器
 * 
 * **注意事项：**
 * - 不需要玩家选择位置或方向
 * - PlacementType 应设置为 Global
 */
UCLASS(BlueprintType, Blueprintable)
class SGUO_API ASG_StrategyEffect_RollingLog : public ASG_StrategyEffectBase
{
    GENERATED_BODY()

public:
    ASG_StrategyEffect_RollingLog();

public:
    // ==================== 接口重写 ====================

    /**
     * @brief 检查是否需要目标选择
     * @return false - 不需要，直接激活场景中的生成器
     */
    virtual bool RequiresTargetSelection_Implementation() const override;

    /**
     * @brief 检查是否可以执行
     * @return 是否有可用的生成器
     */
    virtual bool CanExecute_Implementation() const override;

    /**
     * @brief 获取不可执行的原因
     */
    virtual FText GetCannotExecuteReason_Implementation() const override;

    /**
     * @brief 执行效果
     */
    virtual void ExecuteEffect_Implementation() override;

public:
    // ==================== 配置 ====================

    /**
     * @brief 是否只激活同阵营的生成器
     * @details 
     * - true: 只激活与施放者同阵营的生成器
     * - false: 激活所有生成器
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Rolling Log Config", 
        meta = (DisplayName = "只激活同阵营生成器"))
    bool bOnlyActivateSameFaction = true;

protected:
    // ==================== 内部函数 ====================

    /**
     * @brief 查找场景中所有可用的生成器
     * @param OutSpawners 输出：可用的生成器列表
     * @return 找到的生成器数量
     */
    int32 FindAvailableSpawners(TArray<ASG_RollingLogSpawner*>& OutSpawners) const;

    /**
     * @brief 激活生成器
     * @param Spawner 要激活的生成器
     * @return 是否成功激活
     */
    bool ActivateSpawner(ASG_RollingLogSpawner* Spawner);

protected:
    /** 已激活的生成器列表 */
    UPROPERTY(Transient)
    TArray<TWeakObjectPtr<ASG_RollingLogSpawner>> ActivatedSpawners;

public:
    // ==================== 蓝图事件 ====================

    /**
     * @brief 生成器激活蓝图事件
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "Rolling Log", meta = (DisplayName = "On Spawner Activated (BP)"))
    void K2_OnSpawnerActivated(ASG_RollingLogSpawner* Spawner);

    /**
     * @brief 所有生成器激活完成蓝图事件
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "Rolling Log", meta = (DisplayName = "On All Spawners Activated (BP)"))
    void K2_OnAllSpawnersActivated(int32 ActivatedCount);
};
