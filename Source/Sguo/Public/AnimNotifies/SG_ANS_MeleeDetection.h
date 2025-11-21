// Source/Sguo/Public/AnimNotifies/SG_ANS_MeleeDetection.h

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "GameplayTagContainer.h"
#include "Kismet/KismetSystemLibrary.h" // 引入以使用 EDrawDebugTrace
#include "SG_ANS_MeleeDetection.generated.h"

/**
 * @brief 增强版近战攻击检测通知状态
 * @details 
 * 功能说明：
 * - 在动画期间执行带旋转的胶囊体扫掠检测
 * - 支持双插槽定义扫掠路径（Start -> End）
 * - 支持位置与旋转偏移
 * - 使用原生 DrawDebugTrace 枚举控制调试
 */
UCLASS()
class SGUO_API USG_ANS_MeleeDetection : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	USG_ANS_MeleeDetection();

	// 起始插槽名称（如：WeaponHandle）
	UPROPERTY(EditAnywhere, Category = "Config|Socket", meta = (DisplayName = "起始插槽"))
	FName StartSocketName;

	// 结束插槽名称（如：WeaponTip），扫掠将从起始插槽指向结束插槽
	UPROPERTY(EditAnywhere, Category = "Config|Socket", meta = (DisplayName = "结束插槽"))
	FName EndSocketName;

	// 胶囊体半径
	UPROPERTY(EditAnywhere, Category = "Config|Shape", meta = (DisplayName = "胶囊体半径"))
	float CapsuleRadius = 30.0f;

	// 胶囊体半高
	UPROPERTY(EditAnywhere, Category = "Config|Shape", meta = (DisplayName = "胶囊体半高"))
	float CapsuleHalfHeight = 60.0f;

	// 位置偏移（局部空间）
	UPROPERTY(EditAnywhere, Category = "Config|Offset", meta = (DisplayName = "位置偏移"))
	FVector LocationOffset = FVector::ZeroVector;

	// 旋转偏移（局部空间）
	UPROPERTY(EditAnywhere, Category = "Config|Offset", meta = (DisplayName = "旋转偏移"))
	FRotator RotationOffset = FRotator::ZeroRotator;

	// 发送的事件标签
	UPROPERTY(EditAnywhere, Category = "Config|Event", meta = (DisplayName = "监听事件"))
	FGameplayTag HitEventTag;

	// 调试类型（None, ForOneFrame, ForDuration, Persistent）
	UPROPERTY(EditAnywhere, Category = "Debug", meta = (DisplayName = "调试类型"))
	TEnumAsByte<EDrawDebugTrace::Type> DrawDebugType = EDrawDebugTrace::None;

	// 调试颜色（未命中）
	UPROPERTY(EditAnywhere, Category = "Debug", meta = (DisplayName = "未命中调试颜色"))
	FLinearColor TraceColor = FLinearColor::Red;

	// 调试颜色（命中）
	UPROPERTY(EditAnywhere, Category = "Debug", meta = (DisplayName = "命中调试颜色"))
	FLinearColor TraceHitColor = FLinearColor::Green;

	// 调试显示时间（仅当 DrawDebugType 为 ForDuration 时有效）
	UPROPERTY(EditAnywhere, Category = "Debug", meta = (DisplayName = "调试显示事件"))
	float DrawTime = 5.0f;

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;

private:
	// 缓存本次攻击已命中的 Actor
	TArray<AActor*> HitActors;
};