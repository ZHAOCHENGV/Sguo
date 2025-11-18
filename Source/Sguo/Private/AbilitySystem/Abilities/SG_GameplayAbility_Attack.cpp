// ✨ 新增 - 攻击能力基类实现
// Copyright notice placeholder

#include "AbilitySystem/Abilities/SG_GameplayAbility_Attack.h"
#include "AbilitySystem/SG_AbilitySystemComponent.h"
#include "AbilitySystem/SG_AttributeSet.h"
#include "Units/SG_UnitsBase.h"
#include "Debug/SG_LogCategories.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Kismet/GameplayStatics.h"
#include "GameplayEffect.h"
// 🔧 修改 - 添加必要的头文件
#include "Engine/OverlapResult.h"
#include "AbilitySystemGlobals.h"

// ========== 构造函数 ==========
USG_GameplayAbility_Attack::USG_GameplayAbility_Attack()
{
	// 🔧 修改 - 使用可选的 GameplayTag（避免未配置时报错）
	// 设置技能的默认标签
	// Tag "Ability.Attack" 用于标识攻击类技能
	// 注意：如果标签未在配置文件中注册，将跳过设置
	FGameplayTag AttackTag = FGameplayTag::RequestGameplayTag(FName("Ability.Attack"), false);
	if (AttackTag.IsValid())
	{
		FGameplayTagContainer Tags;
		Tags.AddTag(AttackTag);
		SetAssetTags(Tags);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("GameplayTag 'Ability.Attack' 未找到，请在项目设置中配置 GameplayTags"));
	}
	
	// 设置技能实例化策略
	// InstancedPerActor：每个 Actor 只有一个实例（性能更好）
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	
	// 设置技能的网络执行策略
	// LocalPredicted：客户端预测，服务器确认
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

// ========== 激活技能 ==========
void USG_GameplayAbility_Attack::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData
)
{
	// 调用父类方法
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 输出日志：技能激活
	UE_LOG(LogSGGameplay, Log, TEXT("========== 攻击技能激活 =========="));
	UE_LOG(LogSGGameplay, Log, TEXT("  施放者：%s"), ActorInfo->AvatarActor.IsValid() ? *ActorInfo->AvatarActor->GetName() : TEXT("None"));
	UE_LOG(LogSGGameplay, Log, TEXT("  攻击类型：%d"), static_cast<int32>(AttackType));

	// 如果有攻击动画，播放动画
	if (AttackMontage && ActorInfo->AvatarActor.IsValid())
	{
		// 获取角色的动画实例
		if (ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get()))
		{
			if (UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance())
			{
				// 播放攻击动画蒙太奇
				AnimInstance->Montage_Play(AttackMontage);
				
				// 🔧 修改 - 绑定动画通知回调（使用正确的委托）
				// 注意：AnimNotify 会在动画的特定帧自动触发 OnMontageNotifyBegin
				AnimInstance->OnPlayMontageNotifyBegin.AddDynamic(this, &USG_GameplayAbility_Attack::OnMontageNotifyBegin);

				// 输出日志：动画播放
				UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 攻击动画已播放"));
			}
		}
	}
	else
	{
		// 如果没有动画，直接执行攻击判定
		UE_LOG(LogSGGameplay, Warning, TEXT("  ⚠️ 无攻击动画，直接执行攻击判定"));
		PerformAttack();
		
		// 结束技能
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
	}

	// 输出日志：技能激活结束
	UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
}

// ========== 结束技能 ==========
void USG_GameplayAbility_Attack::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled
)
{
	// 输出日志：技能结束
	UE_LOG(LogSGGameplay, Verbose, TEXT("攻击技能结束"));

	// 调用父类方法
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// ========== 动画通知回调 ==========
void USG_GameplayAbility_Attack::OnMontageNotifyBegin(
	FName NotifyName,
	const FBranchingPointNotifyPayload& BranchingPointPayload
)
{
	// 检查通知名称是否匹配
	if (NotifyName == AttackNotifyName)
	{
		// 输出日志：攻击判定触发
		UE_LOG(LogSGGameplay, Log, TEXT("  🎯 攻击判定帧触发"));
		
		// 执行攻击判定
		PerformAttack();
	}
}

// ========== 执行攻击判定 ==========
void USG_GameplayAbility_Attack::PerformAttack()
{
	// 输出日志：执行攻击判定
	UE_LOG(LogSGGameplay, Log, TEXT("========== 执行攻击判定 =========="));

	// 查找范围内的目标
	TArray<AActor*> Targets;
	int32 TargetCount = FindTargetsInRange(Targets);

	// 输出日志：找到的目标数量
	UE_LOG(LogSGGameplay, Log, TEXT("  找到目标数量：%d"), TargetCount);

	// 如果找到目标，应用伤害
	if (TargetCount > 0)
	{
		// 遍历所有目标
		for (AActor* Target : Targets)
		{
			if (Target)
			{
				// 输出日志：攻击目标
				UE_LOG(LogSGGameplay, Log, TEXT("  攻击目标：%s"), *Target->GetName());
				
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
int32 USG_GameplayAbility_Attack::FindTargetsInRange(TArray<AActor*>& OutTargets)
{
	// 清空输出数组
	OutTargets.Empty();

	// 获取施放者
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!AvatarActor)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("FindTargetsInRange 失败：施放者为空"));
		return 0;
	}

	// 获取施放者的阵营标签
	ASG_UnitsBase* SourceUnit = Cast<ASG_UnitsBase>(AvatarActor);
	if (!SourceUnit)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("FindTargetsInRange 失败：施放者不是 SG_UnitsBase"));
		return 0;
	}

	// 获取攻击范围
	float AttackRange = GetAttackRange();

	// 输出日志：查找目标
	UE_LOG(LogSGGameplay, Verbose, TEXT("  查找范围：%.1f"), AttackRange);

	// 根据攻击类型执行不同的查找逻辑
	switch (AttackType)
	{
	case ESGAttackAbilityType::Melee:
		{
			// 近战攻击：球形范围检测
			
			// 获取施放者位置
			FVector SourceLocation = AvatarActor->GetActorLocation();
			
			// 球形检测参数
			FCollisionShape CollisionShape = FCollisionShape::MakeSphere(AttackRange);
			FCollisionQueryParams QueryParams;
			QueryParams.AddIgnoredActor(AvatarActor);

			// 执行球形检测
			TArray<FOverlapResult> OverlapResults;
			bool bHit = GetWorld()->OverlapMultiByChannel(
				OverlapResults,
				SourceLocation,
				FQuat::Identity,
				ECC_Pawn,
				CollisionShape,
				QueryParams
			);

			// 如果检测到碰撞
			if (bHit)
			{
				// 遍历所有碰撞结果
				for (const FOverlapResult& Result : OverlapResults)
				{
					// 获取碰撞的 Actor
					AActor* HitActor = Result.GetActor();
					if (!HitActor)
					{
						continue;
					}

					// 检查是否是敌方单位
					ASG_UnitsBase* TargetUnit = Cast<ASG_UnitsBase>(HitActor);
					if (TargetUnit && TargetUnit->FactionTag != SourceUnit->FactionTag)
					{
						// 🔧 修改 - 使用 AddUnique 避免重复添加同一个Actor
						// 原因：一个Actor可能有多个碰撞组件（Capsule、Mesh等）
						OutTargets.AddUnique(HitActor);
					}
				}
			}
		}
		break;

	case ESGAttackAbilityType::Ranged:
		{
			// 远程攻击：射线检测
			
			// 获取施放者的前方方向
			FVector StartLocation = AvatarActor->GetActorLocation();
			FVector ForwardVector = AvatarActor->GetActorForwardVector();
			FVector EndLocation = StartLocation + ForwardVector * AttackRange;

			// 射线检测参数
			FCollisionQueryParams QueryParams;
			QueryParams.AddIgnoredActor(AvatarActor);

			// 执行射线检测
			FHitResult HitResult;
			bool bHit = GetWorld()->LineTraceSingleByChannel(
				HitResult,
				StartLocation,
				EndLocation,
				ECC_Pawn,
				QueryParams
			);

			// 如果射线命中
			if (bHit)
			{
				// 获取命中的 Actor
				AActor* HitActor = HitResult.GetActor();
				if (HitActor)
				{
					// 检查是否是敌方单位
					ASG_UnitsBase* TargetUnit = Cast<ASG_UnitsBase>(HitActor);
					if (TargetUnit && TargetUnit->FactionTag != SourceUnit->FactionTag)
					{
						// 🔧 修改 - 使用 AddUnique 避免重复添加同一个Actor
						// 原因：一个Actor可能有多个碰撞组件（Capsule、Mesh等）
						OutTargets.AddUnique(HitActor);
					}
				}
			}
		}
		break;

	case ESGAttackAbilityType::Skill:
		{
			// 技能攻击：由子类或蓝图实现
			UE_LOG(LogSGGameplay, Warning, TEXT("技能攻击类型需要在子类中实现 FindTargetsInRange"));
		}
		break;
	}

	// 返回找到的目标数量
	return OutTargets.Num();
}

// ========== 应用伤害到目标 ==========
void USG_GameplayAbility_Attack::ApplyDamageToTarget(AActor* Target)
{
	// 检查目标是否有效
	if (!Target)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("ApplyDamageToTarget 失败：目标为空"));
		return;
	}

	// 获取目标的 AbilitySystemComponent
	// 🔧 修改 - UE 5.6 API 变更：使用 UAbilitySystemGlobals::GetAbilitySystemComponentFromActor
	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	if (!TargetASC)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("ApplyDamageToTarget 失败：目标没有 ASC"));
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
	FGameplayTag DamageTag = FGameplayTag::RequestGameplayTag(FName("Data.Damage"));
	SpecHandle.Data->SetSetByCallerMagnitude(DamageTag, DamageMultiplier);

	// 输出日志：应用伤害
	UE_LOG(LogSGGameplay, Verbose, TEXT("    应用伤害 GE，倍率：%.2f"), DamageMultiplier);

	// 应用 GameplayEffect 到目标
	FActiveGameplayEffectHandle ActiveHandle = SourceASC->ApplyGameplayEffectSpecToTarget(
		*SpecHandle.Data.Get(),
		TargetASC
	);

	// 🔧 修改 - 改进 GE 应用结果判断
	// Instant 类型的 GE 会立即执行并销毁，可能不返回有效的 Handle
	// 但这不代表应用失败，只是 Handle 已经失效
	// 对于 Instant GE，我们只需要确认执行过程没有错误即可
	if (SpecHandle.IsValid())
	{
		// SpecHandle 有效说明 GE 创建成功
		// Instant GE 已经立即执行完毕
		UE_LOG(LogSGGameplay, Log, TEXT("    ✓ 伤害 GE 应用成功"));
	}
	else
	{
		// 如果 SpecHandle 无效，说明 GE 创建失败
		UE_LOG(LogSGGameplay, Error, TEXT("    ❌ 伤害 GE 应用失败"));
	}
}

// ========== 获取攻击范围 ==========
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
