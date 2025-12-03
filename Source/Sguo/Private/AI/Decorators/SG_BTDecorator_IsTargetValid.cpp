// 📄 文件：Source/Sguo/Private/AI/Decorators/SG_BTDecorator_IsTargetValid.cpp
// 🔧 修改 - 添加详细调试日志定位问题
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
 * 2. 增加详细日志便于调试
 */
bool USG_BTDecorator_IsTargetValid::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
    // ========== 步骤1：获取基础组件 ==========
    AAIController* AIController = OwnerComp.GetAIOwner();
    if (!AIController)
    {
        UE_LOG(LogSGGameplay, Warning, TEXT("❌ 目标有效性检查失败：AIController 为空"));
        return false;
    }
    
    // 🔧 调试 - 获取控制的单位名称
    FString UnitName = AIController->GetPawn() ? AIController->GetPawn()->GetName() : TEXT("Unknown");
    
    UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
    if (!BlackboardComp)
    {
        UE_LOG(LogSGGameplay, Warning, TEXT("❌ [%s] 目标有效性检查失败：黑板组件为空"), *UnitName);
        return false;
    }
    
    // ========== 步骤2：获取目标 ==========
    FName KeyName = TargetKey.SelectedKeyName;
    if (KeyName.IsNone())
    {
        KeyName = FName("CurrentTarget");
    }
    
    UObject* TargetObject = BlackboardComp->GetValueAsObject(KeyName);
    
    // 🔧 调试日志
    UE_LOG(LogSGGameplay, Verbose, TEXT("🔍 [%s] 目标有效性检查：键名=%s, 目标对象=%s"), 
        *UnitName,
        *KeyName.ToString(),
        TargetObject ? *TargetObject->GetName() : TEXT("NULL"));
    
    if (!TargetObject)
    {
        UE_LOG(LogSGGameplay, Verbose, TEXT("❌ [%s] 目标有效性检查失败：目标对象为空"), *UnitName);
        return false;
    }
    
    AActor* Target = Cast<AActor>(TargetObject);
    if (!Target)
    {
        UE_LOG(LogSGGameplay, Warning, TEXT("❌ [%s] 目标有效性检查失败：无法转换为 AActor（类型：%s）"), 
            *UnitName, *TargetObject->GetClass()->GetName());
        return false;
    }
    
    // ========== 步骤3：检查 Actor 基础有效性 ==========
    if (!IsValid(Target))
    {
        UE_LOG(LogSGGameplay, Warning, TEXT("❌ [%s] 目标有效性检查失败：Actor 已失效（PendingKill）"), *UnitName);
        return false;
    }
    
    // 🔧 调试 - 输出目标类型
    UE_LOG(LogSGGameplay, Verbose, TEXT("🔍 [%s] 目标类型：%s"), *UnitName, *Target->GetClass()->GetName());
    
    // ========== 🔧 修复 - 步骤4：优先检查主城类型 ==========
    ASG_MainCityBase* TargetMainCity = Cast<ASG_MainCityBase>(Target);
    if (TargetMainCity)
    {
        // 🔧 调试 - 输出主城详细信息
        UE_LOG(LogSGGameplay, Log, TEXT("🏰 [%s] 检查主城目标：%s"), *UnitName, *TargetMainCity->GetName());
        UE_LOG(LogSGGameplay, Log, TEXT("    bIsDestroyed: %s"), TargetMainCity->bIsDestroyed ? TEXT("true") : TEXT("false"));
        UE_LOG(LogSGGameplay, Log, TEXT("    IsAlive(): %s"), TargetMainCity->IsAlive() ? TEXT("true") : TEXT("false"));
        UE_LOG(LogSGGameplay, Log, TEXT("    CurrentHealth: %.0f"), TargetMainCity->GetCurrentHealth());
        UE_LOG(LogSGGameplay, Log, TEXT("    MaxHealth: %.0f"), TargetMainCity->GetMaxHealth());
        
        // 检查主城是否存活
        if (!TargetMainCity->IsAlive())
        {
            UE_LOG(LogSGGameplay, Warning, TEXT("❌ [%s] 目标主城已被摧毁：%s"), *UnitName, *TargetMainCity->GetName());
            return false;
        }
        
        // ✨ 主城有效
        UE_LOG(LogSGGameplay, Log, TEXT("✓ [%s] 目标主城有效：%s"), *UnitName, *TargetMainCity->GetName());
        return true;
    }
    
    // ========== 步骤5：检查单位类型 ==========
    ASG_UnitsBase* TargetUnit = Cast<ASG_UnitsBase>(Target);
    if (TargetUnit)
    {
        // 检查死亡状态
        if (TargetUnit->bIsDead)
        {
            UE_LOG(LogSGGameplay, Verbose, TEXT("❌ [%s] 目标单位已死亡：%s"), *UnitName, *TargetUnit->GetName());
            return false;
        }
        
        // 检查生命值
        if (TargetUnit->AttributeSet && TargetUnit->AttributeSet->GetHealth() <= 0.0f)
        {
            UE_LOG(LogSGGameplay, Verbose, TEXT("❌ [%s] 目标单位生命值为 0：%s"), *UnitName, *TargetUnit->GetName());
            return false;
        }
        
        // 检查是否可被选为目标
        if (!TargetUnit->CanBeTargeted())
        {
            UE_LOG(LogSGGameplay, Verbose, TEXT("❌ [%s] 目标单位不可被选中：%s"), *UnitName, *TargetUnit->GetName());
            return false;
        }
        
        UE_LOG(LogSGGameplay, Verbose, TEXT("✓ [%s] 目标单位有效：%s"), *UnitName, *TargetUnit->GetName());
        return true;
    }
    
    // ========== 步骤6：未知类型 ==========
    UE_LOG(LogSGGameplay, Error, TEXT("❌ [%s] 未知目标类型：%s（类：%s）- 既不是单位也不是主城！"), 
        *UnitName, *Target->GetName(), *Target->GetClass()->GetName());
    
    // 🔧 修改 - 未知类型返回 false，强制重新查找
    return false;
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
       
    }
}
