// ✨ 新增 - 查找目标任务
/**
 * @file SG_BTTask_FindTarget.h
 * @brief 行为树任务：查找目标
 * @details
 * 功能说明：
 * - 查找最近的敌方单位或主城
 * - 更新黑板中的目标数据
 * 使用方式：
 * - 在行为树中添加此任务节点
 * - 配置黑板键
 */

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "SG_BTTask_FindTarget.generated.h"

/**
 * @brief 查找目标任务
 * @details
 * 功能说明：
 * - 调用 AI Controller 的 FindNearestTarget
 * - 更新黑板中的目标
 */
UCLASS()
class SGUO_API USG_BTTask_FindTarget : public UBTTaskNode
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
	USG_BTTask_FindTarget();

	/**
	 * @brief 执行任务
	 * @param OwnerComp 行为树组件
	 * @param NodeMemory 节点内存
	 * @return 任务执行结果
	 * @details
	 * 功能说明：
	 * - 查找最近的目标
	 * - 更新黑板
	 * - 返回成功或失败
	 */
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

protected:
	/**
	 * @brief 黑板键：目标
	 * @details 存储找到的目标 Actor
	 */
	UPROPERTY(EditAnywhere, Category = "Blackboard", meta = (DisplayName = "目标键"))
	FBlackboardKeySelector TargetKey;
};
