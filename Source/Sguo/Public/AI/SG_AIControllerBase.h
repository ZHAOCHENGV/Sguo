/**
 * @file SG_AIControllerBase.h
 * @brief AI控制器基类
 * @details
 * 功能说明：
 * - 管理单位的AI行为
 * - 集成StateTree系统
 * - 提供目标查找和导航功能
 * - 与GAS攻击系统无缝集成
 */

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "GameplayTagContainer.h"
#include "SG_AIControllerBase.generated.h"

/**
 * @brief AI控制器基类
 */
UCLASS()
class SGUO_API ASG_AIControllerBase : public AAIController
{
	GENERATED_BODY()

public:
	// 构造函数
	ASG_AIControllerBase();

protected:
	// 生命周期函数
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;

public:
	// ========== 目标管理 ==========
	
	/**
	 * @brief 查找最近的敌人
	 * @param SearchRadius 搜索半径
	 * @return 找到的目标Actor，如果没有返回nullptr
	 */
	UFUNCTION(BlueprintCallable, Category = "AI|Target")
	AActor* FindNearestEnemy(float SearchRadius = 2000.0f);
	
	/**
	 * @brief 查找敌方主城
	 * @return 敌方主城Actor，如果没有返回nullptr
	 */
	UFUNCTION(BlueprintCallable, Category = "AI|Target")
	AActor* FindEnemyMainCity();
	
	/**
	 * @brief 设置当前目标
	 * @param NewTarget 新的目标Actor
	 */
	UFUNCTION(BlueprintCallable, Category = "AI|Target")
	void SetCurrentTarget(AActor* NewTarget);
	
	/**
	 * @brief 获取当前目标
	 * @return 当前目标Actor
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "AI|Target")
	AActor* GetCurrentTarget() const { return CurrentTarget; }
	
	/**
	 * @brief 检查当前目标是否有效
	 * @details
	 * 功能说明：
	 * - 检查目标是否存在、是否存活、是否在范围内
	 * 详细流程：
	 * 1. 检查 CurrentTarget 是否为空
	 * 2. 检查目标是否已死亡
	 * 3. 检查目标是否仍在搜索范围内
	 * 注意事项：
	 * - 在 AI 中每帧检查
	 * - 如果无效，需要重新查找目标
	 * @return 目标是否有效
	 */
	UFUNCTION(BlueprintCallable, Category = "AI|Target")
	bool IsTargetValid() const;

	// ========== 移动控制 ==========
	
	/**
	 * @brief 移动到目标位置
	 * @param TargetLocation 目标位置
	 * @param AcceptanceRadius 接受半径（到达此距离即认为成功）
	 * @return 是否成功开始移动
	 */
	UFUNCTION(BlueprintCallable, Category = "AI|Movement")
	bool MoveToTargetLocation(FVector TargetLocation, float AcceptanceRadius = 50.0f);
	
	/**
	 * @brief 移动到目标Actor
	 * @param TargetActor 目标Actor
	 * @param AcceptanceRadius 接受半径
	 * @return 是否成功开始移动
	 */
	UFUNCTION(BlueprintCallable, Category = "AI|Movement")
	bool MoveToTargetActor(AActor* TargetActor, float AcceptanceRadius = 150.0f);
	
	/**
	 * @brief 停止移动
	 */
	UFUNCTION(BlueprintCallable, Category = "AI|Movement")
	void StopMovement();

	// ========== 战斗控制 ==========
	
	/**
	 * @brief 检查是否在攻击范围内
	 * @param Target 目标Actor
	 * @param AttackRange 攻击范围（如果为0则使用单位的BaseAttackRange）
	 * @return 是否在攻击范围内
	 */
	UFUNCTION(BlueprintCallable, Category = "AI|Combat")
	bool IsInAttackRange(AActor* Target, float AttackRange = 0.0f) const;
	
	/**
	 * @brief 面向目标
	 * @param Target 目标Actor
	 */
	UFUNCTION(BlueprintCallable, Category = "AI|Combat")
	void FaceTarget(AActor* Target);
	
	/**
	 * @brief 执行攻击
	 * @details
	 * 功能说明：
	 * - 调用控制单位的 PerformAttack() 函数
	 * - 触发 GAS 攻击能力
	 * - 自动应用伤害到目标
	 * 注意事项：
	 * - 需要先设置目标（SetCurrentTarget）
	 * - 单位必须已经授予攻击能力
	 * @return 是否成功触发攻击
	 */
	UFUNCTION(BlueprintCallable, Category = "AI|Combat")
	bool PerformAttack();

protected:
	// ========== 属性 ==========
	
	/** 当前目标Actor */
	UPROPERTY(BlueprintReadOnly, Category = "AI")
	TObjectPtr<AActor> CurrentTarget;
	
	/** 目标搜索半径 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Config")
	float TargetSearchRadius = 2000.0f;
	
	/** 是否自动查找目标 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Config")
	bool bAutoFindTarget = true;
	
	/** 是否优先攻击主城 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Config")
	bool bPrioritizeMainCity = false;

private:
	// ========== 辅助函数 ==========
	
	/**
	 * @brief 获取控制的单位
	 * @return 单位Character指针
	 */
	class ASG_UnitsBase* GetControlledUnit() const;
	
	/**
	 * @brief 获取单位的阵营标签
	 * @return 阵营标签
	 */
	FGameplayTag GetUnitFactionTag() const;
};
