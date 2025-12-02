// 📄 文件：Source/Sguo/Private/AbilitySystem/Abilities/SG_GameplayAbility_SkyBarrage.cpp
// 🔧 修改 - 增加蒙太奇播放失败的安全检查，防止技能卡死

#include "AbilitySystem/Abilities/SG_GameplayAbility_SkyBarrage.h"
#include "Actors/SG_Projectile.h"
#include "Units/SG_UnitsBase.h"
#include "GameFramework/Character.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h" // ✨ 必须包含
#include "Kismet/KismetMathLibrary.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Data/Type/SG_UnitDataTable.h"

USG_GameplayAbility_SkyBarrage::USG_GameplayAbility_SkyBarrage()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    TriggerEventTag = FGameplayTag::RequestGameplayTag(FName("Ability.Event.Spawn"));
}

void USG_GameplayAbility_SkyBarrage::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
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
        UE_LOG(LogTemp, Error, TEXT("SkyBarrage: 未找到蒙太奇，技能结束"));
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    // 2.【先】监听事件
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

    WaitEventTask->EventReceived.AddDynamic(this, &USG_GameplayAbility_SkyBarrage::OnStartBarrageEvent);
    WaitEventTask->ReadyForActivation();

    // 3.【后】播放动画
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

    PlayMontageTask->OnBlendOut.AddDynamic(this, &USG_GameplayAbility_SkyBarrage::OnMontageCompleted);
    PlayMontageTask->OnCompleted.AddDynamic(this, &USG_GameplayAbility_SkyBarrage::OnMontageCompleted);
    PlayMontageTask->OnInterrupted.AddDynamic(this, &USG_GameplayAbility_SkyBarrage::OnMontageCancelled);
    PlayMontageTask->OnCancelled.AddDynamic(this, &USG_GameplayAbility_SkyBarrage::OnMontageCancelled);

    PlayMontageTask->ReadyForActivation();
}



void USG_GameplayAbility_SkyBarrage::OnStartBarrageEvent(FGameplayEventData Payload)
{
    StartBarrageLoop();
}

// ✨ 新增 - 处理动画正常结束
void USG_GameplayAbility_SkyBarrage::OnMontageCompleted()
{
    // 剑雨比较特殊：如果剑雨还在下（Timer还在跑），蒙太奇结束了，是否要结束技能？
    // 通常我们希望技能保持 Active 直到剑雨下完。
    // 所以这里我们需要判断一下
    
    // 如果 Timer 已经跑完了（BarrageTimerHandle 无效），则结束技能
    if (!GetWorld() || !GetWorld()->GetTimerManager().IsTimerActive(BarrageTimerHandle))
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
    }
    // 如果 Timer 还在跑，我们不在这里结束技能，而是等 SpawnProjectileLoop 里的 Timer 结束时调用 EndAbility
}

// ✨ 新增 - 处理动画被取消/打断
void USG_GameplayAbility_SkyBarrage::OnMontageCancelled()
{
    // 如果动作被打断（比如被晕了），通常逻辑是停止施法，剑雨也应该停
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(BarrageTimerHandle);
    }
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

UAnimMontage* USG_GameplayAbility_SkyBarrage::FindMontageFromUnitData() const
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

void USG_GameplayAbility_SkyBarrage::StartBarrageLoop()
{
    ACharacter* AvatarChar = Cast<ACharacter>(GetAvatarActorFromActorInfo());
    FVector OwnerLoc = AvatarChar ? AvatarChar->GetActorLocation() : FVector::ZeroVector;
    FVector Forward = AvatarChar ? AvatarChar->GetActorForwardVector() : FVector::ForwardVector;
    
    CachedTargetCenter = OwnerLoc + (Forward * TargetDistance);
    
    FHitResult HitResult;
    FVector TraceStart = CachedTargetCenter + FVector(0, 0, 1000);
    FVector TraceEnd = CachedTargetCenter - FVector(0, 0, 1000);
    if (GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_WorldStatic))
    {
        CachedTargetCenter = HitResult.Location;
    }

    ProjectilesSpawned = 0;
    if (TotalProjectiles > 0)
    {
        IntervalPerShot = Duration / (float)TotalProjectiles;
        if (IntervalPerShot < 0.01f) IntervalPerShot = 0.01f;
    }

    if (GetWorld())
    {
        GetWorld()->GetTimerManager().SetTimer(
            BarrageTimerHandle,
            this,
            &USG_GameplayAbility_SkyBarrage::SpawnProjectileLoop,
            IntervalPerShot,
            true,
            0.0f
        );
    }
}

void USG_GameplayAbility_SkyBarrage::SpawnProjectileLoop()
{
    if (ProjectilesSpawned >= TotalProjectiles || !GetAvatarActorFromActorInfo())
    {
        GetWorld()->GetTimerManager().ClearTimer(BarrageTimerHandle);
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
        return;
    }

    ProjectilesSpawned++;
    if (!ProjectileClass) return;

    // 1. 计算生成位置：基于中心点 + 偏移配置 + 随机抖动
    FVector SpawnLoc = CachedTargetCenter + SpawnOriginOffset;
    SpawnLoc.X += FMath::FRandRange(-SpawnSourceSpread, SpawnSourceSpread);
    SpawnLoc.Y += FMath::FRandRange(-SpawnSourceSpread, SpawnSourceSpread);

    // 2. 计算朝向
    FRotator SpawnRot = OverrideSpawnRotation;
    if (bAutoRotateToTarget)
    {
        SpawnRot = UKismetMathLibrary::FindLookAtRotation(SpawnLoc, CachedTargetCenter);
    }

    // 3. 生成投射物
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SpawnParams.Instigator = Cast<APawn>(GetAvatarActorFromActorInfo());

    ASG_Projectile* NewProjectile = GetWorld()->SpawnActor<ASG_Projectile>(
        ProjectileClass,
        SpawnLoc,
        SpawnRot,
        SpawnParams
    );

    if (NewProjectile)
    {
        ASG_UnitsBase* Unit = Cast<ASG_UnitsBase>(GetAvatarActorFromActorInfo());
        FGameplayTag Faction = Unit ? Unit->FactionTag : FGameplayTag();
        UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();

        if (OverrideFlightSpeed > 0.0f)
        {
            NewProjectile->SetFlightSpeed(OverrideFlightSpeed);
        }

        NewProjectile->TargetMode = ESGProjectileTargetMode::AreaRandom;
        NewProjectile->SetAreaParameters(ESGProjectileAreaShape::Circle, AreaRadius);
        
        NewProjectile->InitializeProjectileToArea(
            ASC,
            Faction,
            CachedTargetCenter,
            FRotator::ZeroRotator,
            0.0f
        );
    }
}

void USG_GameplayAbility_SkyBarrage::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(BarrageTimerHandle);
    }
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}