/**
 * @file SG_UnitsBase.h
 * @brief 角色基类
 */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagContainer.h"
#include "SG_UnitsBase.generated.h"

// 前置声明
class USG_AbilitySystemComponent;
class USG_AttributeSet;
class UGameplayAbility;
class UAnimMontage;
struct FOnAttributeChangeData;
struct FSGUnitDataRow;
// ✨ 新增 - 单位死亡委托声明
/**
 * @brief 单位死亡委托
 * @details 当单位死亡时广播，供前线管理器等系统监听
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSGUnitDeathSignature, ASG_UnitsBase*, DeadUnit);
/**
 * @brief 角色基类
 */
UCLASS()
class SGUO_API ASG_UnitsBase : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	// 构造函数
	ASG_UnitsBase();
	
	// ✨ 新增 - 单位死亡事件
	/**
	 * @brief 单位死亡事件
	 * @details 当单位死亡时广播此事件
	 */
	UPROPERTY(BlueprintAssignable, Category = "Unit Events")
	FSGUnitDeathSignature OnUnitDeathEvent;
	// ========== GAS 组件 ==========
	
	// Ability System Component
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	USG_AbilitySystemComponent* AbilitySystemComponent;
	
	// Attribute Set
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	USG_AttributeSet* AttributeSet;

	// ========== 角色信息 ==========
	
	// 阵营标签（玩家/敌人）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Info", meta = (Categories = "Unit.Faction"))
	FGameplayTag FactionTag;
	
	// 单位类型标签（步兵/骑兵等）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Info", meta = (Categories = "Unit.Type"))
	FGameplayTag UnitTypeTag;
	
	// 当前攻击目标
	UPROPERTY(BlueprintReadWrite, Category = "Combat")
	AActor* CurrentTarget;

	// ========== 基础属性配置 ==========
	
	// 基础生命值
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Attributes")
	float BaseHealth = 500.0f;
	
	// 基础攻击力
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Attributes")
	float BaseAttackDamage = 50.0f;
	
	// 基础移动速度
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Attributes")
	float BaseMoveSpeed = 400.0f;
	
	// 基础攻击速度（每秒攻击次数）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Attributes")
	float BaseAttackSpeed = 1.0f;
	
	// 基础攻击范围
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Attributes")
	float BaseAttackRange = 150.0f;

	// ========== ✨ 新增 - DataTable 配置 ==========
	
	/**
	 * @brief 单位数据表引用
	 * @details 存储所有单位的属性配置（生命值、攻击力、移动速度等）
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unit Config", meta = (DisplayName = "单位数据表"))
	TObjectPtr<UDataTable> UnitDataTable;
	
	/**
	 * @brief 单位数据表行名称
	 * @details 指定在 DataTable 中使用哪一行的数据来初始化此单位
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unit Config", meta = (DisplayName = "数据表行名称"))
	FName UnitDataRowName;
	
	/**
	 * @brief 是否从 DataTable 加载配置
	 * @details 如果为 true，将从 DataTable 读取配置覆盖基础属性
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unit Config", meta = (DisplayName = "使用数据表配置"))
	bool bUseDataTable = false;

	// ========== ✨ 新增 - 攻击配置 ==========
	
	/**
	 * @brief 攻击动画蒙太奇
	 * @details 播放攻击动画时使用的蒙太奇资源
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack Config", meta = (DisplayName = "攻击动画"))
	TObjectPtr<UAnimMontage> AttackMontage;
	
	/**
	 * @brief 投射物类（远程攻击使用）
	 * @details 弓箭手和弩兵等远程单位的投射物 Actor 类
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack Config", meta = (DisplayName = "投射物类"))
	TSubclassOf<AActor> ProjectileClass;
	
	// ========== ✨ 新增 - 可配置的攻击能力 ==========
	
	/**
	 * @brief 攻击能力类（可在 Blueprint 中配置）
	 * @details
	 * 功能说明：
	 * - 可以在单位 Blueprint 中直接指定攻击能力类
	 * - 如果设置了此属性，将使用指定的能力类
	 * - 如果未设置，将根据 UnitTypeTag 自动选择默认能力
	 * 使用方式：
	 * - 在 Blueprint 中设置为 GA_Attack_Melee 或 GA_Attack_Ranged
	 * - 或者自定义的攻击能力类
	 * 优先级：
	 * 1. AttackAbilityClass（如果设置）
	 * 2. DataTable 配置（如果 bUseDataTable = true）
	 * 3. 根据 UnitTypeTag 自动选择（默认行为）
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack Config", meta = (DisplayName = "攻击能力类"))
	TSubclassOf<UGameplayAbility> AttackAbilityClass;
	
	/**
	 * @brief 当前已授予的攻击能力
	 * @details 缓存已授予的攻击 GA，用于后续移除或管理
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Attack")
	FGameplayAbilitySpecHandle GrantedAttackAbilityHandle;

	// ========== GAS 接口实现 ==========
	
	/**
	 * @brief 获取 AbilitySystemComponent（GAS 接口要求）
	 */
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// ========== 初始化函数 ==========
	
	/**
	 * @brief 初始化角色属性
	 */
	UFUNCTION(BlueprintCallable, Category = "Character")
	void InitializeCharacter(
		FGameplayTag InFactionTag,
		float HealthMultiplier = 1.0f,
		float DamageMultiplier = 1.0f,
		float SpeedMultiplier = 1.0f
	);

	// ========== ✨ 新增 - DataTable 相关函数 ==========
	
	/**
	 * @brief 从 DataTable 加载单位配置
	 * @details
	 * 功能说明：
	 * - 从 DataTable 读取指定行的数据
	 * - 应用属性到 BaseHealth、BaseAttackDamage 等
	 * - 应用攻击配置（攻击动画、投射物类等）
	 * 详细流程：
	 * 1. 检查 DataTable 和行名称是否有效
	 * 2. 从 DataTable 查找指定行
	 * 3. 读取属性值并覆盖基础属性
	 * 4. 读取攻击配置
	 * 注意事项：
	 * - 在 InitializeCharacter() 之前调用
	 * - 如果 bUseDataTable = false，不会执行
	 */
	UFUNCTION(BlueprintCallable, Category = "Character")
	void LoadUnitDataFromTable();

	// ========== ✨ 新增 - 攻击系统函数 ==========
	
	/**
	 * @brief 授予攻击能力
	 * @details
	 * 功能说明：
	 * - 根据单位类型授予对应的攻击 Gameplay Ability
	 * - 近战单位使用 GA_Attack_Melee
	 * - 远程单位使用 GA_Attack_Ranged
	 * 详细流程：
	 * 1. 检查 ASC 是否有效
	 * 2. 根据 UnitTypeTag 确定攻击类型
	 * 3. 创建 Ability Spec 并授予能力
	 * 4. 缓存 Ability Handle 供后续使用
	 * 注意事项：
	 * - 在 BeginPlay 中自动调用
	 * - 需要先配置 UnitTypeTag
	 */
	UFUNCTION(BlueprintCallable, Category = "Attack")
	void GrantAttackAbility();
	
	/**
	 * @brief 执行攻击
	 * @details
	 * 功能说明：
	 * - 触发已授予的攻击能力
	 * - 供 AI 或玩家输入调用
	 * 详细流程：
	 * 1. 检查 ASC 和攻击能力是否有效
	 * 2. 检查能力是否可以激活（冷却、成本等）
	 * 3. 激活攻击能力
	 * 注意事项：
	 * - 在 StateTree AI 中调用
	 * - 需要先调用 GrantAttackAbility()
	 * @return 是否成功触发攻击
	 */
	UFUNCTION(BlueprintCallable, Category = "Attack")
	bool PerformAttack();

	// ========== 战斗相关函数 ==========
	
	/**
	 * @brief 查找最近的敌人
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	AActor* FindNearestTarget();
	
	/**
	 * @brief 设置当前目标
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void SetTarget(AActor* NewTarget);
	
	/**
	 * @brief 检查当前目标是否有效
	 * @details
	 * 功能说明：
	 * - 检查目标是否存在、是否存活、是否在范围内
	 * 详细流程：
	 * 1. 检查 CurrentTarget 是否为空
	 * 2. 检查目标是否已死亡
	 * 3. 检查目标是否仍在攻击范围内
	 * 注意事项：
	 * - 在 AI 中每帧检查
	 * - 如果无效，需要重新查找目标
	 * @return 目标是否有效
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	bool IsTargetValid() const;

protected:
	// ========== 生命周期函数 ==========
	
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;

	// ========== GAS 初始化 ==========
	
	/**
	 * @brief 初始化 AttributeSet 的值
	 */
	void InitializeAttributes(float HealthMult, float DamageMult, float SpeedMult);
	
	/**
	 * @brief 绑定属性变化委托
	 */
	void BindAttributeDelegates();

	// ========== 属性变化回调 ==========
	
	/**
	 * @brief 生命值变化时调用
	 * 
	 * 注意：参数类型必须是 const FOnAttributeChangeData&
	 * 这是 GAS 属性变化委托要求的签名
	 */
	void OnHealthChanged(const FOnAttributeChangeData& Data);
	
	/**
	 * @brief 角色死亡时调用
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Character")
	void OnDeath();
	virtual void OnDeath_Implementation();

public:
	// ✨ NEW - 死亡标记（防止重复触发）
	/**
	 * @brief 是否已经死亡
	 * @details 防止死亡逻辑被重复触发
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Character")
	bool bIsDead = false;

	// ========== ✨ 新增 - 调试可视化系统 ==========

	/**
	 * @brief 是否显示攻击范围
	 * @details 在编辑器和运行时显示攻击范围的圆圈
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Visualization", meta = (DisplayName = "显示攻击范围"))
	bool bShowAttackRange = false;

	/**
	 * @brief 是否显示视野范围
	 * @details 在编辑器和运行时显示视野范围的圆圈
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Visualization", meta = (DisplayName = "显示视野范围"))
	bool bShowVisionRange = false;

	/**
	 * @brief 视野范围（厘米）
	 * @details
	 * 功能说明：
	 * - 单位可以检测到敌人的最大距离
	 * - 如果启用 DataTable，此值从 DataTable 读取
	 * - 如果未启用 DataTable，使用此处配置的值
	 * 注意事项：
	 * - 当 bUseDataTable = true 时，此属性不可编辑
	 * - 当 bUseDataTable = false 时，可以在 Blueprint 中修改
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Visualization", meta = (DisplayName = "视野范围", EditCondition = "!bUseDataTable", EditConditionHides))
	float VisionRange = 1500.0f;

	/**
	 * @brief 攻击范围可视化颜色
	 * @details 攻击范围圆圈的颜色
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Visualization", meta = (DisplayName = "攻击范围颜色"))
	FLinearColor AttackRangeColor = FLinearColor::Red;

	/**
	 * @brief 视野范围可视化颜色
	 * @details 视野范围圆圈的颜色
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Visualization", meta = (DisplayName = "视野范围颜色"))
	FLinearColor VisionRangeColor = FLinearColor::Yellow;

	/**
	 * @brief 切换攻击范围显示
	 * @details 蓝图和控制台命令可调用此函数
	 */
	UFUNCTION(BlueprintCallable, Category = "Debug Visualization")
	void ToggleAttackRangeVisualization();

	/**
	 * @brief 切换视野范围显示
	 * @details 蓝图和控制台命令可调用此函数
	 */
	UFUNCTION(BlueprintCallable, Category = "Debug Visualization")
	void ToggleVisionRangeVisualization();

	/**
	 * @brief 绘制调试可视化
	 * @details 在编辑器和运行时绘制攻击范围和视野范围
	 */
	virtual void Tick(float DeltaTime) override;


	/**
	 * @brief 从 DataTable 加载单位配置
	 * @return 是否加载成功
	 * @details
	 * 功能说明：
	 * - 从 DataTable 读取指定行的数据
	 * - 应用属性到 BaseHealth、BaseAttackDamage 等
	 * - 应用攻击配置（攻击动画、投射物类等）
	 * 详细流程：
	 * 1. 检查 DataTable 和行名称是否有效
	 * 2. 从 DataTable 查找指定行
	 * 3. 读取属性值并覆盖基础属性
	 * 4. 读取攻击配置
	 * 注意事项：
	 * - 在 BeginPlay 中自动调用（如果 bUseDataTable = true）
	 * - 如果加载失败，返回 false
	 */
	UFUNCTION(BlueprintCallable, Category = "Character")
	bool IsLoadUnitDataFromTable();
	
protected:
	/**
	 * @brief 确定单位的阵营标签
	 * @return 阵营标签
	 * @details
	 * 功能说明：
	 * - 如果 FactionTag 已设置，使用已设置的值
	 * - 否则使用默认阵营标签（玩家阵营）
	 */
	FGameplayTag DetermineFactionTag() const;
	
	/**
	 * @brief 使用默认值初始化单位
	 * @details
	 * 功能说明：
	 * - 使用 Blueprint 中配置的 Base 属性
	 * - 使用默认阵营标签
	 */
	void InitializeWithDefaults();

	
};
