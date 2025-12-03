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
 * @details
 * 核心逻辑优化：
 * 1. 验证当前目标有效性。
 * 2. ✨ 动态择优：只要单位**未处于攻击动作中**（包含移动、发呆、被阻挡），就持续寻找更佳目标。
 * 这能解决单位被友军阻挡无法攻击当前目标时，自动切换到旁边没人打的敌人。
 */
void USG_BTService_UpdateTarget::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);
    
    ASG_AIControllerBase* AIController = Cast<ASG_AIControllerBase>(OwnerComp.GetAIOwner());
    if (!AIController) return;
    
    // 获取控制的单位
    ASG_UnitsBase* ControlledUnit = Cast<ASG_UnitsBase>(AIController->GetPawn());
    if (!ControlledUnit) return;
    
    UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
    if (!BlackboardComp) return;
    
    AActor* CurrentTarget = Cast<AActor>(BlackboardComp->GetValueAsObject(TargetKey.SelectedKeyName));
    
    // ========== 1. 验证当前目标是否有效 ==========
    bool bIsTargetValid = false;
    if (CurrentTarget)
    {
        ASG_UnitsBase* TargetUnit = Cast<ASG_UnitsBase>(CurrentTarget);
        if (TargetUnit)
        {
            // 目标必须：未死亡 + 可被选取 + 血量 > 0
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

    // ========== 2. ✨ 动态择优 (涵盖：移动中 / 站立不动 / 被阻挡) ==========
    // 只要单位没有在播放攻击动画/执行攻击逻辑 (!bIsAttacking)，它就有资格切换目标。
    // 场景举例：
    // - 单位正在前往目标 A（移动中）-> 发现路边有个更近的 B -> 切换。
    // - 单位被友军卡住走不到 A（被阻挡/原地踏步）-> 发现 B 就在脸前 -> 切换。
    // - 单位刚杀完人发呆（站立）-> 立即寻找最近的 B -> 切换。
    if (bIsTargetValid && !ControlledUnit->bIsAttacking)
    {
        // 调用底层寻敌逻辑（已包含拥挤度评分和距离权重）
        AActor* BetterTarget = AIController->FindNearestTarget();

        // 如果找到了目标，且这个目标不是当前目标，说明它是评分更高的“更优解”
        if (BetterTarget && BetterTarget != CurrentTarget)
        {
            // 获取当前和新目标的距离，仅用于日志显示
            float OldDist = ControlledUnit->GetDistanceTo(CurrentTarget);
            float NewDist = ControlledUnit->GetDistanceTo(BetterTarget);

            UE_LOG(LogSGGameplay, Log, TEXT("⚡ [动态索敌] %s 切换目标 (状态: %s): %s(%.0f) -> %s(%.0f)"), 
                *ControlledUnit->GetName(),
                ControlledUnit->GetVelocity().IsZero() ? TEXT("静止/阻挡") : TEXT("移动"), // 区分显示是卡住了还是在跑路
                *CurrentTarget->GetName(), OldDist,
                *BetterTarget->GetName(), NewDist);

            // 执行切换
            BlackboardComp->SetValueAsObject(TargetKey.SelectedKeyName, BetterTarget);
            AIController->SetCurrentTarget(BetterTarget);
            
            // 更新本地引用，直接跳过后续逻辑
            CurrentTarget = BetterTarget;
            return; 
        }
    }
    
    // ========== 3. 如果当前目标彻底失效（死亡/消失），查找新目标 ==========
    if (!bIsTargetValid)
    {
        bool bIsAttacking = ControlledUnit->bIsAttacking;
        
        // 仅在之前有目标时输出日志，避免空闲时刷屏
        if (CurrentTarget)
        {
            UE_LOG(LogSGGameplay, Log, TEXT("🔄 目标失效，重新寻敌 (曾攻击: %s)"), 
                bIsAttacking ? TEXT("是") : TEXT("否"));
        }
        
        AActor* NewTarget = AIController->FindNearestTarget();
        if (NewTarget)
        {
            BlackboardComp->SetValueAsObject(TargetKey.SelectedKeyName, NewTarget);
            AIController->SetCurrentTarget(NewTarget);
            
            UE_LOG(LogSGGameplay, Log, TEXT("✓ 找到新目标：%s"), *NewTarget->GetName());
            
            // 如果之前在攻击状态（比如站桩输出打死人了），重置攻击状态以便让单位动起来
            if (bIsAttacking)
            {
                BlackboardComp->SetValueAsBool(FName("IsInAttackRange"), false);
                ControlledUnit->bIsAttacking = false; 
            }
        }
        else
        {
            // 确实没目标了，清空数据
            if (CurrentTarget != nullptr)
            {
                BlackboardComp->ClearValue(TargetKey.SelectedKeyName);
                AIController->SetCurrentTarget(nullptr);
                UE_LOG(LogSGGameplay, Log, TEXT("⚠️ 视野内无有效目标，进入待机"));
            }
        }
    }
}
