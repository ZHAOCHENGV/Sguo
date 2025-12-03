// 📄 文件：Source/Sguo/Private/AI/SG_StationaryAIController.cpp
// ✨ 新增 - 站桩单位专用 AI 控制器实现
// ✅ 这是完整文件

#include "AI/SG_StationaryAIController.h"
#include "Units/SG_StationaryUnit.h"
#include "Units/SG_UnitsBase.h"
#include "AbilitySystem/SG_AttributeSet.h"
#include "Debug/SG_LogCategories.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/OverlapResult.h"

// ========== 构造函数 ==========
ASG_StationaryAIController::ASG_StationaryAIController()
{
    PrimaryActorTick.bCanEverTick = true;
    bWantsPlayerState = false;
}

// ========== BeginPlay ==========
void ASG_StationaryAIController::BeginPlay()
{
    Super::BeginPlay();
}

// ========== OnPossess ==========
void ASG_StationaryAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);

    // 缓存控制的站桩单位
    ControlledStationaryUnit = Cast<ASG_StationaryUnit>(InPawn);

    if (ControlledStationaryUnit.IsValid())
    {
        UE_LOG(LogSGGameplay, Log, TEXT("[站桩AI] 控制单位：%s，攻击范围：%.0f"),
            *InPawn->GetName(),
            ControlledStationaryUnit->GetAttackRangeForAI());
    }
    else
    {
        UE_LOG(LogSGGameplay, Warning, TEXT("[站桩AI] %s 不是站桩单位类型"), *InPawn->GetName());
    }
}

// ========== OnUnPossess ==========
void ASG_StationaryAIController::OnUnPossess()
{
    // 解绑目标死亡事件
    if (CurrentListenedTarget.IsValid())
    {
        UnbindTargetDeathEvent(CurrentListenedTarget.Get());
        CurrentListenedTarget = nullptr;
    }

    CurrentTarget = nullptr;
    ControlledStationaryUnit = nullptr;

    Super::OnUnPossess();
}

// ========== Tick ==========
void ASG_StationaryAIController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bAIEnabled)
    {
        UpdateAI(DeltaTime);
    }
}

/**
 * @brief 更新 AI 逻辑
 * @param DeltaTime 帧间隔
 * @details
 * 功能说明：
 * - 检测当前目标是否有效
 * - 定期查找新目标
 * - 自动执行攻击
 */
void ASG_StationaryAIController::UpdateAI(float DeltaTime)
{
    if (!ControlledStationaryUnit.IsValid())
    {
        return;
    }

    // 检查单位是否已死亡
    if (ControlledStationaryUnit->bIsDead)
    {
        return;
    }

    // ========== 步骤1：检查当前目标有效性 ==========
    if (CurrentTarget.IsValid())
    {
        // 检查目标是否仍然有效且在攻击范围内
        if (!IsTargetValid() || !IsTargetInAttackRange(CurrentTarget.Get()))
        {
            // 目标无效或超出范围，清除目标
            SetCurrentTarget(nullptr);
        }
    }

    // ========== 步骤2：定期查找新目标 ==========
    TargetDetectionTimer += DeltaTime;
    if (TargetDetectionTimer >= TargetDetectionInterval)
    {
        TargetDetectionTimer = 0.0f;

        // 如果没有目标，查找新目标
        if (!CurrentTarget.IsValid())
        {
            AActor* NewTarget = FindTargetInAttackRange();
            if (NewTarget)
            {
                SetCurrentTarget(NewTarget);
            }
        }
    }

    // ========== 步骤3：自动攻击 ==========
    if (bAutoAttack && CurrentTarget.IsValid())
    {
        PerformAttack();
    }
}

/**
 * @brief 查找攻击范围内的目标
 * @return 找到的目标
 * @details
 * 功能说明：
 * - 使用球形检测查找范围内的敌方单位
 * - 不使用攻击槽位系统
 * - 优先选择最近的目标
 */
AActor* ASG_StationaryAIController::FindTargetInAttackRange()
{
    if (!ControlledStationaryUnit.IsValid())
    {
        return nullptr;
    }

    ASG_StationaryUnit* Unit = ControlledStationaryUnit.Get();
    FVector UnitLocation = Unit->GetActorLocation();
    FGameplayTag MyFaction = Unit->FactionTag;
    
    // 获取攻击范围
    float AttackRange = Unit->GetAttackRangeForAI() * AttackRangeMultiplier;

    // 使用球形检测
    UWorld* World = GetWorld();
    if (!World)
    {
        return nullptr;
    }

    // 设置查询参数
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(Unit);
    QueryParams.bTraceComplex = false;

    FCollisionObjectQueryParams ObjectParams;
    ObjectParams.AddObjectTypesToQuery(ECC_Pawn);

    FCollisionShape SphereShape = FCollisionShape::MakeSphere(AttackRange);

    // 执行球形重叠检测
    TArray<FOverlapResult> Overlaps;
    bool bHasOverlap = World->OverlapMultiByObjectType(
        Overlaps,
        UnitLocation,
        FQuat::Identity,
        ObjectParams,
        SphereShape,
        QueryParams
    );

    if (!bHasOverlap)
    {
        return nullptr;
    }

    // 查找最近的敌方单位
    AActor* NearestEnemy = nullptr;
    float NearestDistance = FLT_MAX;

    for (const FOverlapResult& Overlap : Overlaps)
    {
        AActor* Actor = Overlap.GetActor();
        if (!Actor)
        {
            continue;
        }

        // 检查是否是单位
        ASG_UnitsBase* TargetUnit = Cast<ASG_UnitsBase>(Actor);
        if (!TargetUnit)
        {
            continue;
        }

        // 跳过同阵营
        if (TargetUnit->FactionTag == MyFaction)
        {
            continue;
        }

        // 跳过死亡单位
        if (TargetUnit->bIsDead)
        {
            continue;
        }

        // 跳过不可被选为目标的单位
        if (!TargetUnit->CanBeTargeted())
        {
            continue;
        }

        // 计算距离
        float Distance = FVector::Dist(UnitLocation, TargetUnit->GetActorLocation());
        
        // 更新最近目标
        if (Distance < NearestDistance)
        {
            NearestDistance = Distance;
            NearestEnemy = TargetUnit;
        }
    }

    if (NearestEnemy)
    {
        UE_LOG(LogSGGameplay, Verbose, TEXT("[站桩AI] %s 找到目标：%s (距离: %.0f)"),
            *Unit->GetName(), *NearestEnemy->GetName(), NearestDistance);
    }

    return NearestEnemy;
}

/**
 * @brief 设置当前目标
 * @param NewTarget 新目标
 */
void ASG_StationaryAIController::SetCurrentTarget(AActor* NewTarget)
{
    AActor* OldTarget = CurrentTarget.Get();

    // 如果目标没变，不处理
    if (OldTarget == NewTarget)
    {
        return;
    }

    // 解绑旧目标的死亡事件
    if (CurrentListenedTarget.IsValid())
    {
        UnbindTargetDeathEvent(CurrentListenedTarget.Get());
        CurrentListenedTarget = nullptr;
    }

    // 设置新目标
    CurrentTarget = NewTarget;

    // 更新控制单位的目标
    if (ControlledStationaryUnit.IsValid())
    {
        ControlledStationaryUnit->SetTarget(NewTarget);
    }

    // 绑定新目标的死亡事件
    if (NewTarget)
    {
        if (ASG_UnitsBase* TargetUnit = Cast<ASG_UnitsBase>(NewTarget))
        {
            BindTargetDeathEvent(TargetUnit);
            CurrentListenedTarget = TargetUnit;
        }

        UE_LOG(LogSGGameplay, Log, TEXT("[站桩AI] %s 锁定目标：%s"),
            ControlledStationaryUnit.IsValid() ? *ControlledStationaryUnit->GetName() : TEXT("Unknown"),
            *NewTarget->GetName());
    }
    else
    {
        UE_LOG(LogSGGameplay, Verbose, TEXT("[站桩AI] %s 清除目标"),
            ControlledStationaryUnit.IsValid() ? *ControlledStationaryUnit->GetName() : TEXT("Unknown"));
    }
}

/**
 * @brief 检查当前目标是否有效
 * @return 目标是否有效
 */
bool ASG_StationaryAIController::IsTargetValid() const
{
    if (!CurrentTarget.IsValid())
    {
        return false;
    }

    AActor* Target = CurrentTarget.Get();

    // 检查是否是单位
    ASG_UnitsBase* TargetUnit = Cast<ASG_UnitsBase>(Target);
    if (TargetUnit)
    {
        // 检查是否死亡
        if (TargetUnit->bIsDead)
        {
            return false;
        }

        // 检查生命值
        if (TargetUnit->AttributeSet && TargetUnit->AttributeSet->GetHealth() <= 0.0f)
        {
            return false;
        }

        // 检查是否可被选为目标
        if (!TargetUnit->CanBeTargeted())
        {
            return false;
        }
    }

    return true;
}

/**
 * @brief 检查目标是否在攻击范围内
 * @param Target 要检查的目标
 * @return 是否在攻击范围内
 */
bool ASG_StationaryAIController::IsTargetInAttackRange(AActor* Target) const
{
    if (!Target || !ControlledStationaryUnit.IsValid())
    {
        return false;
    }

    ASG_StationaryUnit* Unit = ControlledStationaryUnit.Get();
    float AttackRange = Unit->GetAttackRangeForAI() * AttackRangeMultiplier;
    float Distance = FVector::Dist(Unit->GetActorLocation(), Target->GetActorLocation());

    return Distance <= AttackRange;
}

/**
 * @brief 执行攻击
 * @return 是否成功执行攻击
 */
bool ASG_StationaryAIController::PerformAttack()
{
    if (!ControlledStationaryUnit.IsValid())
    {
        return false;
    }

    ASG_StationaryUnit* Unit = ControlledStationaryUnit.Get();

    // 检查是否正在攻击
    if (Unit->bIsAttacking)
    {
        return false;
    }

    // ✨ 新增 - 检查是否正在执行计谋技能
    if (Unit->IsExecutingStrategySkill())
    {
        return false;
    }

    // 检查是否正在执行火矢技能（旧接口兼容）
    if (Unit->bIsExecutingFireArrow)
    {
        return false;
    }

    // 检查是否有目标
    if (!CurrentTarget.IsValid())
    {
        return false;
    }

    // 执行攻击
    return Unit->PerformAttack();
}

// ========== 目标死亡事件处理 ==========

/**
 * @brief 目标死亡回调
 * @param DeadUnit 死亡的单位
 */
void ASG_StationaryAIController::OnTargetDeath(ASG_UnitsBase* DeadUnit)
{
    // 检查是否是当前目标
    if (CurrentTarget.Get() != DeadUnit)
    {
        return;
    }

    UE_LOG(LogSGGameplay, Log, TEXT("[站桩AI] %s 的目标 %s 已死亡，查找新目标"),
        ControlledStationaryUnit.IsValid() ? *ControlledStationaryUnit->GetName() : TEXT("Unknown"),
        *DeadUnit->GetName());

    // 清除当前目标
    CurrentListenedTarget = nullptr;
    CurrentTarget = nullptr;

    // 更新控制单位的目标
    if (ControlledStationaryUnit.IsValid())
    {
        ControlledStationaryUnit->SetTarget(nullptr);
    }

    // 立即查找新目标
    AActor* NewTarget = FindTargetInAttackRange();
    if (NewTarget)
    {
        SetCurrentTarget(NewTarget);
    }
}

/**
 * @brief 绑定目标死亡事件
 * @param Target 目标单位
 */
void ASG_StationaryAIController::BindTargetDeathEvent(ASG_UnitsBase* Target)
{
    if (!Target)
    {
        return;
    }

    Target->OnUnitDeathEvent.AddDynamic(this, &ASG_StationaryAIController::OnTargetDeath);
}

/**
 * @brief 解绑目标死亡事件
 * @param Target 目标单位
 */
void ASG_StationaryAIController::UnbindTargetDeathEvent(ASG_UnitsBase* Target)
{
    if (!Target)
    {
        return;
    }

    Target->OnUnitDeathEvent.RemoveDynamic(this, &ASG_StationaryAIController::OnTargetDeath);
}
