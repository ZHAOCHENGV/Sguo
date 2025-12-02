// 📄 文件：Source/Sguo/Public/AI/Services/SG_BTService_CheckStuck.h
// ✨ 新增 - 检测卡住服务

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "SG_BTService_CheckStuck.generated.h"

/**
 * @brief 检测卡住服务
 * @details
 * 功能说明：
 * - 定期检测单位是否卡住
 * - 卡住时自动切换到可达目标
 * - 只在移动状态下生效
 */
UCLASS()
class SGUO_API USG_BTService_CheckStuck : public UBTService
{
	GENERATED_BODY()

public:
	/**
	 * @brief 构造函数
	 */
	USG_BTService_CheckStuck();

	/**
	 * @brief Tick 更新
	 * @param OwnerComp 行为树组件
	 * @param NodeMemory 节点内存
	 * @param DeltaSeconds 时间间隔
	 */
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
};
