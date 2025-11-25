// 📄 文件：Source/Sguo/Private/AnimNotifies/SG_AN_SpawnProjectile.cpp

#include "AnimNotifies/SG_AN_SpawnProjectile.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Debug/SG_LogCategories.h" // ✨ 新增 - 引入日志分类

/**
 * @brief 构造函数
 * @details
 * 功能说明：
 * - 初始化所有默认值
 * - 设置默认事件标签
 * - 设置编辑器显示颜色
 */
USG_AN_SpawnProjectile::USG_AN_SpawnProjectile()
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
void USG_AN_SpawnProjectile::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp) return;
	
	AActor* OwnerActor = MeshComp->GetOwner();
	if (!OwnerActor) return;

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

	UE_LOG(LogSGGameplay, Log, TEXT("发送投射物生成事件：%s"), *OwnerActor->GetName());
	
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
FString USG_AN_SpawnProjectile::GetNotifyName_Implementation() const
{
	return FString::Printf(TEXT("Spawn Proj (%s)"), SocketName.IsNone() ? TEXT("Root") : *SocketName.ToString());

}
