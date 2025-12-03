// 📄 文件：Source/Sguo/Private/AI/Decorators/SG_BTDecorator_IsTargetValid.cpp
// 🔧 修改 - 完整修复主城目标有效性检查
// ✅ 这是完整文件

#include "AI/Decorators/SG_BTDecorator_IsTargetValid.h"

#include "AbilitySystem/SG_AttributeSet.h"
#include "AI/SG_AIControllerBase.h"
#include "Units/SG_UnitsBase.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Buildings/SG_MainCityBase.h"
#include "Debug/SG_LogCategories.h"
#include "BehaviorTree/BehaviorTreeComponent.h"

/**
 * @brief 构造函数
 */
USG_BTDecorator_IsTargetValid::USG_BTDecorator_IsTargetValid()
{
    NodeName = TEXT("目标是否有效");
    
    bNotifyTick = true;
    bNotifyBecomeRelevant = true;
    bNotifyCeaseRelevant = true;
    
    FlowAbortMode = EBTFlowAbortMode::Self;
    
    TargetKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(USG_BTDecorator_IsTargetValid, TargetKey), AActor::StaticClass());
    TargetKey.SelectedKeyName = FName("CurrentTarget");
}

/**
 * @brief 计算条件
 * @param OwnerComp 行为树组件
 * @param NodeMemory 节点内存
 * @return 条件是否满足
 * @details
 * 🔧 核心修复：
 * 1. 先检查主城类型（主城不是 ASG_UnitsBase 的子类）
 * 2. 增加 IsValid() 检查确保 Actor 未被销毁
 * 3. 增加详细日志便于调试
 */
bool USG_BTDecorator_IsTargetValid::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
    // ========== 步骤1：获取基础组件 ==========
    AAIController* AIController = OwnerComp.GetAIOwner();
    if (!AIController)
    {
        return false;
    }
    
    UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
    if (!BlackboardComp)
    {
        return false;
    }
    
    // ========== 步骤2：获取目标 ==========
    FName KeyName = TargetKey.SelectedKeyName;
    if (KeyName.IsNone())
    {
        KeyName = FName("CurrentTarget");
    }
    
    UObject* TargetObject = BlackboardComp->GetValueAsObject(KeyName);
    if (!TargetObject)
    {
        return false;
    }
    
    AActor* Target = Cast<AActor>(TargetObject);
    if (!Target)
    {
        return false;
    }
    
    // ========== 🔧 修复 - 步骤3：检查 Actor 基础有效性 ==========
    // 使用 IsValid() 检查 Actor 是否被标记为 PendingKill
    if (!IsValid(Target))
    {
        UE_LOG(LogSGGameplay, Verbose, TEXT("目标有效性检查：Actor 已失效（PendingKill）"));
        return false;
    }
    
    // ========== 🔧 修复 - 步骤4：优先检查主城类型 ==========
    // 主城不是 ASG_UnitsBase 的子类，必须单独检查
    if (ASG_MainCityBase* TargetMainCity = Cast<ASG_MainCityBase>(Target))
    {
        // 检查主城是否存活
        if (!TargetMainCity->IsAlive())
        {
            UE_LOG(LogSGGameplay, Log, TEXT("✗ 目标主城已被摧毁：%s"), *TargetMainCity->GetName());
            return false;
        }
        
        // ✨ 主城有效
        UE_LOG(LogSGGameplay, Verbose, TEXT("✓ 目标主城有效：%s"), *TargetMainCity->GetName());
        return true;
    }
    
    // ========== 步骤5：检查单位类型 ==========
    if (ASG_UnitsBase* TargetUnit = Cast<ASG_UnitsBase>(Target))
    {
        // 检查死亡状态
        if (TargetUnit->bIsDead)
        {
            UE_LOG(LogSGGameplay, Verbose, TEXT("目标单位已死亡：%s"), *TargetUnit->GetName());
            return false;
        }
        
        // 检查生命值
        if (TargetUnit->AttributeSet && TargetUnit->AttributeSet->GetHealth() <= 0.0f)
        {
            UE_LOG(LogSGGameplay, Verbose, TEXT("目标单位生命值为 0：%s"), *TargetUnit->GetName());
            return false;
        }
        
        // 检查是否可被选为目标
        if (!TargetUnit->CanBeTargeted())
        {
            UE_LOG(LogSGGameplay, Verbose, TEXT("目标单位不可被选中：%s"), *TargetUnit->GetName());
            return false;
        }
        
        return true;
    }
    
    // ========== 步骤6：未知类型，默认有效 ==========
    // 支持未来扩展的其他目标类型
    UE_LOG(LogSGGameplay, Warning, TEXT("⚠️ 未知目标类型：%s（%s）"), 
        *Target->GetName(), *Target->GetClass()->GetName());
    
    return true;
}

/**
 * @brief Tick 更新
 */
void USG_BTDecorator_IsTargetValid::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);
    
    bool bCurrentCondition = CalculateRawConditionValue(OwnerComp, NodeMemory);
    
    if (!bCurrentCondition)
    {
        OwnerComp.RequestExecution(this);
        UE_LOG(LogSGGameplay, Log, TEXT("🎯 目标无效，请求行为树重新评估"));
    }
}
