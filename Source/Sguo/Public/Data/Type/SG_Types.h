/**
 * @file SG_Types.h
 * @brief 游戏核心类型定义（最小化版本）
 * 
 * 功能说明：
 * - 只定义必须在C++中定义的类型
 * - 其他类型在蓝图中定义
 * 
 * 为什么这些必须用C++：
 * - 枚举需要在DataAsset中使用，必须在C++定义
 * - 结构体需要序列化和网络复制支持
 * 
 * 注意事项：
 * - 所有枚举都标记为BlueprintType，可以在蓝图中使用
 * - 所有结构体都标记为BlueprintType，可以在蓝图中创建变量
 */

#pragma once

#include "CoreMinimal.h"
#include "SG_Types.generated.h"


class USG_CardDataBase;

/**
 * @brief 卡牌放置类型枚举
 * 
 * 定义卡牌使用时的放置方式
 * 
 * 为什么需要这个枚举：
 * - 不同卡牌有不同的放置方式
 * - 用枚举而不是字符串，提供类型安全
 * - 便于在代码中判断和处理不同的放置逻辑
 */
UENUM(BlueprintType)
enum class ESGPlacementType : uint8
{
	// 单点放置：用于单个英雄或单位
	// 玩家点击位置后，在该点生成一个角色
	Single      UMETA(DisplayName = "Single"),
	
	// 区域放置：用于兵团或范围计谋
	// 玩家选择一个区域，在区域内按阵型生成多个单位或施放范围效果
	Area        UMETA(DisplayName = "Area"),
	
	// 全局效果：不需要选择位置
	// 点击卡牌后直接生效，如全体Buff（强攻计、神速计）
	Global      UMETA(DisplayName = "Global")
};

/**
 * @brief 计谋目标类型枚举
 * 
 * 定义计谋卡的作用目标
 * 
 * 为什么需要这个枚举：
 * - 计谋卡需要知道作用于哪一方
 * - 用于过滤目标单位（友方/敌方）
 * - 决定计谋的施放逻辑
 */
UENUM(BlueprintType)
enum class ESGStrategyTargetType : uint8
{
	// 作用于友方单位（如康复计、强攻计）
	// 只会影响玩家控制的单位
	Friendly    UMETA(DisplayName = "Friendly"),
	
	// 作用于敌方单位（如业火计、滚石计）
	// 只会影响敌人单位
	Enemy       UMETA(DisplayName = "Enemy"),
	
	// 作用于指定区域，敌我都受影响
	// 在区域内的所有单位都会受到效果
	Area        UMETA(DisplayName = "Area"),
	
	// 全局效果（通常是友方Buff）
	// 影响场上所有友方单位
	Global      UMETA(DisplayName = "Global")
};

/**
 * @brief 攻击类型枚举
 * 
 * 定义单位的攻击方式
 * 
 * 为什么需要这个枚举：
 * - 不同单位有不同的攻击方式
 * - 决定攻击判定逻辑（直接伤害、生成投射物等）
 * - 影响攻击动画和特效
 */
UENUM(BlueprintType)
enum class ESGAttackType : uint8
{
	// 近战攻击：直接造成伤害
	// 攻击范围内的敌人直接受到伤害，无需投射物
	// 例如：步兵、骑兵
	Melee       UMETA(DisplayName = "Melee"),
	
	// 远程直线攻击：生成直线飞行的投射物
	// 投射物直线飞行，击中第一个目标后消失
	// 例如：弩兵、连弩战车
	Ranged      UMETA(DisplayName = "Ranged"),
	
	// 抛物线攻击：生成抛物线飞行的投射物
	// 投射物抛物线飞行，落地后可能有AOE效果
	// 例如：弓兵、投石车
	Projectile  UMETA(DisplayName = "Projectile")
};

/**
 * @brief 游戏阶段枚举
 * 
 * 定义游戏的不同阶段
 * 
 * 为什么需要这个枚举：
 * - 游戏有明确的阶段划分
 * - 不同阶段有不同的逻辑和UI
 * - 便于状态管理和流程控制
 */
UENUM(BlueprintType)
enum class ESGGamePhase : uint8
{
	// 游戏初始化阶段
	// 加载资源、初始化系统
	Setup               UMETA(DisplayName = "Setup"),
	
	// 建筑布置阶段（游戏开始前）
	// 玩家和敌人放置主城和防御建筑
	BuildingPlacement   UMETA(DisplayName = "Building Placement"),
	
	// 游戏进行中
	// 正常的游戏玩法，可以使用卡牌、单位战斗
	Playing             UMETA(DisplayName = "Playing"),
	
	// 游戏暂停
	// 暂停所有游戏逻辑，显示暂停菜单
	Paused              UMETA(DisplayName = "Paused"),
	
	// 玩家胜利
	// 敌方主城被摧毁，显示胜利界面
	Victory             UMETA(DisplayName = "Victory"),
	
	// 玩家失败
	// 玩家主城被摧毁，显示失败界面
	Defeat              UMETA(DisplayName = "Defeat")
};


// ✨ NEW - 卡牌配置槽位结构体
/**
 * @brief 卡牌配置槽位
 * @details
 * 功能说明：
 * - 在卡组配置中定义单张卡牌的抽取规则
 * - 支持权重、保底、初始手牌保证等高级配置
 * 使用场景：
 * - 在 USG_DeckConfig 中配置可用卡牌列表
 * - 在编辑器中调整卡牌出现概率
 * - 设计师可以精细控制卡牌分布
 * 注意事项：
 * - DrawWeight 为 0 表示此卡牌不会被抽到（用于临时禁用）
 * - bGuaranteedInInitialHand 只对初始手牌生效
 * - MaxOccurrences 为 0 表示无限制
 */
USTRUCT(BlueprintType)
struct FSGCardConfigSlot
{
	GENERATED_BODY()

	// ========== 基础配置 ==========
	
	/**
	 * @brief 卡牌数据引用
	 * @details
	 * 功能说明：
	 * - 指向具体的卡牌数据资产
	 * - 使用软引用支持异步加载
	 * 注意事项：
	 * - 不要引用空资产
	 * - 确保资产路径正确
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card Config", meta = (DisplayName = "卡牌"))
	TSoftObjectPtr<USG_CardDataBase> CardData;

	// ========== 权重配置 ==========
	
	/**
	 * @brief 抽取权重
	 * @details
	 * 功能说明：
	 * - 基础抽取权重，数值越大越容易抽到
	 * - 默认 1.0 表示标准概率
	 * 使用示例：
	 * - 设置为 2.0：抽到概率是其他卡牌的 2 倍
	 * - 设置为 0.5：抽到概率是其他卡牌的一半
	 * - 设置为 0.0：此卡牌不会被抽到（用于临时禁用）
	 * 注意事项：
	 * - 权重为负数会被视为 0
	 * - 所有卡牌权重为 0 时无法抽卡
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card Config", 
		meta = (DisplayName = "抽取权重", ClampMin = "0.0", UIMin = "0.0", UIMax = "5.0"))
	float DrawWeight = 1.0f;

	// ========== 保底配置 ==========
	
	/**
	 * @brief 保底系数
	 * @details
	 * 功能说明：
	 * - 每次未抽到时，权重增加的倍率
	 * - 默认 0.1 表示每次未抽到权重增加 10%
	 * 计算公式：
	 * - 实际权重 = DrawWeight * (1.0 + MissCount * PityMultiplier)
	 * 使用示例：
	 * - 设置为 0.1：连续 10 次未抽到，权重翻倍
	 * - 设置为 0.2：连续 5 次未抽到，权重翻倍
	 * - 设置为 0.0：不启用保底机制
	 * 注意事项：
	 * - 数值过大会导致保底过快触发
	 * - 建议范围：0.05 ~ 0.2
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card Config", 
		meta = (DisplayName = "保底系数", ClampMin = "0.0", UIMin = "0.0", UIMax = "0.5"))
	float PityMultiplier = 0.1f;

	/**
	 * @brief 保底上限
	 * @details
	 * 功能说明：
	 * - 限制保底机制的最大权重倍率
	 * - 默认 5.0 表示权重最多增加到原来的 5 倍
	 * 使用场景：
	 * - 防止长时间未抽到导致权重过高
	 * - 确保卡牌分布的合理性
	 * 注意事项：
	 * - 设置为 1.0 表示不启用保底
	 * - 建议范围：2.0 ~ 10.0
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card Config", 
		meta = (DisplayName = "保底上限（倍率）", ClampMin = "1.0", UIMin = "1.0", UIMax = "10.0"))
	float PityMaxMultiplier = 5.0f;

	// ========== 初始手牌配置 ==========
	
	/**
	 * @brief 是否保证在初始手牌中出现
	 * @details
	 * 功能说明：
	 * - True：此卡牌必定出现在初始手牌中
	 * - False：正常随机抽取
	 * 使用场景：
	 * - 教学关卡：保证新手拿到特定卡牌
	 * - 剧情关卡：保证关键卡牌出现
	 * - 测试模式：快速测试特定卡牌
	 * 注意事项：
	 * - 保证卡牌数量不能超过初始手牌数
	 * - 如果保证卡牌过多，会按配置顺序优先保证
	 * - 唯一卡牌只会出现一次
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card Config", 
		meta = (DisplayName = "保证初始手牌"))
	bool bGuaranteedInInitialHand = false;

	// ========== 出现次数限制 ==========
	
	/**
	 * @brief 最大出现次数（整局游戏）
	 * @details
	 * 功能说明：
	 * - 限制此卡牌在整局游戏中的最大出现次数
	 * - 0 表示无限制（默认）
	 * - >0 表示最多出现指定次数
	 * 使用场景：
	 * - 限制强力卡牌的使用次数
	 * - 控制游戏节奏和难度
	 * - 实现"消耗品"类型的卡牌
	 * 注意事项：
	 * - 唯一卡牌（bIsUnique）会忽略此设置，始终只出现一次
	 * - 设置为 1 等同于唯一卡牌
	 * - 达到上限后，此卡牌不会再出现
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card Config", 
		meta = (DisplayName = "最大出现次数（0=无限）", ClampMin = "0", UIMin = "0", UIMax = "10"))
	int32 MaxOccurrences = 0;

	// ========== 调试信息 ==========
	
	/**
	 * @brief 配置备注
	 * @details
	 * 功能说明：
	 * - 设计师可以在此添加备注信息
	 * - 不影响游戏逻辑
	 * 使用场景：
	 * - 记录配置意图
	 * - 团队协作沟通
	 * - 版本迭代记录
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card Config", 
		meta = (DisplayName = "备注", MultiLine = true))
	FString ConfigNote;

	// ========== 构造函数 ==========
	
	/**
	 * @brief 默认构造函数
	 * @details
	 * 功能说明：
	 * - 初始化所有字段为默认值
	 */
	FSGCardConfigSlot()
		: DrawWeight(1.0f)
		, PityMultiplier(0.1f)
		, PityMaxMultiplier(5.0f)
		, bGuaranteedInInitialHand(false)
		, MaxOccurrences(0)
	{
	}
};


/**
 * @brief 玩家配置数据结构
 * 
 * 包含玩家相关的所有配置参数
 * 
 * 为什么需要这个结构体：
 * - 将相关配置组织在一起，便于管理
 * - 可以整体传递，避免多个参数
 * - 支持序列化，可以保存和加载配置
 * 
 * 使用场景：
 * - 游戏配置界面
 * - 难度调整
 * - 存档系统
 */
USTRUCT(BlueprintType)
struct FSGPlayerConfigData
{
	GENERATED_BODY()

	// ========== 主城配置 ==========
	
	// 主城最大生命值
	// 决定玩家主城的耐久度，主城生命归零则游戏失败
	// 默认10000，可以在配置界面调整
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Main City")
	float MainCityHealth = 10000.0f;

	// 主城弓箭手伤害倍率
	// 基于弓箭手基础伤害的倍率
	// 例如：基础伤害50，倍率1.5，实际伤害75
	// 用于调整游戏难度，增强或削弱防御力
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Main City")
	float ArcherDamageMultiplier = 1.0f;

	// ========== 单位属性倍率 ==========
	// 这些倍率应用于所有玩家单位，用于整体调整难度
	
	// 玩家单位生命值倍率
	// 应用于从卡牌生成的所有单位
	// 例如：步兵基础生命500，倍率1.2，实际生命600
	// 为什么用倍率而不是固定值：便于平衡调整，保持单位间的相对强度
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unit Multipliers")
	float UnitHealthMultiplier = 1.0f;

	// 玩家单位伤害倍率
	// 应用于所有单位的攻击伤害
	// 影响单位的输出能力
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unit Multipliers")
	float UnitDamageMultiplier = 1.0f;

	// 玩家单位速度倍率
	// 同时影响移动速度和攻击速度
	// 为什么同时影响：保持游戏节奏统一，避免单位移动快但攻击慢的不协调
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unit Multipliers")
	float UnitSpeedMultiplier = 1.0f;

	// ========== 卡牌系统配置 ==========
	
	// 出卡冷却时间（秒）
	// 使用一张卡牌或放弃行动后，需要等待的时间才能抽取新卡
	// 控制游戏节奏，避免玩家快速刷卡
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card System")
	float CardCooldown = 3.0f;

	// 武将技能冷却倍率
	// 应用于所有英雄技能的CD
	// 例如：技能基础CD 10秒，倍率0.5，实际CD 5秒
	// 用于调整技能使用频率
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card System")
	float AbilityCooldownMultiplier = 1.0f;
};

/**
 * @brief 敌人配置数据结构
 * 
 * 包含敌人相关的所有配置参数
 * 
 * 为什么单独定义敌人配置：
 * - 敌人和玩家的配置需求不同（如刷怪模式）
 * - 便于独立调整敌人难度
 * - 支持不对称平衡（敌我双方可以有不同的倍率）
 */
USTRUCT(BlueprintType)
struct FSGEnemyConfigData
{
	GENERATED_BODY()

	// ========== 主城配置 ==========
	// 配置说明同玩家配置
	
	// 敌方主城最大生命值
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Main City")
	float MainCityHealth = 10000.0f;

	// 敌方主城弓箭手伤害倍率
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Main City")
	float ArcherDamageMultiplier = 1.0f;

	// ========== 单位属性倍率 ==========
	
	// 敌方单位生命值倍率
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unit Multipliers")
	float UnitHealthMultiplier = 1.0f;

	// 敌方单位伤害倍率
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unit Multipliers")
	float UnitDamageMultiplier = 1.0f;

	// 敌方单位速度倍率
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unit Multipliers")
	float UnitSpeedMultiplier = 1.0f;

	// ========== 刷怪系统配置 ==========
	
	// 刷怪冷却时间（秒）
	// 敌人生成单位的时间间隔
	// 控制敌人的进攻频率
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn System")
	float SpawnCooldown = 5.0f;

	// 武将技能冷却倍率
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn System")
	float AbilityCooldownMultiplier = 1.0f;
};
