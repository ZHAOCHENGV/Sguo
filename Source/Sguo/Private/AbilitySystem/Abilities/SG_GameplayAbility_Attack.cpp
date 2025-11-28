// ✨ 新增 - 攻击能力基类实现
// Copyright Epic Games, Inc. All Rights Reserved.

/**
 * @file SG_GameplayAbility_Attack.cpp
 * @brief 攻击能力基类实现
 */

#include "AbilitySystem/Abilities/SG_GameplayAbility_Attack.h"
#include "AbilitySystem/SG_AbilitySystemComponent.h"
#include "AbilitySystem/SG_AttributeSet.h"
#include "Units/SG_UnitsBase.h"
#include "Debug/SG_LogCategories.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "GameplayEffect.h"
#include "Engine/OverlapResult.h"
#include "AbilitySystemGlobals.h"
#include "DrawDebugHelpers.h"
#include "Buildings/SG_BuildingAttributeSet.h"
#include "Buildings/SG_MainCityBase.h"
#include "Components/BoxComponent.h"  // ✨ 新增 - 必须包含完整定义
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Data/Type/SG_UnitDataTable.h" // ✨ 新增 - 包含完整定义
#include "Kismet/GameplayStatics.h" // ✨ 新增 - 用于 SuggestProjectileVelocity
#include "Actors/SG_Projectile.h"   // ✨ 新增 - 引用投射物头文件
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStaticsTypes.h" 
// ========== 构造函数 ==========

/**
 * @brief 构造函数
 * @details
 * 功能说明：
 * - 设置技能标签
 * - 配置实例化策略
 * - 配置网络执行策略
 */
USG_GameplayAbility_Attack::USG_GameplayAbility_Attack()
{
	// 设置技能的默认标签
	// Tag "Ability.Attack" 用于标识攻击类技能
	FGameplayTag AttackTag = FGameplayTag::RequestGameplayTag(FName("Ability.Attack"), false);
	if (AttackTag.IsValid())
	{
		FGameplayTagContainer Tags;
		Tags.AddTag(AttackTag);
		SetAssetTags(Tags);
		
		UE_LOG(LogSGGameplay, Log, TEXT("✓ 攻击能力标签设置成功：%s"), *AttackTag.ToString());
	}
	else
	{
		UE_LOG(LogSGGameplay, Warning, TEXT("⚠️ GameplayTag 'Ability.Attack' 未找到"));
		UE_LOG(LogSGGameplay, Warning, TEXT("  请在 Config/DefaultGameplayTags.ini 中配置"));
	}
	
	// 设置技能实例化策略
	// InstancedPerActor：每个 Actor 只有一个实例（性能更好）
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	
	// 设置技能的网络执行策略
	// LocalPredicted：客户端预测，服务器确认
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;


}

// ========== 激活技能 ==========

/**
 * @brief 激活能力
 * @param Handle 能力句柄
 * @param ActorInfo Actor信息
 * @param ActivationInfo 激活信息
 * @param TriggerEventData 触发事件数据
 * @details
 * 功能说明：
 * - 1. 从单位加载最新的攻击配置（动画、伤害倍率等）。
 * - 2. 启动攻击命中事件的监听任务。
 * - 3. 计算动画实际时长（考虑攻速倍率）。
 * - 4. 播放攻击蒙太奇动画。
 * - 5. ✨ 关键：立即通知单位开始攻击循环（StartAttackCycle），传入动画时长以正确计算冷却。
 * - 6. 如果没有动画，则按默认时长处理并直接执行判定。
 */
void USG_GameplayAbility_Attack::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData
)
{
	// 调用父类激活逻辑
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	
	UE_LOG(LogSGGameplay, Log, TEXT("========== 攻击技能激活 =========="));
	
	// 1. 从单位加载当前攻击配置 (AttackMontage, DamageMultiplier, etc.)
	LoadAttackConfigFromUnit();
	
	// 2. 创建并激活"等待游戏事件"任务，用于监听 AnimNotifyState 发送的命中事件 (Event.Attack.Hit)
	UAbilityTask_WaitGameplayEvent* WaitEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		FGameplayTag::RequestGameplayTag(FName("Event.Attack.Hit")),
		nullptr,
		false, // OnlyTriggerOnce = false（允许一次攻击多段命中）
		false  // OnlyMatchExact
	);

	if (WaitEventTask)
	{
		// 绑定到 OnAttackHitEvent 回调
		WaitEventTask->EventReceived.AddDynamic(this, &USG_GameplayAbility_Attack::OnAttackHitEvent);
		// 激活任务
		WaitEventTask->ReadyForActivation();
		
		UE_LOG(LogSGGameplay, Verbose, TEXT("  ✓ 已启动命中事件监听"));
	}
	

	// 3. ✨ 监听投射物生成事件 (Event.Attack.SpawnProjectile)
	// 这是由 USG_AN_SpawnProjectile 动画通知发送的
	UAbilityTask_WaitGameplayEvent* WaitSpawnTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		FGameplayTag::RequestGameplayTag(FName("Event.Attack.SpawnProjectile")),
		nullptr,
		false, 
		false
	);

	if (WaitSpawnTask)
	{
		WaitSpawnTask->EventReceived.AddDynamic(this, &USG_GameplayAbility_Attack::OnSpawnProjectileEvent);
		WaitSpawnTask->ReadyForActivation();
	}
	// 日志输出当前攻击信息
	UE_LOG(LogSGGameplay, Log, TEXT("  施放者：%s"), 
		ActorInfo->AvatarActor.IsValid() ? *ActorInfo->AvatarActor->GetName() : TEXT("None"));
	UE_LOG(LogSGGameplay, Log, TEXT("  攻击类型：%s"), 
		*UEnum::GetValueAsString(AttackType));
	
	// 准备变量存储动画实际时长
	float ActualDuration = 0.0f;

	// 3. 处理动画播放逻辑
	if (AttackMontage && ActorInfo->AvatarActor.IsValid())
	{
		// 获取 Character 指针
		if (ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get()))
		{
			// 获取 AnimInstance
			if (UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance())
			{
				// 3.1 获取攻击速度倍率 (从 AttributeSet)
				float PlayRate = 1.0f;
				if (const USG_AbilitySystemComponent* SGASC = Cast<USG_AbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo()))
				{
					if (const USG_AttributeSet* AttributeSet = SGASC->GetSet<USG_AttributeSet>())
					{
						PlayRate = AttributeSet->GetAttackSpeed();
					}
				}

				// 3.2 播放蒙太奇
				float MontageLength = AnimInstance->Montage_Play(AttackMontage, PlayRate);
				
				// 3.3 计算实际时长 = 原始时长 / 播放速率
				// 防止除零错误
				ActualDuration = (PlayRate > 0.0f) ? (MontageLength / PlayRate) : MontageLength;
				
				// 3.4 绑定 AnimNotify 回调 (用于触发伤害判定点)
				AnimInstance->OnPlayMontageNotifyBegin.AddDynamic(
					this, 
					&USG_GameplayAbility_Attack::OnMontageNotifyBegin
				);
				
				UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 攻击动画已播放：%s"), *AttackMontage->GetName());
				UE_LOG(LogSGGameplay, Log, TEXT("  实际动画时长：%.2f 秒 (倍率: %.2f)"), ActualDuration, PlayRate);
				
				// 3.5 设置定时器，确保能力在动画结束后正确结束
				FTimerHandle TimerHandle;
				FTimerDelegate TimerDelegate;
				// 使用 Lambda 绑定结束逻辑
				TimerDelegate.BindLambda([this, Handle, ActorInfo, ActivationInfo, AnimInstance]()
				{
					// 清理委托绑定
					if (AnimInstance)
					{
						AnimInstance->OnPlayMontageNotifyBegin.RemoveDynamic(
							this, 
							&USG_GameplayAbility_Attack::OnMontageNotifyBegin
						);
						UE_LOG(LogSGGameplay, Verbose, TEXT("  ✓ 解绑动画通知回调"));
					}
					
					// 正常结束能力
					UE_LOG(LogSGGameplay, Verbose, TEXT("  ⏰ 攻击动画自然结束，结束 Ability"));
					EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
				});
				
				// 启动定时器
				ActorInfo->AvatarActor->GetWorldTimerManager().SetTimer(
					TimerHandle,
					TimerDelegate,
					ActualDuration,
					false
				);
			}
			else
			{
				UE_LOG(LogSGGameplay, Error, TEXT("  ❌ 无法获取 AnimInstance"));
				EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
			}
		}
		else
		{
			UE_LOG(LogSGGameplay, Error, TEXT("  ❌ 施放者不是 Character 类型"));
			EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		}
	}
	else
	{
		// 处理无动画的情况（瞬发）
		UE_LOG(LogSGGameplay, Warning, TEXT("  ⚠️ 无攻击动画，直接执行攻击判定"));
		
		// 设置一个默认短时长，防止逻辑瞬间完成导致的问题
		ActualDuration = 0.5f; 
		
		// 直接执行一次攻击判定
		PerformAttack();
		
		// 立即结束能力
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
	}

	// 4. ✨ 关键修改：立即通知 Unit 开始计算冷却循环
	// 这样 Unit 就能知道："虽然动画要播 X 秒，但我现在就要开始计算 (X + Cooldown) 秒的计时了"
	if (ASG_UnitsBase* SourceUnit = Cast<ASG_UnitsBase>(ActorInfo->AvatarActor.Get()))
	{
		SourceUnit->StartAttackCycle(ActualDuration);
	}

	UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
}
/**
 * @brief 发射投射物到目标
 * @param Target 目标 Actor
 * @param OverrideSpawnLocation (可选) 覆盖发射位置，通常来自 AnimNotify
 * @details
 * 功能说明：
 * - 1. 确定发射点：优先使用 Override -> 其次使用 Socket -> 最后使用 Offset。
 * - 2. 计算弹道：使用 SuggestProjectileVelocity 计算抛物线。
 * - 3. 生成 Actor：使用 SpawnActorDeferred。
 * - 4. 初始化并完成生成。
 */
void USG_GameplayAbility_Attack::SpawnProjectileToTarget(AActor* Target, const FVector* OverrideSpawnLocation)
{
	if (!Target || !ProjectileClass) return;

	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!AvatarActor) return;

	UWorld* World = GetWorld();
	if (!World) return;

	// ========== 1. 计算发射起点 ==========
	FVector SpawnLocation;

	if (OverrideSpawnLocation)
	{
		SpawnLocation = *OverrideSpawnLocation;
	}
	else
	{
		FVector StartLocation = AvatarActor->GetActorLocation();
		FRotator ActorRotation = AvatarActor->GetActorRotation();
		SpawnLocation = StartLocation + ActorRotation.RotateVector(ProjectileSpawnOffset);
	}

	// ========== 2. 计算初始朝向 ==========
	FVector ToTarget = Target->GetActorLocation() - SpawnLocation;
	FRotator SpawnRotation = ToTarget.Rotation();

	// ========== 3. 生成投射物 ==========
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwningActorFromActorInfo();
	SpawnParams.Instigator = Cast<APawn>(AvatarActor);
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	
	ASG_Projectile* NewProjectile = World->SpawnActor<ASG_Projectile>(
		ProjectileClass,
		SpawnLocation,
		SpawnRotation,
		SpawnParams
	);

	if (!NewProjectile)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("  ❌ 投射物生成失败"));
		return;
	}

	// ========== 4. 初始化投射物 ==========
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	FGameplayTag SourceFaction;
	if (ASG_UnitsBase* Unit = Cast<ASG_UnitsBase>(AvatarActor))
	{
		SourceFaction = Unit->FactionTag;
	}

	// 🔧 修改 - 使用新的初始化函数
	NewProjectile->InitializeProjectile(
		SourceASC,
		SourceFaction,
		Target,
		-1.0f  // 使用投射物默认弧度
	);
	
	UE_LOG(LogSGGameplay, Log, TEXT("  🚀 投射物发射成功"));
}
/**
 * @brief 接收投射物生成事件（从 AnimNotify 发送）
 * @param Payload 事件数据（包含发射变换和参数）
 * @details
 * 功能说明：
 * - 从 Payload 中提取发射位置、旋转
 * - 从 Scale3D 中提取速度和重力参数
 * - 调用 SpawnProjectileToTarget 生成投射物
 * 详细流程：
 * 1. 验证目标有效性
 * 2. 从 TargetData 中提取 LiteralTransform
 * 3. 从 Scale3D 中解析速度和重力参数
 * 4. 调用生成函数
 * 注意事项：
 * - Scale3D.X = 覆盖速度（0 = 使用默认）
 * - Scale3D.Y = 重力缩放
 */
void USG_GameplayAbility_Attack::OnSpawnProjectileEvent(FGameplayEventData Payload)
{
UE_LOG(LogSGGameplay, Warning, TEXT("========== 🎯 处理投射物生成事件 =========="));
    
    AActor* AvatarActor = GetAvatarActorFromActorInfo();
    if (!AvatarActor)
    {
        UE_LOG(LogSGGameplay, Error, TEXT("  ❌ 施放者为空"));
        return;
    }
    
    UE_LOG(LogSGGameplay, Warning, TEXT("  施放者：%s"), *AvatarActor->GetName());
    UE_LOG(LogSGGameplay, Warning, TEXT("  施放者位置：%s"), *AvatarActor->GetActorLocation().ToString());

    ASG_UnitsBase* SourceUnit = Cast<ASG_UnitsBase>(AvatarActor);
    if (!SourceUnit)
    {
        UE_LOG(LogSGGameplay, Error, TEXT("  ❌ 施放者不是 SG_UnitsBase"));
        return;
    }
    
    // ✨ 新增 - 检查单位状态
    UE_LOG(LogSGGameplay, Warning, TEXT("  单位是否死亡：%s"), SourceUnit->bIsDead ? TEXT("是") : TEXT("否"));
    UE_LOG(LogSGGameplay, Warning, TEXT("  单位是否正在攻击：%s"), SourceUnit->bIsAttacking ? TEXT("是") : TEXT("否"));
    
    // 获取目标
    AActor* CurrentTarget = SourceUnit->CurrentTarget;
    
    // ✨ 新增 - 详细的目标检查
    if (!CurrentTarget)
    {
        UE_LOG(LogSGGameplay, Warning, TEXT("  ⚠️ CurrentTarget 为空，尝试查找目标..."));
        
        TArray<AActor*> PotentialTargets;
        if (FindTargetsInRange(PotentialTargets) > 0)
        {
            CurrentTarget = PotentialTargets[0];
            UE_LOG(LogSGGameplay, Warning, TEXT("  ✓ 找到替代目标：%s"), *CurrentTarget->GetName());
        }
        else
        {
            UE_LOG(LogSGGameplay, Error, TEXT("  ❌ 无法找到任何目标，取消生成投射物"));
            return;
        }
    }
    else
    {
        UE_LOG(LogSGGameplay, Warning, TEXT("  目标：%s"), *CurrentTarget->GetName());
        UE_LOG(LogSGGameplay, Warning, TEXT("  目标位置：%s"), *CurrentTarget->GetActorLocation().ToString());
    }

    // 从 Payload 中提取参数
    FVector SpawnLocation = AvatarActor->GetActorLocation();
    float OverrideSpeed = 0.0f;
    float OverrideArcHeight = -1.0f;

    if (Payload.TargetData.IsValid(0))
    {
        const FGameplayAbilityTargetData* Data = Payload.TargetData.Get(0);
        if (Data)
        {
            const FGameplayAbilityTargetData_LocationInfo* LocationData = 
                static_cast<const FGameplayAbilityTargetData_LocationInfo*>(Data);
            
            if (LocationData)
            {
                FTransform FullTransform = LocationData->TargetLocation.LiteralTransform;
                SpawnLocation = FullTransform.GetLocation();
                
                FVector ParamsPayload = FullTransform.GetScale3D();
                OverrideSpeed = ParamsPayload.X;
                OverrideArcHeight = ParamsPayload.Y;
                
                UE_LOG(LogSGGameplay, Warning, TEXT("  ✓ 从 Payload 获取生成位置：%s"), *SpawnLocation.ToString());
            }
        }
    }
    else
    {
        UE_LOG(LogSGGameplay, Warning, TEXT("  ⚠️ Payload.TargetData 无效，使用施放者位置"));
    }

    // 检查投射物类
    if (!ProjectileClass)
    {
        UE_LOG(LogSGGameplay, Error, TEXT("  ❌ ProjectileClass 未设置！"));
        UE_LOG(LogSGGameplay, Error, TEXT("    请检查 DataTable 中该单位的 Abilities 配置"));
        return;
    }
    
    UE_LOG(LogSGGameplay, Warning, TEXT("  投射物类：%s"), *ProjectileClass->GetName());

    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogSGGameplay, Error, TEXT("  ❌ World 为空"));
        return;
    }

    // 生成投射物
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = GetOwningActorFromActorInfo();
    SpawnParams.Instigator = Cast<APawn>(AvatarActor);
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    // 计算初始朝向
    FVector ToTarget = CurrentTarget->GetActorLocation() - SpawnLocation;
    FRotator SpawnRotation = ToTarget.Rotation();
    
    UE_LOG(LogSGGameplay, Warning, TEXT("  生成位置：%s"), *SpawnLocation.ToString());
    UE_LOG(LogSGGameplay, Warning, TEXT("  生成旋转：%s"), *SpawnRotation.ToString());

    ASG_Projectile* NewProjectile = World->SpawnActor<ASG_Projectile>(
        ProjectileClass,
        SpawnLocation,
        SpawnRotation,
        SpawnParams
    );

    if (!NewProjectile)
    {
        UE_LOG(LogSGGameplay, Error, TEXT("  ❌ 投射物生成失败！"));
        UE_LOG(LogSGGameplay, Error, TEXT("    可能原因："));
        UE_LOG(LogSGGameplay, Error, TEXT("    1. 生成位置在碰撞体内"));
        UE_LOG(LogSGGameplay, Error, TEXT("    2. SpawnActor 返回 nullptr"));
        return;
    }
    
    UE_LOG(LogSGGameplay, Warning, TEXT("  ✓ 投射物生成成功：%s"), *NewProjectile->GetName());

    // 应用覆盖参数
    if (OverrideSpeed > 0.0f)
    {
        NewProjectile->SetFlightSpeed(OverrideSpeed);
        UE_LOG(LogSGGameplay, Warning, TEXT("  应用覆盖速度：%.1f"), OverrideSpeed);
    }

    // 获取施放者信息
    UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
    FGameplayTag SourceFaction;
    if (SourceUnit)
    {
        SourceFaction = SourceUnit->FactionTag;
    }

    // 初始化投射物
    NewProjectile->InitializeProjectile(
        SourceASC,
        SourceFaction,
        CurrentTarget,
        OverrideArcHeight
    );

    UE_LOG(LogSGGameplay, Warning, TEXT("  ✓ 投射物初始化完成"));
    UE_LOG(LogSGGameplay, Warning, TEXT("    目标：%s"), *CurrentTarget->GetName());
    UE_LOG(LogSGGameplay, Warning, TEXT("    速度：%.1f"), NewProjectile->FlightSpeed);
    UE_LOG(LogSGGameplay, Warning, TEXT("    弧度：%.1f"), NewProjectile->ArcHeight);
    UE_LOG(LogSGGameplay, Warning, TEXT("========================================"));
}

void USG_GameplayAbility_Attack::SpawnProjectileToTargetWithParams(AActor* Target, const FVector& SpawnLocation,
	const FRotator& SpawnRotation, float OverrideSpeed, float GravityScale)
{
UE_LOG(LogSGGameplay, Log, TEXT("========== 生成投射物（带参数）=========="));
	
	if (!Target)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("  ❌ 目标为空"));
		return;
	}
	
	if (!ProjectileClass)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("  ❌ ProjectileClass 未设置"));
		return;
	}

	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!AvatarActor)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("  ❌ 施放者为空"));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("  ❌ World 为空"));
		return;
	}

	// ========== 生成投射物 ==========
	FVector ToTarget = Target->GetActorLocation() - SpawnLocation;
	FRotator ActualSpawnRotation = ToTarget.Rotation();

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwningActorFromActorInfo();
	SpawnParams.Instigator = Cast<APawn>(AvatarActor);
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	
	ASG_Projectile* NewProjectile = World->SpawnActor<ASG_Projectile>(
		ProjectileClass,
		SpawnLocation,
		ActualSpawnRotation,
		SpawnParams
	);

	if (!NewProjectile)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("  ❌ 投射物生成失败"));
		return;
	}

	// ========== 应用覆盖参数 ==========
	// 🔧 修改 - 使用新的 API
	if (OverrideSpeed > 0.0f)
	{
		NewProjectile->SetFlightSpeed(OverrideSpeed);
	}

	// ========== 初始化投射物 ==========
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	FGameplayTag SourceFaction;
	if (ASG_UnitsBase* Unit = Cast<ASG_UnitsBase>(AvatarActor))
	{
		SourceFaction = Unit->FactionTag;
	}

	// 🔧 修改 - 使用新的初始化函数
	// GravityScale 在新系统中不再使用，改用 ArcHeight
	// 如果 GravityScale > 0，转换为大约的弧度高度
	float ArcHeight = -1.0f;  // 使用默认值
	if (GravityScale > 0.0f)
	{
		// 粗略转换：GravityScale 1.0 约等于 ArcHeight 200
		ArcHeight = GravityScale * 200.0f;
	}

	NewProjectile->InitializeProjectile(
		SourceASC,
		SourceFaction,
		Target,
		ArcHeight
	);
	
	UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 投射物生成成功"));
	UE_LOG(LogSGGameplay, Log, TEXT("    速度：%.1f"), NewProjectile->FlightSpeed);
	UE_LOG(LogSGGameplay, Log, TEXT("    弧度：%.1f"), NewProjectile->ArcHeight);
	UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
}

/**
 * @brief 处理攻击命中事件（从 AnimNotifyState 发送）
 * @param Payload 事件数据（包含目标和伤害倍率）
 * @details
 * 功能说明：
 * - 接收 AnimNotifyState 发送的命中事件
 * - 从 EventData 中读取伤害倍率
 * - 应用伤害到目标
 */
UFUNCTION()
void USG_GameplayAbility_Attack::OnAttackHitEvent(FGameplayEventData Payload)
{
	UE_LOG(LogSGGameplay, Log, TEXT("========== 处理命中事件 =========="));
	
	// ========== 步骤1：获取目标 ==========
	AActor* Target = const_cast<AActor*>(Payload.Target.Get());
	if (!Target)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("  ❌ 目标为空"));
		return;
	}
	
	UE_LOG(LogSGGameplay, Log, TEXT("  目标：%s"), *Target->GetName());
	
	// ========== 步骤2：获取伤害倍率 ==========
	// EventMagnitude 存储了 AnimNotifyState 传递的伤害倍率
	float HitDamageMultiplier = Payload.EventMagnitude;
	
	if (HitDamageMultiplier <= 0.0f)
	{
		// 如果没有传递倍率，使用默认倍率
		HitDamageMultiplier = DamageMultiplier;
		UE_LOG(LogSGGameplay, Warning, TEXT("  ⚠️ 未传递伤害倍率，使用默认值：%.2f"), HitDamageMultiplier);
	}
	else
	{
		UE_LOG(LogSGGameplay, Log, TEXT("  伤害倍率：%.2f"), HitDamageMultiplier);
	}
	
	// ========== 步骤3：临时修改伤害倍率 ==========
	float OriginalMultiplier = DamageMultiplier;
	DamageMultiplier = HitDamageMultiplier;
	
	// ========== 步骤4：应用伤害 ==========
	ApplyDamageToTarget(Target);
	
	// ========== 步骤5：恢复原始倍率 ==========
	DamageMultiplier = OriginalMultiplier;
	
	// ========== 步骤6：触发蓝图事件 ==========
	TArray<AActor*> HitActors;
	HitActors.Add(Target);
	OnAttackHit(HitActors);
	
	UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
}
// ========== 结束技能 ==========

/**
 * @brief 结束能力
 * @details
 * 功能说明：
 * - 清理资源
 * - 调用父类结束
 */
void USG_GameplayAbility_Attack::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled
)
{


	UE_LOG(LogSGGameplay, Verbose, TEXT("攻击技能结束 (取消: %s)"), 
		bWasCancelled ? TEXT("是") : TEXT("否"));

	// ✨ 新增 - 通知单位技能结束，开始计算冷却
	if (ActorInfo && ActorInfo->AvatarActor.IsValid())
	{
		if (ASG_UnitsBase* SourceUnit = Cast<ASG_UnitsBase>(ActorInfo->AvatarActor.Get()))
		{
			// 只有在正常结束（非取消）或者你需要取消也算冷却时调用
			// 通常这里无论是否取消都应该通知 Unit 重置 bIsAttacking 状态，否则 Unit 会卡死在攻击状态
			SourceUnit->OnAttackAbilityFinished();
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
/**
 * @brief 从单位加载当前攻击配置
 * @details
 * 功能说明：
 * - 从施放者（SG_UnitsBase）获取当前攻击配置
 * - 更新 AttackMontage、DamageMultiplier 等属性
 * - 在 ActivateAbility 开始时调用
 * 详细流程：
 * 1. 获取施放者（SG_UnitsBase）
 * 2. 调用 GetCurrentAttackDefinition() 获取配置
 * 3. 更新本地属性
 */
void USG_GameplayAbility_Attack::LoadAttackConfigFromUnit()
{
	// ========== 步骤1：获取施放者 ==========
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!AvatarActor)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("❌ LoadAttackConfigFromUnit: 施放者为空"));
		return;
	}
	
	ASG_UnitsBase* SourceUnit = Cast<ASG_UnitsBase>(AvatarActor);
	if (!SourceUnit)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("❌ LoadAttackConfigFromUnit: 施放者不是 SG_UnitsBase"));
		return;
	}
	
	// ========== 步骤2：获取当前攻击配置 ==========
	FSGUnitAttackDefinition AttackDef = SourceUnit->GetCurrentAttackDefinition();
	
	// ========== 步骤3：更新本地属性 ==========
	AttackMontage = AttackDef.Montage;
	/*DamageMultiplier = AttackDef.DamageMultiplier;*/
	ProjectileClass = AttackDef.ProjectileClass;
	ProjectileSpawnOffset = AttackDef.ProjectileSpawnOffset;
	
	// 根据攻击类型设置 AttackType
	switch (AttackDef.AttackType)
	{
	case ESGUnitAttackType::Melee:
		AttackType = ESGAttackAbilityType::Melee;
		break;
	case ESGUnitAttackType::Ranged:
	case ESGUnitAttackType::Projectile:
		AttackType = ESGAttackAbilityType::Ranged;
		break;
	default:
		AttackType = ESGAttackAbilityType::Melee;
		break;
	}
	
	// ========== 步骤4：输出日志 ==========
	UE_LOG(LogSGGameplay, Log, TEXT("========== 从单位加载攻击配置 =========="));
	UE_LOG(LogSGGameplay, Log, TEXT("  施放者：%s"), *SourceUnit->GetName());
	UE_LOG(LogSGGameplay, Log, TEXT("  攻击动画：%s"), AttackMontage ? *AttackMontage->GetName() : TEXT("未设置"));
	UE_LOG(LogSGGameplay, Log, TEXT("  攻击类型：%s"), *UEnum::GetValueAsString(AttackType));
	UE_LOG(LogSGGameplay, Log, TEXT("  伤害倍率：%.2f"), DamageMultiplier);
	
	if (AttackType == ESGAttackAbilityType::Ranged && ProjectileClass)
	{
		UE_LOG(LogSGGameplay, Log, TEXT("  投射物类：%s"), *ProjectileClass->GetName());
		UE_LOG(LogSGGameplay, Log, TEXT("  生成偏移：%s"), *ProjectileSpawnOffset.ToString());
	}
	
	UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
}

// ========== 动画通知回调 ==========

/**
 * @brief 动画通知回调
 * @param NotifyName 通知名称
 * @param BranchingPointPayload 分支点载荷
 * @details
 * 功能说明：
 * - 在动画播放到特定帧时触发
 * - 检查通知名称是否匹配
 * - 执行攻击判定
 */
void USG_GameplayAbility_Attack::OnMontageNotifyBegin(
	FName NotifyName,
	const FBranchingPointNotifyPayload& BranchingPointPayload
)
{
	// ✨ 新增 - 简化版：任何 AnimNotify 都触发攻击判定
	// 不再检查特定的通知名称列表
	if (NotifyName != NAME_None)
	{
		UE_LOG(LogSGGameplay, Log, TEXT("  🎯 攻击判定帧触发 (通知: %s)"), *NotifyName.ToString());
		
		// 执行攻击判定
		PerformAttack();
	}
	
}

// ========== 执行攻击判定 ==========

/**
 * @brief 执行攻击判定
 * @details
 * 功能说明：
 * - 查找范围内的目标
 * - 对每个目标应用伤害
 * - 触发蓝图事件
 */
void USG_GameplayAbility_Attack::PerformAttack()
{


	// ========== ✨ 新增 - 优先处理投射物攻击 ==========
	// 如果配置了投射物类，且攻击类型不是近战，则执行投射物生成逻辑
	if (ProjectileClass && AttackType != ESGAttackAbilityType::Melee)
	{
		// 输出日志：执行攻击判定
		UE_LOG(LogSGGameplay, Log, TEXT("========== 执行抛物线攻击判定 =========="));
		// 获取当前目标（从单位身上获取，因为远程攻击通常针对锁定目标）
		AActor* AvatarActor = GetAvatarActorFromActorInfo();
		if (ASG_UnitsBase* SourceUnit = Cast<ASG_UnitsBase>(AvatarActor))
		{
			AActor* CurrentTarget = SourceUnit->CurrentTarget;
            
			// 如果有有效目标，发射投射物
			if (CurrentTarget)
			{
				SpawnProjectileToTarget(CurrentTarget);
				UE_LOG(LogSGGameplay, Log, TEXT("  🏹 执行投射物攻击 -> %s"), *CurrentTarget->GetName());
			}
			else
			{
				// 如果没有锁定目标，尝试查找范围内的敌人（作为备选）
				TArray<AActor*> PotentialTargets;
				if (FindTargetsInRange(PotentialTargets) > 0)
				{
					SpawnProjectileToTarget(PotentialTargets[0]);
					UE_LOG(LogSGGameplay, Log, TEXT("  🏹 执行投射物攻击（自动索敌） -> %s"), *PotentialTargets[0]->GetName());
				}
				else
				{
					UE_LOG(LogSGGameplay, Warning, TEXT("  ⚠️ 投射物攻击失败：没有有效目标"));
				}
			}
		}
        
		// 投射物生成后，伤害由投射物碰撞触发，此处直接返回
		UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
		return;
	}

	
	// 查找范围内的目标
	TArray<AActor*> Targets;
	int32 TargetCount = FindTargetsInRange(Targets);
	// 输出日志：执行攻击判定
	UE_LOG(LogSGGameplay, Log, TEXT("========== 执行近战攻击判定 =========="));
	// 输出日志：找到的目标数量
	UE_LOG(LogSGGameplay, Log, TEXT("  找到目标数量：%d"), TargetCount);

	// 如果找到目标，应用伤害
	if (TargetCount > 0)
	{
		// 遍历所有目标
		for (int32 i = 0; i < Targets.Num(); ++i)
		{
			AActor* Target = Targets[i];
			if (Target)
			{
				// 输出日志：攻击目标
				UE_LOG(LogSGGameplay, Log, TEXT("  [%d] 攻击目标：%s"), i + 1, *Target->GetName());
				
				// 应用伤害
				ApplyDamageToTarget(Target);
			}
		}

		// 触发蓝图事件：攻击命中
		OnAttackHit(Targets);
	}
	else
	{
		// 输出日志：未找到目标
		UE_LOG(LogSGGameplay, Warning, TEXT("  ⚠️ 未找到任何目标"));
	}

	// 输出日志：攻击判定结束
	UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
}

// ========== 查找范围内的目标 ==========

/**
 * @brief 查找范围内的目标
 * @param OutTargets 输出：找到的目标列表
 * @return 找到的目标数量
 * @details
 * 功能说明：
 * - 根据攻击类型执行不同的检测
 * - 近战：球形范围检测
 * - 远程：射线检测
 * - 技能：由子类实现
 */
int32 USG_GameplayAbility_Attack::FindTargetsInRange(TArray<AActor*>& OutTargets)
{
	OutTargets.Empty();

	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!AvatarActor)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("FindTargetsInRange 失败：施放者为空"));
		return 0;
	}

	ASG_UnitsBase* SourceUnit = Cast<ASG_UnitsBase>(AvatarActor);
	if (!SourceUnit)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("FindTargetsInRange 失败：施放者不是 SG_UnitsBase"));
		return 0;
	}

	FGameplayTag MyFaction = SourceUnit->FactionTag;
	float AttackRange = GetAttackRange();
	FVector SourceLocation = AvatarActor->GetActorLocation();

	UE_LOG(LogSGGameplay, Verbose, TEXT("  查找范围：%.1f"), AttackRange);

	switch (AttackType)
	{
	case ESGAttackAbilityType::Melee:
		{
			// ========== 近战攻击：球形范围检测 ==========
			
			FCollisionShape CollisionShape = FCollisionShape::MakeSphere(AttackRange);
			FCollisionQueryParams QueryParams;
			QueryParams.AddIgnoredActor(AvatarActor);

			TArray<FOverlapResult> OverlapResults;
			bool bHit = GetWorld()->OverlapMultiByChannel(
				OverlapResults,
				SourceLocation,
				FQuat::Identity,
				ECC_Pawn,
				CollisionShape,
				QueryParams
			);

		

			if (bHit)
			{
				for (const FOverlapResult& Result : OverlapResults)
				{
					AActor* HitActor = Result.GetActor();
					if (!HitActor)
					{
						continue;
					}

					// ========== 检查是否是敌方单位 ==========
					ASG_UnitsBase* TargetUnit = Cast<ASG_UnitsBase>(HitActor);
					if (TargetUnit && TargetUnit->FactionTag != MyFaction)
					{
						OutTargets.AddUnique(HitActor);
						UE_LOG(LogSGGameplay, Verbose, TEXT("    找到敌方单位：%s"), *HitActor->GetName());
						continue;
					}

					// ========== 🔧 修复 - 检查是否是主城的攻击检测盒 ==========
					UPrimitiveComponent* HitComponent = Result.GetComponent();
					if (HitComponent)
					{
						AActor* ComponentOwner = HitComponent->GetOwner();
						ASG_MainCityBase* MainCity = Cast<ASG_MainCityBase>(ComponentOwner);
						
						if (MainCity && MainCity->FactionTag != MyFaction)
						{
							UBoxComponent* HitBoxComponent = Cast<UBoxComponent>(HitComponent);
							UBoxComponent* MainCityDetectionBox = MainCity->GetAttackDetectionBox();
							
							if (HitBoxComponent && MainCityDetectionBox && HitBoxComponent == MainCityDetectionBox)
							{
								// ✨ 新增 - 验证距离（使用与装饰器相同的逻辑）
								FVector BoxCenter = MainCityDetectionBox->GetComponentLocation();
								FVector BoxExtent = MainCityDetectionBox->GetScaledBoxExtent();
								float BoxRadius = FMath::Max3(BoxExtent.X, BoxExtent.Y, BoxExtent.Z);
								
								float DistanceToCenter = FVector::Dist(SourceLocation, BoxCenter);
								float DistanceToSurface = FMath::Max(0.0f, DistanceToCenter - BoxRadius);
								
								if (DistanceToSurface <= AttackRange)
								{
									OutTargets.AddUnique(MainCity);
									UE_LOG(LogSGGameplay, Log, TEXT("    找到敌方主城（通过攻击检测盒）：%s"), 
										*MainCity->GetName());
									UE_LOG(LogSGGameplay, Log, TEXT("      到表面距离：%.2f / 攻击范围：%.2f"), 
										DistanceToSurface, AttackRange);
								}
								else
								{
									UE_LOG(LogSGGameplay, Warning, TEXT("    检测到主城但距离不足：%.2f > %.2f"), 
										DistanceToSurface, AttackRange);
								}
								
								continue;
							}
						}
					}
				}
			}
		}
		break;

	case ESGAttackAbilityType::Ranged:
		{
			// ========== 远程攻击：射线检测 ==========
			
			FVector StartLocation = AvatarActor->GetActorLocation();
			FVector ForwardVector = AvatarActor->GetActorForwardVector();
			FVector EndLocation = StartLocation + ForwardVector * AttackRange;

			FCollisionQueryParams QueryParams;
			QueryParams.AddIgnoredActor(AvatarActor);

			FHitResult HitResult;
			bool bHit = GetWorld()->LineTraceSingleByChannel(
				HitResult,
				StartLocation,
				EndLocation,
				ECC_Pawn,
				QueryParams
			);


			if (bHit)
			{
				AActor* HitActor = HitResult.GetActor();
				if (HitActor)
				{
					ASG_UnitsBase* TargetUnit = Cast<ASG_UnitsBase>(HitActor);
					if (TargetUnit && TargetUnit->FactionTag != MyFaction)
					{
						OutTargets.AddUnique(HitActor);
						UE_LOG(LogSGGameplay, Verbose, TEXT("    找到敌方单位：%s"), *HitActor->GetName());
					}
					
					// ========== 🔧 修复 - 检查是否是主城的攻击检测盒 ==========
					UPrimitiveComponent* HitComponent = HitResult.GetComponent();
					if (HitComponent)
					{
						AActor* ComponentOwner = HitComponent->GetOwner();
						ASG_MainCityBase* MainCity = Cast<ASG_MainCityBase>(ComponentOwner);
						
						if (MainCity && MainCity->FactionTag != MyFaction)
						{
							UBoxComponent* HitBoxComponent = Cast<UBoxComponent>(HitComponent);
							UBoxComponent* MainCityDetectionBox = MainCity->GetAttackDetectionBox();
							
							if (HitBoxComponent && MainCityDetectionBox && HitBoxComponent == MainCityDetectionBox)
							{
								// ✨ 新增 - 验证距离
								FVector BoxCenter = MainCityDetectionBox->GetComponentLocation();
								FVector BoxExtent = MainCityDetectionBox->GetScaledBoxExtent();
								float BoxRadius = FMath::Max3(BoxExtent.X, BoxExtent.Y, BoxExtent.Z);
								
								float DistanceToCenter = FVector::Dist(SourceLocation, BoxCenter);
								float DistanceToSurface = FMath::Max(0.0f, DistanceToCenter - BoxRadius);
								
								if (DistanceToSurface <= AttackRange)
								{
									OutTargets.AddUnique(MainCity);
									UE_LOG(LogSGGameplay, Log, TEXT("    找到敌方主城（通过攻击检测盒）：%s"), 
										*MainCity->GetName());
									UE_LOG(LogSGGameplay, Log, TEXT("      到表面距离：%.2f / 攻击范围：%.2f"), 
										DistanceToSurface, AttackRange);
								}
								else
								{
									UE_LOG(LogSGGameplay, Warning, TEXT("    检测到主城但距离不足：%.2f > %.2f"), 
										DistanceToSurface, AttackRange);
								}
							}
						}
					}
				}
			}
		}
		break;

	case ESGAttackAbilityType::Skill:
		{
			UE_LOG(LogSGGameplay, Warning, TEXT("技能攻击类型需要在子类中实现 FindTargetsInRange"));
		}
		break;
	}

	

	return OutTargets.Num();
}

// ========== 应用伤害到目标 ==========

/**
 * @brief 应用伤害到目标
 * @param Target 目标 Actor
 * @details
 * 功能说明：
 * - 创建伤害 GameplayEffect
 * - 设置伤害倍率
 * - 应用到目标
 */
void USG_GameplayAbility_Attack::ApplyDamageToTarget(AActor* Target)
{
	UE_LOG(LogSGGameplay, Error, TEXT("========================================"));
	UE_LOG(LogSGGameplay, Error, TEXT("🔥 ApplyDamageToTarget 开始"));
	UE_LOG(LogSGGameplay, Error, TEXT("========================================"));
	// 检查目标是否有效
	if (!Target)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("ApplyDamageToTarget 失败：目标为空"));
		return;
	}

	// 获取目标的 AbilitySystemComponent
	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	if (!TargetASC)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("ApplyDamageToTarget 失败：目标 %s 没有 ASC"), *Target->GetName());
		return;
	}

	// 检查伤害 GE 是否有效
	if (!DamageEffectClass)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("ApplyDamageToTarget 失败：伤害 GE 未设置"));
		return;
	}

	// 获取施放者的 ASC
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	if (!SourceASC)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("ApplyDamageToTarget 失败：施放者没有 ASC"));
		return;
	}

	// ========== ✨ 新增 - 验证目标的 AttributeSet ==========
	UE_LOG(LogSGGameplay, Warning, TEXT("========== 验证目标 AttributeSet =========="));
	UE_LOG(LogSGGameplay, Warning, TEXT("  目标：%s"), *Target->GetName());
	UE_LOG(LogSGGameplay, Warning, TEXT("  目标 ASC：%s"), *TargetASC->GetName());
	
	// 获取目标的 AttributeSet
	const UAttributeSet* TargetAttributeSet = TargetASC->GetAttributeSet(USG_BuildingAttributeSet::StaticClass());
	if (TargetAttributeSet)
	{
		UE_LOG(LogSGGameplay, Warning, TEXT("  ✓ 找到 BuildingAttributeSet：%s"), *TargetAttributeSet->GetName());
		
		// 检查 IncomingDamage 属性
		FGameplayAttribute IncomingDamageAttr = USG_BuildingAttributeSet::GetIncomingDamageAttribute();
		if (IncomingDamageAttr.IsValid())
		{
			UE_LOG(LogSGGameplay, Warning, TEXT("  ✓ IncomingDamage 属性有效"));
			UE_LOG(LogSGGameplay, Warning, TEXT("    属性名称：%s"), *IncomingDamageAttr.GetName());
			UE_LOG(LogSGGameplay, Warning, TEXT("    属性所属类：%s"), *IncomingDamageAttr.GetAttributeSetClass()->GetName());
		}
		else
		{
			UE_LOG(LogSGGameplay, Error, TEXT("  ❌ IncomingDamage 属性无效！"));
		}
	}
	else
	{
		UE_LOG(LogSGGameplay, Error, TEXT("  ❌ 未找到 BuildingAttributeSet！"));
		UE_LOG(LogSGGameplay, Error, TEXT("  目标可能使用了错误的 AttributeSet 类型"));
	}
	UE_LOG(LogSGGameplay, Warning, TEXT("========================================"));

	// 创建 EffectContext
	FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
	EffectContext.AddSourceObject(this);

	// 创建 EffectSpec
	FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(
		DamageEffectClass,
		GetAbilityLevel(),
		EffectContext
	);

	// 检查 SpecHandle 是否有效
	if (!SpecHandle.IsValid())
	{
		UE_LOG(LogSGGameplay, Error, TEXT("ApplyDamageToTarget 失败：创建 EffectSpec 失败"));
		return;
	}

	// 设置伤害倍率（SetByCaller）
	// 使用 GameplayTag "Data.Damage" 传递伤害倍率
	FGameplayTag DamageTag = FGameplayTag::RequestGameplayTag(FName("Data.Damage"), false);
	if (DamageTag.IsValid())
	{
		SpecHandle.Data->SetSetByCallerMagnitude(DamageTag, DamageMultiplier);
		
		// 输出日志：应用伤害
		UE_LOG(LogSGGameplay, Verbose, TEXT("    应用伤害 GE，倍率：%.2f"), DamageMultiplier);
	}
	else
	{
		UE_LOG(LogSGGameplay, Warning, TEXT("    ⚠️ Data.Damage 标签未找到，伤害倍率未设置"));
	}

	// ========== 步骤10：应用 GameplayEffect ==========
	UE_LOG(LogSGGameplay, Error, TEXT("========== 应用 GE =========="));
	UE_LOG(LogSGGameplay, Error, TEXT("施放者 ASC：%s"), *SourceASC->GetName());
	UE_LOG(LogSGGameplay, Error, TEXT("目标 ASC：%s"), *TargetASC->GetName());
	// 应用 GameplayEffect 到目标
	FActiveGameplayEffectHandle ActiveHandle = SourceASC->ApplyGameplayEffectSpecToTarget(
		*SpecHandle.Data.Get(),
		TargetASC
	);

	// Instant 类型的 GE 会立即执行并销毁
	// SpecHandle 有效说明 GE 创建和应用过程正常
	if (SpecHandle.IsValid())
	{
		UE_LOG(LogSGGameplay, Log, TEXT("✓ GE 应用成功（Handle 有效）"));
	}
	else
	{
		UE_LOG(LogSGGameplay, Error, TEXT("❌ GE 应用失败（Handle 无效）"));
	}

	UE_LOG(LogSGGameplay, Error, TEXT("========================================"));
	UE_LOG(LogSGGameplay, Error, TEXT("🔥 ApplyDamageToTarget 结束"));
	UE_LOG(LogSGGameplay, Error, TEXT("========================================"));
}

// ========== 获取攻击范围 ==========

/**
 * @brief 获取攻击范围
 * @return 攻击范围（厘米）
 * @details
 * 功能说明：
 * - 从施放者的 AttributeSet 读取攻击范围
 * - 如果无法获取，返回默认值（150）
 */
float USG_GameplayAbility_Attack::GetAttackRange() const
{
	// 获取施放者的 ASC
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	if (!SourceASC)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("GetAttackRange 失败：施放者没有 ASC"));
		return 150.0f; // 默认近战范围
	}

	// 从 AttributeSet 读取攻击范围
	const USG_AttributeSet* AttributeSet = SourceASC->GetSet<USG_AttributeSet>();
	if (AttributeSet)
	{
		return AttributeSet->GetAttackRange();
	}

	// 如果没有 AttributeSet，返回默认值
	UE_LOG(LogSGGameplay, Warning, TEXT("GetAttackRange 失败：没有 AttributeSet，使用默认值"));
	return 150.0f;
}

void USG_GameplayAbility_Attack::SpawnProjectileWithArc(AActor* Target, const FVector& SpawnLocation,
	const FRotator& SpawnRotation, float OverrideSpeed, float GravityScale, float ArcParam)
{
	UE_LOG(LogSGGameplay, Log, TEXT("========== 生成投射物（带弧度控制）=========="));
	
	if (!Target)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("  ❌ 目标为空"));
		return;
	}
	
	if (!ProjectileClass)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("  ❌ ProjectileClass 未设置"));
		return;
	}

	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!AvatarActor)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("  ❌ 施放者为空"));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("  ❌ World 为空"));
		return;
	}

	// ========== 生成投射物 ==========
	FVector ToTarget = Target->GetActorLocation() - SpawnLocation;
	FRotator ActualSpawnRotation = ToTarget.Rotation();

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwningActorFromActorInfo();
	SpawnParams.Instigator = Cast<APawn>(AvatarActor);
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	
	ASG_Projectile* NewProjectile = World->SpawnActor<ASG_Projectile>(
		ProjectileClass,
		SpawnLocation,
		ActualSpawnRotation,
		SpawnParams
	);

	if (!NewProjectile)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("  ❌ 投射物生成失败"));
		return;
	}

	// ========== 应用覆盖参数 ==========
	if (OverrideSpeed > 0.0f)
	{
		NewProjectile->SetFlightSpeed(OverrideSpeed);
	}

	// ========== 初始化投射物 ==========
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	FGameplayTag SourceFaction;
	if (ASG_UnitsBase* Unit = Cast<ASG_UnitsBase>(AvatarActor))
	{
		SourceFaction = Unit->FactionTag;
	}

	// 🔧 修改 - ArcParam 现在直接作为 ArcHeight 使用
	// 如果传入的是 0-1 范围的比例值，转换为实际高度
	float ActualArcHeight = ArcParam;
	if (ArcParam >= 0.0f && ArcParam <= 1.0f)
	{
		// 将 0-1 转换为 0-500 的弧度高度
		ActualArcHeight = ArcParam * 500.0f;
	}

	NewProjectile->InitializeProjectile(
		SourceASC,
		SourceFaction,
		Target,
		ActualArcHeight
	);
	
	UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 投射物生成成功"));
	UE_LOG(LogSGGameplay, Log, TEXT("    速度：%.1f"), NewProjectile->FlightSpeed);
	UE_LOG(LogSGGameplay, Log, TEXT("    弧度：%.1f"), NewProjectile->ArcHeight);
	UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
}
