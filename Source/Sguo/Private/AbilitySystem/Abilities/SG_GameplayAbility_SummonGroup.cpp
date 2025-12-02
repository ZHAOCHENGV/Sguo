// 📄 文件：Source/Sguo/Private/AbilitySystem/Abilities/SG_GameplayAbility_SummonGroup.cpp
// 🔧 修改 - 增加蒙太奇播放失败的安全检查，防止技能卡死

#include "AbilitySystem/Abilities/SG_GameplayAbility_SummonGroup.h"
#include "Units/SG_UnitsBase.h"
#include "GameFramework/Character.h"
#include "Kismet/KismetMathLibrary.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Data/Type/SG_UnitDataTable.h"

USG_GameplayAbility_SummonGroup::USG_GameplayAbility_SummonGroup()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    TriggerEventTag = FGameplayTag::RequestGameplayTag(FName("Ability.Event.Spawn")); 
}

void USG_GameplayAbility_SummonGroup::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
   Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 1. 获取蒙太奇
    UAnimMontage* MontageToPlay = FindMontageFromUnitData();
    if (!MontageToPlay)
    {
        UE_LOG(LogTemp, Error, TEXT("SummonGroup: 未找到蒙太奇，技能结束"));
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    // ========== 关键修改：调整顺序与 Task 管理 ==========

    // 2.【先】创建事件监听 Task (确保不会错过第0帧的 Notify)
    UAbilityTask_WaitGameplayEvent* WaitEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
        this,
        TriggerEventTag,
        nullptr, 
        false,   
        true     
    );

    if (!WaitEventTask)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    // 绑定事件触发逻辑
    WaitEventTask->EventReceived.AddDynamic(this, &USG_GameplayAbility_SummonGroup::OnSpawnEventReceived);
    // 激活监听
    WaitEventTask->ReadyForActivation();

    // 3.【后】创建播放蒙太奇 Task (管理动画生命周期)
    UAbilityTask_PlayMontageAndWait* PlayMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
        this,
        NAME_None,
        MontageToPlay,
        1.0f,
        NAME_None,
        false,
        1.0f
    );

    if (!PlayMontageTask)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    // 绑定动画结束/取消/中断的回调 -> 确保技能会正常结束
    PlayMontageTask->OnBlendOut.AddDynamic(this, &USG_GameplayAbility_SummonGroup::OnMontageCompleted);
    PlayMontageTask->OnCompleted.AddDynamic(this, &USG_GameplayAbility_SummonGroup::OnMontageCompleted);
    PlayMontageTask->OnInterrupted.AddDynamic(this, &USG_GameplayAbility_SummonGroup::OnMontageCancelled);
    PlayMontageTask->OnCancelled.AddDynamic(this, &USG_GameplayAbility_SummonGroup::OnMontageCancelled);

    // 激活播放
    PlayMontageTask->ReadyForActivation();
}

// ✨ 新增 - 处理动画正常结束
void USG_GameplayAbility_SummonGroup::OnMontageCompleted()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

// ✨ 新增 - 处理动画被取消/打断
void USG_GameplayAbility_SummonGroup::OnMontageCancelled()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void USG_GameplayAbility_SummonGroup::OnSpawnEventReceived(FGameplayEventData Payload)
{
    // 收到通知，执行核心逻辑
    ExecuteSpawn();
}

UAnimMontage* USG_GameplayAbility_SummonGroup::FindMontageFromUnitData() const
{
    ASG_UnitsBase* OwnerUnit = Cast<ASG_UnitsBase>(GetAvatarActorFromActorInfo());
    if (!OwnerUnit || !OwnerUnit->UnitDataTable) return nullptr;

    const FSGUnitDataRow* Row = OwnerUnit->UnitDataTable->FindRow<FSGUnitDataRow>(OwnerUnit->UnitDataRowName, TEXT("FindMontage"));
    if (!Row) return nullptr;

    for (const FSGUnitAttackDefinition& AbilityDef : Row->Abilities)
    {
        if (AbilityDef.SpecificAbilityClass == GetClass())
        {
            return AbilityDef.Montage;
        }
    }
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