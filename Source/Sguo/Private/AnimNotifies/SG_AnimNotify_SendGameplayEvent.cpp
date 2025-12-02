// 📄 文件：Source/Sguo/Private/AnimNotifies/SG_AnimNotify_SendGameplayEvent.cpp
// ✨ 新增 - 完整实现

#include "AnimNotifies/SG_AnimNotify_SendGameplayEvent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"

USG_AnimNotify_SendGameplayEvent::USG_AnimNotify_SendGameplayEvent()
{
	// 默认颜色，方便在编辑器轨道中区分
	NotifyColor = FColor(255, 128, 0, 255);
}

void USG_AnimNotify_SendGameplayEvent::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp || !MeshComp->GetOwner()) return;

	// 尝试获取 ASC
	UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(MeshComp->GetOwner());
	if (ASC)
	{
		// 构建 Payload
		FGameplayEventData Payload;
		Payload.EventTag = EventTag;
		Payload.EventMagnitude = EventMagnitude;
		Payload.Instigator = MeshComp->GetOwner();
		Payload.Target = MeshComp->GetOwner();

		// 发送事件
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(MeshComp->GetOwner(), EventTag, Payload);
	}
}

FString USG_AnimNotify_SendGameplayEvent::GetNotifyName_Implementation() const
{
	if (EventTag.IsValid())
	{
		return FString::Printf(TEXT("Send Event: %s"), *EventTag.ToString());
	}
	return Super::GetNotifyName_Implementation();
}