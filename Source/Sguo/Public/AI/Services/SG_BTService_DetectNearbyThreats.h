// ✨ 新增 - 检测周边威胁服务
/**
 * @file SG_BTService_DetectNearbyThreats.h
 * @brief 行为树服务：检测周边威胁
 * @details
 * 功能说明：
 * - 在行军或攻击主城时，检测周边新目标
 * - 发现新目标时转移仇恨
 * - 实现策划案的仇恨转移机制
 */

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "SG_BTService_DetectNearbyThreats.generated.h"

/**
 * @brief 检测周边威胁服务
 * @details
 * 功能说明：
 * - 定期检测周边是否有新敌人
 * - 仅在攻击主城时生效
 */
UCLASS()
class SGUO_API USG_BTService_DetectNearbyThreats : public UBTService
{
	GENERATED_BODY()

public:
	/**
	 * @brief 构造函数
	 * @details
	 * 功能说明：
	 * - 设置服务名称
	 * - 配置更新间隔
	 */
	USG_BTService_DetectNearbyThreats();

	/**
	 * @brief Tick 更新
	 * @param OwnerComp 行为树组件
	 * @param NodeMemory 节点内存
	 * @param DeltaSeconds 时间间隔
	 * @details
	 * 功能说明：
	 * - 检测周边威胁
	 * - 发现新目标时转移仇恨
	 */
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

protected:

	/**
	 * @brief 黑板键：目标
	 * @details 存储当前目标的 Actor
	 */
	UPROPERTY(EditAnywhere, Category = "Blackboard", meta = (DisplayName = "目标键"))
	FBlackboardKeySelector TargetKey;
};
