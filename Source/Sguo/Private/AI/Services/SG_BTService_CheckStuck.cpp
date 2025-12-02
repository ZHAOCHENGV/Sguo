// 📄 文件：Source/Sguo/Private/AI/Services/SG_BTService_CheckStuck.cpp
// ✨ 新增 - 检测卡住服务实现

#include "AI/Services/SG_BTService_CheckStuck.h"
#include "AI/SG_AIControllerBase.h"
#include "Units/SG_UnitsBase.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Debug/SG_LogCategories.h"

/**
 * @brief 构造函数
 */
USG_BTService_CheckStuck::USG_BTService_CheckStuck()
{
    NodeName = TEXT("检测卡住");
    
    // 每 0.5 秒检查一次
    Interval = 0.5f;
    RandomDeviation = 0.1f;
}

/**
 * @brief Tick 更新
 * @param OwnerComp 行为树组件
 * @param NodeMemory 节点内存
 * @param DeltaSeconds 时间间隔
 * @details
 * 功能说明：
 * - 检测单位是否卡住
 * - 卡住时标记目标不可达
 * - 自动切换到可达目标
 */
void USG_BTService_CheckStuck::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);
    
    // 获取 AI Controller
    ASG_AIControllerBase* AIController = Cast<ASG_AIControllerBase>(OwnerComp.GetAIOwner());
    if (!AIController)
    {
        return;
    }
    
    // 只在移动状态下检测
    ESGTargetEngagementState CurrentState = AIController->GetTargetEngagementState();
    if (CurrentState != ESGTargetEngagementState::Moving)
    {
        return;
    }
    
    // 检测是否卡住
    if (AIController->IsStuck())
    {
        ASG_UnitsBase* ControlledUnit = Cast<ASG_UnitsBase>(AIController->GetPawn());
        
        UE_LOG(LogSGGameplay, Warning, TEXT("🚧 %s 检测到卡住，切换目标"),
            ControlledUnit ? *ControlledUnit->GetName() : TEXT("Unknown"));
        
        // 标记当前目标不可达
        AIController->MarkCurrentTargetUnreachable();
        
        // 停止当前移动
        AIController->StopMovement();
        
        // 查找新的可达目标
        AActor* NewTarget = AIController->FindNearestReachableTarget();
        if (NewTarget)
        {
            AIController->SetCurrentTarget(NewTarget);
            UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 切换到新目标：%s"), *NewTarget->GetName());
        }
        else
        {
            // 没有可达目标，清除不可达列表后再试
            AIController->ClearUnreachableTargets();
            NewTarget = AIController->FindNearestTarget();
            if (NewTarget)
            {
                AIController->SetCurrentTarget(NewTarget);
                UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 清除不可达列表后找到目标：%s"), *NewTarget->GetName());
            }
            else
            {
                UE_LOG(LogSGGameplay, Warning, TEXT("  ⚠️ 完全没有可攻击的目标"));
            }
        }
    }
}
