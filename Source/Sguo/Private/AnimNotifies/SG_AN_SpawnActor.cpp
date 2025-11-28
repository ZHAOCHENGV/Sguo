// Fill out your copyright notice in the Description page of Project Settings.


#include "AnimNotifies/SG_AN_SpawnActor.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Debug/SG_LogCategories.h"

USG_AN_SpawnActor::USG_AN_SpawnActor()
: SocketName(NAME_None)
	, LocationOffset(FVector::ZeroVector)
	, RotationOffset(FRotator::ZeroRotator)
	, OverrideFlightSpeed(0.0f)
	, OverrideArcHeight(-1.0f)
	, bDrawDebug(false)
{
	EventTag = FGameplayTag::RequestGameplayTag(FName("Event.Attack.SpawnProjectile"));
	
#if WITH_EDITORONLY_DATA
	NotifyColor = FColor(255, 128, 0, 255);
#endif
}


/**
 * @brief 通知触发回调
 * @param MeshComp 骨骼网格体组件
 * @param Animation 动画序列
 * @param EventReference 事件引用
 * @details
 * 功能说明：
 * - 计算发射变换（位置 + 旋转）
 * - 使用 Scale3D 传递额外参数（速度、重力）
 * - 发送 GameplayEvent 给 GAS
 * 详细流程：
 * 1. 获取 Socket 变换（如果存在）
 * 2. 应用位置和旋转偏移
 * 3. 将速度和重力参数打包到 Scale3D
 * 4. 构建 TargetData 传递变换信息
 * 5. 发送 GameplayEvent
 * 注意事项：
 * - Scale3D.X = 速度
 * - Scale3D.Y = 重力
 * - Scale3D.Z = 保留（默认 1.0）
 */
void USG_AN_SpawnActor::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
                               const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	
	// ✨ 新增 - 更详细的日志
	UE_LOG(LogSGGameplay, Log, TEXT("========== 动画通知触发：SG_AN_SpawnActor =========="));
    
	if (!MeshComp)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("  ❌ MeshComp 为空"));
		return;
	}
    
	AActor* OwnerActor = MeshComp->GetOwner();
	if (!OwnerActor)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("  ❌ OwnerActor 为空"));
		return;
	}
	UE_LOG(LogSGGameplay, Log, TEXT("  拥有者：%s"), *OwnerActor->GetName());
	UE_LOG(LogSGGameplay, Log, TEXT("  动画：%s"), Animation ? *Animation->GetName() : TEXT("None"));
	UE_LOG(LogSGGameplay, Log, TEXT("  Socket：%s"), SocketName.IsNone() ? TEXT("Root") : *SocketName.ToString());
	// 计算发射变换
	FTransform SocketTransform = FTransform::Identity;
	
	if (!SocketName.IsNone() && MeshComp->DoesSocketExist(SocketName))
	{
		SocketTransform = MeshComp->GetSocketTransform(SocketName);
	}
	else
	{
		SocketTransform = MeshComp->GetComponentTransform();
	}

	FTransform OffsetTransform(RotationOffset, LocationOffset);
	FTransform FinalTransform = OffsetTransform * SocketTransform;

	FVector SpawnLocation = FinalTransform.GetLocation();
	FRotator SpawnRotation = FinalTransform.Rotator();

	// 使用 Scale3D 传递参数
	// X = 覆盖速度
	// Y = 覆盖弧度高度
	// Z = 保留
	FVector ParamsPayload(OverrideFlightSpeed, OverrideArcHeight, 0.0f);
	FinalTransform.SetScale3D(ParamsPayload);

#if WITH_EDITOR
	if (bDrawDebug && MeshComp->GetWorld())
	{
		DrawDebugCoordinateSystem(MeshComp->GetWorld(), SpawnLocation, SpawnRotation, 30.0f, false, 3.0f, 0, 2.0f);
		DrawDebugSphere(MeshComp->GetWorld(), SpawnLocation, 10.0f, 12, FColor::Yellow, false, 3.0f, 0, 1.0f);
		
		FString DebugMsg = FString::Printf(TEXT("Speed: %s\nArc: %s"), 
			OverrideFlightSpeed > 0 ? *FString::SanitizeFloat(OverrideFlightSpeed) : TEXT("Default"),
			OverrideArcHeight >= 0 ? *FString::SanitizeFloat(OverrideArcHeight) : TEXT("Default"));
		
		DrawDebugString(MeshComp->GetWorld(), SpawnLocation + FVector(0, 0, 30), DebugMsg, nullptr, FColor::White, 3.0f);
	}
#endif

	// 构建事件数据
	FGameplayEventData Payload;
	Payload.EventTag = EventTag;
	Payload.Instigator = OwnerActor;
	Payload.Target = OwnerActor;

	FGameplayAbilityTargetData_LocationInfo* LocationData = new FGameplayAbilityTargetData_LocationInfo();
	LocationData->TargetLocation.LiteralTransform = FinalTransform;
	LocationData->TargetLocation.LocationType = EGameplayAbilityTargetingLocationType::LiteralTransform;

	FGameplayAbilityTargetDataHandle TargetDataHandle;
	TargetDataHandle.Add(LocationData);
	Payload.TargetData = TargetDataHandle;
	// ✨ 新增 - 发送事件前的日志
	UE_LOG(LogSGGameplay, Log, TEXT("  📤 发送 GameplayEvent：%s"), *EventTag.ToString());
	UE_LOG(LogSGGameplay, Log, TEXT("    位置：%s"), *SpawnLocation.ToString());
	UE_LOG(LogSGGameplay, Log, TEXT("    旋转：%s"), *SpawnRotation.ToString());
    
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OwnerActor, EventTag, Payload);
    
	UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 事件已发送"));
	UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
	
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OwnerActor, EventTag, Payload);
}


/**
 * @brief 获取通知名称（编辑器显示）
 * @return 格式化的通知名称
 * @details
 * 功能说明：
 * - 在动画编辑器的时间轴上显示
 * - 包含 Socket 名称便于识别
 */
FString USG_AN_SpawnActor::GetNotifyName_Implementation() const
{
	return FString::Printf(TEXT("生成Actor插槽： (%s)"), SocketName.IsNone() ? TEXT("Root") : *SocketName.ToString());
}
