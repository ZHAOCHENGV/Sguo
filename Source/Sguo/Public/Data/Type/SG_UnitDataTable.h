// 📄 文件：Source/Sguo/Public/Data/Type/SG_UnitDataTable.h
// 🔧 修改 - 在 FSGUnitAttackDefinition 结构体中添加优先级字段

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
 */
UENUM(BlueprintType)
enum class ESGUnitAttackType : uint8
{
    Melee       UMETA(DisplayName = "近战"),
    Ranged      UMETA(DisplayName = "远程直线"),
    Projectile  UMETA(DisplayName = "远程抛物线")
};

/**
 * @brief 攻击技能定义
 * @details
 * 功能说明：
 * - 定义单个攻击动作及其关联的数值
 * - 每个技能有独立的冷却时间和优先级
 * - 用于构建攻击列表，让单位可以根据优先级和冷却状态选择攻击方式
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
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack", meta = (DisplayName = "指定释放的能力"))
    TSubclassOf<UGameplayAbility> SpecificAbilityClass;

    /**
     * @brief 攻击类型
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack Config", meta = (DisplayName = "攻击类型"))
    ESGUnitAttackType AttackType = ESGUnitAttackType::Melee;

    /**
     * @brief 投射物类（仅远程单位）
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack Config", meta = (DisplayName = "投射物类", EditCondition = "AttackType != ESGUnitAttackType::Melee", EditConditionHides))
    TSubclassOf<AActor> ProjectileClass;

    /**
     * @brief 投射物生成偏移
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack Config", meta = (DisplayName = "投射物生成偏移", EditCondition = "AttackType != ESGUnitAttackType::Melee", EditConditionHides))
    FVector ProjectileSpawnOffset = FVector(50.0f, 0.0f, 80.0f);

    /**
     * @brief 冷却时间（秒）
     * @details 
     * - 此技能释放后进入独立冷却
     * - 冷却期间此技能不可用，但不影响其他技能
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack", meta = (DisplayName = "冷却时间", ClampMin = "0.0", UIMin = "0.0", UIMax = "100.0"))
    float Cooldown = 1.0f;

    // ✨ 新增 - 技能优先级
    /**
     * @brief 技能优先级
     * @details 
     * - 数值越大，优先级越高
     * - 当多个技能都未在冷却时，优先释放优先级高的技能
     * - 相同优先级时随机选择
     * 使用建议：
     * - 普通攻击：0
     * - 特殊技能：10-50
     * - 大招/必杀：100+
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack", meta = (DisplayName = "优先级", ToolTip = "数值越大越优先释放，相同优先级随机选择"))
    int32 Priority = 0;

    // 构造函数
    FSGUnitAttackDefinition()
        : Montage(nullptr)
        , SpecificAbilityClass(nullptr)
        , AttackType(ESGUnitAttackType::Melee)
        , ProjectileClass(nullptr)
        , ProjectileSpawnOffset(FVector(50.0f, 0.0f, 80.0f))
        , Cooldown(1.0f)
        , Priority(0)  // ✨ 新增 - 默认优先级为0
    {}
};

/**
 * @brief 单位数据表行结构
 */
USTRUCT(BlueprintType)
struct FSGUnitDataRow : public FTableRowBase
{
    GENERATED_BODY()

    // ========== 基础信息 ==========
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basic Info", meta = (DisplayName = "单位名称"))
    FText UnitName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basic Info", meta = (DisplayName = "单位描述"))
    FText UnitDescription;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basic Info", meta = (DisplayName = "单位类型标签", Categories = "Unit.Type"))
    FGameplayTag UnitTypeTag;

    // ========== 属性配置 ==========

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes", meta = (DisplayName = "基础生命值", ClampMin = "1.0", UIMin = "1.0", UIMax = "5000.0"))
    float BaseHealth = 500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes", meta = (DisplayName = "基础攻击力", ClampMin = "1.0", UIMin = "1.0", UIMax = "1000.0"))
    float BaseAttackDamage = 50.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes", meta = (DisplayName = "基础移动速度", ClampMin = "1.0", UIMin = "1.0", UIMax = "2000.0"))
    float BaseMoveSpeed = 400.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes", meta = (DisplayName = "基础攻击速度", ClampMin = "0.1", UIMin = "0.1", UIMax = "5.0"))
    float BaseAttackSpeed = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes", meta = (DisplayName = "基础攻击范围", ClampMin = "10.0", UIMin = "10.0", UIMax = "3000.0"))
    float BaseAttackRange = 150.0f;

    // ========== 攻击配置 ==========

    /**
     * @brief 攻击技能配置列表
     * @details
     * - 配置该单位拥有的所有攻击方式
     * - 每个技能有独立的冷却时间和优先级
     * - AI 攻击时会选择优先级最高且未冷却的技能
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack Config", meta = (DisplayName = "攻击技能列表"))
    TArray<FSGUnitAttackDefinition> Abilities;

    // ========== AI 配置 ==========

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Config", meta = (DisplayName = "寻敌范围", ClampMin = "100.0", UIMin = "100.0", UIMax = "999999.0"))
    float DetectionRange = 1500.0f;

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
