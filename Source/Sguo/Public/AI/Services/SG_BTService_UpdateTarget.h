// ✨ 新增 - 更新目标服务
/**
 * @file SG_BTService_UpdateTarget.h
 * @brief 行为树服务：更新目标
 * @details
 * 功能说明：
 * - 定期检查目标是否有效
 * - 目标死亡时自动切换新目标
 * - 更新黑板数据
 */

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "SG_BTService_UpdateTarget.generated.h"

/**
 * @brief 更新目标服务
 * @details
 * 功能说明：
 * - 定期检查目标有效性
 * - 目标无效时查找新目标
 */
UCLASS()
class SGUO_API USG_BTService_UpdateTarget : public UBTService
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
	USG_BTService_UpdateTarget();

	/**
	 * @brief Tick 更新
	 * @param OwnerComp 行为树组件
	 * @param NodeMemory 节点内存
	 * @param DeltaSeconds 时间间隔
	 * @details
	 * 功能说明：
	 * - 检查目标有效性
	 * - 目标无效时查找新目标
	 * - 更新黑板数据
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
