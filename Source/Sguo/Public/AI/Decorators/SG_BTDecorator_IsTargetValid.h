// ✨ 新增 - 目标有效性检查装饰器
/**
 * @file SG_BTDecorator_IsTargetValid.h
 * @brief 行为树装饰器：检查目标是否有效
 * @details
 * 功能说明：
 * - 检查目标是否存在、是否存活
 * - 用于条件判断
 */

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTDecorator.h"
#include "SG_BTDecorator_IsTargetValid.generated.h"

/**
 * @brief 目标有效性检查装饰器
 * @details
 * 功能说明：
 * - 检查黑板中的目标是否有效
 * - 目标无效时中断节点
 */
UCLASS()
class SGUO_API USG_BTDecorator_IsTargetValid : public UBTDecorator
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
	USG_BTDecorator_IsTargetValid();

	/**
	 * @brief 计算条件
	 * @param OwnerComp 行为树组件
	 * @param NodeMemory 节点内存
	 * @return 条件是否满足
	 * @details
	 * 功能说明：
	 * - 检查目标是否有效
	 * - 返回 true 或 false
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
