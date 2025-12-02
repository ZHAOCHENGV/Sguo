// 📄 文件：Source/Sguo/Public/AI/SG_AIControllerBase.h
// 🔧 修改 - 新增目标可达性检测和切换机制

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
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

    // ✨ 新增 - 查找最近的可达目标（排除不可达的目标）
    /**
     * @brief 查找最近的可达目标
     * @return 可达的目标 Actor，如果没有则返回 nullptr
     * @details
     * 功能说明：
     * - 排除已标记为不可达的目标
     * - 优先选择可以到达的敌人
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

    // ✨ 新增 - 目标锁定状态管理
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
     * @brief 检查是否处于战斗锁定状态
     * @return 是否已锁定目标（在攻击范围内）
     */
    UFUNCTION(BlueprintPure, Category = "AI|Target", meta = (DisplayName = "是否战斗锁定"))
    bool IsEngagedInCombat() const { return TargetEngagementState == ESGTargetEngagementState::Engaged; }

    /**
     * @brief 标记当前目标为不可达
     * @details 将目标加入不可达列表，下次寻敌时会跳过
     */
    UFUNCTION(BlueprintCallable, Category = "AI|Target", meta = (DisplayName = "标记目标不可达"))
    void MarkCurrentTargetUnreachable();

    /**
     * @brief 清除不可达目标列表
     * @details 周期性调用，给目标第二次机会
     */
    UFUNCTION(BlueprintCallable, Category = "AI|Target", meta = (DisplayName = "清除不可达列表"))
    void ClearUnreachableTargets();

    /**
     * @brief 检查目标是否在不可达列表中
     */
    UFUNCTION(BlueprintPure, Category = "AI|Target")
    bool IsTargetUnreachable(AActor* Target) const;

    // ✨ 新增 - 移动状态检测
    /**
     * @brief 检查是否卡住（移动超时）
     * @return 是否被判定为卡住
     */
    UFUNCTION(BlueprintPure, Category = "AI|Movement", meta = (DisplayName = "是否卡住"))
    bool IsStuck() const;

    /**
     * @brief 重置移动计时器
     * @details 开始移动时调用
     */
    UFUNCTION(BlueprintCallable, Category = "AI|Movement")
    void ResetMovementTimer();

    /**
     * @brief 更新移动计时器
     * @param DeltaTime 帧间隔
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

protected:
    UFUNCTION()
    void OnTargetDeath(ASG_UnitsBase* DeadUnit);

    void BindTargetDeathEvent(ASG_UnitsBase* Target);
    void UnbindTargetDeathEvent(ASG_UnitsBase* Target);

    bool SetupBehaviorTree(UBehaviorTree* BehaviorTreeToUse);

    // ✨ 新增 - Tick 函数（用于更新移动计时器）
    virtual void Tick(float DeltaTime) override;

private:
    TWeakObjectPtr<ASG_UnitsBase> CurrentListenedTarget;

    // ✨ 新增 - 目标锁定状态
    UPROPERTY()
    ESGTargetEngagementState TargetEngagementState = ESGTargetEngagementState::Searching;

    // ✨ 新增 - 不可达目标列表
    UPROPERTY()
    TSet<TWeakObjectPtr<AActor>> UnreachableTargets;

    // ✨ 新增 - 移动计时器（检测卡住）
    float MovementTimer = 0.0f;
    FVector LastPosition = FVector::ZeroVector;
    
    // ✨ 新增 - 卡住检测参数
    UPROPERTY(EditDefaultsOnly, Category = "AI|Movement", meta = (DisplayName = "卡住判定时间"))
    float StuckThresholdTime = 2.0f;
    
    UPROPERTY(EditDefaultsOnly, Category = "AI|Movement", meta = (DisplayName = "移动距离阈值"))
    float MinMovementDistance = 50.0f;

    // ✨ 新增 - 不可达列表清理计时器
    float UnreachableClearTimer = 0.0f;
    
    UPROPERTY(EditDefaultsOnly, Category = "AI|Target", meta = (DisplayName = "不可达清理间隔"))
    float UnreachableClearInterval = 10.0f;
};
