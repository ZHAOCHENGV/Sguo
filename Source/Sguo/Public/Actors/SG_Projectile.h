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
 * @details 定义投射物的飞行行为类型
 */
UENUM(BlueprintType)
enum class ESGProjectileFlightMode : uint8
{
    /** 直线飞行 - 直接飞向目标 */
    Linear          UMETA(DisplayName = "直线飞行"),
    
    /** 抛物线飞行 - 带弧度的飞行（物理正确的重力弹道） */
    Parabolic       UMETA(DisplayName = "抛物线飞行"),
    
    /** 归航飞行 - 持续追踪目标 */
    Homing          UMETA(DisplayName = "归航飞行")
};

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
 * @details 包含投射物击中目标时的所有相关信息
 */
USTRUCT(BlueprintType)
struct FSGProjectileHitInfo
{
    GENERATED_BODY()

    /** 被击中的 Actor */
    UPROPERTY(BlueprintReadOnly, Category = "Hit Info", meta = (DisplayName = "击中目标"))
    AActor* HitActor = nullptr;

    /** 击中的世界位置 */
    UPROPERTY(BlueprintReadOnly, Category = "Hit Info", meta = (DisplayName = "击中位置"))
    FVector HitLocation = FVector::ZeroVector;

    /** 击中表面的法线方向 */
    UPROPERTY(BlueprintReadOnly, Category = "Hit Info", meta = (DisplayName = "击中法线"))
    FVector HitNormal = FVector::UpVector;

    /** 击中的骨骼名称（如果是骨骼网格体） */
    UPROPERTY(BlueprintReadOnly, Category = "Hit Info", meta = (DisplayName = "击中骨骼"))
    FName HitBoneName = NAME_None;

    /** 投射物击中时的飞行方向 */
    UPROPERTY(BlueprintReadOnly, Category = "Hit Info", meta = (DisplayName = "飞行方向"))
    FVector ProjectileDirection = FVector::ForwardVector;

    /** 投射物击中时的飞行速度 */
    UPROPERTY(BlueprintReadOnly, Category = "Hit Info", meta = (DisplayName = "飞行速度"))
    float ProjectileSpeed = 0.0f;
};

/** 击中事件委托 - 当投射物击中目标时广播 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSGProjectileHitSignature, const FSGProjectileHitInfo&, HitInfo);

/**
 * @brief 自定义弹道投射物（物理正确版）
 * 
 * @details
 * **功能说明：**
 * - 不使用 ProjectileMovementComponent，采用自定义 Tick 驱动的飞行系统
 * - 支持直线、抛物线、归航三种飞行模式
 * - 抛物线模式使用物理正确的二次曲线公式：h(t) = 4 * ArcHeight * t * (1-t)
 * - 支持多种目标模式：Actor、位置、区域中心、区域随机点
 * - 采用"弹道延展"策略：目标丢失后箭矢自然惯性落地，无突变
 * - 使用胶囊体碰撞，碰撞尺寸直接在组件上配置
 * 
 * **物理模型：**
 * - 抛物线轨迹遵循标准重力公式的归一化形式
 * - 当飞行进度 t > 1.0 时，t(1-t) 变为负数，产生自然下落效果
 * - 水平速度分量在忽略空气阻力时保持恒定
 * 
 * **使用方式：**
 * 1. 创建投射物蓝图继承此类
 * 2. 在 CollisionCapsule 组件上配置碰撞尺寸
 * 3. 配置飞行参数和目标参数
 * 4. 调用 InitializeProjectile 系列函数初始化
 * 
 * **注意事项：**
 * - 碰撞半径和半高不再作为单独属性暴露，直接在组件详情面板配置
 * - 投射物生成后会延迟启用碰撞，防止在友方建筑内部立即碰撞
 * - FlightProgress 不再钳位，允许超过 1.0 以实现弹道延展
 */
UCLASS()
class SGUO_API ASG_Projectile : public AActor, public IGameplayCueInterface
{
    GENERATED_BODY()
    
public:    
    /**
     * @brief 构造函数
     * @details 创建并配置所有组件，设置默认碰撞响应
     */
    ASG_Projectile();

    // ==================== 蓝图事件委托 ====================

    /** 击中目标事件 - 当投射物击中有效目标时广播 */
    UPROPERTY(BlueprintAssignable, Category = "Projectile Events", meta = (DisplayName = "击中目标事件"))
    FSGProjectileHitSignature OnProjectileHitTarget;

    /** 投射物销毁事件 - 当投射物被销毁时广播 */
    UPROPERTY(BlueprintAssignable, Category = "Projectile Events", meta = (DisplayName = "投射物销毁事件"))
    FSGProjectileHitSignature OnProjectileDestroyed;

    /** 落地事件 - 当投射物落地时广播（未命中目标的情况） */
    UPROPERTY(BlueprintAssignable, Category = "Projectile Events", meta = (DisplayName = "落地事件"))
    FSGProjectileHitSignature OnProjectileGroundImpact;

    // ==================== 组件 ====================

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
     * - 碰撞尺寸（半径和半高）直接在此组件的详情面板中配置
     * - 适合箭矢等细长投射物
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "碰撞胶囊体"))
    TObjectPtr<UCapsuleComponent> CollisionCapsule;

    /**
     * @brief 网格体组件
     * @details 用于显示投射物的视觉效果
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "网格体"))
    TObjectPtr<UStaticMeshComponent> MeshComponent;

public:
    // ==================== 飞行配置 ====================

    /**
     * @brief 飞行模式
     * @details 决定投射物的飞行轨迹类型
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Flight Config", meta = (DisplayName = "飞行模式"))
    ESGProjectileFlightMode FlightMode = ESGProjectileFlightMode::Parabolic;

    /**
     * @brief 飞行速度（厘米/秒）
     * @details 
     * 功能说明：
     * - 投射物沿弹道曲线的移动速度
     * - 在抛物线模式下，这是沿曲线的弧长速度，而非水平速度
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Flight Config", meta = (DisplayName = "飞行速度", ClampMin = "100.0", UIMin = "100.0", UIMax = "10000.0"))
    float FlightSpeed = 3000.0f;

    /**
     * @brief 抛物线弧度高度（厘米）
     * @details 
     * 功能说明：
     * - 抛物线最高点相对于起点-终点连线的高度
     * - 使用物理公式：h(t) = 4 * ArcHeight * t * (1-t)
     * - 0 = 直线飞行
     * - 100 = 轻微弧度
     * - 300 = 中等弧度
     * - 500+ = 高抛
     * 
     * 物理说明：
     * - 当 t = 0.5 时达到最高点，高度为 ArcHeight
     * - 当 t > 1.0 时，高度偏移变为负数，实现自然下落
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Flight Config", meta = (DisplayName = "弧度高度", ClampMin = "0.0", UIMin = "0.0", UIMax = "1000.0", EditCondition = "FlightMode == ESGProjectileFlightMode::Parabolic", EditConditionHides))
    float ArcHeight = 200.0f;

    /**
     * @brief 归航强度（仅归航模式）
     * @details 
     * 功能说明：
     * - 每秒转向角度（度）
     * - 数值越大追踪越灵敏
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Flight Config", meta = (DisplayName = "归航强度", ClampMin = "0.0", UIMin = "0.0", UIMax = "720.0", EditCondition = "FlightMode == ESGProjectileFlightMode::Homing", EditConditionHides))
    float HomingStrength = 180.0f;

    /**
     * @brief 生存时间（秒）
     * @details 投射物超过此时间后自动销毁
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Flight Config", meta = (DisplayName = "生存时间", ClampMin = "0.1", UIMin = "0.1", UIMax = "30.0"))
    float LifeSpan = 10.0f;

    /**
     * @brief 是否穿透
     * @details 启用后投射物可以穿透目标继续飞行
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Flight Config", meta = (DisplayName = "是否穿透"))
    bool bPenetrate = false;

    /**
     * @brief 最大穿透数量
     * @details 穿透模式下最多可穿透的目标数量，0 表示无限制
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Flight Config", meta = (DisplayName = "最大穿透数量", EditCondition = "bPenetrate", EditConditionHides, ClampMin = "0", UIMin = "0", UIMax = "10"))
    int32 MaxPenetrateCount = 0;

    // ==================== 目标配置 ====================

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

    // ==================== 区域配置 ====================

    /**
     * @brief 区域形状
     * @details 用于区域随机点模式下生成随机点的区域形状
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Area Config", meta = (DisplayName = "区域形状", EditCondition = "TargetMode == ESGProjectileTargetMode::AreaCenter || TargetMode == ESGProjectileTargetMode::AreaRandom || TargetMode == ESGProjectileTargetMode::TargetAreaRandom", EditConditionHides))
    ESGProjectileAreaShape AreaShape = ESGProjectileAreaShape::Circle;

    /**
     * @brief 区域半径（圆形/扇形）
     * @details 用于圆形和扇形区域的外半径
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

    // ==================== 碰撞配置 ====================

    /**
     * @brief 碰撞体相对旋转
     * @details 
     * 用于调整碰撞体方向，使其与网格体对齐
     * 
     * 注意事项：
     * - 胶囊体的半径和半高请直接在 CollisionCapsule 组件上配置
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Collision Config", meta = (DisplayName = "碰撞体旋转偏移"))
    FRotator CollisionRotationOffset = FRotator(90.0f, 0.0f, 0.0f);

    /**
     * @brief 碰撞启用延迟时间（秒）
     * @details
     * 功能说明：
     * - 投射物生成后，延迟多久启用碰撞检测
     * - 用于防止投射物在友方建筑内部生成时立即碰撞
     * - 默认 0.1 秒，足够投射物飞出建筑
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Collision Config", meta = (DisplayName = "碰撞启用延迟", ClampMin = "0.0", UIMin = "0.0", UIMax = "1.0"))
    float CollisionEnableDelay = 0.1f;

    // ==================== 伤害配置 ====================

    /**
     * @brief 伤害倍率
     * @details 应用于基础伤害的倍率
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Damage Config", meta = (DisplayName = "伤害倍率", ClampMin = "0.0", UIMin = "0.0", UIMax = "5.0"))
    float DamageMultiplier = 1.0f;

    /**
     * @brief 伤害效果类
     * @details 用于应用伤害的 GameplayEffect 类
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage Config", meta = (DisplayName = "伤害效果"))
    TSubclassOf<UGameplayEffect> DamageEffectClass;

    // ==================== GameplayCue 配置 ====================

    /**
     * @brief 击中 GameplayCue
     * @details 投射物击中目标时执行的视觉/音效特效
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "GameplayCue", meta = (DisplayName = "击中 GameplayCue", Categories = "GameplayCue"))
    FGameplayTag HitGameplayCueTag;

    /**
     * @brief 飞行 GameplayCue
     * @details 投射物飞行时的拖尾特效
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "GameplayCue", meta = (DisplayName = "飞行 GameplayCue", Categories = "GameplayCue"))
    FGameplayTag TrailGameplayCueTag;

    /**
     * @brief 销毁 GameplayCue
     * @details 投射物销毁时执行的特效
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "GameplayCue", meta = (DisplayName = "销毁 GameplayCue", Categories = "GameplayCue"))
    FGameplayTag DestroyGameplayCueTag;

    /**
     * @brief 落地 GameplayCue
     * @details 投射物落地时触发的特效（未命中目标时）
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "GameplayCue", meta = (DisplayName = "落地 GameplayCue", Categories = "GameplayCue"))
    FGameplayTag GroundImpactGameplayCueTag;

    // ==================== 运行时数据 ====================

    /**
     * @brief 攻击者的能力系统组件
     * @details 用于应用伤害和执行 GameplayCue
     */
    UPROPERTY(Transient, BlueprintReadOnly, Category = "Runtime", meta = (DisplayName = "攻击者ASC"))
    TObjectPtr<UAbilitySystemComponent> InstigatorASC;

    /**
     * @brief 攻击者的阵营标签
     * @details 用于判断友方和敌方
     */
    UPROPERTY(Transient, BlueprintReadOnly, Category = "Runtime", meta = (DisplayName = "攻击者阵营"))
    FGameplayTag InstigatorFactionTag;

    /**
   * @brief 已击中的 Actor 列表
   * @details 
   * 功能说明：
   * - 用于确保每个目标只受到一次伤害
   * - 穿透模式下记录所有已击中的目标
   * - 非穿透模式下通常只有一个元素
   * 
   * 注意事项：
   * - 初始化时会自动清空
   * - 不要手动修改此数组
   */
    UPROPERTY(Transient, BlueprintReadOnly, Category = "Runtime", meta = (DisplayName = "已击中目标列表"))
    TArray<TObjectPtr<AActor>> HitActors;

    /**
     * @brief 当前目标
     * @details 用于归航和抛物线模式的目标追踪
     */
    UPROPERTY(Transient, BlueprintReadOnly, Category = "Runtime", meta = (DisplayName = "当前目标"))
    TWeakObjectPtr<AActor> CurrentTarget;

protected:
    // ==================== 飞行状态（内部使用） ====================

    /** 起始位置 - 投射物发射时的世界坐标 */
    FVector StartLocation;

    /** 
     * @brief 目标位置（初始目标点）
     * @details 
     * - 对于 TargetActor 模式：目标 Actor 的中心（可能带偏移）
     * - 对于其他模式：指定的世界坐标位置
     * - 此位置在目标丢失后会被锁定，不再更新
     */
    FVector TargetLocation;

    /** 
     * @brief 地面高度（Z 坐标）
     * @details 用于计算弹道延展时的最终落点高度
     */
    float GroundZ;

    /** 区域中心位置 */
    FVector AreaCenterLocation;

    /** 区域朝向（用于扇形和矩形） */
    FRotator AreaRotation;

    /** 
     * @brief 目标是否已丢失（死亡或消失）
     * @details 
     * - 当目标丢失时，锁定最后的目标位置
     * - 停止归航微调，让投射物遵循惯性落地
     */
    bool bTargetLost = false;

    /** 
     * @brief 飞行进度
     * @details 
     * - 范围：0.0 到无穷大（不再钳位到 [0,1]）
     * - 0.0 = 起点
     * - 1.0 = 原目标点
     * - >1.0 = 弹道延展阶段（自然下落）
     */
    float FlightProgress = 0.0f;

    /** 总飞行距离（起点到目标点的直线距离） */
    float TotalFlightDistance = 0.0f;

    /** 当前速度向量 */
    FVector CurrentVelocity;

    /** 是否已初始化 */
    bool bIsInitialized = false;

    /** 飞行 GC 是否已激活 */
    bool bTrailCueActive = false;

    /** 是否已落地 */
    bool bHasLanded = false;

    /** 碰撞启用定时器句柄 */
    FTimerHandle CollisionEnableTimerHandle;

protected:
    /**
     * @brief BeginPlay 生命周期函数
     * @details 初始化投射物配置，设置碰撞延迟
     */
    virtual void BeginPlay() override;

    /**
     * @brief EndPlay 生命周期函数
     * @param EndPlayReason 结束原因
     * @details 清理定时器，执行销毁特效
     */
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:    
    /**
     * @brief Tick 函数
     * @param DeltaTime 帧间隔时间
     * @details 更新投射物飞行位置和旋转
     */
    virtual void Tick(float DeltaTime) override;

    // ==================== 初始化接口 ====================

    /**
     * @brief 初始化投射物（目标为 Actor）
     * @param InInstigatorASC 攻击者 ASC
     * @param InFactionTag 攻击者阵营
     * @param InTarget 目标 Actor
     * @param InArcHeight 弧度高度（覆盖默认值，-1 表示使用默认）
     * 
     * @details 
     * **功能说明：**
     * - TargetMode 为 TargetActor 时：飞向目标中心
     * - TargetMode 为 TargetAreaRandom 时：飞向目标周围随机点
     * 
     * **详细流程：**
     * 1. 保存攻击者信息
     * 2. 记录起始位置
     * 3. 根据目标模式计算目标位置
     * 4. 计算地面高度（用于弹道延展）
     * 5. 初始化飞行参数
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
     * 
     * @details 
     * **功能说明：**
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

    /**
     * @brief 初始化投射物（目标为区域）
     * @param InInstigatorASC 攻击者 ASC
     * @param InFactionTag 攻击者阵营
     * @param InAreaCenter 区域中心位置
     * @param InAreaRotation 区域朝向
     * @param InArcHeight 弧度高度（覆盖默认值，-1 表示使用默认）
     * 
     * @details 
     * **功能说明：**
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

    // ==================== 运行时设置接口 ====================

    /**
     * @brief 设置飞行速度（运行时）
     * @param NewSpeed 新的飞行速度（最小 100）
     */
    UFUNCTION(BlueprintCallable, Category = "Projectile", meta = (DisplayName = "设置飞行速度"))
    void SetFlightSpeed(float NewSpeed);

    /**
     * @brief 设置目标位置偏移（运行时）
     * @param NewOffset 新的偏移向量
     * @param bWorldSpace 是否使用世界空间
     */
    UFUNCTION(BlueprintCallable, Category = "Projectile", meta = (DisplayName = "设置目标位置偏移"))
    void SetTargetLocationOffset(FVector NewOffset, bool bWorldSpace = true);

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

    // ==================== 查询接口 ====================

    /**
     * @brief 获取当前速度向量
     * @return 当前速度向量
     */
    UFUNCTION(BlueprintPure, Category = "Projectile", meta = (DisplayName = "获取当前速度"))
    FVector GetCurrentVelocity() const { return CurrentVelocity; }

    /**
     * @brief 获取当前目标位置
     * @return 目标位置
     */
    UFUNCTION(BlueprintPure, Category = "Projectile", meta = (DisplayName = "获取目标位置"))
    FVector GetProjectileTargetLocation() const { return TargetLocation; }

    /**
     * @brief 获取地面高度
     * @return 地面 Z 坐标
     */
    UFUNCTION(BlueprintPure, Category = "Projectile", meta = (DisplayName = "获取地面高度"))
    float GetGroundZ() const { return GroundZ; }

    /**
     * @brief 获取区域中心位置
     * @return 区域中心位置
     */
    UFUNCTION(BlueprintPure, Category = "Projectile", meta = (DisplayName = "获取区域中心位置"))
    FVector GetAreaCenterLocation() const { return AreaCenterLocation; }

    /**
     * @brief 获取碰撞胶囊体的半径
     * @return 胶囊体半径，如果组件无效返回 0
     * 
     * @details 
     * 功能说明：
     * - 直接从 CollisionCapsule 组件读取缩放后的实际半径
     * - 用于需要知道碰撞范围的逻辑
     */
    UFUNCTION(BlueprintPure, Category = "Projectile", meta = (DisplayName = "获取碰撞半径"))
    float GetCapsuleRadius() const;

    /**
     * @brief 获取碰撞胶囊体的半高
     * @return 胶囊体半高，如果组件无效返回 0
     * 
     * @details 
     * 功能说明：
     * - 直接从 CollisionCapsule 组件读取缩放后的实际半高
     * - 用于需要知道碰撞范围的逻辑
     */
    UFUNCTION(BlueprintPure, Category = "Projectile", meta = (DisplayName = "获取碰撞半高"))
    float GetCapsuleHalfHeight() const;

    /**
     * @brief 获取当前飞行进度
     * @return 飞行进度（0.0 = 起点，1.0 = 目标点，>1.0 = 延展阶段）
     */
    UFUNCTION(BlueprintPure, Category = "Projectile", meta = (DisplayName = "获取飞行进度"))
    float GetFlightProgress() const { return FlightProgress; }

protected:
    // ==================== 飞行逻辑（内部使用） ====================

    /**
     * @brief 更新直线飞行
     * @param DeltaTime 帧间隔时间
     */
    void UpdateLinearFlight(float DeltaTime);

    /**
     * @brief 更新抛物线飞行（物理正确版）
     * @param DeltaTime 帧间隔时间
     * 
     * @details 
     * **物理模型：**
     * - 使用二次曲线公式：h(t) = 4 * ArcHeight * t * (1-t)
     * - 当 t > 1.0 时自然延展，高度偏移变为负数
     * - 投射物会继续沿弹道飞行直到撞击地面
     * 
     * **目标跟踪：**
     * - 目标存活时：动态更新目标位置
     * - 目标丢失后：锁定最后位置，进入惯性落地模式
     */
    void UpdateParabolicFlight(float DeltaTime);

    /**
     * @brief 更新归航飞行
     * @param DeltaTime 帧间隔时间
     */
    void UpdateHomingFlight(float DeltaTime);

    /**
     * @brief 计算抛物线位置（物理正确的二次曲线）
     * @param Progress 飞行进度（可以超过 1.0）
     * @return 当前应处于的世界位置
     * 
     * @details 
     * **物理公式：**
     * $$Position = Lerp(Start, Target, t) + (0, 0, 4 \cdot ArcHeight \cdot t \cdot (1-t))$$
     * 
     * **特性：**
     * - t = 0: 起点
     * - t = 0.5: 最高点，高度 = ArcHeight
     * - t = 1.0: 目标点（高度偏移 = 0）
     * - t > 1.0: 延展阶段，高度偏移为负数，自然下落
     */
    FVector CalculateParabolicPosition(float Progress) const;

    /**
     * @brief 更新旋转（朝向速度方向）
     */
    void UpdateRotation();

    // ==================== 目标位置计算（内部使用） ====================

    /**
     * @brief 计算目标位置（应用偏移）
     * @param InTarget 目标 Actor
     * @return 计算后的目标位置
     */
    FVector CalculateTargetLocation(AActor* InTarget) const;

    /**
     * @brief 计算地面高度
     * @param InLocation 参考位置
     * @return 地面 Z 坐标
     * 
     * @details 
     * 从参考位置向下进行射线检测，找到地面高度
     */
    float CalculateGroundZ(const FVector& InLocation) const;

    /**
     * @brief 检查目标是否仍然有效
     * @return 目标是否有效
     */
    bool IsTargetValid() const;

    /**
     * @brief 处理目标丢失
     * 
     * @details 
     * **弹道延展策略：**
     * - 不重新计算路径，保持当前弹道
     * - 锁定最后的目标位置
     * - 让投射物自然惯性落地
     */
    void HandleTargetLost();

    /**
     * @brief 处理投射物落地
     */
    void HandleGroundImpact();

    // ==================== 区域随机点计算（内部使用） ====================

    /**
     * @brief 在区域内生成随机点
     * @param InCenter 区域中心
     * @param InRotation 区域朝向
     * @return 随机点位置（世界坐标）
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

    // ==================== 碰撞处理（内部使用） ====================

    /**
     * @brief 胶囊体 Overlap 事件回调
     */
    UFUNCTION()
    void OnCapsuleOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult
    );

    /**
     * @brief 胶囊体 Hit 事件回调
     */
    UFUNCTION()
    void OnCapsuleHit(
        UPrimitiveComponent* HitComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        FVector NormalImpulse,
        const FHitResult& Hit
    );

    /**
     * @brief 处理投射物碰撞
     * @param OtherActor 碰撞的 Actor
     * @param Hit 碰撞结果
     */
    void HandleProjectileImpact(AActor* OtherActor, const FHitResult& Hit);

    /**
     * @brief 对目标应用伤害
     * @param Target 目标 Actor
     */
    void ApplyDamageToTarget(AActor* Target);

    /**
     * @brief 启用碰撞的回调函数
     * @details 延迟启用碰撞，防止在友方建筑内部生成时立即碰撞
     */
    UFUNCTION()
    void EnableCollision();

    // ==================== GameplayCue（内部使用） ====================

    /** 执行击中 GameplayCue */
    void ExecuteHitGameplayCue(const FSGProjectileHitInfo& HitInfo);

    /** 激活飞行 GameplayCue */
    void ActivateTrailGameplayCue();

    /** 移除飞行 GameplayCue */
    void RemoveTrailGameplayCue();

    /** 执行销毁 GameplayCue */
    void ExecuteDestroyGameplayCue();

    /** 执行落地 GameplayCue */
    void ExecuteGroundImpactGameplayCue(const FVector& ImpactLocation);

public:
    // ==================== 蓝图事件 ====================

    /**
     * @brief 击中目标蓝图事件
     * @param HitInfo 击中信息
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "Projectile", meta = (DisplayName = "On Hit Target (BP)"))
    void K2_OnHitTarget(const FSGProjectileHitInfo& HitInfo);

    /**
     * @brief 投射物销毁蓝图事件
     * @param LastLocation 销毁时的位置
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "Projectile", meta = (DisplayName = "On Projectile Destroyed (BP)"))
    void K2_OnProjectileDestroyed(FVector LastLocation);

    /**
     * @brief 落地蓝图事件
     * @param ImpactLocation 落地位置
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "Projectile", meta = (DisplayName = "On Ground Impact (BP)"))
    void K2_OnGroundImpact(FVector ImpactLocation);

    // ==================== 调试配置 ====================

#if WITH_EDITORONLY_DATA
    /** 是否绘制飞行轨迹调试线 */
    UPROPERTY(EditDefaultsOnly, Category = "Debug", meta = (DisplayName = "显示飞行轨迹"))
    bool bDrawDebugTrajectory = false;

    /** 是否绘制目标点调试球 */
    UPROPERTY(EditDefaultsOnly, Category = "Debug", meta = (DisplayName = "显示目标点"))
    bool bDrawDebugTargetPoint = false;

    /** 是否绘制区域范围调试图形 */
    UPROPERTY(EditDefaultsOnly, Category = "Debug", meta = (DisplayName = "显示区域范围"))
    bool bDrawDebugArea = false;
#endif


    public:
    // ==================== 命中后配置 ====================

    /**
     * @brief 命中后延迟销毁时间（秒）
     * @details 
     * 功能说明：
     * - 投射物命中目标后，延迟多久销毁
     * - 用于播放命中特效、音效等
     * - 0 表示立即销毁
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Hit Config", meta = (DisplayName = "命中后销毁延迟", ClampMin = "0.0", UIMin = "0.0", UIMax = "5.0"))
    float HitDestroyDelay = 0.5f;

    /**
     * @brief 命中后是否附着到目标
     * @details 
     * 功能说明：
     * - 启用后，投射物命中目标会附着在目标身上
     * - 适用于箭矢插入敌人身体的效果
     * - 禁用时，投射物会在原地停止
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Hit Config", meta = (DisplayName = "命中后附着目标"))
    bool bAttachToTargetOnHit = false;

    /**
     * @brief 附着时的骨骼名称
     * @details 
     * 功能说明：
     * - 当 bAttachToTargetOnHit 为 true 时使用
     * - 如果为空，则附着到击中的骨骼（如果有）或根组件
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Hit Config", meta = (DisplayName = "附着骨骼名称", EditCondition = "bAttachToTargetOnHit", EditConditionHides))
    FName AttachBoneName = NAME_None;

    /**
     * @brief 落地后延迟销毁时间（秒）
     * @details 投射物落地后（未命中目标），延迟多久销毁
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Hit Config", meta = (DisplayName = "落地后销毁延迟", ClampMin = "0.0", UIMin = "0.0", UIMax = "10.0"))
    float GroundImpactDestroyDelay = 3.0f;

protected:
    // ==================== 命中状态（内部使用） ====================

    /** 
     * @brief 是否已命中目标
     * @details 命中后停止移动，等待销毁
     */
    bool bHasHitTarget = false;

    /** 
     * @brief 命中销毁定时器句柄
     */
    FTimerHandle HitDestroyTimerHandle;

protected:
    // ==================== 命中处理函数（内部使用） ====================

    /**
     * @brief 处理命中目标后的逻辑
     * @param HitActor 被击中的 Actor
     * @param HitInfo 击中信息
     * 
     * @details 
     * **功能说明：**
     * - 停止投射物移动
     * - 隐藏网格体（可选）
     * - 处理附着逻辑
     * - 设置延迟销毁
     * - 触发蓝图事件
     */
    void HandleHitTarget(AActor* HitActor, const FSGProjectileHitInfo& HitInfo);

    /**
     * @brief 命中后延迟销毁回调
     */
    UFUNCTION()
    void OnHitDestroyTimerExpired();

public:
    // ==================== 蓝图事件（命中相关） ====================

    /**
     * @brief 命中目标后蓝图事件（在停止移动后调用）
     * @param HitInfo 击中信息
     * 
     * @details 
     * **调用时机：**
     * - 在投射物停止移动、隐藏网格体之后调用
     * - 可用于播放额外的命中特效、生成贴花等
     * 
     * **注意事项：**
     * - 此时投射物仍然存在，但已停止移动
     * - 可以访问 HitInfo 获取命中位置、目标等信息
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "Projectile Events", meta = (DisplayName = "On After Hit Target (BP)"))
    void K2_OnAfterHitTarget(const FSGProjectileHitInfo& HitInfo);

    /**
     * @brief 命中后即将销毁蓝图事件
     * 
     * @details 
     * **调用时机：**
     * - 在命中延迟销毁定时器到期后、实际销毁前调用
     * - 可用于清理资源、播放淡出效果等
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "Projectile Events", meta = (DisplayName = "On Before Destroy After Hit (BP)"))
    void K2_OnBeforeDestroyAfterHit();

    // ==================== 查询接口（命中相关） ====================

    /**
     * @brief 检查投射物是否已命中目标
     * @return 是否已命中
     */
    UFUNCTION(BlueprintPure, Category = "Projectile", meta = (DisplayName = "是否已命中目标"))
    bool HasHitTarget() const { return bHasHitTarget; }

    /**
     * @brief 手动隐藏投射物网格体
     * @details 蓝图可调用，用于自定义隐藏时机
     */
    UFUNCTION(BlueprintCallable, Category = "Projectile", meta = (DisplayName = "隐藏网格体"))
    void HideProjectileMesh();

    /**
     * @brief 手动显示投射物网格体
     * @details 蓝图可调用，用于自定义显示时机
     */
    UFUNCTION(BlueprintCallable, Category = "Projectile", meta = (DisplayName = "显示网格体"))
    void ShowProjectileMesh();


};
