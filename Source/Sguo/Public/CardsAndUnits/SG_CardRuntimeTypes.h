// Copyright notice placeholder
/**
 * @file SG_CardRuntimeTypes.h
 * @brief 运行时卡牌类型声明
 * @details
 * 功能说明：
 * - 定义手牌及抽牌过程中使用的运行时结构体。
 * - 为 UI 与抽牌逻辑提供统一的数据表示。
 * 详细流程：
 * - `FSGCardInstance` 记录单张卡牌的实例信息。
 * - `FSGCardDrawSlot` 可用作调试或日志输出。
 * 注意事项：
 * - 该文件只包含无状态类型，所有逻辑位于组件中。
 */
#pragma once

// 🔧 MODIFIED FILE - 卡牌运行时类型定义
// Copyright notice placeholder
/**
 * @file SG_CardRuntimeTypes.h
 * @brief 卡牌系统运行时数据结构定义
 * @details
 * 功能说明：
 * - 定义卡牌实例、抽牌槽位等运行时数据结构
 * - 支持权重随机抽卡和保底机制
 * 详细流程：
 * - FSGCardInstance：表示一张具体的卡牌实例（包含唯一 ID）
 * - FSGCardDrawSlot：表示抽牌池中的一个槽位（支持权重和保底）
 * 注意事项：
 * - 所有结构体都标记为 BlueprintType，可在蓝图中使用
 * - 权重系统需要配合 DrawWeight 和 MissCount 使用
 */
#include "CoreMinimal.h"
// 引入主资产 ID 类型定义
#include "Engine/AssetManagerTypes.h"
// 引入头文件生成宏
#include "SG_CardRuntimeTypes.generated.h"

class USG_CardDataBase;
/**
 * @brief 卡牌实例结构体
 * @details
 * 功能说明：
 * - 表示一张具体的卡牌实例，包含运行时唯一 ID
 * - 用于区分同一张卡牌的不同副本（如两张"步兵"卡）
 * 使用场景：
 * - 手牌列表中的每张卡都是一个实例
 * - 使用卡牌时通过 InstanceId 定位
 * 注意事项：
 * - InstanceId 在每次抽卡时生成，全局唯一
 * - CardData 是对卡牌数据资产的引用，不要直接修改
 */
USTRUCT(BlueprintType)
struct FSGCardInstance
{
public:
	// 生成对象宏
	GENERATED_BODY()

public:
	// 实例唯一 ID
	// 用于区分同一张卡牌的不同副本
	// 在抽卡时生成，使用 FGuid::NewGuid() 确保全局唯一
	UPROPERTY(BlueprintReadOnly, Category = "Card")
	FGuid InstanceId;

	// 卡牌数据资产指针
	// 指向卡牌的静态数据（名称、图标、效果等）
	// 不要直接修改，所有卡牌共享同一份数据
	UPROPERTY(BlueprintReadOnly, Category = "Card")
	TObjectPtr<USG_CardDataBase> CardData = nullptr;

	// 卡牌资产 ID
	// 用于资产管理和序列化
	// 格式：Card:卡牌名称
	UPROPERTY(BlueprintReadOnly, Category = "Card")
	FPrimaryAssetId CardId;

	// 是否为唯一卡牌
	// True：整局游戏只能使用一次（如英雄卡）
	// False：可以重复抽到和使用（如兵团卡、计谋卡）
	UPROPERTY(BlueprintReadOnly, Category = "Card")
	bool bIsUnique = false;
};

/**
 * @brief 抽牌槽位结构体
 * @details
 * 功能说明：
 * - 表示抽牌池中的一个槽位，支持权重随机和保底机制
 * - 每个槽位对应一种卡牌类型
 * 详细流程：
 * - DrawWeight：基础抽取权重，数值越大越容易抽到
 * - MissCount：未抽到的连续次数，用于保底机制
 * - 实际权重 = DrawWeight * (1.0 + MissCount * 0.1)
 * 注意事项：
 * - 唯一卡牌抽到后不会移除槽位，只是不再参与抽取
 * - 权重系统确保长期来看所有卡牌分布均匀
 */
USTRUCT(BlueprintType)
struct FSGCardDrawSlot
{
public:
	// 生成对象宏
	GENERATED_BODY()

	// 卡牌资产 ID
	// 指向卡牌数据资产
	UPROPERTY(BlueprintReadOnly, Category = "Card")
	FPrimaryAssetId CardId;


	// 是否已被消耗（仅用于唯一卡牌）
	// True：唯一卡牌已被抽取，不再参与抽卡
	// False：可以继续抽取
	// 注意：非唯一卡牌此字段始终为 false
	// UPROPERTY(BlueprintReadOnly, Category = "Card")
	// bool bConsumed = false;

	// ✨ NEW - 抽取权重
	// 基础抽取权重，数值越大越容易抽到
	// 默认 1.0 表示标准概率
	// 可以设置更高的权重让某些卡牌更容易出现
	// 例如：设置为 2.0 表示抽到概率是其他卡牌的 2 倍
	UPROPERTY(BlueprintReadWrite, Category = "Card")
	float DrawWeight = 1.0f;

	// ✨ NEW - 未抽到的连续次数
	// 用于保底机制，每次未抽到时 +1，抽到时重置为 0
	// 实际权重 = DrawWeight * (1.0 + MissCount * 0.1)
	// 例如：连续 10 次未抽到，权重变为 DrawWeight * 2.0
	// 确保长期来看所有卡牌都有机会被抽到
	UPROPERTY(BlueprintReadWrite, Category = "Card")
	int32 MissCount = 0;

	// ✨ NEW - 保底系数
	// 从配置中复制，每个槽位可以有不同的保底系数
	UPROPERTY(BlueprintReadWrite, Category = "Card")
	float PityMultiplier = 0.1f;

	// ✨ NEW - 保底上限
	// 限制保底机制的最大权重倍率
	UPROPERTY(BlueprintReadWrite, Category = "Card")
	float PityMaxMultiplier = 5.0f;

	// ✨ NEW - 已出现次数
	// 用于限制卡牌的最大出现次数
	UPROPERTY(BlueprintReadWrite, Category = "Card")
	int32 OccurrenceCount = 0;

	// ✨ NEW - 最大出现次数
	// 0 表示无限制
	UPROPERTY(BlueprintReadWrite, Category = "Card")
	int32 MaxOccurrences = 0;

	// ✨ NEW - 计算实际权重
	/**
	 * @brief 计算实际权重（考虑保底机制）
	 * @return 实际权重
	 * @details
	 * 功能说明：
	 * - 根据基础权重、未抽到次数和保底参数计算实际权重
	 * 计算公式：
	 * - 保底倍率 = 1.0 + MissCount * PityMultiplier
	 * - 保底倍率 = Min(保底倍率, PityMaxMultiplier)
	 * - 实际权重 = DrawWeight * 保底倍率
	 * 注意事项：
	 * - 保底倍率不会超过 PityMaxMultiplier
	 * - 如果 DrawWeight 为 0，实际权重始终为 0
	 */
	float GetEffectiveWeight() const
	{
		// 如果基础权重为 0，直接返回 0
		if (DrawWeight <= 0.0f)
		{
			return 0.0f;
		}
		
		// 计算保底倍率
		float PityBonus = 1.0f + MissCount * PityMultiplier;
		
		// 限制保底倍率不超过上限
		PityBonus = FMath::Min(PityBonus, PityMaxMultiplier);
		
		// 返回实际权重
		return DrawWeight * PityBonus;
	}

	// ✨ NEW - 检查是否可以抽取
	/**
	 * @brief 检查此槽位是否可以抽取
	 * @return 是否可以抽取
	 * @details
	 * 功能说明：
	 * - 检查槽位是否满足抽取条件
	 * 检查项：
	 * - 权重大于 0
	 * - 未达到最大出现次数
	 */
	bool CanDraw() const
	{
		// 权重为 0 不能抽取
		if (DrawWeight <= 0.0f)
		{
			return false;
		}
		
		// 检查是否达到最大出现次数
		if (MaxOccurrences > 0 && OccurrenceCount >= MaxOccurrences)
		{
			return false;
		}
		
		return true;
	}
};

