// 📄 文件：Source/Sguo/Private/AbilitySystem/Abilities/SG_GameplayAbility_SummonGroup.cpp
// 🔧 修改 - 修复蒙太奇播放和动画状态同步问题

#include "AbilitySystem/Abilities/SG_GameplayAbility_SummonGroup.h"
#include "Units/SG_UnitsBase.h"
#include "GameFramework/Character.h"
#include "Kismet/KismetMathLibrary.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystem/SG_AttributeSet.h"
#include "Data/Type/SG_UnitDataTable.h"

USG_GameplayAbility_SummonGroup::USG_GameplayAbility_SummonGroup()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    TriggerEventTag = FGameplayTag::RequestGameplayTag(FName("Ability.Event.Spawn")); 
}

/**
 * @brief 激活能力
 * @details
 * 🔧 修改说明：
 * - 修复蒙太奇播放逻辑
 * - 添加动画状态同步（调用 StartAttackAnimation）
 * - 确保即使没有蒙太奇也能正常执行召唤逻辑
 */
void USG_GameplayAbility_SummonGroup::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle, 
    const FGameplayAbilityActorInfo* ActorInfo, 
    const FGameplayAbilityActivationInfo ActivationInfo, 
    const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 🔧 修改 - 获取施放者单位引用
    ASG_UnitsBase* OwnerUnit = Cast<ASG_UnitsBase>(ActorInfo->AvatarActor.Get());
    
    // 1. 获取蒙太奇
    UAnimMontage* MontageToPlay = FindMontageFromUnitData();
    
    // 🔧 修改 - 如果没有蒙太奇，直接执行召唤（不再强制结束）
    if (!MontageToPlay)
    {
        UE_LOG(LogTemp, Warning, TEXT("SummonGroup: 未找到蒙太奇，直接执行召唤"));
        
        // ✨ 新增 - 即使没有蒙太奇，也设置一个短暂的动画状态
        if (OwnerUnit)
        {
            OwnerUnit->StartAttackAnimation(0.5f);
        }
        
        // 直接执行召唤
        ExecuteSpawn();
        
        // 延迟结束能力
        FTimerHandle TimerHandle;
        FTimerDelegate TimerDelegate;
        TimerDelegate.BindLambda([this, Handle, ActorInfo, ActivationInfo]()
        {
            EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        });
        ActorInfo->AvatarActor->GetWorldTimerManager().SetTimer(TimerHandle, TimerDelegate, 0.5f, false);
        return;
    }

    // ========== 有蒙太奇的正常流程 ==========

    // 2.【先】创建事件监听 Task
    UAbilityTask_WaitGameplayEvent* WaitEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
        this,
        TriggerEventTag,
        nullptr, 
        false,   
        true     
    );

    if (WaitEventTask)
    {
        WaitEventTask->EventReceived.AddDynamic(this, &USG_GameplayAbility_SummonGroup::OnSpawnEventReceived);
        WaitEventTask->ReadyForActivation();
    }

    // 3.【后】创建播放蒙太奇 Task
    // 🔧 修改 - 获取攻击速度倍率
    float PlayRate = 1.0f;
    if (OwnerUnit && OwnerUnit->AttributeSet)
    {
        PlayRate = OwnerUnit->AttributeSet->GetAttackSpeed();
        if (PlayRate <= 0.0f) PlayRate = 1.0f;
    }
    
    UAbilityTask_PlayMontageAndWait* PlayMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
        this,
        NAME_None,
        MontageToPlay,
        PlayRate,  // 🔧 修改 - 使用攻击速度倍率
        NAME_None,
        false,
        1.0f
    );

    if (!PlayMontageTask)
    {
        UE_LOG(LogTemp, Error, TEXT("SummonGroup: 创建蒙太奇任务失败"));
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    // 绑定回调
    PlayMontageTask->OnBlendOut.AddDynamic(this, &USG_GameplayAbility_SummonGroup::OnMontageCompleted);
    PlayMontageTask->OnCompleted.AddDynamic(this, &USG_GameplayAbility_SummonGroup::OnMontageCompleted);
    PlayMontageTask->OnInterrupted.AddDynamic(this, &USG_GameplayAbility_SummonGroup::OnMontageCancelled);
    PlayMontageTask->OnCancelled.AddDynamic(this, &USG_GameplayAbility_SummonGroup::OnMontageCancelled);

    PlayMontageTask->ReadyForActivation();

    // ✨ 新增 - 计算实际动画时长并通知单位
    float MontageLength = MontageToPlay->GetPlayLength();
    float ActualDuration = (PlayRate > 0.0f) ? (MontageLength / PlayRate) : MontageLength;
    
    if (OwnerUnit)
    {
        OwnerUnit->StartAttackAnimation(ActualDuration);
        UE_LOG(LogTemp, Log, TEXT("SummonGroup: 开始播放蒙太奇 %s，时长：%.2f秒"), 
            *MontageToPlay->GetName(), ActualDuration);
    }
}

// ✨ 新增 - 处理动画正常结束
void USG_GameplayAbility_SummonGroup::OnMontageCompleted()
{
    // 🔧 修改 - 通知单位动画结束
    if (AActor* AvatarActor = GetAvatarActorFromActorInfo())
    {
        if (ASG_UnitsBase* OwnerUnit = Cast<ASG_UnitsBase>(AvatarActor))
        {
            OwnerUnit->OnAttackAnimationFinished();
        }
    }
    
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

// ✨ 新增 - 处理动画被取消/打断
void USG_GameplayAbility_SummonGroup::OnMontageCancelled()
{
    // 🔧 修改 - 通知单位动画结束
    if (AActor* AvatarActor = GetAvatarActorFromActorInfo())
    {
        if (ASG_UnitsBase* OwnerUnit = Cast<ASG_UnitsBase>(AvatarActor))
        {
            OwnerUnit->OnAttackAnimationFinished();
        }
    }
    
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void USG_GameplayAbility_SummonGroup::OnSpawnEventReceived(FGameplayEventData Payload)
{
    // 收到通知，执行核心逻辑
    ExecuteSpawn();
}

/**
 * @brief 从单位数据中查找蒙太奇
 * @return 找到的蒙太奇，失败返回 nullptr
 * @details
 * 🔧 修改说明：
 * - 简化逻辑，直接信任 CurrentAttackIndex
 * - 添加更详细的日志输出
 */
UAnimMontage* USG_GameplayAbility_SummonGroup::FindMontageFromUnitData() const
{
    ASG_UnitsBase* OwnerUnit = Cast<ASG_UnitsBase>(GetAvatarActorFromActorInfo());
    if (!OwnerUnit)
    {
        UE_LOG(LogTemp, Error, TEXT("SummonGroup::FindMontageFromUnitData - OwnerUnit 为空"));
        return nullptr;
    }

    // 🔧 修改 - 检查攻击配置列表是否有效
    if (OwnerUnit->CachedAttackAbilities.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("SummonGroup::FindMontageFromUnitData - CachedAttackAbilities 为空"));
        return nullptr;
    }
    
    // 🔧 修改 - 检查索引有效性
    if (!OwnerUnit->CachedAttackAbilities.IsValidIndex(OwnerUnit->CurrentAttackIndex))
    {
        UE_LOG(LogTemp, Error, TEXT("SummonGroup::FindMontageFromUnitData - CurrentAttackIndex(%d) 无效，列表大小：%d"), 
            OwnerUnit->CurrentAttackIndex, 
            OwnerUnit->CachedAttackAbilities.Num());
        return nullptr;
    }

    // 直接获取当前攻击配置
    FSGUnitAttackDefinition AttackDef = OwnerUnit->GetCurrentAttackDefinition();
    
    if (AttackDef.Montage)
    {
        UE_LOG(LogTemp, Log, TEXT("[SummonGroup] 成功获取蒙太奇: %s (Index: %d)"), 
            *AttackDef.Montage->GetName(), 
            OwnerUnit->CurrentAttackIndex);
        return AttackDef.Montage;
    }

    UE_LOG(LogTemp, Warning, TEXT("[SummonGroup] 警告：当前攻击配置 (Index: %d) 未设置蒙太奇！"), 
        OwnerUnit->CurrentAttackIndex);
    
    return nullptr;
}

void USG_GameplayAbility_SummonGroup::ExecuteSpawn()
{
    // ... 原有逻辑保持不变 ...
    ACharacter* OwnerCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
    ASG_UnitsBase* OwnerUnit = Cast<ASG_UnitsBase>(OwnerCharacter);
    if (!OwnerCharacter) return;

    FVector OwnerLocation = OwnerCharacter->GetActorLocation();
    FRotator OwnerRotation = OwnerCharacter->GetActorRotation();
    FVector ForwardVector = OwnerRotation.Vector();

    FVector FormationCenter = OwnerLocation;

    switch (LocationType)
    {
    case ESGSummonLocationType::BehindOwner:
        FormationCenter -= ForwardVector * SpawnDistanceOffset;
        break;
    case ESGSummonLocationType::InFrontOfOwner:
        FormationCenter += ForwardVector * SpawnDistanceOffset;
        break;
    case ESGSummonLocationType::AroundOwner:
        FormationCenter = OwnerLocation; 
        break;
    case ESGSummonLocationType::AtTargetLocation:
        if (OwnerUnit && OwnerUnit->CurrentTarget)
        {
            FormationCenter = OwnerUnit->CurrentTarget->GetActorLocation();
        }
        else
        {
            FormationCenter += ForwardVector * SpawnDistanceOffset;
        }
        break;
    }

    for (int32 i = 0; i < SpawnCount; i++)
    {
        TSubclassOf<ASG_UnitsBase> SpawnClass = GetRandomUnitClass();
        if (!SpawnClass) continue;

        FVector SpawnLoc = CalculateSpawnLocation(i, FormationCenter, OwnerRotation);
        
        if (SpawnRandomRange > 0.0f)
        {
            SpawnLoc.X += FMath::FRandRange(-SpawnRandomRange, SpawnRandomRange);
            SpawnLoc.Y += FMath::FRandRange(-SpawnRandomRange, SpawnRandomRange);
        }

        FHitResult HitResult;
        FVector TraceStart = SpawnLoc + FVector(0, 0, 500);
        FVector TraceEnd = SpawnLoc - FVector(0, 0, 500);
        if (GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_WorldStatic))
        {
            SpawnLoc.Z = HitResult.Location.Z + 10.0f;
        }

        FRotator SpawnRot = CalculateSpawnRotation(SpawnLoc, FormationCenter, OwnerRotation);

        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        
        ASG_UnitsBase* NewUnit = GetWorld()->SpawnActor<ASG_UnitsBase>(
            SpawnClass, 
            SpawnLoc, 
            SpawnRot, 
            SpawnParams
        );

        if (NewUnit && OwnerUnit)
        {
            NewUnit->InitializeCharacter(OwnerUnit->FactionTag);
            NewUnit->SpawnDefaultController();
        }
    }
}

FVector USG_GameplayAbility_SummonGroup::CalculateSpawnLocation(int32 Index, const FVector& CenterLocation, const FRotator& BaseRotation) const
{
    if (LocationType == ESGSummonLocationType::AroundOwner)
    {
        float AngleStep = 360.0f / (float)SpawnCount;
        float Angle = Index * AngleStep;
        FVector Offset = BaseRotation.RotateVector(FVector(SpawnDistanceOffset, 0, 0).RotateAngleAxis(Angle, FVector::UpVector));
        return CenterLocation + Offset;
    }

    if (UnitsPerRow <= 0) return CenterLocation;

    int32 Row = Index / UnitsPerRow;
    int32 Col = Index % UnitsPerRow;
    
    float TotalWidth = (UnitsPerRow - 1) * UnitSpacing;
    float StartRightOffset = -TotalWidth / 2.0f;

    FVector RightVector = UKismetMathLibrary::GetRightVector(BaseRotation);
    FVector BackVector = -UKismetMathLibrary::GetForwardVector(BaseRotation);

    FVector RightOffset = RightVector * (StartRightOffset + (Col * UnitSpacing));
    FVector BackOffset = BackVector * (Row * UnitSpacing);

    return CenterLocation + RightOffset + BackOffset;
}

FRotator USG_GameplayAbility_SummonGroup::CalculateSpawnRotation(const FVector& SpawnLocation, const FVector& CenterLocation, const FRotator& OwnerRotation) const
{
    switch (RotationType)
    {
    case ESGSummonRotationType::SameAsOwner: return OwnerRotation;
    case ESGSummonRotationType::FaceOutwards: return UKismetMathLibrary::FindLookAtRotation(CenterLocation, SpawnLocation);
    case ESGSummonRotationType::FaceTarget:
        {
             ASG_UnitsBase* Owner = Cast<ASG_UnitsBase>(GetAvatarActorFromActorInfo());
             if (Owner && Owner->CurrentTarget) return UKismetMathLibrary::FindLookAtRotation(SpawnLocation, Owner->CurrentTarget->GetActorLocation());
             return OwnerRotation;
        }
    case ESGSummonRotationType::Random: return FRotator(0, FMath::FRandRange(0.0f, 360.0f), 0);
    }
    return OwnerRotation;
}

TSubclassOf<ASG_UnitsBase> USG_GameplayAbility_SummonGroup::GetRandomUnitClass() const
{
    if (PossibleUnits.Num() == 0) return nullptr;
    float TotalWeight = 0.0f;
    for (const auto& Option : PossibleUnits) TotalWeight += Option.RandomWeight;
    float RandomValue = FMath::FRandRange(0.0f, TotalWeight);
    float CurrentSum = 0.0f;
    for (const auto& Option : PossibleUnits)
    {
        CurrentSum += Option.RandomWeight;
        if (RandomValue <= CurrentSum) return Option.UnitClass;
    }
    return PossibleUnits[0].UnitClass;
}