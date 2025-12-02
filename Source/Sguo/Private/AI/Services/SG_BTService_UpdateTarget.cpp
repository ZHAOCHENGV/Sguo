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
    if (!AIController) return;
    
    ASG_UnitsBase* ControlledUnit = Cast<ASG_UnitsBase>(AIController->GetPawn());
    if (!ControlledUnit) return;
    
    UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
    if (!BlackboardComp) return;
    
    // 获取当前黑板中的目标
    AActor* CurrentTarget = Cast<AActor>(BlackboardComp->GetValueAsObject(TargetKey.SelectedKeyName));
    
    // ========== 1. 验证当前目标是否有效 ==========
    bool bIsTargetValid = false;
    if (CurrentTarget)
    {
        ASG_UnitsBase* TargetUnit = Cast<ASG_UnitsBase>(CurrentTarget);
        if (TargetUnit)
        {
            // 检查生命值、是否死亡、是否可被选取
            bIsTargetValid = !TargetUnit->bIsDead && 
                             TargetUnit->CanBeTargeted() && 
                             (!TargetUnit->AttributeSet || TargetUnit->AttributeSet->GetHealth() > 0.0f);
        }
        else
        {
            // 检查主城
            ASG_MainCityBase* TargetMainCity = Cast<ASG_MainCityBase>(CurrentTarget);
            if (TargetMainCity)
            {
                bIsTargetValid = TargetMainCity->IsAlive();
            }
        }
    }

    // ========== 2. ✨ 新增：移动过程中的动态择优 ==========
    // 如果当前目标有效，但我们还没开始攻击（说明还在路上），我们看看有没有更近的倒霉蛋
    if (bIsTargetValid && !ControlledUnit->bIsAttacking)
    {
        // 调用 Controller 的寻敌（底层应调用 Subsystem->FindBestTarget）
        // FindBestTarget 会根据距离和拥挤度评分，返回当前这一刻的最佳目标
        AActor* BetterTarget = AIController->FindNearestTarget();

        // 如果找到了目标，且这个目标不是当前目标 -> 说明它是“更好”的目标（通常意味着更近）
        if (BetterTarget && BetterTarget != CurrentTarget)
        {
            // 切换目标！
            UE_LOG(LogSGGameplay, Log, TEXT("⚡ [动态索敌] %s 在移动途中发现更好目标: %s -> %s"), 
                *ControlledUnit->GetName(),
                *CurrentTarget->GetName(),
                *BetterTarget->GetName());

            BlackboardComp->SetValueAsObject(TargetKey.SelectedKeyName, BetterTarget);
            AIController->SetCurrentTarget(BetterTarget);
            
            // 更新本地变量，防止进入下方的无效处理逻辑
            CurrentTarget = BetterTarget;
            
            // 只要切换了目标，就不需要继续执行下面的无效处理了
            return; 
        }
    }
    
    // ========== 3. 如果目标无效，查找新目标 ==========
    if (!bIsTargetValid)
    {
        bool bIsAttacking = ControlledUnit->bIsAttacking;
        
        // 只有当之前有目标且正在攻击时才输出详细日志，避免空闲时刷屏
        if (CurrentTarget)
        {
            UE_LOG(LogSGGameplay, Log, TEXT("🔄 目标无效/死亡，查找新目标 (曾攻击: %s)"), 
                bIsAttacking ? TEXT("是") : TEXT("否"));
        }
        
        AActor* NewTarget = AIController->FindNearestTarget();
        if (NewTarget)
        {
            BlackboardComp->SetValueAsObject(TargetKey.SelectedKeyName, NewTarget);
            AIController->SetCurrentTarget(NewTarget);
            
            UE_LOG(LogSGGameplay, Log, TEXT("✓ 找到新目标：%s"), *NewTarget->GetName());
            
            // 如果处于攻击状态但目标没了，重置攻击状态以便重新寻路
            if (bIsAttacking)
            {
                BlackboardComp->SetValueAsBool(FName("IsInAttackRange"), false);
                ControlledUnit->bIsAttacking = false; // 确保状态复位
            }
        }
        else
        {
            // 真没目标了（全清空了）
            if (CurrentTarget != nullptr)
            {
                BlackboardComp->ClearValue(TargetKey.SelectedKeyName);
                AIController->SetCurrentTarget(nullptr);
                UE_LOG(LogSGGameplay, Log, TEXT("⚠️ 视野内无有效目标"));
            }
        }
    }
}
