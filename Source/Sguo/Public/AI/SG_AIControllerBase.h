// 🔧 修改 - SG_AIControllerBase.h

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "SG_AIControllerBase.generated.h"

// 前置声明
class UBehaviorTree;
class UBlackboardComponent;
class ASG_UnitsBase;
class UBehaviorTreeComponent;  // ✨ 新增

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
    
    /**
     * @brief 控制器默认行为树
     * @details
     * 功能说明：
     * - 如果单位没有设置自己的行为树，则使用此行为树
     * - 作为后备选项
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AI", meta = (DisplayName = "默认行为树"))
    TObjectPtr<UBehaviorTree> DefaultBehaviorTree;

    /**
     * @brief 当前正在使用的行为树
     * @details 运行时确定，可能来自单位配置或控制器默认配置
     */
    UPROPERTY(BlueprintReadOnly, Category = "AI", meta = (DisplayName = "当前行为树"))
    TObjectPtr<UBehaviorTree> CurrentBehaviorTree;

    /**
     * @brief 启动指定的行为树
     * @param BehaviorTreeToRun 要运行的行为树
     * @return 是否成功启动
     */
    UFUNCTION(BlueprintCallable, Category = "AI", meta = (DisplayName = "启动行为树"))
    bool StartBehaviorTree(UBehaviorTree* BehaviorTreeToRun);

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
    UFUNCTION()
    void OnTargetDeath(ASG_UnitsBase* DeadUnit);

    void BindTargetDeathEvent(ASG_UnitsBase* Target);
    void UnbindTargetDeathEvent(ASG_UnitsBase* Target);

    // ✨ 新增 - 初始化黑板并启动行为树
    /**
     * @brief 初始化并启动行为树
     * @param BehaviorTreeToUse 要使用的行为树
     * @return 是否成功
     */
    bool SetupBehaviorTree(UBehaviorTree* BehaviorTreeToUse);

private:
    TWeakObjectPtr<ASG_UnitsBase> CurrentListenedTarget;
};
