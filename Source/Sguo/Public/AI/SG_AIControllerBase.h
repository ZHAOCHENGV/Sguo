// 📄 文件：Source/Sguo/Public/AI/SG_AIControllerBase.h
// 🔧 修改 - 添加目标锁定状态管理
// ✅ 这是完整文件

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "GameplayTagContainer.h"
#include "SG_AITypes.h"
#include "SG_AIControllerBase.generated.h"

// 前置声明
class UBehaviorTree;
class UBlackboardComponent;
class ASG_UnitsBase;
class UBehaviorTreeComponent;


/**
 * @brief AI 控制器基类
 */
UCLASS()
class SGUO_API ASG_AIControllerBase : public AAIController
{
    GENERATED_BODY()

public:
    ASG_AIControllerBase();

    UFUNCTION(BlueprintCallable, Category = "AI|Movement")
    void TryFlankingMove();
    
    virtual void BeginPlay() override;
    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnUnPossess() override;

    UFUNCTION(BlueprintCallable, Category = "AI")
    void FreezeAI();

    // ========== 行为树配置 ==========
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AI", meta = (DisplayName = "默认行为树"))
    TObjectPtr<UBehaviorTree> DefaultBehaviorTree;

    UPROPERTY(BlueprintReadOnly, Category = "AI", meta = (DisplayName = "当前行为树"))
    TObjectPtr<UBehaviorTree> CurrentBehaviorTree;

    UFUNCTION(BlueprintCallable, Category = "AI", meta = (DisplayName = "启动行为树"))
    bool StartBehaviorTree(UBehaviorTree* BehaviorTreeToRun);

    // ========== 目标管理 ==========
    
    UFUNCTION(BlueprintCallable, Category = "AI")
    AActor* FindNearestTarget();

    /**
     * @brief 查找最近的可达目标
     * @return 可达的目标 Actor，如果没有则返回 nullptr
     */
    UFUNCTION(BlueprintCallable, Category = "AI", meta = (DisplayName = "查找可达目标"))
    AActor* FindNearestReachableTarget();

    UFUNCTION(BlueprintCallable, Category = "AI")
    bool DetectNearbyThreats(float DetectionRadius = 800.0f);

    UFUNCTION(BlueprintCallable, Category = "AI")
    void SetCurrentTarget(AActor* NewTarget);

    UFUNCTION(BlueprintPure, Category = "AI")
    AActor* GetCurrentTarget() const;

    UFUNCTION(BlueprintPure, Category = "AI")
    bool IsTargetValid() const;

    
    // ========== 攻击槽位配置 ==========

    /**
     * @brief 需要占用攻击槽位的单位类型标签
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AI|Attack Slot", 
        meta = (DisplayName = "需要占用槽位的单位类型", Categories = "Unit.Type"))
    FGameplayTagContainer SlotOccupyingUnitTypes;

    /**
     * @brief 检查当前控制的单位是否需要占用攻击槽位
     */
    UFUNCTION(BlueprintPure, Category = "AI|Attack Slot", meta = (DisplayName = "是否需要占用攻击槽位"))
    bool ShouldOccupyAttackSlot() const;

    /**
     * @brief 检查指定单位是否需要占用攻击槽位
     */
    UFUNCTION(BlueprintPure, Category = "AI|Attack Slot", meta = (DisplayName = "单位是否需要占用攻击槽位"))
    bool ShouldUnitOccupyAttackSlot(const ASG_UnitsBase* Unit) const;

    

    // ========== 目标锁定状态管理 ==========
    
    /**
     * @brief 获取当前目标锁定状态
     */
    UFUNCTION(BlueprintPure, Category = "AI|Target", meta = (DisplayName = "获取目标状态"))
    ESGTargetEngagementState GetTargetEngagementState() const { return TargetEngagementState; }

    /**
     * @brief 设置目标锁定状态
     */
    UFUNCTION(BlueprintCallable, Category = "AI|Target", meta = (DisplayName = "设置目标状态"))
    void SetTargetEngagementState(ESGTargetEngagementState NewState);

    /**
     * @brief 检查是否处于战斗锁定状态（正在攻击）
     * @return 是否已锁定目标（在攻击范围内且正在攻击）
     */
    UFUNCTION(BlueprintPure, Category = "AI|Target", meta = (DisplayName = "是否战斗锁定"))
    bool IsEngagedInCombat() const { return TargetEngagementState == ESGTargetEngagementState::Engaged; }

    // ✨ 新增 - 检查是否允许切换目标
    /**
     * @brief 检查是否允许切换目标
     * @return 是否允许切换
     * @details
     * 功能说明：
     * - 只有在 Engaged 状态（正在攻击）时不允许切换
     * - Moving、Searching、Blocked 状态都允许切换
     */
    UFUNCTION(BlueprintPure, Category = "AI|Target", meta = (DisplayName = "是否允许切换目标"))
    bool CanSwitchTarget() const;

    /**
     * @brief 标记当前目标为不可达
     */
    UFUNCTION(BlueprintCallable, Category = "AI|Target", meta = (DisplayName = "标记目标不可达"))
    void MarkCurrentTargetUnreachable();

    /**
     * @brief 清除不可达目标列表
     */
    UFUNCTION(BlueprintCallable, Category = "AI|Target", meta = (DisplayName = "清除不可达列表"))
    void ClearUnreachableTargets();

    /**
     * @brief 检查目标是否在不可达列表中
     */
    UFUNCTION(BlueprintPure, Category = "AI|Target")
    bool IsTargetUnreachable(AActor* Target) const;

    // ========== 移动状态检测 ==========
    
    /**
     * @brief 检查是否卡住（移动超时）
     */
    UFUNCTION(BlueprintPure, Category = "AI|Movement", meta = (DisplayName = "是否卡住"))
    bool IsStuck() const;

    /**
     * @brief 重置移动计时器
     */
    UFUNCTION(BlueprintCallable, Category = "AI|Movement")
    void ResetMovementTimer();

    /**
     * @brief 更新移动计时器
     */
    void UpdateMovementTimer(float DeltaTime);

    // ========== 主城特殊逻辑 ==========
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AI|Main City", meta = (DisplayName = "是否为主城"))
    bool bIsMainCity = false;

    UPROPERTY(BlueprintReadWrite, Category = "AI|Main City", meta = (DisplayName = "攻击被打断"))
    bool bAttackInterrupted = false;

    UFUNCTION(BlueprintCallable, Category = "AI|Main City")
    void InterruptAttack();

    UFUNCTION(BlueprintCallable, Category = "AI|Main City")
    void ResumeAttack();

    // ========== 黑板键名称 ==========
    
    static const FName BB_CurrentTarget;
    static const FName BB_IsInAttackRange;
    static const FName BB_IsTargetLocked;
    static const FName BB_IsTargetMainCity;

    // ✨ 新增 - 目标切换检测配置
    /**
     * @brief 移动中目标切换检测间隔（秒）
     * @details 在移动状态下，多久检测一次是否有更好的目标
     */
    UPROPERTY(EditDefaultsOnly, Category = "AI|Target", meta = (DisplayName = "目标切换检测间隔", ClampMin = "0.1", UIMin = "0.1", UIMax = "1.0"))
    float TargetSwitchCheckInterval = 0.3f;

    /**
     * @brief 目标切换距离阈值
     * @details 新目标必须比当前目标近这么多才会切换
     */
    UPROPERTY(EditDefaultsOnly, Category = "AI|Target", meta = (DisplayName = "目标切换距离阈值", ClampMin = "0.0", UIMin = "0.0", UIMax = "500.0"))
    float TargetSwitchDistanceThreshold = 100.0f;

protected:
    UFUNCTION()
    void OnTargetDeath(ASG_UnitsBase* DeadUnit);

    void BindTargetDeathEvent(ASG_UnitsBase* Target);
    void UnbindTargetDeathEvent(ASG_UnitsBase* Target);

    bool SetupBehaviorTree(UBehaviorTree* BehaviorTreeToUse);

    virtual void Tick(float DeltaTime) override;

    // ✨ 新增 - 移动中检测更好目标
    /**
     * @brief 在移动状态下检测是否有更好的目标
     * @details 如果发现更近的目标，自动切换
     */
    void CheckForBetterTargetWhileMoving();

    // ✨ 新增 - 攻击主城时检测敌方单位
    /**
     * @brief 攻击主城时检测敌方单位
     * @details
     * 功能说明：
     * - 仅在 Engaged 状态且目标是主城时调用
     * - 如果视野内有敌方单位，切换目标
     */
    void CheckForEnemyUnitsWhileAttackingMainCity();

private:
    TWeakObjectPtr<ASG_UnitsBase> CurrentListenedTarget;

    // 目标锁定状态
    UPROPERTY()
    ESGTargetEngagementState TargetEngagementState = ESGTargetEngagementState::Searching;

    // 不可达目标列表
    UPROPERTY()
    TSet<TWeakObjectPtr<AActor>> UnreachableTargets;

    // 移动计时器（检测卡住）
    float MovementTimer = 0.0f;
    FVector LastPosition = FVector::ZeroVector;
    
    // 卡住检测参数
    UPROPERTY(EditDefaultsOnly, Category = "AI|Movement", meta = (DisplayName = "卡住判定时间"))
    float StuckThresholdTime = 1.0f;
    
    UPROPERTY(EditDefaultsOnly, Category = "AI|Movement", meta = (DisplayName = "移动距离阈值"))
    float MinMovementDistance = 50.0f;

    // 不可达列表清理计时器
    float UnreachableClearTimer = 0.0f;

    //不可达清除间隔
    UPROPERTY(EditDefaultsOnly, Category = "AI|Target", meta = (DisplayName = "不可达清理间隔"))
    float UnreachableClearInterval = 5.0f;

    // ✨ 新增 - 目标切换检测计时器
    float TargetSwitchCheckTimer = 0.0f;
};
