// 📄 文件：Source/Sguo/Public/AnimNotifies/SG_AnimNotify_SendGameplayEvent.h
// ✨ 新增 - 自定义 GameplayEvent 通知，确保编辑器可见

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameplayTagContainer.h"
#include "SG_AnimNotify_SendGameplayEvent.generated.h"

/**
 * @brief 发送 Gameplay Event 的动画通知
 * @details
 * 功能说明：
 * - 在动画蒙太奇的特定帧触发
 * - 向动画拥有者的 AbilitySystemComponent 发送带有指定 Tag 的事件
 * - 用于触发 GA 中的 WaitGameplayEvent 节点
 */
UCLASS(meta = (DisplayName = "SG Send Gameplay Event"))
class SGUO_API USG_AnimNotify_SendGameplayEvent : public UAnimNotify
{
	GENERATED_BODY()

public:
	USG_AnimNotify_SendGameplayEvent();

	// 要发送的事件标签
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnimNotify", meta = (DisplayName = "事件标签"))
	FGameplayTag EventTag;

	// 可选的传递数值（Magnitude）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnimNotify", meta = (DisplayName = "事件数值"))
	float EventMagnitude = 1.0f;

	// 核心重写函数
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
    
	// 编辑器显示名称优化
	virtual FString GetNotifyName_Implementation() const override;
};