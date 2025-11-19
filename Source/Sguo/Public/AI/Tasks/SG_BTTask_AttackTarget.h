// ✨ 新增 - 攻击目标任务
/**
 * @file SG_BTTask_AttackTarget.h
 * @brief 行为树任务：攻击目标
 * @details
 * 功能说明：
 * - 触发单位的攻击能力
 * - 使用 GAS 系统
 * - 等待攻击完成
 */

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "SG_BTTask_AttackTarget.generated.h"

/**
 * @brief 攻击目标任务
 * @details
 * 功能说明：
 * - 调用单位的 PerformAttack 函数
 * - 等待攻击完成
 */
UCLASS()
class SGUO_API USG_BTTask_AttackTarget : public UBTTaskNode
{
	GENERATED_BODY()

public:
	/**
	 * @brief 构造函数
	 * @details
	 * 功能说明：
	 * - 设置任务名称
	 * - 配置节点参数
	 */
	USG_BTTask_AttackTarget();

	/**
	 * @brief 执行任务
	 * @param OwnerComp 行为树组件
	 * @param NodeMemory 节点内存
	 * @return 任务执行结果
	 * @details
	 * 功能说明：
	 * - 触发攻击能力
	 * - 返回成功或失败
	 */
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
