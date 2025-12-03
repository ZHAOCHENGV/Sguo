// 📄 文件：Source/Sguo/Public/AI/SG_StationaryAIController.h
// ✨ 新增 - 站桩单位专用 AI 控制器
// ✅ 这是完整文件

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "GameplayTagContainer.h"
#include "SG_StationaryAIController.generated.h"

// 前置声明
class ASG_StationaryUnit;
class ASG_UnitsBase;

/**
 * @brief 站桩单位专用 AI 控制器
 * @details
 * 功能说明：
 * - 简化的 AI 逻辑，不使用行为树
 * - 不使用攻击槽位系统
 * - 只在攻击范围内查找目标
 * - 目标死亡后自动切换下一个
 * 使用场景：
 * - 主城弓手
 * - 箭塔
 * - 其他固定炮台类单位
 */
UCLASS()
class SGUO_API ASG_StationaryAIController : public AAIController
{
    GENERATED_BODY()

public:
    ASG_StationaryAIController();

    virtual void BeginPlay() override;
    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnUnPossess() override;
    virtual void Tick(float DeltaTime) override;

    // ========== 目标管理 ==========

    /**
     * @brief 查找攻击范围内的目标
     * @return 找到的目标，如果没有则返回 nullptr
     * @details
     * 功能说明：
     * - 只在攻击范围内查找敌方单位
     * - 不查找主城（站桩单位只攻击单位）
     * - 优先选择最近的目标
     */
    UFUNCTION(BlueprintCallable, Category = "AI|Stationary", meta = (DisplayName = "查找攻击范围内目标"))
    AActor* FindTargetInAttackRange();

    /**
     * @brief 获取当前目标
     * @return 当前目标 Actor
     */
    UFUNCTION(BlueprintPure, Category = "AI|Stationary", meta = (DisplayName = "获取当前目标"))
    AActor* GetCurrentTarget() const { return CurrentTarget.Get(); }

    /**
     * @brief 设置当前目标
     * @param NewTarget 新目标
     */
    UFUNCTION(BlueprintCallable, Category = "AI|Stationary", meta = (DisplayName = "设置当前目标"))
    void SetCurrentTarget(AActor* NewTarget);

    /**
     * @brief 检查当前目标是否有效
     * @return 目标是否有效
     */
    UFUNCTION(BlueprintPure, Category = "AI|Stationary", meta = (DisplayName = "目标是否有效"))
    bool IsTargetValid() const;

    /**
     * @brief 检查目标是否在攻击范围内
     * @param Target 要检查的目标
     * @return 是否在攻击范围内
     */
    UFUNCTION(BlueprintPure, Category = "AI|Stationary", meta = (DisplayName = "目标是否在攻击范围内"))
    bool IsTargetInAttackRange(AActor* Target) const;

    /**
     * @brief 执行攻击
     * @return 是否成功执行攻击
     */
    UFUNCTION(BlueprintCallable, Category = "AI|Stationary", meta = (DisplayName = "执行攻击"))
    bool PerformAttack();

    // ========== 配置 ==========

    /**
     * @brief 目标检测间隔（秒）
     * @details 多久检测一次新目标
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Stationary", 
        meta = (DisplayName = "目标检测间隔", ClampMin = "0.1", UIMin = "0.1", UIMax = "2.0"))
    float TargetDetectionInterval = 0.5f;

    /**
     * @brief 攻击范围倍率
     * @details 使用单位攻击范围乘以此倍率作为实际检测范围
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Stationary", 
        meta = (DisplayName = "攻击范围倍率", ClampMin = "0.5", UIMin = "0.5", UIMax = "2.0"))
    float AttackRangeMultiplier = 1.0f;

    /**
     * @brief 是否自动攻击
     * @details 启用后会自动查找目标并攻击
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Stationary", 
        meta = (DisplayName = "自动攻击"))
    bool bAutoAttack = true;

    /**
     * @brief 是否启用 AI
     */
    UPROPERTY(BlueprintReadWrite, Category = "AI|Stationary", 
        meta = (DisplayName = "AI 启用"))
    bool bAIEnabled = true;

protected:
    /**
     * @brief 目标死亡回调
     * @param DeadUnit 死亡的单位
     */
    UFUNCTION()
    void OnTargetDeath(ASG_UnitsBase* DeadUnit);

    /**
     * @brief 绑定目标死亡事件
     * @param Target 目标单位
     */
    void BindTargetDeathEvent(ASG_UnitsBase* Target);

    /**
     * @brief 解绑目标死亡事件
     * @param Target 目标单位
     */
    void UnbindTargetDeathEvent(ASG_UnitsBase* Target);

    /**
     * @brief 更新 AI 逻辑
     * @param DeltaTime 帧间隔
     */
    void UpdateAI(float DeltaTime);

private:
    // 当前目标
    UPROPERTY()
    TWeakObjectPtr<AActor> CurrentTarget;

    // 当前监听的目标（用于死亡事件）
    TWeakObjectPtr<ASG_UnitsBase> CurrentListenedTarget;

    // 控制的站桩单位
    UPROPERTY()
    TWeakObjectPtr<ASG_StationaryUnit> ControlledStationaryUnit;

    // 目标检测计时器
    float TargetDetectionTimer = 0.0f;
};
