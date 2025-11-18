// ✨ 新增 - 攻击能力基类
// Copyright Epic Games, Inc. All Rights Reserved.

/**
 * @file SG_GameplayAbility_Attack.h
 * @brief 攻击能力基类
 * @details
 * 功能说明：
 * - 提供攻击能力的基础实现
 * - 支持近战、远程、技能三种攻击类型
 * - 提供可视化调试功能
 * 使用方式：
 * - 近战攻击：继承并配置为 Melee 类型
 * - 远程攻击：继承并配置为 Ranged 类型
 * - 技能攻击：继承并重写 FindTargetsInRange
 * 注意事项：
 * - 需要配置攻击动画和伤害 GE
 * - 调试可视化仅在开发版本中启用
 */

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "SG_GameplayAbility_Attack.generated.h"

// 前向声明
class UAnimMontage;
class UGameplayEffect;

/**
 * @brief 攻击类型枚举
 * @details
 * 功能说明：
 * - 定义攻击能力的判定方式
 * - 近战：球形范围检测
 * - 远程：射线检测
 * - 技能：自定义检测（由子类实现）
 */
UENUM(BlueprintType)
enum class ESGAttackAbilityType : uint8
{
	// 近战攻击（球形范围检测）
	Melee   UMETA(DisplayName = "近战"),
	
	// 远程攻击（射线检测）
	Ranged  UMETA(DisplayName = "远程"),
	
	// 技能攻击（自定义检测）
	Skill   UMETA(DisplayName = "技能")
};

/**
 * @brief 攻击能力基类
 * @details
 * 功能说明：
 * - 提供完整的攻击流程（动画 → 判定 → 伤害）
 * - 支持多种攻击类型
 * - 提供蓝图可重写事件
 * - 提供可视化调试功能
 * 技术细节：
 * - 使用 AnimNotify 触发攻击判定
 * - 使用 GameplayEffect 应用伤害
 * - 支持网络同步（LocalPredicted）
 */
UCLASS()
class SGUO_API USG_GameplayAbility_Attack : public UGameplayAbility
{
	GENERATED_BODY()

public:
	// ========== 构造函数 ==========
	
	/**
	 * @brief 构造函数
	 * @details
	 * 功能说明：
	 * - 设置技能标签（Ability.Attack）
	 * - 配置实例化策略
	 * - 配置网络执行策略
	 */
	USG_GameplayAbility_Attack();

	// ========== 攻击配置 ==========

	/**
	 * @brief 攻击类型
	 * @details
	 * 功能说明：
	 * - 决定攻击判定方式
	 * - 近战：球形范围检测
	 * - 远程：射线检测
	 * - 技能：自定义检测
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack Config", 
		meta = (DisplayName = "攻击类型"))
	ESGAttackAbilityType AttackType = ESGAttackAbilityType::Melee;

	/**
	 * @brief 攻击动画蒙太奇
	 * @details
	 * 功能说明：
	 * - 攻击时播放的动画
	 * - 需要在动画中添加 AnimNotify 触发攻击判定
	 * 注意事项：
	 * - AnimNotify 名称必须与 AttackNotifyName 匹配
	 * - 默认为 "AttackHit"
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack Config", 
		meta = (DisplayName = "攻击动画"))
	TObjectPtr<UAnimMontage> AttackMontage;

	/**
	 * @brief 伤害 GameplayEffect 类
	 * @details
	 * 功能说明：
	 * - 应用到目标的伤害效果
	 * - 必须是 Instant 类型的 GE
	 * - 使用 SetByCaller 传递伤害倍率
	 * 注意事项：
	 * - GE 中必须配置 Data.Damage 标签
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack Config", 
		meta = (DisplayName = "伤害效果"))
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	/**
	 * @brief 伤害倍率
	 * @details
	 * 功能说明：
	 * - 控制伤害的缩放
	 * - 最终伤害 = 基础伤害 * 伤害倍率
	 * 使用场景：
	 * - 技能升级：增加倍率
	 * - 暴击：倍率 > 1.0
	 * - 弱化：倍率 < 1.0
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack Config", 
		meta = (DisplayName = "伤害倍率", ClampMin = "0.1", UIMin = "0.1", UIMax = "10.0"))
	float DamageMultiplier = 1.0f;

	/**
	 * @brief 动画通知名称
	 * @details
	 * 功能说明：
	 * - 在动画中触发攻击判定的 AnimNotify 名称
	 * - 当动画播放到此通知时，执行攻击判定
	 * 默认值：
	 * - "AttackHit"
	 * 注意事项：
	 * - 必须与动画中的 AnimNotify 名称完全匹配（区分大小写）
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack Config", 
		meta = (DisplayName = "攻击判定通知名称"))
	FName AttackNotifyName = TEXT("AttackHit");

	// ========== ✨ 新增 - 调试可视化配置 ==========

	/**
	 * @brief 是否显示攻击范围检测可视化
	 * @details
	 * 功能说明：
	 * - 在攻击时绘制检测范围（球体或射线）
	 * - 显示检测到的目标
	 * - 用于调试攻击判定问题
	 * 可视化内容：
	 * - 近战：球形检测范围 + 目标标记
	 * - 远程：射线 + 命中点 + 目标标记
	 * - 施放者到目标的连线
	 * - 检测信息文本
	 * 注意事项：
	 * - 仅在开发版本中启用
	 * - 会轻微影响性能
	 * - 正式版本应禁用
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Visualization", 
		meta = (DisplayName = "显示攻击检测可视化"))
	bool bShowAttackDetection = false;

	/**
	 * @brief 攻击检测持续时间（秒）
	 * @details
	 * 功能说明：
	 * - 控制调试绘制的持续时间
	 * - 0 = 只显示一帧（不推荐，太快看不清）
	 * - >0 = 持续显示指定秒数
	 * 建议值：
	 * - 0.5 秒：快速查看
	 * - 1.0 秒：标准查看（推荐）
	 * - 2.0 秒：详细查看
	 * - 5.0 秒：截图/录制用
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Visualization", 
		meta = (DisplayName = "检测可视化持续时间", ClampMin = "0.0", UIMin = "0.0", UIMax = "5.0",
		EditCondition = "bShowAttackDetection", EditConditionHides))
	float DetectionVisualizationDuration = 1.0f;

	/**
	 * @brief 攻击范围颜色（未命中）
	 * @details
	 * 功能说明：
	 * - 当攻击未命中任何目标时的颜色
	 * - 用于球形检测范围或射线
	 * 推荐颜色：
	 * - Yellow：清晰可见，表示警告/未命中
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Visualization", 
		meta = (DisplayName = "未命中颜色",
		EditCondition = "bShowAttackDetection", EditConditionHides))
	FColor AttackRangeMissColor = FColor::Yellow;

	/**
	 * @brief 攻击范围颜色（命中）
	 * @details
	 * 功能说明：
	 * - 当攻击命中目标时的颜色
	 * - 用于球形检测范围或射线
	 * 推荐颜色：
	 * - Red：醒目，表示攻击成功
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Visualization", 
		meta = (DisplayName = "命中颜色",
		EditCondition = "bShowAttackDetection", EditConditionHides))
	FColor AttackRangeHitColor = FColor::Red;

	/**
	 * @brief 目标标记颜色
	 * @details
	 * 功能说明：
	 * - 标记检测到的目标的颜色
	 * - 用于目标位置的球体标记
	 * 推荐颜色：
	 * - Orange：与攻击颜色区分，易于识别
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Visualization", 
		meta = (DisplayName = "目标标记颜色",
		EditCondition = "bShowAttackDetection", EditConditionHides))
	FColor TargetMarkerColor = FColor::Orange;

	/**
	 * @brief 目标标记大小
	 * @details
	 * 功能说明：
	 * - 标记检测到的目标的球体大小（厘米）
	 * 建议值：
	 * - 30 - 50：小型标记，不遮挡视野
	 * - 50 - 100：标准标记（推荐）
	 * - 100+：大型标记，远距离可见
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Visualization", 
		meta = (DisplayName = "目标标记大小", ClampMin = "10.0", UIMin = "10.0", UIMax = "200.0",
		EditCondition = "bShowAttackDetection", EditConditionHides))
	float TargetMarkerSize = 50.0f;

	// ========== 能力接口 ==========

	/**
	 * @brief 激活能力
	 * @details
	 * 功能说明：
	 * - 播放攻击动画
	 * - 绑定动画通知回调
	 * - 如果没有动画，直接执行攻击判定
	 * 执行流程：
	 * 1. 调用父类激活
	 * 2. 播放攻击动画
	 * 3. 绑定 AnimNotify 回调
	 * 4. 等待动画通知触发攻击判定
	 */
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData
	) override;

	/**
	 * @brief 结束能力
	 * @details
	 * 功能说明：
	 * - 清理资源
	 * - 调用父类结束
	 */
	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled
	) override;

protected:
	// ========== 攻击逻辑 ==========

	/**
	 * @brief 动画通知回调
	 * @param NotifyName 通知名称
	 * @param BranchingPointPayload 分支点载荷
	 * @details
	 * 功能说明：
	 * - 在动画播放到特定帧时触发
	 * - 检查通知名称是否匹配
	 * - 执行攻击判定
	 * 注意事项：
	 * - 必须在动画中添加 AnimNotify
	 * - 通知名称必须与 AttackNotifyName 匹配
	 */
	UFUNCTION()
	void OnMontageNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointPayload);

	/**
	 * @brief 执行攻击判定
	 * @details
	 * 功能说明：
	 * - 查找范围内的目标
	 * - 对每个目标应用伤害
	 * - 触发蓝图事件
	 * 执行流程：
	 * 1. 调用 FindTargetsInRange 查找目标
	 * 2. 遍历目标列表
	 * 3. 对每个目标调用 ApplyDamageToTarget
	 * 4. 触发 OnAttackHit 蓝图事件
	 */
	UFUNCTION(BlueprintCallable, Category = "Attack")
	void PerformAttack();

	/**
	 * @brief 查找范围内的目标
	 * @param OutTargets 输出：找到的目标列表
	 * @return 找到的目标数量
	 * @details
	 * 功能说明：
	 * - 根据攻击类型执行不同的检测
	 * - 近战：球形范围检测
	 * - 远程：射线检测
	 * - 技能：由子类实现
	 * 检测逻辑：
	 * - 只检测敌方单位（阵营不同）
	 * - 自动过滤施放者
	 * - 避免重复添加同一目标
	 * 注意事项：
	 * - 子类可以重写此函数实现自定义检测
	 */
	UFUNCTION(BlueprintCallable, Category = "Attack")
	virtual int32 FindTargetsInRange(TArray<AActor*>& OutTargets);

	/**
	 * @brief 应用伤害到目标
	 * @param Target 目标 Actor
	 * @details
	 * 功能说明：
	 * - 创建伤害 GameplayEffect
	 * - 设置伤害倍率
	 * - 应用到目标
	 * 执行流程：
	 * 1. 获取目标的 ASC
	 * 2. 创建 EffectSpec
	 * 3. 设置伤害倍率（SetByCaller）
	 * 4. 应用 GE 到目标
	 * 注意事项：
	 * - 目标必须有 AbilitySystemComponent
	 * - 必须配置 DamageEffectClass
	 */
	UFUNCTION(BlueprintCallable, Category = "Attack")
	void ApplyDamageToTarget(AActor* Target);

	/**
	 * @brief 获取攻击范围
	 * @return 攻击范围（厘米）
	 * @details
	 * 功能说明：
	 * - 从施放者的 AttributeSet 读取攻击范围
	 * - 如果无法获取，返回默认值（150）
	 * 使用场景：
	 * - FindTargetsInRange 中使用
	 * - 调试可视化中使用
	 */
	UFUNCTION(BlueprintPure, Category = "Attack")
	float GetAttackRange() const;

	// ========== ✨ 新增 - 调试可视化函数 ==========

	/**
	 * @brief 绘制近战攻击范围检测
	 * @param Center 检测中心位置
	 * @param Radius 检测半径
	 * @param bHit 是否命中目标
	 * @details
	 * 功能说明：
	 * - 绘制球形检测范围
	 * - 根据是否命中使用不同颜色
	 * - 显示检测信息文本
	 * 可视化内容：
	 * - 球体（检测范围）
	 * - 中心点标记
	 * - 文本标签（半径、命中状态）
	 */
	void DrawMeleeAttackDetection(const FVector& Center, float Radius, bool bHit);

	/**
	 * @brief 绘制远程攻击范围检测
	 * @param Start 射线起点
	 * @param End 射线终点
	 * @param bHit 是否命中目标
	 * @param HitLocation 命中位置（如果命中）
	 * @details
	 * 功能说明：
	 * - 绘制射线检测
	 * - 根据是否命中使用不同颜色
	 * - 在命中点绘制标记
	 * 可视化内容：
	 * - 射线（起点到终点）
	 * - 命中点十字标记
	 * - 起点/终点标记
	 * - 文本标签（距离、命中状态）
	 */
	void DrawRangedAttackDetection(const FVector& Start, const FVector& End, bool bHit, const FVector& HitLocation);

	/**
	 * @brief 绘制目标标记
	 * @param Targets 检测到的目标列表
	 * @details
	 * 功能说明：
	 * - 在每个目标位置绘制球体标记
	 * - 绘制从施放者到目标的连线
	 * - 显示目标序号和名称
	 * - 显示目标朝向箭头
	 * 可视化内容：
	 * - 目标位置球体
	 * - 施放者到目标的连线
	 * - 目标序号和名称文本
	 * - 目标朝向箭头
	 * - 总结信息（目标数量）
	 */
	void DrawTargetMarkers(const TArray<AActor*>& Targets);

	// ========== 蓝图事件 ==========

	/**
	 * @brief 攻击命中时触发（蓝图可重写）
	 * @param Targets 命中的目标列表
	 * @details
	 * 功能说明：
	 * - 在攻击命中目标后触发
	 * - 蓝图可以重写此事件添加自定义逻辑
	 * 使用场景：
	 * - 播放命中特效
	 * - 播放命中音效
	 * - 触发连击系统
	 * - 触发成就/任务
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Attack", 
		meta = (DisplayName = "攻击命中时"))
	void OnAttackHit(const TArray<AActor*>& Targets);
};
