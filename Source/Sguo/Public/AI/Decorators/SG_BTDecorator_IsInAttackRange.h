// 🔧 完全重写 - SG_BTDecorator_IsInAttackRange.h
/**
 * @file SG_BTDecorator_IsInAttackRange.h
 * @brief 行为树装饰器：检查是否在攻击范围内
 */

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTDecorator.h"
#include "SG_BTDecorator_IsInAttackRange.generated.h"

/**
 * @brief 攻击范围检查装饰器
 * @details
 * 功能说明：
 * - 检查是否在攻击范围内
 * - 🔧 关键：定期重新评估条件
 */
UCLASS()
class SGUO_API USG_BTDecorator_IsInAttackRange : public UBTDecorator
{
	GENERATED_BODY()

public:
	/**
	 * @brief 构造函数
	 */
	USG_BTDecorator_IsInAttackRange();

	/**
	 * @brief 计算条件
	 */
	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;

	// ✨ 新增 - 定期重新评估条件
	/**
	 * @brief Tick 更新
	 * @details
	 * 功能说明：
	 * - 每帧检查条件是否变化
	 * - 条件变化时通知行为树
	 */
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

	// ✨ 新增 - 描述节点
	virtual FString GetStaticDescription() const override;

protected:
	/**
	 * @brief 黑板键：目标
	 */
	UPROPERTY(EditAnywhere, Category = "Blackboard", meta = (DisplayName = "目标键"))
	FBlackboardKeySelector TargetKey;
	
	/**
	 * @brief 距离容差
	 * @details 避免边界抖动
	 */
	UPROPERTY(EditAnywhere, Category = "Attack Range", meta = (DisplayName = "距离容差", ClampMin = "0.0", UIMin = "0.0", UIMax = "200.0"))
	float DistanceTolerance = 50.0f;

	// ✨ 新增 - 检查间隔
	/**
	 * @brief 检查间隔（秒）
	 * @details
	 * 功能说明：
	 * - 控制多久检查一次条件
	 * - 默认 0.1 秒（每秒检查 10 次）
	 * - 不要设置太小，避免性能问题
	 */
	UPROPERTY(EditAnywhere, Category = "Attack Range", meta = (DisplayName = "检查间隔", ClampMin = "0.05", UIMin = "0.05", UIMax = "1.0"))
	float CheckInterval = 0.1f;

private:
	// ✨ 新增 - 上次检查时间
	mutable float LastCheckTime = 0.0f;
	
	// ✨ 新增 - 上次条件结果
	mutable bool LastConditionResult = false;
};
