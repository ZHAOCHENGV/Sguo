// 🔧 修改 - SG_AIControllerBase.cpp

#include "AI/SG_AIControllerBase.h"
#include "AbilitySystem/SG_AttributeSet.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Units/SG_UnitsBase.h"
#include "Kismet/GameplayStatics.h"
#include "Buildings/SG_MainCityBase.h"
#include "Debug/SG_LogCategories.h"
#include "Components/BoxComponent.h"

// ========== 黑板键名称定义 ==========
const FName ASG_AIControllerBase::BB_CurrentTarget = TEXT("CurrentTarget");
const FName ASG_AIControllerBase::BB_IsInAttackRange = TEXT("IsInAttackRange");
const FName ASG_AIControllerBase::BB_IsTargetLocked = TEXT("IsTargetLocked");
const FName ASG_AIControllerBase::BB_IsTargetMainCity = TEXT("IsTargetMainCity");

// ========== 构造函数 ==========
ASG_AIControllerBase::ASG_AIControllerBase()
{
    PrimaryActorTick.bCanEverTick = true;
    bWantsPlayerState = false;
    bSetControlRotationFromPawnOrientation = false;
}

// ========== BeginPlay ==========
void ASG_AIControllerBase::BeginPlay()
{
    Super::BeginPlay();
    UE_LOG(LogSGGameplay, Log, TEXT("✓ AI 控制器 BeginPlay 完成"));
}

// ========== OnPossess ==========
/**
 * @brief 控制 Pawn 时调用
 * @param InPawn 被控制的 Pawn
 */
void ASG_AIControllerBase::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    
    UE_LOG(LogSGGameplay, Log, TEXT("========== AI 控制器 OnPossess =========="));
    UE_LOG(LogSGGameplay, Log, TEXT("  控制的 Pawn：%s"), *InPawn->GetName());
    
    // ========== 步骤1：确定要使用的行为树 ==========
    UBehaviorTree* BehaviorTreeToUse = nullptr;
    
    // 优先检查单位是否有自定义行为树
    ASG_UnitsBase* ControlledUnit = Cast<ASG_UnitsBase>(InPawn);
    if (ControlledUnit)
    {
        BehaviorTreeToUse = ControlledUnit->GetUnitBehaviorTree();
        
        if (BehaviorTreeToUse)
        {
            UE_LOG(LogSGGameplay, Log, TEXT("  📋 使用单位自定义行为树：%s"), *BehaviorTreeToUse->GetName());
        }
    }
    
    // 如果单位没有自定义行为树，使用控制器默认的
    if (!BehaviorTreeToUse && DefaultBehaviorTree)
    {
        BehaviorTreeToUse = DefaultBehaviorTree;
        UE_LOG(LogSGGameplay, Log, TEXT("  📋 使用控制器默认行为树：%s"), *BehaviorTreeToUse->GetName());
    }
    
    // 如果都没有，输出警告
    if (!BehaviorTreeToUse)
    {
        UE_LOG(LogSGGameplay, Warning, TEXT("  ⚠️ 没有可用的行为树！"));
        return;
    }
    
    // ========== 步骤2：启动行为树 ==========
    bool bSuccess = StartBehaviorTree(BehaviorTreeToUse);
    
    if (bSuccess)
    {
        UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 行为树启动成功"));
    }
    else
    {
        UE_LOG(LogSGGameplay, Error, TEXT("  ❌ 行为树启动失败"));
    }
    
    UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
}

// ========== SetupBehaviorTree ==========
/**
 * @brief 初始化并启动行为树
 * @param BehaviorTreeToUse 要使用的行为树
 * @return 是否成功
 */
bool ASG_AIControllerBase::SetupBehaviorTree(UBehaviorTree* BehaviorTreeToUse)
{
    if (!BehaviorTreeToUse)
    {
        return false;
    }
    
    // 获取行为树的黑板资产
    UBlackboardData* BlackboardAsset = BehaviorTreeToUse->BlackboardAsset;
    if (!BlackboardAsset)
    {
        UE_LOG(LogSGGameplay, Error, TEXT("  ❌ 行为树没有关联的黑板资产"));
        return false;
    }
    
    // 🔧 修复 - 使用正确的方式调用 UseBlackboard
    UBlackboardComponent* BlackboardComp = nullptr;
    bool bSuccess = UseBlackboard(BlackboardAsset, BlackboardComp);
    
    if (bSuccess && BlackboardComp)
    {
        // 初始化黑板数据
        BlackboardComp->SetValueAsBool(BB_IsTargetLocked, false);
        BlackboardComp->SetValueAsBool(BB_IsInAttackRange, false);
        BlackboardComp->SetValueAsBool(BB_IsTargetMainCity, false);
        
        UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 黑板初始化成功"));
        return true;
    }
    
    UE_LOG(LogSGGameplay, Error, TEXT("  ❌ 黑板初始化失败"));
    return false;
}

// ========== StartBehaviorTree ==========
/**
 * @brief 启动指定的行为树
 * @param BehaviorTreeToRun 要运行的行为树
 * @return 是否成功启动
 */
bool ASG_AIControllerBase::StartBehaviorTree(UBehaviorTree* BehaviorTreeToRun)
{
    if (!BehaviorTreeToRun)
    {
        UE_LOG(LogSGGameplay, Error, TEXT("  ❌ StartBehaviorTree：行为树为空"));
        return false;
    }
    
    // 🔧 修复 - 使用 GetBrainComponent 获取行为树组件
    UBehaviorTreeComponent* BTComp = Cast<UBehaviorTreeComponent>(GetBrainComponent());
    
    // 停止当前行为树（如果有）
    if (BTComp && BTComp->IsRunning())
    {
        BTComp->StopTree(EBTStopMode::Safe);
        UE_LOG(LogSGGameplay, Verbose, TEXT("  🛑 停止当前行为树"));
    }
    
    // 初始化黑板
    if (!SetupBehaviorTree(BehaviorTreeToRun))
    {
        UE_LOG(LogSGGameplay, Error, TEXT("  ❌ 黑板初始化失败，无法启动行为树"));
        return false;
    }
    
    // 🔧 修复 - 使用 AAIController 的 RunBehaviorTree 函数
    bool bSuccess = AAIController::RunBehaviorTree(BehaviorTreeToRun);
    
    if (bSuccess)
    {
        CurrentBehaviorTree = BehaviorTreeToRun;
        UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 行为树启动成功：%s"), *BehaviorTreeToRun->GetName());
    }
    else
    {
        UE_LOG(LogSGGameplay, Error, TEXT("  ❌ RunBehaviorTree 失败：%s"), *BehaviorTreeToRun->GetName());
    }
    
    return bSuccess;
}

// ========== OnUnPossess ==========
void ASG_AIControllerBase::OnUnPossess()
{
    // 解绑目标死亡事件
    if (CurrentListenedTarget.IsValid())
    {
        UnbindTargetDeathEvent(CurrentListenedTarget.Get());
        CurrentListenedTarget = nullptr;
    }
    
    // 🔧 修复 - 使用 GetBrainComponent 获取行为树组件
    UBehaviorTreeComponent* BTComp = Cast<UBehaviorTreeComponent>(GetBrainComponent());
    if (BTComp && BTComp->IsRunning())
    {
        BTComp->StopTree(EBTStopMode::Safe);
    }
    
    // 清空当前行为树引用
    CurrentBehaviorTree = nullptr;
    
    Super::OnUnPossess();
}

// ========== FreezeAI ==========
void ASG_AIControllerBase::FreezeAI()
{
    // 1. 停止行为树
    // 🔧 修复 - 使用 GetBrainComponent 获取行为树组件
    UBehaviorTreeComponent* BTComp = Cast<UBehaviorTreeComponent>(GetBrainComponent());
    if (BTComp)
    {
        BTComp->StopTree(EBTStopMode::Safe);
    }
    
    // 2. 停止移动
    StopMovement();
    
    // 3. 解绑目标死亡事件
    if (CurrentListenedTarget.IsValid())
    {
        UnbindTargetDeathEvent(CurrentListenedTarget.Get());
        CurrentListenedTarget = nullptr;
    }
    
    // 4. 清除目标
    SetCurrentTarget(nullptr);
    
    // 5. 停止所有逻辑更新
    SetActorTickEnabled(false);
    
    UE_LOG(LogSGGameplay, Log, TEXT("🥶 AI 已冻结：%s"), 
        GetPawn() ? *GetPawn()->GetName() : TEXT("None"));
}

// ========== 以下是原有函数，保持不变 ==========

// FindNearestTarget
AActor* ASG_AIControllerBase::FindNearestTarget()
{
    // ... 保持原有代码不变 ...
    
    // 1. 获取控制的单位
    ASG_UnitsBase* ControlledUnit = Cast<ASG_UnitsBase>(GetPawn());
    if (!ControlledUnit) 
    {
        UE_LOG(LogSGGameplay, Error, TEXT("FindNearestTarget: 控制的单位为空"));
        return nullptr;
    }

    FGameplayTag MyFaction = ControlledUnit->FactionTag;
    FVector MyLoc = ControlledUnit->GetActorLocation();
    
    float DetectionRadius = ControlledUnit->GetDetectionRange();
    ESGTargetSearchShape SearchShape = ControlledUnit->TargetSearchShape;
    bool bPrioritizeFrontmost = ControlledUnit->bPrioritizeFrontmost;

    UE_LOG(LogSGGameplay, Verbose, TEXT("FindNearestTarget: %s 开始寻找目标"), *ControlledUnit->GetName());

    // 2. 准备候选列表
    TArray<AActor*> AllUnits;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASG_UnitsBase::StaticClass(), AllUnits);
    
    TArray<AActor*> AllMainCities;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASG_MainCityBase::StaticClass(), AllMainCities);

    // 3. 筛选有效的敌方单位
    TArray<AActor*> ValidEnemyUnits;
    
    for (AActor* Actor : AllUnits)
    {
        if (Actor == ControlledUnit) continue;

        ASG_UnitsBase* Unit = Cast<ASG_UnitsBase>(Actor);
        if (!Unit) continue;
        if (Unit->bIsDead) continue;
        if (Unit->FactionTag == MyFaction) continue;
        if (!Unit->CanBeTargeted()) continue;

        FVector TargetLoc = Unit->GetActorLocation();
        bool bInRange = false;
        
        if (SearchShape == ESGTargetSearchShape::Square)
        {
            float DiffX = FMath::Abs(TargetLoc.X - MyLoc.X);
            float DiffY = FMath::Abs(TargetLoc.Y - MyLoc.Y);
            bInRange = (DiffX <= DetectionRadius && DiffY <= DetectionRadius);
        }
        else
        {
            bInRange = (FVector::DistSquared(TargetLoc, MyLoc) <= (DetectionRadius * DetectionRadius));
        }

        if (bInRange)
        {
            ValidEnemyUnits.Add(Unit);
        }
    }

    // 4. 如果有敌方单位，选择最佳目标
    if (ValidEnemyUnits.Num() > 0)
    {
        AActor* BestTarget = nullptr;
        
        if (bPrioritizeFrontmost)
        {
            float BestXDiff = FLT_MAX;
            for (AActor* Target : ValidEnemyUnits)
            {
                float DistX = FMath::Abs(Target->GetActorLocation().X - MyLoc.X);
                if (DistX < BestXDiff)
                {
                    BestXDiff = DistX;
                    BestTarget = Target;
                }
            }
        }
        else
        {
            float BestDistSq = FLT_MAX;
            for (AActor* Target : ValidEnemyUnits)
            {
                float DistSq = FVector::DistSquared(Target->GetActorLocation(), MyLoc);
                if (DistSq < BestDistSq)
                {
                    BestDistSq = DistSq;
                    BestTarget = Target;
                }
            }
        }
        
        if (BestTarget)
        {
            UE_LOG(LogSGGameplay, Log, TEXT("FindNearestTarget: 选中敌方单位 %s"), *BestTarget->GetName());
            return BestTarget;
        }
    }

    // 5. 如果没有敌方单位，查找敌方主城
    AActor* NearestMainCity = nullptr;
    float NearestMainCityDist = FLT_MAX;
    
    for (AActor* Actor : AllMainCities)
    {
        ASG_MainCityBase* City = Cast<ASG_MainCityBase>(Actor);
        if (!City) continue;
        if (!City->IsAlive()) continue;
        if (City->FactionTag == MyFaction) continue;
        
        float Dist = FVector::Dist(MyLoc, City->GetActorLocation());
        
        if (Dist < NearestMainCityDist)
        {
            NearestMainCityDist = Dist;
            NearestMainCity = City;
        }
    }
    
    if (NearestMainCity)
    {
        UE_LOG(LogSGGameplay, Log, TEXT("FindNearestTarget: 选中敌方主城 %s"), *NearestMainCity->GetName());
        return NearestMainCity;
    }

    UE_LOG(LogSGGameplay, Warning, TEXT("FindNearestTarget: 未找到任何敌方目标"));
    return nullptr;
}

// DetectNearbyThreats
bool ASG_AIControllerBase::DetectNearbyThreats(float DetectionRadius)
{
    ASG_UnitsBase* ControlledUnit = Cast<ASG_UnitsBase>(GetPawn());
    if (!ControlledUnit)
    {
        return false;
    }
    
    AActor* CurrentTarget = GetCurrentTarget();
    
    UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
    if (BlackboardComp && !BlackboardComp->GetValueAsBool(BB_IsTargetMainCity))
    {
        return false;
    }
    
    FGameplayTag MyFaction = ControlledUnit->FactionTag;
    
    TArray<AActor*> AllUnits;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASG_UnitsBase::StaticClass(), AllUnits);
    
    for (AActor* Actor : AllUnits)
    {
        if (Actor == ControlledUnit || Actor == CurrentTarget)
        {
            continue;
        }
        
        ASG_UnitsBase* Unit = Cast<ASG_UnitsBase>(Actor);
        if (!Unit)
        {
            continue;
        }
        
        if (Unit->FactionTag != MyFaction)
        {
            if (Unit->bIsDead)
            {
                continue;
            }
            
            if (!Unit->CanBeTargeted())
            {
                continue;
            }
            
            float Distance = FVector::Dist(ControlledUnit->GetActorLocation(), Unit->GetActorLocation());
            
            if (Distance <= DetectionRadius)
            {
                SetCurrentTarget(Unit);
                StopMovement();

                UE_LOG(LogSGGameplay, Log, TEXT("🔄 %s 检测到周边威胁，转移目标到：%s"), 
                    *ControlledUnit->GetName(), *Unit->GetName());
                return true;
            }
        }
    }
    
    return false;
}

// SetCurrentTarget
void ASG_AIControllerBase::SetCurrentTarget(AActor* NewTarget)
{
    UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
    if (!BlackboardComp)
    {
        return;
    }
    
    if (CurrentListenedTarget.IsValid())
    {
        UnbindTargetDeathEvent(CurrentListenedTarget.Get());
        CurrentListenedTarget = nullptr;
    }
    
    BlackboardComp->SetValueAsObject(BB_CurrentTarget, NewTarget);
    
    bool bTargetIsMainCity = false;
    if (NewTarget)
    {
        bTargetIsMainCity = NewTarget->IsA(ASG_MainCityBase::StaticClass());
    }
    BlackboardComp->SetValueAsBool(BB_IsTargetMainCity, bTargetIsMainCity);
    BlackboardComp->SetValueAsBool(BB_IsTargetLocked, NewTarget != nullptr);
    
    if (ASG_UnitsBase* ControlledUnit = Cast<ASG_UnitsBase>(GetPawn()))
    {
        ControlledUnit->SetTarget(NewTarget);
    }
    
    if (NewTarget)
    {
        if (ASG_UnitsBase* TargetUnit = Cast<ASG_UnitsBase>(NewTarget))
        {
            BindTargetDeathEvent(TargetUnit);
            CurrentListenedTarget = TargetUnit;
        }
        
        UE_LOG(LogSGGameplay, Verbose, TEXT("🎯 设置目标：%s%s"), 
            *NewTarget->GetName(),
            bTargetIsMainCity ? TEXT(" (主城)") : TEXT(""));
    }
    else
    {
        UE_LOG(LogSGGameplay, Verbose, TEXT("🎯 清空目标"));
    }
}

// GetCurrentTarget
AActor* ASG_AIControllerBase::GetCurrentTarget() const
{
    const UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
    if (!BlackboardComp)
    {
        return nullptr;
    }
    
    return Cast<AActor>(BlackboardComp->GetValueAsObject(BB_CurrentTarget));
}

// IsTargetValid
bool ASG_AIControllerBase::IsTargetValid() const
{
    AActor* CurrentTarget = GetCurrentTarget();
    if (!CurrentTarget)
    {
        return false;
    }
    
    ASG_UnitsBase* TargetUnit = Cast<ASG_UnitsBase>(CurrentTarget);
    if (TargetUnit)
    {
        if (TargetUnit->bIsDead)
        {
            return false;
        }
        
        if (TargetUnit->AttributeSet && TargetUnit->AttributeSet->GetHealth() <= 0.0f)
        {
            return false;
        }
        
        if (!TargetUnit->CanBeTargeted())
        {
            return false;
        }
    }
    
    ASG_MainCityBase* TargetMainCity = Cast<ASG_MainCityBase>(CurrentTarget);
    if (TargetMainCity)
    {
        float MainCityHealth = TargetMainCity->GetCurrentHealth();
        if (MainCityHealth <= 0.0f)
        {
            return false;
        }
    }
    
    return true;
}

// InterruptAttack
void ASG_AIControllerBase::InterruptAttack()
{
    if (!bIsMainCity)
    {
        return;
    }
    
    bAttackInterrupted = true;
    
    UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
    if (BlackboardComp)
    {
        BlackboardComp->SetValueAsBool(TEXT("AttackInterrupted"), true);
    }
    
    UE_LOG(LogSGGameplay, Log, TEXT("🛑 主城攻击被打断"));
}

// ResumeAttack
void ASG_AIControllerBase::ResumeAttack()
{
    if (!bIsMainCity)
    {
        return;
    }
    
    bAttackInterrupted = false;
    
    UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
    if (BlackboardComp)
    {
        BlackboardComp->SetValueAsBool(TEXT("AttackInterrupted"), false);
    }
    
    UE_LOG(LogSGGameplay, Log, TEXT("▶️ 主城恢复攻击"));
}

// OnTargetDeath
void ASG_AIControllerBase::OnTargetDeath(ASG_UnitsBase* DeadUnit)
{
    AActor* CurrentTarget = GetCurrentTarget();
    if (CurrentTarget != DeadUnit)
    {
        return;
    }
    
    UE_LOG(LogSGGameplay, Log, TEXT("🎯 目标死亡，需要重新寻找目标"));
    
    CurrentListenedTarget = nullptr;
    
    UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
    if (BlackboardComp)
    {
        BlackboardComp->SetValueAsObject(BB_CurrentTarget, nullptr);
        BlackboardComp->SetValueAsBool(BB_IsTargetLocked, false);
        BlackboardComp->SetValueAsBool(BB_IsTargetMainCity, false);
    }
    
    if (ASG_UnitsBase* ControlledUnit = Cast<ASG_UnitsBase>(GetPawn()))
    {
        ControlledUnit->SetTarget(nullptr);
    }
    
    AActor* NewTarget = FindNearestTarget();
    if (NewTarget)
    {
        SetCurrentTarget(NewTarget);
        UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 找到新目标：%s"), *NewTarget->GetName());
    }
    else
    {
        UE_LOG(LogSGGameplay, Log, TEXT("  ⚠️ 未找到新目标"));
    }
}

// BindTargetDeathEvent
void ASG_AIControllerBase::BindTargetDeathEvent(ASG_UnitsBase* Target)
{
    if (!Target)
    {
        return;
    }
    
    Target->OnUnitDeathEvent.AddDynamic(this, &ASG_AIControllerBase::OnTargetDeath);
    
    UE_LOG(LogSGGameplay, Verbose, TEXT("  ✓ 绑定目标死亡事件：%s"), *Target->GetName());
}

// UnbindTargetDeathEvent
void ASG_AIControllerBase::UnbindTargetDeathEvent(ASG_UnitsBase* Target)
{
    if (!Target)
    {
        return;
    }
    
    Target->OnUnitDeathEvent.RemoveDynamic(this, &ASG_AIControllerBase::OnTargetDeath);
    
    UE_LOG(LogSGGameplay, Verbose, TEXT("  ✓ 解绑目标死亡事件：%s"), *Target->GetName());
}
