// 📄 文件：Source/Sguo/Private/AnimNotifies/SG_ANS_MeleeDetection.cpp

#include "AnimNotifies/SG_ANS_MeleeDetection.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Units/SG_UnitsBase.h"
#include "Buildings/SG_MainCityBase.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Components/SkeletalMeshComponent.h"
#include "Debug/SG_LogCategories.h"

// ========== 构造函数 ==========

USG_ANS_MeleeDetection::USG_ANS_MeleeDetection()
{
	// 默认插槽名称
	StartSocketName = FName("WeaponStart");
	EndSocketName = FName("WeaponEnd");
	
	// 默认事件标签
	HitEventTag = FGameplayTag::RequestGameplayTag(FName("Event.Attack.Hit"), false);
	if (!HitEventTag.IsValid())
	{
		UE_LOG(LogSGGameplay, Warning, TEXT("⚠️ GameplayTag 'Event.Attack.Hit' 未配置"));
	}
	
	// 默认调试开启一帧（方便测试）
	DrawDebugType = EDrawDebugTrace::ForOneFrame;
	
	// 默认伤害倍率
	DamageMultiplier = 1.0f;
}

// ========== NotifyBegin ==========

void USG_ANS_MeleeDetection::NotifyBegin(
	USkeletalMeshComponent* MeshComp, 
	UAnimSequenceBase* Animation, 
	float TotalDuration, 
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	
	// 清空已命中列表
	HitActors.Empty();
	
	// 输出日志
	if (MeshComp && MeshComp->GetOwner())
	{
		UE_LOG(LogSGGameplay, Verbose, TEXT("========== 近战检测开始 =========="));
		UE_LOG(LogSGGameplay, Verbose, TEXT("  施放者：%s"), *MeshComp->GetOwner()->GetName());
		UE_LOG(LogSGGameplay, Verbose, TEXT("  起始插槽：%s"), *StartSocketName.ToString());
		UE_LOG(LogSGGameplay, Verbose, TEXT("  结束插槽：%s"), *EndSocketName.ToString());
		UE_LOG(LogSGGameplay, Verbose, TEXT("  伤害倍率：%.2f"), DamageMultiplier);
		UE_LOG(LogSGGameplay, Verbose, TEXT("========================================"));
	}
}

// ========== NotifyTick ==========

void USG_ANS_MeleeDetection::NotifyTick(
	USkeletalMeshComponent* MeshComp, 
	UAnimSequenceBase* Animation, 
	float FrameDeltaTime, 
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	// ========== 步骤1：检查有效性 ==========
	if (!MeshComp || !MeshComp->GetOwner() || !MeshComp->GetWorld())
	{
		return;
	}

	AActor* OwnerActor = MeshComp->GetOwner();
	UWorld* World = MeshComp->GetWorld();

	// ========== 步骤2：检查插槽是否存在 ==========
	if (!MeshComp->DoesSocketExist(StartSocketName))
	{
		UE_LOG(LogSGGameplay, Error, TEXT("❌ 起始插槽不存在：%s"), *StartSocketName.ToString());
		return;
	}

	// ========== 步骤3：计算起始位置和旋转 ==========
	FTransform StartSocketTransform = MeshComp->GetSocketTransform(StartSocketName);
	
	// 应用起始插槽的位置偏移（局部空间）
	FVector StartLocation = StartSocketTransform.TransformPosition(StartLocationOffset);
	
	// 应用起始插槽的旋转偏移
	FQuat StartBaseQuat = StartSocketTransform.GetRotation();
	FQuat StartOffsetQuat = StartRotationOffset.Quaternion();
	FQuat StartFinalQuat = StartBaseQuat * StartOffsetQuat;

	// ========== 步骤4：计算结束位置和旋转 ==========
	FVector EndLocation;
	FQuat EndFinalQuat;
	
	if (!EndSocketName.IsNone() && MeshComp->DoesSocketExist(EndSocketName))
	{
		// 使用结束插槽
		FTransform EndSocketTransform = MeshComp->GetSocketTransform(EndSocketName);
		
		// 应用结束插槽的位置偏移（局部空间）
		EndLocation = EndSocketTransform.TransformPosition(EndLocationOffset);
		
		// 应用结束插槽的旋转偏移
		FQuat EndBaseQuat = EndSocketTransform.GetRotation();
		FQuat EndOffsetQuat = EndRotationOffset.Quaternion();
		EndFinalQuat = EndBaseQuat * EndOffsetQuat;
	}
	else
	{
		// 没有结束插槽，使用起始位置
		EndLocation = StartLocation;
		EndFinalQuat = StartFinalQuat;
	}

	// ========== 步骤5：执行胶囊体扫掠检测 ==========
	TArray<FHitResult> HitResults;
	FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight);
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(OwnerActor);
	QueryParams.bTraceComplex = false; // 使用简单碰撞
	QueryParams.bReturnPhysicalMaterial = false;

	// 使用起始位置的旋转进行扫掠
	bool bHit = World->SweepMultiByChannel(
		HitResults,
		StartLocation,
		EndLocation,
		StartFinalQuat,
		ECC_Pawn, // 🔧 可以改为可配置的 TraceChannel
		CapsuleShape,
		QueryParams
	);

	// ========== 步骤6：处理命中结果 ==========
	if (bHit)
	{
		UE_LOG(LogSGGameplay, Verbose, TEXT("  检测到 %d 个碰撞"), HitResults.Num());
		
		for (const FHitResult& Hit : HitResults)
		{
			AActor* HitActor = Hit.GetActor();
			
			// ========== 步骤6.1：基础检查 ==========
			if (!HitActor || HitActor == OwnerActor || HitActors.Contains(HitActor))
			{
				continue;
			}

			// ========== 步骤6.2：阵营检查 ==========
			bool bIsEnemy = false;
			
			// 获取施放者阵营
			FGameplayTag SourceFaction;
			if (ASG_UnitsBase* SourceUnit = Cast<ASG_UnitsBase>(OwnerActor))
			{
				SourceFaction = SourceUnit->FactionTag;
			}
			else if (ASG_MainCityBase* SourceMainCity = Cast<ASG_MainCityBase>(OwnerActor))
			{
				SourceFaction = SourceMainCity->FactionTag;
			}
			
			// 检查目标阵营
			if (SourceFaction.IsValid())
			{
				// 检查单位
				if (ASG_UnitsBase* TargetUnit = Cast<ASG_UnitsBase>(HitActor))
				{
					if (TargetUnit->FactionTag != SourceFaction)
					{
						bIsEnemy = true;
					}
				}
				// 检查主城
				else if (ASG_MainCityBase* TargetMainCity = Cast<ASG_MainCityBase>(HitActor))
				{
					if (TargetMainCity->FactionTag != SourceFaction)
					{
						bIsEnemy = true;
					}
				}
			}

			// ========== 步骤6.3：发送命中事件 ==========
			if (bIsEnemy)
			{
				// 添加到已命中列表
				HitActors.Add(HitActor);

				// 构建 GameplayEventData
				FGameplayEventData EventData;
				EventData.Instigator = OwnerActor;
				EventData.Target = HitActor;
				
				// ✨ 新增 - 传递伤害倍率
				EventData.EventMagnitude = DamageMultiplier;
				
				// ✨ 新增 - 传递命中位置
				EventData.ContextHandle.AddHitResult(Hit);
				
				// 发送事件到施放者的 ASC
				UAbilitySystemComponent* SourceASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OwnerActor);
				if (SourceASC && HitEventTag.IsValid())
				{
					SourceASC->HandleGameplayEvent(HitEventTag, &EventData);
					
					UE_LOG(LogSGGameplay, Log, TEXT("  ✅ 命中敌人：%s"), *HitActor->GetName());
					UE_LOG(LogSGGameplay, Log, TEXT("    伤害倍率：%.2f"), DamageMultiplier);
				}
				else
				{
					UE_LOG(LogSGGameplay, Warning, TEXT("  ⚠️ 无法发送事件：ASC 或 HitEventTag 无效"));
				}
			}
			else
			{
				UE_LOG(LogSGGameplay, Verbose, TEXT("  跳过友方单位：%s"), *HitActor->GetName());
			}
		}
	}

	// ========== 步骤7：绘制调试信息 ==========
	if (DrawDebugType != EDrawDebugTrace::None)
	{
		// 计算调试显示时间
		bool bPersistent = (DrawDebugType == EDrawDebugTrace::Persistent);
		float LifeTime = -1.0f; // 默认一帧
		
		if (DrawDebugType == EDrawDebugTrace::ForDuration)
		{
			LifeTime = DrawTime;
		}
		else if (bPersistent)
		{
			LifeTime = 1000.0f;
		}

		// 选择颜色
		FColor DebugColor = bHit ? TraceHitColor.ToFColor(true) : TraceColor.ToFColor(true);

		// 绘制起始位置的胶囊体
		DrawDebugCapsule(
			World,
			StartLocation,
			CapsuleHalfHeight,
			CapsuleRadius,
			StartFinalQuat,
			DebugColor,
			bPersistent,
			LifeTime,
			0,
			2.0f
		);

		// 如果起始和结束位置不同，绘制扫掠路径
		if (!StartLocation.Equals(EndLocation, 1.0f))
		{
			// 绘制扫掠路径线
			DrawDebugLine(
				World,
				StartLocation,
				EndLocation,
				DebugColor,
				bPersistent,
				LifeTime,
				0,
				2.0f
			);
			
			// 绘制结束位置的胶囊体（半透明）
			DrawDebugCapsule(
				World,
				EndLocation,
				CapsuleHalfHeight,
				CapsuleRadius,
				EndFinalQuat,
				DebugColor,
				bPersistent,
				LifeTime,
				0,
				1.0f
			);
		}

		// 绘制命中点
		for (const FHitResult& Hit : HitResults)
		{
			DrawDebugPoint(
				World,
				Hit.ImpactPoint,
				10.0f,
				FColor::Orange,
				bPersistent,
				LifeTime
			);
		}
	}
}

// ========== NotifyEnd ==========

void USG_ANS_MeleeDetection::NotifyEnd(
	USkeletalMeshComponent* MeshComp, 
	UAnimSequenceBase* Animation, 
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	
	// 输出日志
	if (MeshComp && MeshComp->GetOwner())
	{
		UE_LOG(LogSGGameplay, Verbose, TEXT("========== 近战检测结束 =========="));
		UE_LOG(LogSGGameplay, Verbose, TEXT("  施放者：%s"), *MeshComp->GetOwner()->GetName());
		UE_LOG(LogSGGameplay, Verbose, TEXT("  总共命中：%d 个敌人"), HitActors.Num());
		UE_LOG(LogSGGameplay, Verbose, TEXT("========================================"));
	}
	
	// 清空已命中列表
	HitActors.Empty();
}
