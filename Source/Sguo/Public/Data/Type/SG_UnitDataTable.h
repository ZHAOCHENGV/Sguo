// ✨ 新增 - 单位数据表结构
// Copyright notice placeholder
/**
 * @file SG_UnitDataTable.h
 * @brief 单位数据表结构（用于 DataTable 配置）
 * @details
 * 功能说明：
 * - 定义单位的基础属性配置
 * - 在编辑器中通过 DataTable 配置不同单位
 * - 支持步兵、骑兵、弓兵等所有单位类型
 * 使用方式：
 * 1. 创建 DataTable 资产（基于 FSGUnitDataRow）
 * 2. 在表格中添加不同单位的配置行
 * 3. 在角色卡数据中引用 DataTable 行
 * 注意事项：
 * - DataTable 行名称建议使用单位ID（如 "Infantry_Basic"）
 * - 所有数值都是基础值，可通过倍率调整
 */

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "SG_UnitDataTable.generated.h"

// 前置声明
class UAnimMontage;

/**
 * @brief 攻击类型枚举
 * @details
 * 功能说明：
 * - 定义单位的攻击方式
 * - 决定攻击判定逻辑
 */
UENUM(BlueprintType)
enum class ESGUnitAttackType : uint8
{
	// 近战攻击（直接伤害）
	Melee       UMETA(DisplayName = "近战"),
	
	// 远程直线攻击（生成投射物）
	Ranged      UMETA(DisplayName = "远程直线"),
	
	// 远程抛物线攻击（生成投射物）
	Projectile  UMETA(DisplayName = "远程抛物线")
};

/**
 * @brief 单位数据表行结构
 * @details
 * 功能说明：
 * - DataTable 的行结构
 * - 配置单位的所有基础属性
 * - 在编辑器中填写数据
 * 使用示例：
 * 步兵：
 *   - 生命值：500
 *   - 攻击力：50
 *   - 移动速度：400
 *   - 攻击速度：1.5
 *   - 攻击范围：150
 *   - 攻击类型：近战
 * 骑兵：
 *   - 生命值：600
 *   - 攻击力：70
 *   - 移动速度：600
 *   - 攻击速度：1.2
 *   - 攻击范围：150
 *   - 攻击类型：近战
 * 弓兵：
 *   - 生命值：300
 *   - 攻击力：40
 *   - 移动速度：400
 *   - 攻击速度：1.0
 *   - 攻击范围：1000
 *   - 攻击类型：远程抛物线
 */
USTRUCT(BlueprintType)
struct FSGUnitDataRow : public FTableRowBase
{
	GENERATED_BODY()

	// ========== 基础信息 ==========

	/**
	 * @brief 单位名称
	 * @details
	 * 功能说明：
	 * - 显示在 UI 上的名称
	 * - 用于调试日志
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basic Info", meta = (DisplayName = "单位名称"))
	FText UnitName;

	/**
	 * @brief 单位描述
	 * @details
	 * 功能说明：
	 * - 显示在 UI 上的描述
	 * - 说明单位特性
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basic Info", meta = (DisplayName = "单位描述"))
	FText UnitDescription;

	/**
	 * @brief 单位类型标签
	 * @details
	 * 功能说明：
	 * - 标识单位类型（步兵/骑兵/弓兵等）
	 * - 用于兵种克制等游戏逻辑
	 * 标签示例：
	 * - Unit.Type.Infantry（步兵）
	 * - Unit.Type.Cavalry（骑兵）
	 * - Unit.Type.Archer（弓兵）
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basic Info", meta = (DisplayName = "单位类型标签", Categories = "Unit.Type"))
	FGameplayTag UnitTypeTag;

	// ========== 属性配置 ==========

	/**
	 * @brief 基础生命值
	 * @details
	 * 功能说明：
	 * - 单位的初始生命值
	 * - 实际生命值 = 基础生命值 * 难度倍率
	 * 参考值：
	 * - 步兵：500
	 * - 骑兵：600
	 * - 弓兵：300
	 * - 英雄：1000 - 2000
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes", meta = (DisplayName = "基础生命值", ClampMin = "1.0", UIMin = "1.0", UIMax = "5000.0"))
	float BaseHealth = 500.0f;

	/**
	 * @brief 基础攻击力
	 * @details
	 * 功能说明：
	 * - 单位的基础伤害值
	 * - 实际伤害 = 基础攻击力 * 技能倍率 * 难度倍率
	 * 参考值：
	 * - 步兵：50
	 * - 骑兵：70
	 * - 弓兵：40
	 * - 英雄：100 - 200
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes", meta = (DisplayName = "基础攻击力", ClampMin = "1.0", UIMin = "1.0", UIMax = "1000.0"))
	float BaseAttackDamage = 50.0f;

	/**
	 * @brief 基础移动速度
	 * @details
	 * 功能说明：
	 * - 单位的移动速度（厘米/秒）
	 * - UE 默认角色移动速度约 600
	 * 参考值：
	 * - 步兵：400
	 * - 骑兵：600
	 * - 弓兵：400
	 * - 英雄：450 - 550
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes", meta = (DisplayName = "基础移动速度", ClampMin = "1.0", UIMin = "1.0", UIMax = "2000.0"))
	float BaseMoveSpeed = 400.0f;

	/**
	 * @brief 基础攻击速度
	 * @details
	 * 功能说明：
	 * - 每秒可以攻击的次数
	 * - 例如：1.0 = 每秒1次，1.5 = 每秒1.5次
	 * 参考值：
	 * - 步兵：1.5（攻速快）
	 * - 骑兵：1.2（攻速中）
	 * - 弓兵：1.0（攻速中）
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes", meta = (DisplayName = "基础攻击速度", ClampMin = "0.1", UIMin = "0.1", UIMax = "5.0"))
	float BaseAttackSpeed = 1.0f;

	/**
	 * @brief 基础攻击范围
	 * @details
	 * 功能说明：
	 * - 单位可以攻击的最大距离（厘米）
	 * 参考值：
	 * - 近战单位：150 - 200
	 * - 远程单位：800 - 1500
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes", meta = (DisplayName = "基础攻击范围", ClampMin = "10.0", UIMin = "10.0", UIMax = "3000.0"))
	float BaseAttackRange = 150.0f;

	// ========== 攻击配置 ==========

	/**
	 * @brief 攻击类型
	 * @details
	 * 功能说明：
	 * - 定义单位的攻击方式
	 * - 决定攻击判定逻辑
	 * - 近战：球形范围检测
	 * - 远程：生成投射物
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack Config", meta = (DisplayName = "攻击类型"))
	ESGUnitAttackType AttackType = ESGUnitAttackType::Melee;

	/**
	 * @brief 攻击动画蒙太奇
	 * @details
	 * 功能说明：
	 * - 攻击时播放的动画
	 * - 需要在动画中添加 AnimNotify 触发攻击判定
	 * 注意事项：
	 * - AnimNotify 名称必须为 "AttackHit"
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack Config", meta = (DisplayName = "攻击动画"))
	TObjectPtr<UAnimMontage> AttackMontage;

	/**
	 * @brief 投射物类（仅远程单位）
	 * @details
	 * 功能说明：
	 * - 远程攻击时生成的投射物
	 * - 弓兵：弓箭投射物
	 * - 弩兵：弩箭投射物
	 * 注意事项：
	 * - 只有远程单位需要设置
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack Config", meta = (DisplayName = "投射物类", EditCondition = "AttackType != ESGUnitAttackType::Melee", EditConditionHides))
	TSubclassOf<AActor> ProjectileClass;

	/**
	 * @brief 投射物生成偏移
	 * @details
	 * 功能说明：
	 * - 投射物生成位置相对于单位的偏移
	 * - 用于调整投射物从弓弩发射的位置
	 * 建议值：
	 * - X: 50（前方）
	 * - Y: 0
	 * - Z: 80（弓的高度）
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack Config", meta = (DisplayName = "投射物生成偏移", EditCondition = "AttackType != ESGUnitAttackType::Melee", EditConditionHides))
	FVector ProjectileSpawnOffset = FVector(50.0f, 0.0f, 80.0f);

	// ========== AI 配置 ==========

	/**
	 * @brief 寻敌范围
	 * @details
	 * 功能说明：
	 * - AI 自动寻找目标的范围（厘米）
	 * - 超出此范围的敌人不会被主动攻击
	 * 建议值：
	 * - 近战单位：1000 - 1500
	 * - 远程单位：1500 - 2000
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Config", meta = (DisplayName = "寻敌范围", ClampMin = "100.0", UIMin = "100.0", UIMax = "999999.0"))
	float DetectionRange = 99999.0f;

	/**
	 * @brief 追击范围
	 * @details
	 * 功能说明：
	 * - 锁定目标后，追击的最大距离
	 * - 超出此范围会放弃追击
	 * 建议值：
	 * - 寻敌范围 * 1.5
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Config", meta = (DisplayName = "追击范围", ClampMin = "100.0", UIMin = "100.0", UIMax = "999999.0"))
	float ChaseRange = 99999.0f;

	// ========== 构造函数 ==========

	/**
	 * @brief 默认构造函数
	 * @details
	 * 功能说明：
	 * - 初始化所有字段为默认值
	 */
	FSGUnitDataRow()
		: UnitName(FText::FromString(TEXT("未命名单位")))
		, UnitDescription(FText::FromString(TEXT("单位描述")))
		, BaseHealth(500.0f)
		, BaseAttackDamage(50.0f)
		, BaseMoveSpeed(400.0f)
		, BaseAttackSpeed(1.0f)
		, BaseAttackRange(150.0f)
		, AttackType(ESGUnitAttackType::Melee)
		, ProjectileSpawnOffset(FVector(50.0f, 0.0f, 80.0f))
		, DetectionRange(1500.0f)
		, ChaseRange(2000.0f)
	{
	}
};
