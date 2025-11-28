// Copyright (C) 2024 Sguo Project. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "AbilitySystemInterface.h"
#include "SG_RollingLog.generated.h"

// ============================================================================
// 前向声明
// ============================================================================
class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
class UGameplayEffect;
class UAbilitySystemComponent;
class USoundBase;
class UParticleSystem;
class UNiagaraSystem;

// ============================================================================
// 委托声明
// ============================================================================
// ✨ 新增: 木桩销毁事件委托
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRollingLogDestroyed, ASG_RollingLog*, DestroyedLog);

/**
 * @class ASG_RollingLog
 * @brief 流木计 - 滚动木桩Actor类
 * 
 * @details 功能说明:
 *          本类实现「流木计」策略卡中的单个滚动木桩实体。
 *          木桩从释放者主城方向生成，沿直线滚向敌方主城，
 *          击中敌方单位时造成伤害并击退，随后木桩破碎销毁。
 * 
 * @details 详细流程:
 *          1. InitializeLog() - 设置木桩运动方向、速度、伤害参数
 *          2. Tick() - 持续更新位置、执行滚动动画、检测超出范围
 *          3. OnSphereOverlap() - 检测与敌方单位碰撞
 *          4. HandleHitTarget() - 应用伤害和击退效果
 *          5. DestroyLog() - 播放破碎特效并销毁Actor
 * 
 * @note    使用注意:
 *          - 需要配合 ASG_RollingLogsEffect 使用
 *          - 击退方向为木桩运动方向
 *          - 击中一个目标后立即销毁（不穿透）
 * 
 * @see     ASG_RollingLogsEffect
 * @see     ASG_StrategyEffectBase
 */
UCLASS(Blueprintable, BlueprintType)
class SGUO_API ASG_RollingLog : public AActor
{
	GENERATED_BODY()
	
public:
	// ========================================================================
	// 构造函数
	// ========================================================================
	
	/**
	 * @brief 默认构造函数
	 * @details 初始化组件层级结构和默认碰撞设置
	 */
	ASG_RollingLog();

protected:
	// ========================================================================
	// 生命周期函数
	// ========================================================================
	
	/**
	 * @brief Actor开始播放时调用
	 * @details 设置生命周期、启动滚动动画、播放生成音效
	 */
	virtual void BeginPlay() override;

	/**
	 * @brief Actor结束播放时调用
	 * @param EndPlayReason - 结束原因枚举
	 * @details 清理定时器、解绑事件、播放销毁特效
	 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	// ========================================================================
	// Tick更新
	// ========================================================================
	
	/**
	 * @brief 每帧更新
	 * @param DeltaTime - 帧间隔时间
	 * @details 更新木桩位置、旋转（滚动动画）、检测是否超出战场范围
	 */
	virtual void Tick(float DeltaTime) override;

	// ========================================================================
	// 初始化接口
	// ========================================================================
	
	/**
	 * @brief 初始化木桩参数
	 * @param InMoveDirection - 移动方向（已归一化）
	 * @param InInstigator - 效果发起者（用于伤害归属）
	 * @param InInstigatorFactionTag - 发起者阵营标签
	 * @param InDamageEffect - 伤害GameplayEffect类
	 * @param InKnockbackEffect - 击退GameplayEffect类（可选）
	 * 
	 * @details 详细流程:
	 *          1. 缓存移动方向和发起者信息
	 *          2. 设置伤害和击退效果类
	 *          3. 根据移动方向计算初始旋转
	 *          4. 启动滚动运动
	 * 
	 * @note    必须在SpawnActor后立即调用此函数
	 */
	UFUNCTION(BlueprintCallable, Category = "Rolling Log|Initialization", 
		meta = (DisplayName = "初始化木桩"))
	void InitializeLog(
		const FVector& InMoveDirection,
		AActor* InInstigator,
		const FGameplayTag& InInstigatorFactionTag,
		TSubclassOf<UGameplayEffect> InDamageEffect,
		TSubclassOf<UGameplayEffect> InKnockbackEffect = nullptr
	);

	// ========================================================================
	// 公共接口
	// ========================================================================
	
	/**
	 * @brief 获取木桩当前移动方向
	 * @return 归一化的移动方向向量
	 */
	UFUNCTION(BlueprintPure, Category = "Rolling Log|State", 
		meta = (DisplayName = "获取移动方向"))
	FVector GetMoveDirection() const { return MoveDirection; }

	/**
	 * @brief 检查木桩是否仍然有效（未被销毁）
	 * @return true表示木桩有效
	 */
	UFUNCTION(BlueprintPure, Category = "Rolling Log|State", 
		meta = (DisplayName = "是否有效"))
	bool IsLogValid() const { return !bIsDestroying; }

	/**
	 * @brief 手动销毁木桩
	 * @details 播放破碎特效后延迟销毁Actor
	 */
	UFUNCTION(BlueprintCallable, Category = "Rolling Log|Actions", 
		meta = (DisplayName = "销毁木桩"))
	void DestroyLog();

	// ========================================================================
	// 事件委托
	// ========================================================================
	
	/** ✨ 新增: 木桩销毁时广播 */
	UPROPERTY(BlueprintAssignable, Category = "Rolling Log|Events", 
		meta = (DisplayName = "木桩销毁事件"))
	FOnRollingLogDestroyed OnRollingLogDestroyed;

protected:
	// ========================================================================
	// 组件
	// ========================================================================
	
	/** 根组件 - 场景组件 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rolling Log|Components", 
		meta = (DisplayName = "根组件"))
	USceneComponent* RootSceneComponent;

	/** ✨ 新增: 碰撞检测球体组件 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rolling Log|Components", 
		meta = (DisplayName = "碰撞球体"))
	USphereComponent* CollisionSphere;

	/** ✨ 新增: 木桩网格体组件 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rolling Log|Components", 
		meta = (DisplayName = "木桩网格"))
	UStaticMeshComponent* LogMesh;

	// ========================================================================
	// 运动配置
	// ========================================================================
	
	/** ✨ 新增: 木桩滚动速度（厘米/秒） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rolling Log|Movement", 
		meta = (DisplayName = "滚动速度", ClampMin = "100.0", ClampMax = "3000.0"))
	float RollSpeed = 800.0f;

	/** ✨ 新增: 木桩滚动旋转速度（度/秒） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rolling Log|Movement", 
		meta = (DisplayName = "旋转速度", ClampMin = "0.0", ClampMax = "1440.0"))
	float RotationSpeed = 360.0f;

	/** ✨ 新增: 木桩最大存活时间（秒） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rolling Log|Movement", 
		meta = (DisplayName = "最大存活时间", ClampMin = "1.0", ClampMax = "30.0"))
	float MaxLifeTime = 10.0f;

	/** ✨ 新增: 木桩超出战场范围后销毁的额外距离 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rolling Log|Movement", 
		meta = (DisplayName = "超界销毁距离", ClampMin = "0.0"))
	float DestroyBeyondDistance = 500.0f;

	// ========================================================================
	// 碰撞配置
	// ========================================================================
	
	/** ✨ 新增: 碰撞球体半径 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rolling Log|Collision", 
		meta = (DisplayName = "碰撞半径", ClampMin = "10.0", ClampMax = "500.0"))
	float CollisionRadius = 80.0f;

	// ========================================================================
	// 伤害配置
	// ========================================================================
	
	/** ✨ 新增: 伤害GameplayEffect类 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rolling Log|Damage", 
		meta = (DisplayName = "伤害效果类"))
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	/** ✨ 新增: 击退GameplayEffect类 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rolling Log|Damage", 
		meta = (DisplayName = "击退效果类"))
	TSubclassOf<UGameplayEffect> KnockbackEffectClass;

	/** ✨ 新增: 伤害倍率 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rolling Log|Damage", 
		meta = (DisplayName = "伤害倍率", ClampMin = "0.1", ClampMax = "10.0"))
	float DamageMultiplier = 1.0f;

	/** ✨ 新增: 击退距离（厘米） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rolling Log|Damage", 
		meta = (DisplayName = "击退距离", ClampMin = "0.0", ClampMax = "1000.0"))
	float KnockbackDistance = 200.0f;

	/** ✨ 新增: 击退持续时间（秒） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rolling Log|Damage", 
		meta = (DisplayName = "击退时间", ClampMin = "0.1", ClampMax = "2.0"))
	float KnockbackDuration = 0.3f;

	// ========================================================================
	// 视觉效果配置
	// ========================================================================
	
	/** ✨ 新增: 击中特效（Niagara） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rolling Log|VFX", 
		meta = (DisplayName = "击中特效"))
	UNiagaraSystem* HitEffect;

	/** ✨ 新增: 破碎特效（Niagara） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rolling Log|VFX", 
		meta = (DisplayName = "破碎特效"))
	UNiagaraSystem* DestroyEffect;

	/** ✨ 新增: 滚动拖尾特效（Niagara） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rolling Log|VFX", 
		meta = (DisplayName = "拖尾特效"))
	UNiagaraSystem* TrailEffect;

	// ========================================================================
	// 音效配置
	// ========================================================================
	
	/** ✨ 新增: 生成音效 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rolling Log|SFX", 
		meta = (DisplayName = "生成音效"))
	USoundBase* SpawnSound;

	/** ✨ 新增: 滚动循环音效 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rolling Log|SFX", 
		meta = (DisplayName = "滚动音效"))
	USoundBase* RollSound;

	/** ✨ 新增: 击中音效 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rolling Log|SFX", 
		meta = (DisplayName = "击中音效"))
	USoundBase* HitSound;

	/** ✨ 新增: 破碎音效 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rolling Log|SFX", 
		meta = (DisplayName = "破碎音效"))
	USoundBase* DestroySound;

	// ========================================================================
	// 调试配置
	// ========================================================================
	
	/** ✨ 新增: 是否显示调试信息 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rolling Log|Debug", 
		meta = (DisplayName = "显示调试"))
	bool bShowDebug = false;

	/** ✨ 新增: 调试绘制颜色 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rolling Log|Debug", 
		meta = (DisplayName = "调试颜色"))
	FColor DebugColor = FColor::Orange;

	// ========================================================================
	// 运行时状态
	// ========================================================================
	
	/** 移动方向（归一化） */
	UPROPERTY(BlueprintReadOnly, Category = "Rolling Log|Runtime", 
		meta = (DisplayName = "移动方向"))
	FVector MoveDirection;

	/** 效果发起者 */
	UPROPERTY(BlueprintReadOnly, Category = "Rolling Log|Runtime", 
		meta = (DisplayName = "发起者"))
	TWeakObjectPtr<AActor> EffectInstigator;

	/** 发起者阵营标签 */
	UPROPERTY(BlueprintReadOnly, Category = "Rolling Log|Runtime", 
		meta = (DisplayName = "发起者阵营"))
	FGameplayTag InstigatorFactionTag;

	/** 是否正在销毁中 */
	UPROPERTY(BlueprintReadOnly, Category = "Rolling Log|Runtime", 
		meta = (DisplayName = "正在销毁"))
	bool bIsDestroying = false;

	/** 已行进距离 */
	float TraveledDistance = 0.0f;

	/** 滚动音效组件引用 */
	UPROPERTY()
	UAudioComponent* RollSoundComponent;

	// ========================================================================
	// 内部函数
	// ========================================================================
	
	/**
	 * @brief 碰撞球体重叠事件回调
	 * @param OverlappedComponent - 被重叠的组件
	 * @param OtherActor - 重叠的其他Actor
	 * @param OtherComp - 重叠的其他组件
	 * @param OtherBodyIndex - 其他物体索引
	 * @param bFromSweep - 是否来自扫描
	 * @param SweepResult - 扫描结果
	 * 
	 * @details 详细流程:
	 *          1. 忽略自身和发起者
	 *          2. 检查目标是否为敌方单位（通过阵营标签）
	 *          3. 如果是敌方单位，调用HandleHitTarget处理伤害
	 *          4. 销毁木桩
	 */
	UFUNCTION()
	void OnSphereOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult
	);

	/**
	 * @brief 处理击中目标
	 * @param HitTarget - 被击中的目标Actor
	 * 
	 * @details 详细流程:
	 *          1. 获取目标的AbilitySystemComponent
	 *          2. 应用伤害GameplayEffect
	 *          3. 应用击退效果（如果配置了）
	 *          4. 播放击中特效和音效
	 */
	void HandleHitTarget(AActor* HitTarget);

	/**
	 * @brief 应用击退效果到目标
	 * @param Target - 目标Actor
	 * 
	 * @details 详细流程:
	 *          1. 获取目标CharacterMovementComponent
	 *          2. 计算击退方向（木桩移动方向）
	 *          3. 应用冲量或位移
	 */
	void ApplyKnockbackToTarget(AActor* Target);

	/**
	 * @brief 播放击中特效
	 * @param HitLocation - 击中位置
	 */
	void PlayHitEffects(const FVector& HitLocation);

	/**
	 * @brief 播放破碎特效
	 */
	void PlayDestroyEffects();

	/**
	 * @brief 检查目标是否为敌方
	 * @param TargetActor - 目标Actor
	 * @return true表示是敌方单位
	 * 
	 * @details 通过比较目标的阵营标签与发起者阵营标签判断
	 */
	bool IsEnemyTarget(AActor* TargetActor) const;

	/**
	 * @brief 更新滚动动画
	 * @param DeltaTime - 帧间隔
	 * @details 根据移动速度计算网格体旋转，模拟滚动效果
	 */
	void UpdateRollingAnimation(float DeltaTime);

	/**
	 * @brief 绘制调试信息
	 */
	void DrawDebugInfo();
};
