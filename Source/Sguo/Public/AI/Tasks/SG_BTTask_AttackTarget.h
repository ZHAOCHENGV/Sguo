// 🔧 修改 - SG_BTTask_AttackTarget.h
/**
 * @file SG_BTTask_AttackTarget.h
 * @brief 行为树任务：攻击目标
 */

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "SG_BTTask_AttackTarget.generated.h"

// ✨ 新增 - 任务内存结构（在类定义之前）
struct FSG_BTTaskAttackMemory
{
	float RemainingWaitTime = 0.0f;
};

/**
 * @brief 攻击目标任务
 */
UCLASS()
class SGUO_API USG_BTTask_AttackTarget : public UBTTaskNode
{
	GENERATED_BODY()

public:
	/**
	 * @brief 构造函数
	 */
	USG_BTTask_AttackTarget();

	/**
	 * @brief 执行任务
	 */
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	
	// ✨ 新增 - Tick 更新（等待攻击完成）
	/**
	 * @brief Tick 更新
	 * @param OwnerComp 行为树组件
	 * @param NodeMemory 节点内存
	 * @param DeltaSeconds 时间间隔
	 * @details
	 * 功能说明：
	 * - 等待攻击冷却时间
	 * - 冷却完成后返回成功
	 */
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	
	// ✨ 新增 - 获取实例内存大小
	/**
	 * @brief 获取实例内存大小
	 * @return 内存大小（字节）
	 * @details
	 * 功能说明：
	 * - 告诉行为树系统需要分配多少内存
	 * - 用于存储任务的运行时数据
	 */
	virtual uint16 GetInstanceMemorySize() const override;

protected:
	// ✨ 新增 - 攻击冷却时间
	/**
	 * @brief 攻击后的等待时间（秒）
	 * @details
	 * 功能说明：
	 * - 攻击后等待一段时间再返回成功
	 * - 避免攻击动画还没播放完就返回
	 * - 默认 0.0 = 根据单位的攻击速度自动计算
	 * 计算公式：
	 * - 等待时间 = 1.0 / 攻击速度
	 * 使用场景：
	 * - 设置为 0.0：自动计算（推荐）
	 * - 设置为固定值：手动控制攻击频率
	 */
	UPROPERTY(EditAnywhere, Category = "Attack", meta = (DisplayName = "攻击冷却时间（0=自动）", ClampMin = "0.0", UIMin = "0.0", UIMax = "5.0"))
	float AttackCooldown = 0.0f;
};
