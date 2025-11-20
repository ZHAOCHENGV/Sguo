// ✨ 新增 - 检测周边威胁服务实现
/**
 * @file SG_BTService_DetectNearbyThreats.cpp
 * @brief 行为树服务：检测周边威胁实现
 */

#include "AI/Services/SG_BTService_DetectNearbyThreats.h"
#include "AI/SG_AIControllerBase.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Debug/SG_LogCategories.h"
#include "Units/SG_UnitsBase.h"

/**
 * @brief 构造函数
 * @details
 * 功能说明：
 * - 设置服务名称
 * - 配置更新间隔
 */
USG_BTService_DetectNearbyThreats::USG_BTService_DetectNearbyThreats()
{
	// 设置服务名称
	NodeName = TEXT("检测周边威胁");
	
	// 设置更新间隔（每 0.3 秒检查一次）
	Interval = 0.3f;
	RandomDeviation = 0.1f;
	
	// 配置黑板键过滤器（只接受 Object 类型）
	TargetKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(USG_BTService_DetectNearbyThreats, TargetKey), AActor::StaticClass());
}

/**
 * @brief Tick 更新
 * @param OwnerComp 行为树组件
 * @param NodeMemory 节点内存
 * @param DeltaSeconds 时间间隔
 * @details
 * 功能说明：
 * - 检测周边威胁
 * - 🔧 修改：使用单位的攻击范围 * 倍率作为检测半径
 */
void USG_BTService_DetectNearbyThreats::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);
	
	// 获取 AI Controller
	ASG_AIControllerBase* AIController = Cast<ASG_AIControllerBase>(OwnerComp.GetAIOwner());
	if (!AIController)
	{
		return;
	}
	
	// 🔧 修改 - 获取控制的单位
	ASG_UnitsBase* ControlledUnit = Cast<ASG_UnitsBase>(AIController->GetPawn());
	if (!ControlledUnit)
	{
		return;
	}
	
	// 🔧 修改 - 计算检测半径（攻击范围 * 倍率）
	float AttackRange = ControlledUnit->GetAttackRangeForAI();
	float DetectionRadius = AttackRange * DetectionRadiusMultiplier;
	
	// 检测周边威胁
	bool bFoundThreat = AIController->DetectNearbyThreats(DetectionRadius);
	
	if (bFoundThreat)
	{
		UE_LOG(LogSGGameplay, Verbose, TEXT("🔄 检测到周边威胁，已转移目标（检测半径：%.0f）"), DetectionRadius);
	}
}
