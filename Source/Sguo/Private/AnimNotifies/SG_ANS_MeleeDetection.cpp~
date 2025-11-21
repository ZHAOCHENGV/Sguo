// Source/Sguo/Private/AnimNotifies/SG_ANS_MeleeDetection.cpp

#include "AnimNotifies/SG_ANS_MeleeDetection.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Units/SG_UnitsBase.h"
#include "Kismet/KismetMathLibrary.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

USG_ANS_MeleeDetection::USG_ANS_MeleeDetection()
{
	StartSocketName = FName("WeaponStart");
	EndSocketName = FName("WeaponEnd");
	HitEventTag = FGameplayTag::RequestGameplayTag(FName("Event.Attack.Hit"));
	
	// 默认调试关闭，但在编辑器里为了方便可以默认开启一帧
	DrawDebugType = EDrawDebugTrace::ForOneFrame; 
}

void USG_ANS_MeleeDetection::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	HitActors.Empty();
}

void USG_ANS_MeleeDetection::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	if (!MeshComp || !MeshComp->GetOwner() || !MeshComp->GetWorld()) return;

	AActor* OwnerActor = MeshComp->GetOwner();

	// 1. 计算位置和旋转
	// 获取插槽变换
	FTransform StartSocketTransform = MeshComp->GetSocketTransform(StartSocketName);
	FTransform EndSocketTransform = MeshComp->GetSocketTransform(EndSocketName);

	// 应用偏移 (在插槽的局部空间应用偏移，然后转到世界空间)
	// 这里假设偏移是相对于 StartSocket 的坐标系的
	FVector StartLocation = StartSocketTransform.TransformPosition(FVector::ZeroVector + LocationOffset);
	FVector EndLocation = EndSocketTransform.GetLocation(); // End Socket 通常不需要偏移，或者使用相同的逻辑
	
	// 如果只设置了 StartSocket，EndLocation 默认为 StartLocation (原地检测)
	if (EndSocketName.IsNone())
	{
		EndLocation = StartLocation;
	}

	// 计算胶囊体的旋转：
	// 基础旋转取自 StartSocket，叠加 RotationOffset
	FQuat BaseQuat = StartSocketTransform.GetRotation();
	FQuat OffsetQuat = RotationOffset.Quaternion();
	FQuat FinalQuat = BaseQuat * OffsetQuat;

	// 2. 执行检测 (SweepMultiByChannel)
	TArray<FHitResult> HitResults;
	FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight);
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(OwnerActor);

	bool bHit = MeshComp->GetWorld()->SweepMultiByChannel(
		HitResults,
		StartLocation,
		EndLocation,
		FinalQuat,
		ECC_Pawn, // 建议后续将此硬编码改为可配置的 TraceChannel 变量
		CapsuleShape,
		QueryParams
	);

	// 3. 处理命中逻辑
	if (bHit)
	{
		for (const FHitResult& Hit : HitResults)
		{
			AActor* HitActor = Hit.GetActor();
			// 排除已命中的、空的、或者是自己的
			if (HitActor && HitActor != OwnerActor && !HitActors.Contains(HitActor))
			{
				// 阵营判断
				bool bIsEnemy = true;
				if (ASG_UnitsBase* SourceUnit = Cast<ASG_UnitsBase>(OwnerActor))
				{
					if (ASG_UnitsBase* TargetUnit = Cast<ASG_UnitsBase>(HitActor))
					{
						if (SourceUnit->FactionTag == TargetUnit->FactionTag)
						{
							bIsEnemy = false;
						}
					}
				}

				if (bIsEnemy)
				{
					HitActors.Add(HitActor);

					FGameplayEventData EventData;
					EventData.Instigator = OwnerActor;
					EventData.Target = HitActor;
					
					// 发送事件
					UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OwnerActor, HitEventTag, EventData);
				}
			}
		}
	}

	// 4. 处理调试绘制 (Draw Debug)
	if (DrawDebugType != EDrawDebugTrace::None)
	{
		bool bPersistent = (DrawDebugType == EDrawDebugTrace::Persistent);
		float LifeTime = (DrawDebugType == EDrawDebugTrace::ForDuration) ? DrawTime : (bPersistent ? 1000.0f : -1.0f);
		
		// 如果是 ForOneFrame，LifeTime 设为 -1.0f (一帧)
		if (DrawDebugType == EDrawDebugTrace::ForOneFrame)
		{
			LifeTime = -1.0f;
		}

		// 绘制胶囊体
		// 注意：Sweep 是从 Start 到 End，这里我们画出 Start 位置的胶囊体和 End 位置的胶囊体，或者画出扫掠过的体积
		// 为了简洁和性能，通常绘制 Start 位置 或者 扫掠轨迹。
		// 标准的 DrawDebugCapsule 只能画静态的，这里我们画在 Start 位置表示当前的检测体
		DrawDebugCapsule(
			MeshComp->GetWorld(),
			StartLocation,
			CapsuleHalfHeight,
			CapsuleRadius,
			FinalQuat,
			bHit ? TraceHitColor.ToFColor(true) : TraceColor.ToFColor(true),
			bPersistent,
			LifeTime,
			0,
			1.0f
		);

		// 如果 Start 和 End 不同，画一条线表示扫掠路径
		if (!StartLocation.Equals(EndLocation, 1.0f))
		{
			DrawDebugLine(
				MeshComp->GetWorld(),
				StartLocation,
				EndLocation,
				bHit ? TraceHitColor.ToFColor(true) : TraceColor.ToFColor(true),
				bPersistent,
				LifeTime,
				0,
				1.0f
			);
			
			// 在结束位置也画一个虚影胶囊体
			DrawDebugCapsule(
				MeshComp->GetWorld(),
				EndLocation,
				CapsuleHalfHeight,
				CapsuleRadius,
				FinalQuat,
				bHit ? TraceHitColor.ToFColor(true) : TraceColor.ToFColor(true),
				bPersistent,
				LifeTime,
				0,
				0.5f
			);
		}
	}
}