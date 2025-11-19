// ✨ 新增 - 攻击能力基类
// Copyright notice placeholder
/**
 * @file SG_GameplayAbility_Attack.h
 * @brief 攻击能力基类
 * @details
 * 功能说明：
 * - 所有攻击类技能的基类（近战、远程、技能攻击等）
 * - 提供统一的攻击流程和伤害应用接口
 * - 支持攻击动画、攻击范围检测、伤害应用
 * 详细流程：
 * 1. ActivateAbility：触发攻击
 * 2. PlayAttackAnimation：播放攻击动画
 * 3. OnAttackAnimationNotify：动画通知回调（攻击判定帧）
 * 4. ApplyDamageToTarget：应用伤害到目标
 * 5. EndAbility：结束攻击
 * 注意事项：
 * - 子类需要重写 GetAttackRange() 返回攻击范围
 * - 子类可以重写 FindTargetsInRange() 自定义目标查找逻辑
 */

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "SG_GameplayAbility_Attack.generated.h"

// 前置声明
class UAnimMontage;
class UGameplayEffect;

/**
 * @brief 攻击类型枚举
 * @details
 * 功能说明：
 * - 定义不同的攻击类型
 * - 用于区分攻击判定逻辑
 */
UENUM(BlueprintType)
enum class ESGAttackAbilityType : uint8
{
	// 近战攻击（球形范围检测）
	Melee       UMETA(DisplayName = "近战攻击"),
	
	// 远程攻击（生成投射物）
	Ranged      UMETA(DisplayName = "远程攻击"),
	
	// 技能攻击（自定义判定）
	Skill       UMETA(DisplayName = "技能攻击")
};

/**
 * @brief 攻击能力基类
 * @details
 * 功能说明：
 * - 提供统一的攻击流程
 * - 管理攻击动画和判定时机
 * - 应用伤害到目标
 * 使用方式：
 * 1. 创建子类继承此基类
 * 2. 设置攻击类型和攻击动画
 * 3. 配置伤害 GameplayEffect
 * 4. 在动画蒙太奇中添加 AnimNotify（攻击判定帧）
 */
UCLASS(Abstract)
class SGUO_API USG_GameplayAbility_Attack : public UGameplayAbility
{
	GENERATED_BODY()

public:
	/**
	 * @brief 构造函数
	 * @details
	 * 功能说明：
	 * - 初始化默认值
	 * - 设置技能标签
	 */
	USG_GameplayAbility_Attack();

protected:
	// ========== 攻击配置 ==========
	
	/**
	 * @brief 攻击类型
	 * @details
	 * 功能说明：
	 * - 定义攻击的判定方式
	 * - 近战：球形范围检测
	 * - 远程：生成投射物
	 * - 技能：自定义判定
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack Config", meta = (DisplayName = "攻击类型"))
	ESGAttackAbilityType AttackType = ESGAttackAbilityType::Melee;

	/**
	 * @brief 攻击动画蒙太奇
	 * @details
	 * 功能说明：
	 * - 攻击时播放的动画
	 * - 需要在动画中添加 AnimNotify 触发攻击判定
	 * 注意事项：
	 * - AnimNotify 名称必须为 "AttackHit" 或配置的通知名称
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack Config", meta = (DisplayName = "攻击动画"))
	TObjectPtr<UAnimMontage> AttackMontage;

	/**
	 * @brief 攻击动画通知名称
	 * @details
	 * 功能说明：
	 * - 用于标识攻击判定帧的 AnimNotify 名称
	 * - 默认为 "AttackHit"
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack Config", meta = (DisplayName = "攻击判定通知名称"))
	FName AttackNotifyName = TEXT("AttackHit");

	/**
	 * @brief 伤害倍率
	 * @details
	 * 功能说明：
	 * - 基于角色攻击力的伤害倍率
	 * - 最终伤害 = 攻击力 * 伤害倍率
	 * - 默认 1.0（100%伤害）
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Attack Config", meta = (DisplayName = "伤害倍率", ClampMin = "0.0", UIMin = "0.0", UIMax = "5.0"))
	float DamageMultiplier = 1.0f;

	/**
	 * @brief 伤害 GameplayEffect 类
	 * @details
	 * 功能说明：
	 * - 用于应用伤害的 GE
	 * - 需要使用 SG_DamageExecutionCalc 作为 Execution
	 * 注意事项：
	 * - 如果为空，使用默认伤害 GE
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack Config", meta = (DisplayName = "伤害效果"))
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	// ========== Gameplay Ability 接口 ==========

	/**
	 * @brief 激活技能
	 * @param Handle 技能句柄
	 * @param ActorInfo Actor 信息
	 * @param ActivationInfo 激活信息
	 * @param TriggerEventData 触发事件数据
	 * @details
	 * 功能说明：
	 * - 技能被激活时调用
	 * - 播放攻击动画
	 * - 绑定动画通知回调
	 * 详细流程：
	 * 1. 调用父类 ActivateAbility
	 * 2. 检查攻击动画是否有效
	 * 3. 播放攻击动画
	 * 4. 绑定动画通知回调
	 * 5. 如果没有动画，直接触发攻击判定
	 */
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData
	) override;

	/**
	 * @brief 结束技能
	 * @param Handle 技能句柄
	 * @param ActorInfo Actor 信息
	 * @param ActivationInfo 激活信息
	 * @param bReplicateEndAbility 是否复制结束
	 * @param bWasCancelled 是否被取消
	 * @details
	 * 功能说明：
	 * - 技能结束时调用
	 * - 清理资源和状态
	 */
	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled
	) override;

	// ========== 攻击逻辑 ==========

	/**
	 * @brief 动画通知回调
	 * @param NotifyName 通知名称
	 * @param NewParam 新参数
	 * @details
	 * 功能说明：
	 * - 动画蒙太奇的 AnimNotify 触发时调用
	 * - 在攻击判定帧执行攻击逻辑
	 * 详细流程：
	 * 1. 检查通知名称是否匹配
	 * 2. 执行攻击判定
	 * 3. 查找目标并应用伤害
	 */
	UFUNCTION()
	virtual void OnMontageNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointPayload);

	/**
	 * @brief 执行攻击判定
	 * @details
	 * 功能说明：
	 * - 根据攻击类型执行不同的判定逻辑
	 * - 近战：球形范围检测
	 * - 远程：生成投射物
	 * - 技能：调用蓝图实现
	 */
	UFUNCTION(BlueprintCallable, Category = "Attack")
	virtual void PerformAttack();

	/**
	 * @brief 查找范围内的目标
	 * @param OutTargets 输出目标数组
	 * @return 找到的目标数量
	 * @details
	 * 功能说明：
	 * - 在攻击范围内查找敌方目标
	 * - 近战：球形范围检测
	 * - 远程：射线检测
	 * 注意事项：
	 * - 子类可以重写此函数自定义目标查找逻辑
	 */
	UFUNCTION(BlueprintCallable, Category = "Attack")
	virtual int32 FindTargetsInRange(TArray<AActor*>& OutTargets);

	/**
	 * @brief 应用伤害到目标
	 * @param Target 目标 Actor
	 * @details
	 * 功能说明：
	 * - 使用 GameplayEffect 对目标应用伤害
	 * - 设置伤害倍率（SetByCaller）
	 * 详细流程：
	 * 1. 获取目标的 AbilitySystemComponent
	 * 2. 创建 EffectContext
	 * 3. 创建 EffectSpec
	 * 4. 设置伤害倍率（SetByCaller）
	 * 5. 应用 GameplayEffect
	 */
	UFUNCTION(BlueprintCallable, Category = "Attack")
	virtual void ApplyDamageToTarget(AActor* Target);

	/**
	 * @brief 获取攻击范围
	 * @return 攻击范围（厘米）
	 * @details
	 * 功能说明：
	 * - 返回攻击的判定范围
	 * - 从角色的 AttributeSet 读取
	 */
	UFUNCTION(BlueprintPure, Category = "Attack")
	virtual float GetAttackRange() const;

	/**
	 * @brief 蓝图事件：攻击命中
	 * @param HitTargets 命中的目标
	 * @details
	 * 功能说明：
	 * - 蓝图可以监听此事件
	 * - 用于播放特效、音效等
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Attack", meta = (DisplayName = "攻击命中"))
	void OnAttackHit(const TArray<AActor*>& HitTargets);
};
