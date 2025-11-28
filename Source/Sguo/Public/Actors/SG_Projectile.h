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
	
	/** 抛物线飞行 - 带弧度的飞行 */
	Parabolic       UMETA(DisplayName = "抛物线飞行"),
	
	/** 归航飞行 - 持续追踪目标 */
	Homing          UMETA(DisplayName = "归航飞行")
};

// ✨ 新增 - 投射物目标模式枚举
/**
 * @brief 投射物目标模式
 * @details 定义投射物的目标类型
 */
UENUM(BlueprintType)
enum class ESGProjectileTargetMode : uint8
{
	/** 目标 Actor - 飞向指定 Actor 的中心 */
	TargetActor         UMETA(DisplayName = "目标Actor"),
	
	/** 指定位置 - 飞向指定的世界坐标位置 */
	TargetLocation      UMETA(DisplayName = "指定位置"),
	
	/** 区域中心 - 飞向指定区域的中心点（落地） */
	AreaCenter          UMETA(DisplayName = "区域中心"),
	
	/** 区域随机点 - 飞向指定区域内的随机点（落地） */
	AreaRandom          UMETA(DisplayName = "区域随机点"),
	
	/** 目标周围随机点 - 飞向目标 Actor 周围的随机点（落地） */
	TargetAreaRandom    UMETA(DisplayName = "目标周围随机点")
};

// ✨ 新增 - 区域形状枚举
/**
 * @brief 区域形状
 * @details 用于定义随机点生成的区域形状
 */
UENUM(BlueprintType)
enum class ESGProjectileAreaShape : uint8
{
	/** 圆形区域 */
	Circle      UMETA(DisplayName = "圆形"),
	
	/** 矩形区域 */
	Rectangle   UMETA(DisplayName = "矩形"),
	
	/** 扇形区域 */
	Sector      UMETA(DisplayName = "扇形")
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
 * - 支持直线、抛物线、归航三种飞行模式
 * - 支持多种目标模式：Actor、位置、区域中心、区域随机点
 * - 抛物线模式：目标丢失时自动飞向地面落点
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

	// ✨ 新增 - 落地事件委托
	/**
	 * @brief 落地事件委托
	 * @details 投射物落地时触发（未命中目标时）
	 */
	UPROPERTY(BlueprintAssignable, Category = "Projectile Events", meta = (DisplayName = "落地事件"))
	FSGProjectileHitSignature OnProjectileGroundImpact;

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

	// ✨ 新增 - 目标配置
	// ========== 目标配置 ==========

	/**
	 * @brief 目标模式
	 * @details 定义投射物的目标类型
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Target Config", meta = (DisplayName = "目标模式"))
	ESGProjectileTargetMode TargetMode = ESGProjectileTargetMode::TargetActor;

	/**
	 * @brief 目标位置偏移（相对于目标）
	 * @details 
	 * 功能说明：
	 * - X: 前后偏移（正值 = 目标前方）
	 * - Y: 左右偏移（正值 = 目标右侧）
	 * - Z: 上下偏移（正值 = 向上）
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Target Config", meta = (DisplayName = "目标位置偏移"))
	FVector TargetLocationOffset = FVector::ZeroVector;

	/**
	 * @brief 是否使用世界空间偏移
	 * @details 
	 * - true: 偏移向量在世界空间中应用
	 * - false: 偏移向量相对于目标的朝向应用
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Target Config", meta = (DisplayName = "使用世界空间偏移"))
	bool bUseWorldSpaceOffset = true;

	/**
	 * @brief 地面检测距离
	 * @details 用于检测地面位置，计算抛物线落点
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Target Config", meta = (DisplayName = "地面检测距离", ClampMin = "100.0", UIMin = "100.0", UIMax = "5000.0"))
	float GroundTraceDistance = 1000.0f;

	/**
	 * @brief 地面检测通道
	 * @details 用于射线检测地面的碰撞通道
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Target Config", meta = (DisplayName = "地面检测通道"))
	TEnumAsByte<ECollisionChannel> GroundTraceChannel = ECC_WorldStatic;

	// ✨ 新增 - 区域配置
	// ========== 区域配置 ==========

	/**
	 * @brief 区域形状
	 * @details 用于区域随机点模式
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Area Config", meta = (DisplayName = "区域形状", EditCondition = "TargetMode == ESGProjectileTargetMode::AreaCenter || TargetMode == ESGProjectileTargetMode::AreaRandom || TargetMode == ESGProjectileTargetMode::TargetAreaRandom", EditConditionHides))
	ESGProjectileAreaShape AreaShape = ESGProjectileAreaShape::Circle;

	/**
	 * @brief 区域半径（圆形/扇形）
	 * @details 用于圆形和扇形区域
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Area Config", meta = (DisplayName = "区域半径", ClampMin = "0.0", UIMin = "0.0", UIMax = "2000.0", EditCondition = "(TargetMode == ESGProjectileTargetMode::AreaCenter || TargetMode == ESGProjectileTargetMode::AreaRandom || TargetMode == ESGProjectileTargetMode::TargetAreaRandom) && (AreaShape == ESGProjectileAreaShape::Circle || AreaShape == ESGProjectileAreaShape::Sector)", EditConditionHides))
	float AreaRadius = 300.0f;

	/**
	 * @brief 区域内半径（圆形/扇形）
	 * @details 用于生成环形区域，随机点不会生成在此半径内
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Area Config", meta = (DisplayName = "区域内半径", ClampMin = "0.0", UIMin = "0.0", UIMax = "1000.0", EditCondition = "(TargetMode == ESGProjectileTargetMode::AreaRandom || TargetMode == ESGProjectileTargetMode::TargetAreaRandom) && (AreaShape == ESGProjectileAreaShape::Circle || AreaShape == ESGProjectileAreaShape::Sector)", EditConditionHides))
	float AreaInnerRadius = 0.0f;

	/**
	 * @brief 区域尺寸（矩形）
	 * @details 矩形区域的长宽（X = 长度, Y = 宽度）
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Area Config", meta = (DisplayName = "区域尺寸", EditCondition = "(TargetMode == ESGProjectileTargetMode::AreaCenter || TargetMode == ESGProjectileTargetMode::AreaRandom || TargetMode == ESGProjectileTargetMode::TargetAreaRandom) && AreaShape == ESGProjectileAreaShape::Rectangle", EditConditionHides))
	FVector2D AreaSize = FVector2D(400.0f, 200.0f);

	/**
	 * @brief 扇形角度（度）
	 * @details 扇形区域的张角
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Area Config", meta = (DisplayName = "扇形角度", ClampMin = "0.0", ClampMax = "360.0", UIMin = "0.0", UIMax = "360.0", EditCondition = "(TargetMode == ESGProjectileTargetMode::AreaCenter || TargetMode == ESGProjectileTargetMode::AreaRandom || TargetMode == ESGProjectileTargetMode::TargetAreaRandom) && AreaShape == ESGProjectileAreaShape::Sector", EditConditionHides))
	float SectorAngle = 90.0f;

	/**
	 * @brief 扇形朝向偏移（度）
	 * @details 扇形中心线相对于目标朝向的偏移角度
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Area Config", meta = (DisplayName = "扇形朝向偏移", ClampMin = "-180.0", ClampMax = "180.0", UIMin = "-180.0", UIMax = "180.0", EditCondition = "(TargetMode == ESGProjectileTargetMode::AreaCenter || TargetMode == ESGProjectileTargetMode::AreaRandom || TargetMode == ESGProjectileTargetMode::TargetAreaRandom) && AreaShape == ESGProjectileAreaShape::Sector", EditConditionHides))
	float SectorDirectionOffset = 0.0f;

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

	/**
	 * @brief 落地 GameplayCue
	 * @details 投射物落地时触发（未命中目标时）
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "GameplayCue", meta = (DisplayName = "落地 GameplayCue", Categories = "GameplayCue"))
	FGameplayTag GroundImpactGameplayCueTag;

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

	/** 目标位置（目标中心或指定位置） */
	FVector TargetLocation;

	/** 地面落点位置（抛物线延伸到地面的点） */
	FVector GroundImpactLocation;

	// ✨ 新增 - 区域中心位置
	/** 区域中心位置 */
	FVector AreaCenterLocation;

	// ✨ 新增 - 区域朝向
	/** 区域朝向（用于扇形和矩形） */
	FRotator AreaRotation;

	/** 目标是否已丢失（死亡或消失） */
	bool bTargetLost = false;

	/** 飞行进度（0-1） */
	float FlightProgress = 0.0f;

	/** 总飞行距离 */
	float TotalFlightDistance = 0.0f;

	/** 到地面落点的总飞行距离 */
	float TotalFlightDistanceToGround = 0.0f;

	/** 当前速度向量 */
	FVector CurrentVelocity;

	/** 是否已初始化 */
	bool bIsInitialized = false;

	/** 飞行 GC 是否已激活 */
	bool bTrailCueActive = false;

	/** 是否已落地 */
	bool bHasLanded = false;

	// ✨ 新增 - 是否飞向地面
	/** 是否飞向地面（区域模式或目标丢失） */
	bool bFlyToGround = false;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:	
	virtual void Tick(float DeltaTime) override;

	// ========== 初始化 ==========

	/**
	 * @brief 初始化投射物（目标为 Actor）
	 * @param InInstigatorASC 攻击者 ASC
	 * @param InFactionTag 攻击者阵营
	 * @param InTarget 目标 Actor
	 * @param InArcHeight 弧度高度（覆盖默认值，-1 表示使用默认）
	 * @details 
	 * 功能说明：
	 * - TargetMode 为 TargetActor 时：飞向目标中心
	 * - TargetMode 为 TargetAreaRandom 时：飞向目标周围随机点
	 */
	UFUNCTION(BlueprintCallable, Category = "Projectile", meta = (DisplayName = "初始化投射物（目标Actor）"))
	void InitializeProjectile(
		UAbilitySystemComponent* InInstigatorASC,
		FGameplayTag InFactionTag,
		AActor* InTarget,
		float InArcHeight = -1.0f
	);

	/**
	 * @brief 初始化投射物（目标为位置）
	 * @param InInstigatorASC 攻击者 ASC
	 * @param InFactionTag 攻击者阵营
	 * @param InTargetLocation 目标位置
	 * @param InArcHeight 弧度高度（覆盖默认值，-1 表示使用默认）
	 * @details 
	 * 功能说明：
	 * - TargetMode 为 TargetLocation 时：飞向指定位置
	 * - TargetMode 为 AreaCenter 时：飞向区域中心地面
	 * - TargetMode 为 AreaRandom 时：飞向区域内随机地面点
	 */
	UFUNCTION(BlueprintCallable, Category = "Projectile", meta = (DisplayName = "初始化投射物（目标位置）"))
	void InitializeProjectileToLocation(
		UAbilitySystemComponent* InInstigatorASC,
		FGameplayTag InFactionTag,
		FVector InTargetLocation,
		float InArcHeight = -1.0f
	);

	// ✨ 新增 - 初始化投射物（目标为区域）
	/**
	 * @brief 初始化投射物（目标为区域）
	 * @param InInstigatorASC 攻击者 ASC
	 * @param InFactionTag 攻击者阵营
	 * @param InAreaCenter 区域中心位置
	 * @param InAreaRotation 区域朝向
	 * @param InArcHeight 弧度高度（覆盖默认值，-1 表示使用默认）
	 * @details 
	 * 功能说明：
	 * - 用于区域攻击
	 * - 根据 TargetMode 决定飞向区域中心还是随机点
	 * - 区域朝向用于扇形和矩形区域
	 */
	UFUNCTION(BlueprintCallable, Category = "Projectile", meta = (DisplayName = "初始化投射物（目标区域）"))
	void InitializeProjectileToArea(
		UAbilitySystemComponent* InInstigatorASC,
		FGameplayTag InFactionTag,
		FVector InAreaCenter,
		FRotator InAreaRotation,
		float InArcHeight = -1.0f
	);

	/**
	 * @brief 设置飞行速度（运行时）
	 */
	UFUNCTION(BlueprintCallable, Category = "Projectile")
	void SetFlightSpeed(float NewSpeed);

	/**
	 * @brief 设置目标位置偏移（运行时）
	 * @param NewOffset 新的偏移向量
	 * @param bWorldSpace 是否使用世界空间
	 */
	UFUNCTION(BlueprintCallable, Category = "Projectile", meta = (DisplayName = "设置目标位置偏移"))
	void SetTargetLocationOffset(FVector NewOffset, bool bWorldSpace = true);

	// ✨ 新增 - 设置区域参数
	/**
	 * @brief 设置区域参数（运行时）
	 * @param InShape 区域形状
	 * @param InRadius 区域半径（圆形/扇形）
	 * @param InInnerRadius 区域内半径
	 * @param InSize 区域尺寸（矩形）
	 * @param InSectorAngle 扇形角度
	 */
	UFUNCTION(BlueprintCallable, Category = "Projectile", meta = (DisplayName = "设置区域参数"))
	void SetAreaParameters(
		ESGProjectileAreaShape InShape,
		float InRadius = 300.0f,
		float InInnerRadius = 0.0f,
		FVector2D InSize = FVector2D(400.0f, 200.0f),
		float InSectorAngle = 90.0f
	);

	/**
	 * @brief 获取当前速度向量
	 */
	UFUNCTION(BlueprintPure, Category = "Projectile")
	FVector GetCurrentVelocity() const { return CurrentVelocity; }

	/**
	 * @brief 获取当前目标位置
	 */
	UFUNCTION(BlueprintPure, Category = "Projectile", meta = (DisplayName = "获取目标位置"))
	FVector GetTargetLocation() const { return TargetLocation; }

	/**
	 * @brief 获取地面落点位置
	 */
	UFUNCTION(BlueprintPure, Category = "Projectile", meta = (DisplayName = "获取地面落点位置"))
	FVector GetGroundImpactLocation() const { return GroundImpactLocation; }

	// ✨ 新增 - 获取区域中心位置
	/**
	 * @brief 获取区域中心位置
	 */
	UFUNCTION(BlueprintPure, Category = "Projectile", meta = (DisplayName = "获取区域中心位置"))
	FVector GetAreaCenterLocation() const { return AreaCenterLocation; }

protected:
	// ========== 飞行逻辑 ==========

	/** 更新直线飞行 */
	void UpdateLinearFlight(float DeltaTime);

	/** 更新抛物线飞行 */
	void UpdateParabolicFlight(float DeltaTime);

	/** 更新归航飞行 */
	void UpdateHomingFlight(float DeltaTime);

	/** 计算抛物线位置（飞向目标中心） */
	FVector CalculateParabolicPosition(float Progress) const;

	/** 计算到地面落点的抛物线位置 */
	FVector CalculateParabolicPositionToGround(float Progress) const;

	/** 更新旋转（朝向速度方向） */
	void UpdateRotation();

	// ========== 目标位置计算 ==========

	/**
	 * @brief 计算目标位置（应用偏移）
	 * @param InTarget 目标 Actor
	 * @return 计算后的目标位置
	 */
	FVector CalculateTargetLocation(AActor* InTarget) const;

	/**
	 * @brief 计算地面落点位置
	 * @param InTargetLocation 目标位置
	 * @return 地面落点位置
	 */
	FVector CalculateGroundImpactLocation(const FVector& InTargetLocation) const;

	/**
	 * @brief 检查目标是否仍然有效
	 * @return 目标是否有效
	 */
	bool IsTargetValid() const;

	/**
	 * @brief 处理目标丢失（切换到地面落点模式）
	 */
	void HandleTargetLost();

	/**
	 * @brief 处理投射物落地
	 */
	void HandleGroundImpact();

	// ✨ 新增 - 区域随机点计算
	// ========== 区域随机点计算 ==========

	/**
	 * @brief 在区域内生成随机点
	 * @param InCenter 区域中心
	 * @param InRotation 区域朝向
	 * @return 随机点位置（世界坐标）
	 * @details 
	 * 功能说明：
	 * - 根据区域形状生成随机点
	 * - 支持圆形、矩形、扇形
	 */
	FVector GenerateRandomPointInArea(const FVector& InCenter, const FRotator& InRotation) const;

	/**
	 * @brief 在圆形区域内生成随机点
	 * @param InCenter 区域中心
	 * @return 随机点位置
	 */
	FVector GenerateRandomPointInCircle(const FVector& InCenter) const;

	/**
	 * @brief 在矩形区域内生成随机点
	 * @param InCenter 区域中心
	 * @param InRotation 区域朝向
	 * @return 随机点位置
	 */
	FVector GenerateRandomPointInRectangle(const FVector& InCenter, const FRotator& InRotation) const;

	/**
	 * @brief 在扇形区域内生成随机点
	 * @param InCenter 区域中心
	 * @param InRotation 区域朝向
	 * @return 随机点位置
	 */
	FVector GenerateRandomPointInSector(const FVector& InCenter, const FRotator& InRotation) const;

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
	void ExecuteGroundImpactGameplayCue(const FVector& ImpactLocation);

public:
	// ========== 蓝图事件 ==========

	UFUNCTION(BlueprintImplementableEvent, Category = "Projectile", meta = (DisplayName = "On Hit Target (BP)"))
	void K2_OnHitTarget(const FSGProjectileHitInfo& HitInfo);

	UFUNCTION(BlueprintImplementableEvent, Category = "Projectile", meta = (DisplayName = "On Projectile Destroyed (BP)"))
	void K2_OnProjectileDestroyed(FVector LastLocation);

	/**
	 * @brief 落地蓝图事件
	 * @param ImpactLocation 落地位置
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Projectile", meta = (DisplayName = "On Ground Impact (BP)"))
	void K2_OnGroundImpact(FVector ImpactLocation);

	// ========== 调试 ==========

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditDefaultsOnly, Category = "Debug", meta = (DisplayName = "显示飞行轨迹"))
	bool bDrawDebugTrajectory = false;

	UPROPERTY(EditDefaultsOnly, Category = "Debug", meta = (DisplayName = "显示地面落点"))
	bool bDrawDebugGroundImpact = false;

	// ✨ 新增 - 显示区域范围
	UPROPERTY(EditDefaultsOnly, Category = "Debug", meta = (DisplayName = "显示区域范围"))
	bool bDrawDebugArea = false;
#endif
};
