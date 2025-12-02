// 📄 文件：Source/Sguo/Private/AbilitySystem/Abilities/SG_GameplayAbility_SkyBarrage.cpp
// 🔧 修改 - 修复蒙太奇播放和动画状态同步问题

#include "AbilitySystem/Abilities/SG_GameplayAbility_SkyBarrage.h"
#include "Actors/SG_Projectile.h"
#include "Units/SG_UnitsBase.h"
#include "AbilitySystem/SG_AttributeSet.h"  // ✨ 新增
#include "GameFramework/Character.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Kismet/KismetMathLibrary.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Data/Type/SG_UnitDataTable.h"

USG_GameplayAbility_SkyBarrage::USG_GameplayAbility_SkyBarrage()
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
 * - 添加攻击速度倍率支持
 */
void USG_GameplayAbility_SkyBarrage::ActivateAbility(
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
    
    // 🔧 修改 - 如果没有蒙太奇，直接开始剑雨（不再强制结束）
    if (!MontageToPlay)
    {
        UE_LOG(LogTemp, Warning, TEXT("SkyBarrage: 未找到蒙太奇，直接开始剑雨"));
        
        // ✨ 新增 - 设置动画状态
        if (OwnerUnit)
        {
            OwnerUnit->StartAttackAnimation(Duration);
        }
        
        // 直接开始剑雨
        StartBarrageLoop();
        return;
    }

    // ========== 有蒙太奇的正常流程 ==========

    // 2.【先】监听事件
    UAbilityTask_WaitGameplayEvent* WaitEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
        this,
        TriggerEventTag,
        nullptr,
        false,
        true
    );

    if (WaitEventTask)
    {
        WaitEventTask->EventReceived.AddDynamic(this, &USG_GameplayAbility_SkyBarrage::OnStartBarrageEvent);
        WaitEventTask->ReadyForActivation();
    }

    // 3.【后】播放动画
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
        UE_LOG(LogTemp, Error, TEXT("SkyBarrage: 创建蒙太奇任务失败"));
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    PlayMontageTask->OnBlendOut.AddDynamic(this, &USG_GameplayAbility_SkyBarrage::OnMontageCompleted);
    PlayMontageTask->OnCompleted.AddDynamic(this, &USG_GameplayAbility_SkyBarrage::OnMontageCompleted);
    PlayMontageTask->OnInterrupted.AddDynamic(this, &USG_GameplayAbility_SkyBarrage::OnMontageCancelled);
    PlayMontageTask->OnCancelled.AddDynamic(this, &USG_GameplayAbility_SkyBarrage::OnMontageCancelled);

    PlayMontageTask->ReadyForActivation();

    // ✨ 新增 - 计算实际动画时长并通知单位
    float MontageLength = MontageToPlay->GetPlayLength();
    float ActualDuration = (PlayRate > 0.0f) ? (MontageLength / PlayRate) : MontageLength;
    
    // 剑雨技能：动画时长可能比剑雨持续时间短，取较大值
    float TotalAbilityDuration = FMath::Max(ActualDuration, Duration);
    
    if (OwnerUnit)
    {
        OwnerUnit->StartAttackAnimation(TotalAbilityDuration);
        UE_LOG(LogTemp, Log, TEXT("SkyBarrage: 开始播放蒙太奇 %s，动画时长：%.2f秒，技能总时长：%.2f秒"), 
            *MontageToPlay->GetName(), ActualDuration, TotalAbilityDuration);
    }
}

void USG_GameplayAbility_SkyBarrage::OnStartBarrageEvent(FGameplayEventData Payload)
{
    StartBarrageLoop();
}

// 🔧 修改 - 处理动画正常结束
void USG_GameplayAbility_SkyBarrage::OnMontageCompleted()
{
    // 如果 Timer 已经跑完了，则结束技能
    if (!GetWorld() || !GetWorld()->GetTimerManager().IsTimerActive(BarrageTimerHandle))
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
    // 如果 Timer 还在跑，等 SpawnProjectileLoop 里的 Timer 结束时调用 EndAbility
}

// 🔧 修改 - 处理动画被取消/打断
void USG_GameplayAbility_SkyBarrage::OnMontageCancelled()
{
    // 停止剑雨
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(BarrageTimerHandle);
    }
    
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

/**
 * @brief 从单位数据中查找蒙太奇
 * @return 找到的蒙太奇，失败返回 nullptr
 * @details
 * 🔧 修改说明：
 * - 简化逻辑，直接信任 CurrentAttackIndex
 * - 添加更详细的日志输出
 */
UAnimMontage* USG_GameplayAbility_SkyBarrage::FindMontageFromUnitData() const
{
    ASG_UnitsBase* OwnerUnit = Cast<ASG_UnitsBase>(GetAvatarActorFromActorInfo());
    if (!OwnerUnit)
    {
        UE_LOG(LogTemp, Error, TEXT("SkyBarrage::FindMontageFromUnitData - OwnerUnit 为空"));
        return nullptr;
    }

    // 🔧 修改 - 检查攻击配置列表是否有效
    if (OwnerUnit->CachedAttackAbilities.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("SkyBarrage::FindMontageFromUnitData - CachedAttackAbilities 为空"));
        return nullptr;
    }
    
    // 🔧 修改 - 检查索引有效性
    if (!OwnerUnit->CachedAttackAbilities.IsValidIndex(OwnerUnit->CurrentAttackIndex))
    {
        UE_LOG(LogTemp, Error, TEXT("SkyBarrage::FindMontageFromUnitData - CurrentAttackIndex(%d) 无效，列表大小：%d"), 
            OwnerUnit->CurrentAttackIndex, 
            OwnerUnit->CachedAttackAbilities.Num());
        return nullptr;
    }

    // 直接获取当前攻击配置
    FSGUnitAttackDefinition AttackDef = OwnerUnit->GetCurrentAttackDefinition();
    
    if (AttackDef.Montage)
    {
        UE_LOG(LogTemp, Log, TEXT("[SkyBarrage] 成功获取蒙太奇: %s (Index: %d)"), 
            *AttackDef.Montage->GetName(), 
            OwnerUnit->CurrentAttackIndex);
        return AttackDef.Montage;
    }

    UE_LOG(LogTemp, Warning, TEXT("[SkyBarrage] 警告：当前攻击配置 (Index: %d) 未设置蒙太奇！"), 
        OwnerUnit->CurrentAttackIndex);
    
    return nullptr;
}

void USG_GameplayAbility_SkyBarrage::StartBarrageLoop()
{
  ACharacter* AvatarChar = Cast<ACharacter>(GetAvatarActorFromActorInfo());
    FVector OwnerLoc = AvatarChar ? AvatarChar->GetActorLocation() : FVector::ZeroVector;
    FVector Forward = AvatarChar ? AvatarChar->GetActorForwardVector() : FVector::ForwardVector;
    
    // ... (目标位置计算保持不变) ...
    CachedTargetCenter = OwnerLoc + (Forward * TargetDistance);
    
    FHitResult HitResult;
    FVector TraceStart = CachedTargetCenter + FVector(0, 0, 1000);
    FVector TraceEnd = CachedTargetCenter - FVector(0, 0, 1000);
    if (GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_WorldStatic))
    {
        CachedTargetCenter = HitResult.Location;
    }

    // ========== 🔧 修复开始 ==========
    
    // 1. 重置计数器
    ProjectilesSpawned = 0;

    // 2. 参数安全检查
    if (TotalProjectiles <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("SkyBarrage: TotalProjectiles (%d) 无效，强制设为 1"), TotalProjectiles);
        TotalProjectiles = 1;
    }
    
    if (Duration <= 0.0f)
    {
        UE_LOG(LogTemp, Warning, TEXT("SkyBarrage: Duration (%.2f) 无效，强制设为 1.0"), Duration);
        Duration = 1.0f;
    }

    // 3. 计算发射间隔
    // 减去 1 是为了让最后一发正好落在 Duration 结束点（例如：0s, 0.5s, 1.0s 共3发，时长1.0s，间隔0.5s）
    // 如果想要每秒 N 发，可以用 Duration / TotalProjectiles
    IntervalPerShot = Duration / (float)FMath::Max(1, TotalProjectiles);

    // 4. 最小间隔钳位日志提示
    if (IntervalPerShot < 0.01f)
    {
        UE_LOG(LogTemp, Warning, TEXT("SkyBarrage: 发射密度过高！间隔 (%.4f) 被钳位到 0.01s。实际持续时间将延长到 %.2f秒"), 
            IntervalPerShot, TotalProjectiles * 0.01f);
        IntervalPerShot = 0.01f;
    }

    // 5. 🔍 输出调试日志 (关键：检查这里输出的数值是否和你设置的一样)
    UE_LOG(LogTemp, Log, TEXT("========== 开始剑雨 =========="));
    UE_LOG(LogTemp, Log, TEXT("  配置数量: %d"), TotalProjectiles);
    UE_LOG(LogTemp, Log, TEXT("  配置时长: %.2f"), Duration);
    UE_LOG(LogTemp, Log, TEXT("  计算间隔: %.4f"), IntervalPerShot);

    // 6. 启动定时器
    if (GetWorld())
    {
        // 立即执行第一次生成 (FirstDelay = 0.0f 会在有些版本立即触发，或下一帧触发)
        // 建议手动调用一次 SpawnProjectileLoop() 确保由第一帧开始，
        // 然后定时器从 IntervalPerShot 后开始。
        // 但为了保持原有结构，我们这里保持原样，只加个检查。
        
        GetWorld()->GetTimerManager().SetTimer(
            BarrageTimerHandle,
            this,
            &USG_GameplayAbility_SkyBarrage::SpawnProjectileLoop,
            IntervalPerShot,
            true, // 循环
            0.0f  // 首次延迟
        );
    }
}

void USG_GameplayAbility_SkyBarrage::SpawnProjectileLoop()
{
    // 检查是否完成
    if (ProjectilesSpawned >= TotalProjectiles || !GetAvatarActorFromActorInfo())
    {
        // 只有当定时器还在跑的时候才打印完成日志，避免重复
        if (GetWorld() && GetWorld()->GetTimerManager().IsTimerActive(BarrageTimerHandle))
        {
            UE_LOG(LogTemp, Log, TEXT("SkyBarrage: 剑雨完成，共生成 %d/%d 个，停止定时器"), 
                ProjectilesSpawned, TotalProjectiles);
                
            GetWorld()->GetTimerManager().ClearTimer(BarrageTimerHandle);
            
            // 通知单位动画结束
            if (AActor* AvatarActor = GetAvatarActorFromActorInfo())
            {
                if (ASG_UnitsBase* OwnerUnit = Cast<ASG_UnitsBase>(AvatarActor))
                {
                    OwnerUnit->OnAttackAnimationFinished();
                }
            }
            
            EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
        }
        return;
    }

    // 增加计数
    ProjectilesSpawned++;
    if (!ProjectileClass) return;

    // 1. 计算生成位置
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
    // 🔧 修改 - 显式设置 Owner，确保 Projectile 的 GetOwner() 有值（虽已修复 Projectile 使用 GetInstigator，但这仍是好习惯）
    SpawnParams.Owner = GetAvatarActorFromActorInfo();

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
        
        // ✨ 新增 - 应用 GA 中配置的伤害倍率到投射物
        // 注意：请确保在 SG_GameplayAbility_SkyBarrage.h 中添加了 DamageMultiplier 变量
        NewProjectile->DamageMultiplier = DamageMultiplier;

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

void USG_GameplayAbility_SkyBarrage::EndAbility(
    const FGameplayAbilitySpecHandle Handle, 
    const FGameplayAbilityActorInfo* ActorInfo, 
    const FGameplayAbilityActivationInfo ActivationInfo, 
    bool bReplicateEndAbility, 
    bool bWasCancelled)
{
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(BarrageTimerHandle);
    }
    
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}