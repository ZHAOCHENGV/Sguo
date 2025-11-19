// ✨ 新增 - 攻击范围检查装饰器
/**
 * @file SG_BTDecorator_IsInAttackRange.h
 * @brief 行为树装饰器：检查是否在攻击范围内
 * @details
 * 功能说明：
 * - 检查与目标的距离
 * - 用于决定是否可以攻击
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
 * - 使用单位的 BaseAttackRange
 */
UCLASS()
class SGUO_API USG_BTDecorator_IsInAttackRange : public UBTDecorator
{
	GENERATED_BODY()

public:
	/**
	 * @brief 构造函数
	 * @details
	 * 功能说明：
	 * - 设置装饰器名称
	 * - 配置观察者中断模式
	 */
	USG_BTDecorator_IsInAttackRange();

	/**
	 * @brief 计算条件
	 * @param OwnerComp 行为树组件
	 * @param NodeMemory 节点内存
	 * @return 条件是否满足
	 * @details
	 * 功能说明：
	 * - 计算与目标的距离
	 * - 比较是否在攻击范围内
	 */
	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;

protected:
	/**
	 * @brief 黑板键：目标
	 * @details 存储要检查的目标 Actor
	 */
	UPROPERTY(EditAnywhere, Category = "Blackboard", meta = (DisplayName = "目标键"))
	FBlackboardKeySelector TargetKey;
};
