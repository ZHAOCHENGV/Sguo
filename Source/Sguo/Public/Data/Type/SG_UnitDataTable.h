// 📄 文件：Source/Sguo/Public/Data/Type/SG_UnitDataTable.h

#pragma once

// 引入核心头文件
#include "CoreMinimal.h"
// 引入数据表头文件
#include "Engine/DataTable.h"
// 引入 GameplayTag 头文件
#include "GameplayTagContainer.h"
// 引入生成宏
#include "SG_UnitDataTable.generated.h"

// 前置声明
class UAnimMontage;
class UGameplayAbility;
class AActor;

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

// ✨ 新增 - 单个攻击技能的定义结构体
/**
 * @brief 攻击技能定义
 * @details
 * 功能说明：
 * - 定义单个攻击动作及其关联的数值
 * - 用于构建攻击列表，让单位可以随机选择不同的攻击方式
 */
USTRUCT(BlueprintType)
struct FSGUnitAttackDefinition
{
    GENERATED_BODY()

    /**
     * @brief 攻击动画
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack", meta = (DisplayName = "攻击动画"))
    TObjectPtr<UAnimMontage> Montage;

    /**
     * @brief 指定释放的能力 (可选)
     * @details 
     * - 如果设置了此项，攻击时将尝试激活此 Ability
     * - 如果未设置，将使用单位默认的通用攻击 Ability
     * - 允许不同动作触发完全不同的技能逻辑（如：平A触发通用GA，重击触发击飞GA）
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack", meta = (DisplayName = "指定释放的能力"))
    TSubclassOf<UGameplayAbility> SpecificAbilityClass;

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
    
 
    /**
     * @brief 冷却时间（秒）
     * @details 此次攻击后的硬直/冷却时间
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack", meta = (DisplayName = "冷却时间", ClampMin = "0.0", UIMin = "0.0", UIMax = "100.0"))
    float Cooldown = 1.0f;
    
    // 构造函数
    FSGUnitAttackDefinition()
        : Montage(nullptr), SpecificAbilityClass(nullptr),  Cooldown(1.0f)
    {}
};

/**
 * @brief 单位数据表行结构
 * @details 定义 DataTable 的行结构
 */
USTRUCT(BlueprintType)
struct FSGUnitDataRow : public FTableRowBase
{
    GENERATED_BODY()

    // ========== 基础信息 ==========

    // 单位名称
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basic Info", meta = (DisplayName = "单位名称"))
    FText UnitName;

    // 单位描述
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basic Info", meta = (DisplayName = "单位描述"))
    FText UnitDescription;

    // 单位类型标签
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basic Info", meta = (DisplayName = "单位类型标签", Categories = "Unit.Type"))
    FGameplayTag UnitTypeTag;

    // ========== 属性配置 ==========

    // 基础生命值
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes", meta = (DisplayName = "基础生命值", ClampMin = "1.0", UIMin = "1.0", UIMax = "5000.0"))
    float BaseHealth = 500.0f;

    // 基础攻击力
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes", meta = (DisplayName = "基础攻击力", ClampMin = "1.0", UIMin = "1.0", UIMax = "1000.0"))
    float BaseAttackDamage = 50.0f;

    // 基础移动速度
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes", meta = (DisplayName = "基础移动速度", ClampMin = "1.0", UIMin = "1.0", UIMax = "2000.0"))
    float BaseMoveSpeed = 400.0f;

    // 基础攻击速度
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes", meta = (DisplayName = "基础攻击速度", ClampMin = "0.1", UIMin = "0.1", UIMax = "5.0"))
    float BaseAttackSpeed = 1.0f;

    // 基础攻击范围
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes", meta = (DisplayName = "基础攻击范围", ClampMin = "10.0", UIMin = "10.0", UIMax = "3000.0"))
    float BaseAttackRange = 150.0f;

    // ========== 攻击配置 ==========

    // ✨ 新增 - 攻击技能列表
    /**
     * @brief 攻击技能配置列表
     * @details
     * 功能说明：
     * - 配置该单位拥有的所有普通攻击方式
     * - AI 攻击时会从中随机选取一个执行
     * - 包含动画、伤害倍率、冷却时间、攻击类型等
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack Config", meta = (DisplayName = "攻击技能列表"))
    TArray<FSGUnitAttackDefinition> Abilities;

    // ========== AI 配置 ==========

    // 寻敌范围
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Config", meta = (DisplayName = "寻敌范围", ClampMin = "100.0", UIMin = "100.0", UIMax = "999999.0"))
    float DetectionRange = 1500.0f;

    // 追击范围
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Config", meta = (DisplayName = "追击范围", ClampMin = "100.0", UIMin = "100.0", UIMax = "999999.0"))
    float ChaseRange = 2000.0f;

    // ========== 构造函数 ==========

    FSGUnitDataRow()
        : UnitName(FText::FromString(TEXT("未命名单位")))
        , UnitDescription(FText::FromString(TEXT("单位描述")))
        , BaseHealth(500.0f)
        , BaseAttackDamage(50.0f)
        , BaseMoveSpeed(400.0f)
        , BaseAttackSpeed(1.0f)
        , BaseAttackRange(150.0f)
        , DetectionRange(1500.0f)
        , ChaseRange(2000.0f)
    {
    }
};
