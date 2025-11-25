// 📄 文件：Source/Sguo/Public/AnimNotifies/SG_AN_SpawnProjectile.h

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameplayTagContainer.h"
#include "SG_AN_SpawnProjectile.generated.h"

UCLASS()
class SGUO_API USG_AN_SpawnProjectile : public UAnimNotify
{
	GENERATED_BODY()

public:
	USG_AN_SpawnProjectile();

	// ========== 发射点配置 ==========

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Transform", meta = (DisplayName = "发射插槽"))
	FName SocketName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Transform", meta = (DisplayName = "位置偏移", MakeEditWidget = true))
	FVector LocationOffset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Transform", meta = (DisplayName = "旋转偏移"))
	FRotator RotationOffset;

	// ========== 投射物参数 ==========

	/**
	 * @brief 覆盖飞行速度
	 * @details 0 = 使用投射物默认速度
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Projectile", meta = (DisplayName = "覆盖飞行速度", ClampMin = "0.0", UIMin = "0.0", UIMax = "10000.0"))
	float OverrideFlightSpeed = 0.0f;

	/**
	 * @brief 覆盖弧度高度
	 * @details 
	 * -1 = 使用投射物默认值
	 * 0 = 直线
	 * 100-500 = 推荐弧度范围
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Projectile", meta = (DisplayName = "覆盖弧度高度", ClampMin = "-1.0", UIMin = "-1.0", UIMax = "1000.0"))
	float OverrideArcHeight = -1.0f;

	// ========== GAS 配置 ==========

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|GAS", meta = (DisplayName = "事件标签"))
	FGameplayTag EventTag;

	// ========== 调试 ==========

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Debug", meta = (DisplayName = "启用调试"))
	bool bDrawDebug = false;

	// ========== 重写函数 ==========

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

#if WITH_EDITORONLY_DATA
	FColor NotifyColor;
#endif
};
