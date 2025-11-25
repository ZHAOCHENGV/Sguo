// 📄 文件：Source/Sguo/Public/Actors/SG_Projectile.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "GameplayCueInterface.h"
#include "SG_Projectile.generated.h"

// 前置声明
class UCapsuleComponent;
class UStaticMeshComponent;
class UGameplayEffect;
class UAbilitySystemComponent;

/**
 * @brief 投射物飞行模式
 */
UENUM(BlueprintType)
enum class ESGProjectileFlightMode : uint8
{
	/** 直线飞行 - 直接飞向目标 */
	Linear          UMETA(DisplayName = "直线飞行"),
	
	/** 抛物线飞行 - 带弧度的飞行，保证命中 */
	Parabolic       UMETA(DisplayName = "抛物线飞行"),
	
	/** 归航飞行 - 持续追踪目标 */
	Homing          UMETA(DisplayName = "归航飞行")
};

/**
 * @brief 投射物击中信息
 */
USTRUCT(BlueprintType)
struct FSGProjectileHitInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Hit Info", meta = (DisplayName = "击中目标"))
	AActor* HitActor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Hit Info", meta = (DisplayName = "击中位置"))
	FVector HitLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Hit Info", meta = (DisplayName = "击中法线"))
	FVector HitNormal = FVector::UpVector;

	UPROPERTY(BlueprintReadOnly, Category = "Hit Info", meta = (DisplayName = "击中骨骼"))
	FName HitBoneName = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Hit Info", meta = (DisplayName = "飞行方向"))
	FVector ProjectileDirection = FVector::ForwardVector;

	UPROPERTY(BlueprintReadOnly, Category = "Hit Info", meta = (DisplayName = "飞行速度"))
	float ProjectileSpeed = 0.0f;
};

// 击中事件委托
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSGProjectileHitSignature, const FSGProjectileHitInfo&, HitInfo);

/**
 * @brief 自定义弹道投射物
 * @details
 * 功能说明：
 * - 不使用 ProjectileMovementComponent
 * - 自定义 Tick 驱动的飞行系统
 * - 支持直线、抛物线、归航三种模式
 * - 抛物线模式保证命中目标
 * - 使用胶囊体碰撞，可调节方向
 */
UCLASS()
class SGUO_API ASG_Projectile : public AActor, public IGameplayCueInterface
{
	GENERATED_BODY()
	
public:	
	ASG_Projectile();

	// ========== 蓝图事件委托 ==========

	UPROPERTY(BlueprintAssignable, Category = "Projectile Events", meta = (DisplayName = "击中目标事件"))
	FSGProjectileHitSignature OnProjectileHitTarget;

	UPROPERTY(BlueprintAssignable, Category = "Projectile Events", meta = (DisplayName = "投射物销毁事件"))
	FSGProjectileHitSignature OnProjectileDestroyed;

protected:
	// ========== 组件 ==========
	
	/**
	 * @brief 场景根组件
	 * @details 作为根组件，允许其他组件自由调整位置和旋转
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "场景根"))
	TObjectPtr<USceneComponent> SceneRoot;

	/**
	 * @brief 胶囊体碰撞组件
	 * @details 
	 * 功能说明：
	 * - 不作为根组件，可自由调整方向
	 * - 适合箭矢等细长投射物
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "碰撞胶囊体"))
	TObjectPtr<UCapsuleComponent> CollisionCapsule;

	/**
	 * @brief 网格体组件
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "网格体"))
	TObjectPtr<UStaticMeshComponent> MeshComponent;

public:
	// ========== 飞行配置 ==========

	/**
	 * @brief 飞行模式
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Flight Config", meta = (DisplayName = "飞行模式"))
	ESGProjectileFlightMode FlightMode = ESGProjectileFlightMode::Parabolic;

	/**
	 * @brief 飞行速度（厘米/秒）
	 * @details 投射物将始终以此速度飞行，不受弧度影响
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Flight Config", meta = (DisplayName = "飞行速度", ClampMin = "100.0", UIMin = "100.0", UIMax = "10000.0"))
	float FlightSpeed = 3000.0f;

	/**
	 * @brief 抛物线弧度高度（厘米）
	 * @details 
	 * 功能说明：
	 * - 抛物线最高点相对于起点-终点连线的高度
	 * - 0 = 直线
	 * - 100 = 轻微弧度
	 * - 300 = 中等弧度
	 * - 500+ = 高抛
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Flight Config", meta = (DisplayName = "弧度高度", ClampMin = "0.0", UIMin = "0.0", UIMax = "1000.0", EditCondition = "FlightMode == ESGProjectileFlightMode::Parabolic", EditConditionHides))
	float ArcHeight = 200.0f;

	/**
	 * @brief 归航强度（仅归航模式）
	 * @details 
	 * 功能说明：
	 * - 每秒转向角度（度）
	 * - 越大追踪越灵敏
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Flight Config", meta = (DisplayName = "归航强度", ClampMin = "0.0", UIMin = "0.0", UIMax = "720.0", EditCondition = "FlightMode == ESGProjectileFlightMode::Homing", EditConditionHides))
	float HomingStrength = 180.0f;

	/**
	 * @brief 生存时间（秒）
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Flight Config", meta = (DisplayName = "生存时间", ClampMin = "0.1", UIMin = "0.1", UIMax = "30.0"))
	float LifeSpan = 10.0f;

	/**
	 * @brief 是否穿透
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Flight Config", meta = (DisplayName = "是否穿透"))
	bool bPenetrate = false;

	/**
	 * @brief 最大穿透数量
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Flight Config", meta = (DisplayName = "最大穿透数量", EditCondition = "bPenetrate", EditConditionHides, ClampMin = "0", UIMin = "0", UIMax = "10"))
	int32 MaxPenetrateCount = 0;

	// ========== 碰撞配置 ==========

	/**
	 * @brief 胶囊体半径
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Collision Config", meta = (DisplayName = "碰撞半径", ClampMin = "1.0", UIMin = "1.0", UIMax = "100.0"))
	float CapsuleRadius = 10.0f;

	/**
	 * @brief 胶囊体半高
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Collision Config", meta = (DisplayName = "碰撞半高", ClampMin = "1.0", UIMin = "1.0", UIMax = "200.0"))
	float CapsuleHalfHeight = 30.0f;

	/**
	 * @brief 碰撞体相对旋转
	 * @details 用于调整碰撞体方向，使其与网格体对齐
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Collision Config", meta = (DisplayName = "碰撞体旋转偏移"))
	FRotator CollisionRotationOffset = FRotator(90.0f, 0.0f, 0.0f);

	// ========== 伤害配置 ==========

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Damage Config", meta = (DisplayName = "伤害倍率", ClampMin = "0.0", UIMin = "0.0", UIMax = "5.0"))
	float DamageMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage Config", meta = (DisplayName = "伤害效果"))
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	// ========== GameplayCue 配置 ==========

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "GameplayCue", meta = (DisplayName = "击中 GameplayCue", Categories = "GameplayCue"))
	FGameplayTag HitGameplayCueTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "GameplayCue", meta = (DisplayName = "飞行 GameplayCue", Categories = "GameplayCue"))
	FGameplayTag TrailGameplayCueTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "GameplayCue", meta = (DisplayName = "销毁 GameplayCue", Categories = "GameplayCue"))
	FGameplayTag DestroyGameplayCueTag;

	// ========== 运行时数据 ==========

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Runtime")
	TObjectPtr<UAbilitySystemComponent> InstigatorASC;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Runtime")
	FGameplayTag InstigatorFactionTag;

	UPROPERTY(Transient)
	TArray<AActor*> HitActors;

	/** 当前目标（用于归航和抛物线） */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Runtime")
	TWeakObjectPtr<AActor> CurrentTarget;

protected:
	// ========== 飞行状态 ==========

	/** 起始位置 */
	FVector StartLocation;

	/** 目标位置（发射时记录） */
	FVector TargetLocation;

	/** 飞行进度（0-1） */
	float FlightProgress = 0.0f;

	/** 总飞行距离 */
	float TotalFlightDistance = 0.0f;

	/** 当前速度向量 */
	FVector CurrentVelocity;

	/** 是否已初始化 */
	bool bIsInitialized = false;

	/** 飞行 GC 是否已激活 */
	bool bTrailCueActive = false;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:	
	virtual void Tick(float DeltaTime) override;

	// ========== 初始化 ==========

	/**
	 * @brief 初始化投射物
	 * @param InInstigatorASC 攻击者 ASC
	 * @param InFactionTag 攻击者阵营
	 * @param InTarget 目标 Actor
	 * @param InArcHeight 弧度高度（覆盖默认值，-1 表示使用默认）
	 */
	UFUNCTION(BlueprintCallable, Category = "Projectile")
	void InitializeProjectile(
		UAbilitySystemComponent* InInstigatorASC,
		FGameplayTag InFactionTag,
		AActor* InTarget,
		float InArcHeight = -1.0f
	);

	/**
	 * @brief 设置飞行速度（运行时）
	 */
	UFUNCTION(BlueprintCallable, Category = "Projectile")
	void SetFlightSpeed(float NewSpeed);

	/**
	 * @brief 获取当前速度向量
	 */
	UFUNCTION(BlueprintPure, Category = "Projectile")
	FVector GetCurrentVelocity() const { return CurrentVelocity; }

protected:
	// ========== 飞行逻辑 ==========

	/** 更新直线飞行 */
	void UpdateLinearFlight(float DeltaTime);

	/** 更新抛物线飞行 */
	void UpdateParabolicFlight(float DeltaTime);

	/** 更新归航飞行 */
	void UpdateHomingFlight(float DeltaTime);

	/** 计算抛物线位置 */
	FVector CalculateParabolicPosition(float Progress) const;

	/** 更新旋转（朝向速度方向） */
	void UpdateRotation();

	// ========== 碰撞处理 ==========

	UFUNCTION()
	void OnCapsuleOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult
	);

	UFUNCTION()
	void OnCapsuleHit(
		UPrimitiveComponent* HitComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		FVector NormalImpulse,
		const FHitResult& Hit
	);

	void HandleProjectileImpact(AActor* OtherActor, const FHitResult& Hit);
	void ApplyDamageToTarget(AActor* Target);

	// ========== GameplayCue ==========

	void ExecuteHitGameplayCue(const FSGProjectileHitInfo& HitInfo);
	void ActivateTrailGameplayCue();
	void RemoveTrailGameplayCue();
	void ExecuteDestroyGameplayCue();

public:
	// ========== 蓝图事件 ==========

	UFUNCTION(BlueprintImplementableEvent, Category = "Projectile", meta = (DisplayName = "On Hit Target (BP)"))
	void K2_OnHitTarget(const FSGProjectileHitInfo& HitInfo);

	UFUNCTION(BlueprintImplementableEvent, Category = "Projectile", meta = (DisplayName = "On Projectile Destroyed (BP)"))
	void K2_OnProjectileDestroyed(FVector LastLocation);

	// ========== 调试 ==========

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditDefaultsOnly, Category = "Debug", meta = (DisplayName = "显示飞行轨迹"))
	bool bDrawDebugTrajectory = false;
#endif
};
