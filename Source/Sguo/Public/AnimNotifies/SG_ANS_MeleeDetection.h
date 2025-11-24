// 📄 文件：Source/Sguo/Public/AnimNotifies/SG_ANS_MeleeDetection.h

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "GameplayTagContainer.h"
#include "Kismet/KismetSystemLibrary.h"
#include "SG_ANS_MeleeDetection.generated.h"

/**
 * @brief 增强版近战攻击检测通知状态
 * @details 
 * 功能说明：
 * - 在动画期间执行带旋转的胶囊体扫掠检测
 * - 支持双插槽定义扫掠路径（Start -> End）
 * - 支持位置与旋转偏移（分别配置起始和结束插槽）
 * - 支持伤害倍率配置
 * - 使用原生 DrawDebugTrace 枚举控制调试
 * - 通过 GameplayEvent 将命中信息传递给 GA
 */
UCLASS()
class SGUO_API USG_ANS_MeleeDetection : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	USG_ANS_MeleeDetection();

	// ========== 插槽配置 ==========
	
	/**
	 * @brief 起始插槽名称
	 * @details 扫掠起点（如：WeaponHandle、hand_r）
	 */
	UPROPERTY(EditAnywhere, Category = "Config|Socket", meta = (DisplayName = "起始插槽"))
	FName StartSocketName;

	/**
	 * @brief 结束插槽名称
	 * @details 扫掠终点（如：WeaponTip、weapon_end）
	 */
	UPROPERTY(EditAnywhere, Category = "Config|Socket", meta = (DisplayName = "结束插槽"))
	FName EndSocketName;

	// ========== 起始插槽偏移 ==========
	
	/**
	 * @brief 起始插槽位置偏移（局部空间）
	 * @details 相对于起始插槽的偏移（前X、右Y、上Z）
	 */
	UPROPERTY(EditAnywhere, Category = "Config|Offset|Start", meta = (DisplayName = "起始位置偏移"))
	FVector StartLocationOffset = FVector::ZeroVector;

	/**
	 * @brief 起始插槽旋转偏移（局部空间）
	 * @details 相对于起始插槽的旋转偏移（俯仰Pitch、偏航Yaw、翻滚Roll）
	 */
	UPROPERTY(EditAnywhere, Category = "Config|Offset|Start", meta = (DisplayName = "起始旋转偏移"))
	FRotator StartRotationOffset = FRotator::ZeroRotator;

	// ========== 结束插槽偏移 ==========
	
	/**
	 * @brief 结束插槽位置偏移（局部空间）
	 * @details 相对于结束插槽的偏移（前X、右Y、上Z）
	 */
	UPROPERTY(EditAnywhere, Category = "Config|Offset|End", meta = (DisplayName = "结束位置偏移"))
	FVector EndLocationOffset = FVector::ZeroVector;

	/**
	 * @brief 结束插槽旋转偏移（局部空间）
	 * @details 相对于结束插槽的旋转偏移（俯仰Pitch、偏航Yaw、翻滚Roll）
	 */
	UPROPERTY(EditAnywhere, Category = "Config|Offset|End", meta = (DisplayName = "结束旋转偏移"))
	FRotator EndRotationOffset = FRotator::ZeroRotator;

	// ========== 检测形状配置 ==========
	
	/**
	 * @brief 胶囊体半径
	 * @details 检测胶囊体的半径（厘米）
	 */
	UPROPERTY(EditAnywhere, Category = "Config|Shape", meta = (DisplayName = "胶囊体半径", ClampMin = "1.0", UIMin = "1.0", UIMax = "2000.0"))
	float CapsuleRadius = 30.0f;

	/**
	 * @brief 胶囊体半高
	 * @details 检测胶囊体的半高（厘米）
	 */
	UPROPERTY(EditAnywhere, Category = "Config|Shape", meta = (DisplayName = "胶囊体半高", ClampMin = "1.0", UIMin = "1.0", UIMax = "5000.0"))
	float CapsuleHalfHeight = 60.0f;

	// ========== ✨ 新增 - 伤害倍率配置 ==========
	
	/**
	 * @brief 伤害倍率
	 * @details
	 * 功能说明：
	 * - 此次攻击的伤害倍率
	 * - 将通过 GameplayEvent 传递给 GA
	 * - 最终伤害 = 基础攻击力 × 伤害倍率
	 * 使用场景：
	 * - 普通攻击：1.0
	 * - 重击：1.5
	 * - 终结技：2.0
	 */
	UPROPERTY(EditAnywhere, Category = "Config|Damage", meta = (DisplayName = "伤害倍率", ClampMin = "0.1", UIMin = "0.1", UIMax = "100.0"))
	float DamageMultiplier = 1.0f;

	// ========== 事件配置 ==========
	
	/**
	 * @brief 发送的事件标签
	 * @details 命中敌人时发送的 GameplayEvent Tag
	 */
	UPROPERTY(EditAnywhere, Category = "Config|Event", meta = (DisplayName = "命中事件标签", Categories = "Event.Attack"))
	FGameplayTag HitEventTag;

	// ========== 调试配置 ==========
	
	/**
	 * @brief 调试类型
	 * @details
	 * - None: 不绘制
	 * - ForOneFrame: 绘制一帧
	 * - ForDuration: 持续绘制
	 * - Persistent: 永久绘制
	 */
	UPROPERTY(EditAnywhere, Category = "Debug", meta = (DisplayName = "调试类型"))
	TEnumAsByte<EDrawDebugTrace::Type> DrawDebugType = EDrawDebugTrace::None;

	/**
	 * @brief 调试颜色（未命中）
	 */
	UPROPERTY(EditAnywhere, Category = "Debug", meta = (DisplayName = "未命中调试颜色"))
	FLinearColor TraceColor = FLinearColor::Red;

	/**
	 * @brief 调试颜色（命中）
	 */
	UPROPERTY(EditAnywhere, Category = "Debug", meta = (DisplayName = "命中调试颜色"))
	FLinearColor TraceHitColor = FLinearColor::Green;

	/**
	 * @brief 调试显示时间
	 * @details 仅当 DrawDebugType 为 ForDuration 时有效
	 */
	UPROPERTY(EditAnywhere, Category = "Debug", meta = (DisplayName = "调试显示时间", ClampMin = "0.1", UIMin = "0.1", UIMax = "10.0"))
	float DrawTime = 5.0f;

	// ========== 重写函数 ==========
	
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

private:
	/**
	 * @brief 缓存本次攻击已命中的 Actor
	 * @details 防止同一个敌人被多次命中
	 */
	TArray<AActor*> HitActors;
};
