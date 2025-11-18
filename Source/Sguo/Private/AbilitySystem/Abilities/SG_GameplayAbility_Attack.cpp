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
#include "Kismet/GameplayStatics.h"
#include "GameplayEffect.h"
#include "Engine/OverlapResult.h"
#include "AbilitySystemGlobals.h"
#include "DrawDebugHelpers.h"

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
 * @details
 * 功能说明：
 * - 播放攻击动画
 * - 绑定动画通知回调
 * - 如果没有动画，直接执行攻击判定
 */
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
	UE_LOG(LogSGGameplay, Log, TEXT("  施放者：%s"), 
		ActorInfo->AvatarActor.IsValid() ? *ActorInfo->AvatarActor->GetName() : TEXT("None"));
	UE_LOG(LogSGGameplay, Log, TEXT("  攻击类型：%s"), 
		*UEnum::GetValueAsString(AttackType));

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
				
				// 绑定动画通知回调
				// 注意：AnimNotify 会在动画的特定帧自动触发 OnMontageNotifyBegin
				AnimInstance->OnPlayMontageNotifyBegin.AddDynamic(
					this, 
					&USG_GameplayAbility_Attack::OnMontageNotifyBegin
				);

				// 输出日志：动画播放
				UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 攻击动画已播放：%s"), *AttackMontage->GetName());
			}
			else
			{
				UE_LOG(LogSGGameplay, Error, TEXT("  ❌ 无法获取 AnimInstance"));
			}
		}
		else
		{
			UE_LOG(LogSGGameplay, Error, TEXT("  ❌ 施放者不是 Character 类型"));
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
	// 输出日志：技能结束
	UE_LOG(LogSGGameplay, Verbose, TEXT("攻击技能结束 (取消: %s)"), 
		bWasCancelled ? TEXT("是") : TEXT("否"));

	// 调用父类方法
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
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
	// 检查通知名称是否匹配
	if (NotifyName == AttackNotifyName)
	{
		// 输出日志：攻击判定触发
		UE_LOG(LogSGGameplay, Log, TEXT("  🎯 攻击判定帧触发 (通知: %s)"), *NotifyName.ToString());
		
		// 执行攻击判定
		PerformAttack();
	}
	else
	{
		// 输出日志：通知名称不匹配
		UE_LOG(LogSGGameplay, Verbose, TEXT("  AnimNotify: %s (跳过，不匹配 %s)"), 
			*NotifyName.ToString(), *AttackNotifyName.ToString());
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

			// ✨ 绘制近战攻击检测可视化
			if (bShowAttackDetection)
			{
				DrawMeleeAttackDetection(SourceLocation, AttackRange, bHit);
			}

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
						// 使用 AddUnique 避免重复添加同一个 Actor
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

			// ✨ 绘制远程攻击检测可视化
			if (bShowAttackDetection)
			{
				FVector HitLocation = bHit ? HitResult.Location : EndLocation;
				DrawRangedAttackDetection(StartLocation, EndLocation, bHit, HitLocation);
			}

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

	// ✨ 绘制目标标记
	if (bShowAttackDetection && OutTargets.Num() > 0)
	{
		DrawTargetMarkers(OutTargets);
	}

	// 返回找到的目标数量
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

	// 应用 GameplayEffect 到目标
	FActiveGameplayEffectHandle ActiveHandle = SourceASC->ApplyGameplayEffectSpecToTarget(
		*SpecHandle.Data.Get(),
		TargetASC
	);

	// Instant 类型的 GE 会立即执行并销毁
	// SpecHandle 有效说明 GE 创建和应用过程正常
	if (SpecHandle.IsValid())
	{
		UE_LOG(LogSGGameplay, Log, TEXT("    ✓ 伤害 GE 应用成功"));
	}
	else
	{
		UE_LOG(LogSGGameplay, Error, TEXT("    ❌ 伤害 GE 应用失败"));
	}
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

// ========== ✨ 调试可视化函数实现 ==========

/**
 * @brief 绘制近战攻击范围检测
 * @param Center 检测中心位置
 * @param Radius 检测半径
 * @param bHit 是否命中目标
 */
void USG_GameplayAbility_Attack::DrawMeleeAttackDetection(const FVector& Center, float Radius, bool bHit)
{
	if (!GetWorld())
	{
		return;
	}

	// 选择颜色（根据是否命中）
	FColor DrawColor = bHit ? AttackRangeHitColor : AttackRangeMissColor;

	// 绘制球体（检测范围）
	DrawDebugSphere(
		GetWorld(),
		Center,
		Radius,
		32,  // 分段数
		DrawColor,
		false,  // 不持久
		DetectionVisualizationDuration,  // 持续时间
		0,  // 深度优先级
		2.0f  // 线条粗细
	);

	// 绘制中心点
	DrawDebugPoint(
		GetWorld(),
		Center,
		10.0f,  // 点的大小
		DrawColor,
		false,
		DetectionVisualizationDuration
	);

	// 绘制文本标签
	FString DebugText = FString::Printf(TEXT("近战检测\n半径: %.0f\n命中: %s"), 
		Radius, 
		bHit ? TEXT("是") : TEXT("否"));
	
	DrawDebugString(
		GetWorld(),
		Center + FVector(0, 0, Radius + 50.0f),  // 文本位置（球体上方）
		DebugText,
		nullptr,  // 不需要 Actor
		DrawColor,
		DetectionVisualizationDuration,
		true  // 绘制阴影
	);
}

/**
 * @brief 绘制远程攻击范围检测
 * @param Start 射线起点
 * @param End 射线终点
 * @param bHit 是否命中目标
 * @param HitLocation 命中位置（如果命中）
 */
void USG_GameplayAbility_Attack::DrawRangedAttackDetection(
	const FVector& Start, 
	const FVector& End, 
	bool bHit, 
	const FVector& HitLocation)
{
	if (!GetWorld())
	{
		return;
	}

	// 选择颜色（根据是否命中）
	FColor DrawColor = bHit ? AttackRangeHitColor : AttackRangeMissColor;

	// 绘制射线
	if (bHit)
	{
		// 如果命中，绘制两段线：起点到命中点（红色），命中点到终点（黄色虚线）
		DrawDebugLine(
			GetWorld(),
			Start,
			HitLocation,
			AttackRangeHitColor,
			false,
			DetectionVisualizationDuration,
			0,
			3.0f  // 线条粗细
		);

		// 绘制未命中部分（虚线）
		DrawDebugLine(
			GetWorld(),
			HitLocation,
			End,
			AttackRangeMissColor,
			false,
			DetectionVisualizationDuration,
			0,
			1.0f  // 更细的线
		);

		// 在命中点绘制十字标记
		DrawDebugCrosshairs(
			GetWorld(),
			HitLocation,
			FRotator::ZeroRotator,
			100.0f,  // 十字大小
			AttackRangeHitColor,
			false,
			DetectionVisualizationDuration,
			0  
		); 
	}
	else
	{
		// 如果未命中，绘制完整射线
		DrawDebugLine(
			GetWorld(),
			Start,
			End,
			AttackRangeMissColor,
			false,
			DetectionVisualizationDuration,
			0,
			2.0f
		);
	}

	// 绘制起点标记
	DrawDebugPoint(
		GetWorld(),
		Start,
		10.0f,
		FColor::Green,
		false,
		DetectionVisualizationDuration
	);

	// 绘制终点标记
	DrawDebugPoint(
		GetWorld(),
		End,
		10.0f,
		FColor::Blue,
		false,
		DetectionVisualizationDuration
	);

	// 绘制文本标签
	float Distance = FVector::Dist(Start, HitLocation);
	FString DebugText = FString::Printf(TEXT("远程检测\n距离: %.0f\n命中: %s"), 
		Distance, 
		bHit ? TEXT("是") : TEXT("否"));
	
	FVector TextLocation = bHit ? HitLocation : ((Start + End) * 0.5f);
	DrawDebugString(
		GetWorld(),
		TextLocation + FVector(0, 0, 100.0f),
		DebugText,
		nullptr,
		DrawColor,
		DetectionVisualizationDuration,
		true
	);
}

/**
 * @brief 绘制目标标记
 * @param Targets 检测到的目标列表
 */
void USG_GameplayAbility_Attack::DrawTargetMarkers(const TArray<AActor*>& Targets)
{
	if (!GetWorld())
	{
		return;
	}

	// 获取施放者位置
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!AvatarActor)
	{
		return;
	}

	FVector SourceLocation = AvatarActor->GetActorLocation();

	// 遍历所有目标
	for (int32 i = 0; i < Targets.Num(); ++i)
	{
		AActor* Target = Targets[i];
		if (!Target)
		{
			continue;
		}

		FVector TargetLocation = Target->GetActorLocation();

		// 绘制目标标记（球体）
		DrawDebugSphere(
			GetWorld(),
			TargetLocation,
			TargetMarkerSize,
			16,
			TargetMarkerColor,
			false,
			DetectionVisualizationDuration,
			0,
			3.0f
		);

		// 绘制从施放者到目标的连线
		DrawDebugLine(
			GetWorld(),
			SourceLocation,
			TargetLocation,
			TargetMarkerColor,
			false,
			DetectionVisualizationDuration,
			0,
			2.0f
		);

		// 绘制目标序号
		FString TargetText = FString::Printf(TEXT("目标 %d\n%s"), 
			i + 1, 
			*Target->GetName());
		
		DrawDebugString(
			GetWorld(),
			TargetLocation + FVector(0, 0, TargetMarkerSize + 20.0f),
			TargetText,
			nullptr,
			TargetMarkerColor,
			DetectionVisualizationDuration,
			true
		);

		// 绘制目标的朝向箭头
		FVector TargetForward = Target->GetActorForwardVector();
		DrawDebugDirectionalArrow(
			GetWorld(),
			TargetLocation,
			TargetLocation + TargetForward * 100.0f,
			50.0f,  // 箭头大小
			FColor::Cyan,
			false,
			DetectionVisualizationDuration,
			0,
			2.0f
		);
	}

	// 绘制总结信息
	FString SummaryText = FString::Printf(TEXT("检测到 %d 个目标"), Targets.Num());
	DrawDebugString(
		GetWorld(),
		SourceLocation + FVector(0, 0, 200.0f),
		SummaryText,
		nullptr,
		FColor::White,
		DetectionVisualizationDuration,
		true,
		2.0f  // 文字大小
	);
}
