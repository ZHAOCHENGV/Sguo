// 📄 文件：Source/Sguo/Public/AI/SG_AIControllerBase.h

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "SG_AIControllerBase.generated.h"

// 前置声明
class UBehaviorTree;
class UBlackboardComponent;
class ASG_UnitsBase;

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
    
    // ✨ 新增 - 解除控制时调用
    /**
     * @brief 解除控制时调用
     * @details
     * 功能说明：
     * - 清理目标死亡监听
     * - 停止行为树
     */
    virtual void OnUnPossess() override;
    
    // ✨ 新增 - 运行指定的行为树
    /**
     * @brief 运行指定的行为树
     * @param NewBehaviorTree 要运行的行为树
     * @return 是否成功启动
     * @details
     * 功能说明：
     * - 停止当前行为树（如果有）
     * - 启动新的行为树
     * - 用于动态切换行为树
     */
    UFUNCTION(BlueprintCallable, Category = "AI", meta = (DisplayName = "运行行为树"))
    bool RunBehaviorTreeAsset(UBehaviorTree* NewBehaviorTree);
    
    UFUNCTION(BlueprintCallable, Category = "AI")
    void FreezeAI();

    // ========== 行为树配置 ==========
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AI", meta = (DisplayName = "行为树"))
    TObjectPtr<UBehaviorTree> BehaviorTreeAsset;

    // ========== 目标管理 ==========
    
    UFUNCTION(BlueprintCallable, Category = "AI")
    AActor* FindNearestTarget();

    UFUNCTION(BlueprintCallable, Category = "AI")
    bool DetectNearbyThreats(float DetectionRadius = 800.0f);

    UFUNCTION(BlueprintCallable, Category = "AI")
    void SetCurrentTarget(AActor* NewTarget);

    UFUNCTION(BlueprintPure, Category = "AI")
    AActor* GetCurrentTarget() const;

    UFUNCTION(BlueprintPure, Category = "AI")
    bool IsTargetValid() const;

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
    // ✨ 新增 - 目标死亡回调
    /**
     * @brief 目标死亡回调
     * @param DeadUnit 死亡的单位
     * @details
     * 功能说明：
     * - 当锁定的目标死亡时触发
     * - 清除当前目标
     * - 立即寻找新目标
     */
    UFUNCTION()
    void OnTargetDeath(ASG_UnitsBase* DeadUnit);

    // ✨ 新增 - 绑定目标死亡事件
    /**
     * @brief 绑定目标死亡事件
     * @param Target 目标单位
     * @details
     * 功能说明：
     * - 监听目标的死亡事件
     * - 目标死亡时自动切换目标
     */
    void BindTargetDeathEvent(ASG_UnitsBase* Target);

    // ✨ 新增 - 解绑目标死亡事件
    /**
     * @brief 解绑目标死亡事件
     * @param Target 目标单位
     * @details
     * 功能说明：
     * - 取消监听目标的死亡事件
     * - 在切换目标或解除控制时调用
     */
    void UnbindTargetDeathEvent(ASG_UnitsBase* Target);

private:
    // ✨ 新增 - 缓存当前监听的目标
    /**
     * @brief 当前监听死亡事件的目标
     * @details 用于在切换目标时解绑旧目标的事件
     */
    TWeakObjectPtr<ASG_UnitsBase> CurrentListenedTarget;
};
