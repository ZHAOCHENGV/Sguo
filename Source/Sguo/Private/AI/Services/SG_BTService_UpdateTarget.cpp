// ✨ 新增 - 更新目标服务实现
/**
 * @file SG_BTService_UpdateTarget.cpp
 * @brief 行为树服务：更新目标实现
 */

#include "AI/Services/SG_BTService_UpdateTarget.h"

#include "AbilitySystem/SG_AttributeSet.h"
#include "AI/SG_AIControllerBase.h"
#include "Units/SG_UnitsBase.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Buildings/SG_MainCityBase.h"
#include "Debug/SG_LogCategories.h"

/**
 * @brief 构造函数
 * @details
 * 功能说明：
 * - 设置服务名称
 * - 配置更新间隔
 */
USG_BTService_UpdateTarget::USG_BTService_UpdateTarget()
{
	// 设置服务名称
	NodeName = TEXT("更新目标");
	
	// 🔧 修改 - 缩短更新间隔，更快响应目标死亡
	Interval = 0.2f;  // 从 0.5 秒改为 0.2 秒
	RandomDeviation = 0.05f;
	
	// 配置黑板键过滤器
	TargetKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(USG_BTService_UpdateTarget, TargetKey), AActor::StaticClass());
}

/**
 * @brief Tick 更新
 * @param OwnerComp 行为树组件
 * @param NodeMemory 节点内存
 * @param DeltaSeconds 时间间隔
 * @details
 * 功能说明：
 * - 🔧 修改 - 增加攻击状态检测
 * - 检查目标有效性
 * - 目标无效时查找新目标
 */
void USG_BTService_UpdateTarget::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	 Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);
    
    ASG_AIControllerBase* AIController = Cast<ASG_AIControllerBase>(OwnerComp.GetAIOwner());
    if (!AIController)
    {
        return;
    }
    
    // ✨ 新增 - 获取控制的单位，检查攻击状态
    ASG_UnitsBase* ControlledUnit = Cast<ASG_UnitsBase>(AIController->GetPawn());
    if (!ControlledUnit)
    {
        return;
    }
    
    UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
    if (!BlackboardComp)
    {
        return;
    }
    
    AActor* CurrentTarget = Cast<AActor>(BlackboardComp->GetValueAsObject(TargetKey.SelectedKeyName));
    
    // ========== 检查目标是否有效 ==========
    bool bIsTargetValid = false;
    if (CurrentTarget)
    {
        // 检查单位
        ASG_UnitsBase* TargetUnit = Cast<ASG_UnitsBase>(CurrentTarget);
        if (TargetUnit)
        {
            bIsTargetValid = !TargetUnit->bIsDead && 
                             (!TargetUnit->AttributeSet || TargetUnit->AttributeSet->GetHealth() > 0.0f);
        }
        // 检查主城
        else
        {
            ASG_MainCityBase* TargetMainCity = Cast<ASG_MainCityBase>(CurrentTarget);
            if (TargetMainCity)
            {
                bIsTargetValid = TargetMainCity->IsAlive();
                
                if (!bIsTargetValid)
                {
                    UE_LOG(LogSGGameplay, Log, TEXT("🏆 目标主城已被摧毁：%s"), *TargetMainCity->GetName());
                }
            }
        }
    }
    
    // ========== 如果目标无效，查找新目标 ==========
    if (!bIsTargetValid)
    {
        // ✨ 新增 - 检查是否正在攻击动画中
        // 如果正在攻击，只更新目标但不强制中断
        // 攻击任务会在 Tick 中检测到目标死亡并处理
        bool bIsAttacking = ControlledUnit->bIsAttacking;
        
        UE_LOG(LogSGGameplay, Log, TEXT("🔄 目标无效，查找新目标 (正在攻击: %s)"), 
            bIsAttacking ? TEXT("是") : TEXT("否"));
        
        AActor* NewTarget = AIController->FindNearestTarget();
        if (NewTarget)
        {
            // 更新黑板和 AI Controller 中的目标
            BlackboardComp->SetValueAsObject(TargetKey.SelectedKeyName, NewTarget);
            AIController->SetCurrentTarget(NewTarget);
            
            UE_LOG(LogSGGameplay, Log, TEXT("✓ 找到新目标：%s"), *NewTarget->GetName());
            
            // ✨ 新增 - 如果不在攻击中，请求行为树重新评估
            if (!bIsAttacking)
            {
                // 通过设置黑板值触发行为树重新评估
                BlackboardComp->SetValueAsBool(FName("IsInAttackRange"), false);
            }
        }
        else
        {
            BlackboardComp->ClearValue(TargetKey.SelectedKeyName);
            AIController->SetCurrentTarget(nullptr);
            
            UE_LOG(LogSGGameplay, Log, TEXT("⚠️ 未找到新目标"));
        }
    }
}
