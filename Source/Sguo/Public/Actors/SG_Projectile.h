// ✨ 新增 - 投射物Actor
// Copyright notice placeholder
/**
 * @file SG_Projectile.h
 * @brief 投射物Actor（箭矢、飞刀等）
 * @details
 * 功能说明：
 * - 远程攻击的投射物基类
 * - 支持直线飞行和抛物线飞行
 * - 碰撞时应用伤害
 * 详细流程：
 * 1. 生成时设置飞行参数
 * 2. Tick 中更新位置（直线或抛物线）
 * 3. 碰撞时应用伤害到目标
 * 4. 击中后销毁或穿透
 * 注意事项：
 * - 需要设置碰撞通道
 * - 需要设置伤害 GameplayEffect
 */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "SG_Projectile.generated.h"

// 前置声明
class USphereComponent;
class UProjectileMovementComponent;
class UStaticMeshComponent;
class UGameplayEffect;
class UAbilitySystemComponent;

/**
 * @brief 投射物飞行类型
 * @details
 * 功能说明：
 * - 定义投射物的飞行轨迹
 */
UENUM(BlueprintType)
enum class ESGProjectileType : uint8
{
	// 直线飞行（弩箭）
	Linear      UMETA(DisplayName = "直线飞行"),
	
	// 抛物线飞行（弓箭）
	Parabolic   UMETA(DisplayName = "抛物线飞行")
};

/**
 * @brief 投射物Actor
 * @details
 * 功能说明：
 * - 远程攻击的投射物
 * - 支持直线和抛物线飞行
 * - 碰撞时应用伤害
 * 使用方式：
 * 1. 创建子类继承此基类
 * 2. 设置飞行类型和速度
 * 3. 配置伤害 GameplayEffect
 * 4. 设置碰撞通道
 */
UCLASS()
class SGUO_API ASG_Projectile : public AActor
{
	GENERATED_BODY()
	
public:	
	/**
	 * @brief 构造函数
	 * @details
	 * 功能说明：
	 * - 创建组件
	 * - 设置默认值
	 */
	ASG_Projectile();

protected:
	// ========== 组件 ==========
	
	/**
	 * @brief 碰撞组件
	 * @details
	 * 功能说明：
	 * - 用于检测碰撞
	 * - 触发 OnComponentHit 事件
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "碰撞体"))
	TObjectPtr<USphereComponent> CollisionComponent;

	/**
	 * @brief 投射物移动组件
	 * @details
	 * 功能说明：
	 * - 管理投射物的移动
	 * - 支持重力和速度衰减
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "移动组件"))
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	/**
	 * @brief 网格体组件
	 * @details
	 * 功能说明：
	 * - 投射物的视觉表现
	 * - 箭矢、飞刀等模型
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "网格体"))
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	// ========== 投射物配置 ==========

	/**
	 * @brief 飞行类型
	 * @details
	 * 功能说明：
	 * - 直线：弩箭（不受重力影响）
	 * - 抛物线：弓箭（受重力影响）
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile Config", meta = (DisplayName = "飞行类型"))
	ESGProjectileType ProjectileType = ESGProjectileType::Linear;

	/**
	 * @brief 飞行速度
	 * @details
	 * 功能说明：
	 * - 投射物的初始速度（厘米/秒）
	 * - 弩箭：3000 - 5000
	 * - 弓箭：2000 - 3000
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Projectile Config", meta = (DisplayName = "飞行速度", ClampMin = "100.0", UIMin = "100.0", UIMax = "10000.0"))
	float ProjectileSpeed = 3000.0f;

	/**
	 * @brief 重力缩放
	 * @details
	 * 功能说明：
	 * - 抛物线飞行时的重力影响程度
	 * - 0.0：无重力（直线飞行）
	 * - 1.0：正常重力
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Projectile Config", meta = (DisplayName = "重力缩放", ClampMin = "0.0", UIMin = "0.0", UIMax = "2.0"))
	float GravityScale = 0.0f;

	/**
	 * @brief 生存时间
	 * @details
	 * 功能说明：
	 * - 投射物最大飞行时间（秒）
	 * - 超时后自动销毁
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Projectile Config", meta = (DisplayName = "生存时间(秒)", ClampMin = "0.1", UIMin = "0.1", UIMax = "10.0"))
	float LifeSpan = 5.0f;

	/**
	 * @brief 是否穿透
	 * @details
	 * 功能说明：
	 * - True：击中后不销毁，继续飞行（穿透攻击）
	 * - False：击中后销毁
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Projectile Config", meta = (DisplayName = "是否穿透"))
	bool bPenetrate = false;

	/**
	 * @brief 最大穿透数量
	 * @details
	 * 功能说明：
	 * - 穿透模式下，最多穿透的目标数量
	 * - 0：无限穿透
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Projectile Config", meta = (DisplayName = "最大穿透数量", EditCondition = "bPenetrate", EditConditionHides, ClampMin = "0", UIMin = "0", UIMax = "10"))
	int32 MaxPenetrateCount = 0;

	// ========== 伤害配置 ==========

	/**
	 * @brief 伤害倍率
	 * @details
	 * 功能说明：
	 * - 基于攻击者攻击力的伤害倍率
	 * - 最终伤害 = 攻击力 * 伤害倍率
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Damage Config", meta = (DisplayName = "伤害倍率", ClampMin = "0.0", UIMin = "0.0", UIMax = "5.0"))
	float DamageMultiplier = 1.0f;

	/**
	 * @brief 伤害 GameplayEffect 类
	 * @details
	 * 功能说明：
	 * - 用于应用伤害的 GE
	 * - 需要使用 SG_DamageExecutionCalc 作为 Execution
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage Config", meta = (DisplayName = "伤害效果"))
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	// ========== 运行时数据 ==========

	/**
	 * @brief 攻击者 ASC
	 * @details
	 * 功能说明：
	 * - 生成投射物时设置
	 * - 用于应用伤害
	 */
	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> InstigatorASC;

	/**
	 * @brief 攻击者阵营标签
	 * @details
	 * 功能说明：
	 * - 用于判断是否是敌方单位
	 */
	UPROPERTY(Transient)
	FGameplayTag InstigatorFactionTag;

	/**
	 * @brief 已穿透的目标列表
	 * @details
	 * 功能说明：
	 * - 防止重复伤害同一目标
	 */
	UPROPERTY(Transient)
	TArray<AActor*> HitActors;

protected:
	// ========== 生命周期 ==========

	/**
	 * @brief BeginPlay
	 * @details
	 * 功能说明：
	 * - 设置生存时间
	 * - 配置投射物移动组件
	 */
	virtual void BeginPlay() override;

public:	
	// ========== 初始化 ==========

	/**
	 * @brief 初始化投射物
	 * @param InInstigatorASC 攻击者的 AbilitySystemComponent
	 * @param InFactionTag 攻击者的阵营标签
	 * @param InDirection 飞行方向
	 * @details
	 * 功能说明：
	 * - 设置攻击者信息
	 * - 设置飞行方向和速度
	 * 详细流程：
	 * 1. 保存攻击者 ASC 和阵营标签
	 * 2. 设置投射物朝向
	 * 3. 设置飞行速度
	 * 4. 根据飞行类型配置重力
	 */
	UFUNCTION(BlueprintCallable, Category = "Projectile")
	void InitializeProjectile(
		UAbilitySystemComponent* InInstigatorASC,
		FGameplayTag InFactionTag,
		FVector InDirection
	);

protected:
	// ========== 碰撞处理 ==========

	/**
	 * @brief 碰撞事件
	 * @param HitComponent 碰撞组件
	 * @param OtherActor 碰撞的 Actor
	 * @param OtherComp 碰撞的组件
	 * @param NormalImpulse 碰撞冲量
	 * @param Hit 碰撞结果
	 * @details
	 * 功能说明：
	 * - 检测碰撞
	 * - 应用伤害
	 * - 销毁或穿透
	 * 详细流程：
	 * 1. 检查是否是敌方单位
	 * 2. 检查是否已经击中过
	 * 3. 应用伤害
	 * 4. 记录已击中目标
	 * 5. 如果不穿透，销毁投射物
	 */
	UFUNCTION()
	void OnProjectileHit(
		UPrimitiveComponent* HitComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		FVector NormalImpulse,
		const FHitResult& Hit
	);

	/**
	 * @brief 应用伤害到目标
	 * @param Target 目标 Actor
	 * @details
	 * 功能说明：
	 * - 使用 GameplayEffect 对目标应用伤害
	 * - 设置伤害倍率（SetByCaller）
	 */
	void ApplyDamageToTarget(AActor* Target);

	/**
	 * @brief 蓝图事件：击中目标
	 * @param HitTarget 击中的目标
	 * @details
	 * 功能说明：
	 * - 蓝图可以监听此事件
	 * - 用于播放击中特效、音效等
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Projectile", meta = (DisplayName = "击中目标"))
	void OnHitTarget(AActor* HitTarget);
};
