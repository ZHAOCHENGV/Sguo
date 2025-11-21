// ✨ 新增 - 移动到目标任务
/**
 * @file SG_BTTask_MoveToTarget.h
 * @brief 行为树任务：移动到目标
 * @details
 * 功能说明：
 * - 移动到目标位置
 * - 考虑攻击范围
 * - 使用 UE 的导航系统
 */

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "SG_BTTask_MoveToTarget.generated.h"

/**
 * @brief 移动到目标任务
 * @details
 * 功能说明：
 * - 使用导航系统移动到目标
 * - 停止距离为攻击范围
 */
UCLASS()
class SGUO_API USG_BTTask_MoveToTarget : public UBTTaskNode
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
	USG_BTTask_MoveToTarget();

	/**
	 * @brief 执行任务
	 * @param OwnerComp 行为树组件
	 * @param NodeMemory 节点内存
	 * @return 任务执行结果
	 * @details
	 * 功能说明：
	 * - 移动到目标位置
	 * - 考虑攻击范围
	 */
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	// ✨ 新增 - 重写 TickTask 以每帧检查移动状态
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	
protected:
	/**
	 * @brief 黑板键：目标
	 * @details 存储移动目标的 Actor
	 */
	UPROPERTY(EditAnywhere, Category = "Blackboard", meta = (DisplayName = "目标键"))
	FBlackboardKeySelector TargetKey;

	/**
	 * @brief 可接受的半径
	 * @details
	 * 功能说明：
	 * - 到达目标的可接受距离
	 * - 默认使用单位的攻击范围
	 */
	UPROPERTY(EditAnywhere, Category = "Movement", meta = (DisplayName = "可接受半径", ClampMin = "0.0"))
	float AcceptableRadius = -1.0f;
};
