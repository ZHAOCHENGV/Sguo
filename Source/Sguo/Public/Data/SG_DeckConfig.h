// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SG_CardDataBase.h"
#include "Type/SG_Types.h"
#include "Engine/DataAsset.h"
#include "SG_DeckConfig.generated.h"

/**
 * @file SG_DeckConfig.h
 * @brief 定义卡组配置数据资产
 * @details
 * 功能说明：
 * - 描述一次对局中卡组的初始化规则（初始手牌、补牌 CD、卡池等）。
 * - 以 Primary Asset 形式被 `USG_AssetManager` 管理，可通过 AssetId 统一加载。
 * 详细流程：
 * 1. 编辑器中创建继承自本类的数据资产。
 * 2. 在数据资产中配置手牌数量、冷却以及可用卡牌列表。
 * 3. 运行时通过 AssetManager 加载并交由 Deck 系统解析。
 * 注意事项：
 * - `AllowedCards` 建议使用软引用，避免初始化阶段强依赖大量卡牌。
 * - `RNGSeed` 仅在需要复现抽卡顺序时启用，可在运行时覆盖。
 */



UCLASS(BlueprintType)
class SGUO_API USG_DeckConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// ========== 游戏规则配置 ==========
	
	/**
	 * @brief 开局手牌数
	 * @details
	 * 功能说明：
	 * - 游戏开始时抽取的卡牌数量
	 * - 默认 5 张
	 * 注意事项：
	 * - 不要设置过大，避免手牌过多
	 * - 建议范围：3 ~ 7
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Rules", 
		meta = (DisplayName = "初始手牌数", ClampMin = "1", UIMin = "1", UIMax = "10"))
	int32 InitialHand = 5;
	
	/**
	 * @brief 行动冷却时间（秒）
	 * @details
	 * 功能说明：
	 * - 使用卡牌或跳过行动后的冷却时间
	 * - 冷却结束后自动抽取一张新卡
	 * - 默认 2 秒
	 * 注意事项：
	 * - 设置为 0 表示无冷却（立即抽卡）
	 * - 建议范围：1.0 ~ 5.0
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Rules", 
		meta = (DisplayName = "行动冷却（秒）", ClampMin = "0.0", UIMin = "0.0", UIMax = "10.0"))
	float DrawCDSeconds = 2.0f;
	
	/**
	 * @brief 手牌上限
	 * @details
	 * 功能说明：
	 * - 手牌数量的最大值
	 * - 达到上限后不会再抽卡
	 * - 默认 99（实际无限制）
	 * 注意事项：
	 * - 当前版本暂不限制手牌上限
	 * - 预留字段，未来可能启用
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Rules", 
		meta = (DisplayName = "手牌上限", ClampMin = "1", UIMin = "1", UIMax = "20"))
	int32 MaxHandSize = 99;

	// ========== 随机种子配置 ==========
	
	// ✨ NEW - 是否使用固定随机种子
	/**
	 * @brief 是否使用固定随机种子
	 * @details
	 * 功能说明：
	 * - True：使用固定种子（FixedRNGSeed），每次游戏抽卡顺序相同
	 * - False：使用自动随机种子（基于当前时间），每次游戏抽卡顺序不同
	 * 使用场景：
	 * - 开发测试：启用固定种子，便于复现问题
	 * - 录制演示：启用固定种子，确保演示效果一致
	 * - 正式游戏：禁用固定种子，使用随机种子增加游戏随机性
	 * - 竞技模式：启用固定种子，确保公平性（双方使用相同种子）
	 * 注意事项：
	 * - 固定种子会导致每次游戏的抽卡顺序完全相同
	 * - 建议在正式发布时禁用此选项
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Random Seed", 
		meta = (DisplayName = "使用固定种子"))
	bool bUseFixedSeed = false;

	
	// ✨ NEW - 固定随机种子值
	/**
	 * @brief 固定随机种子值
	 * @details
	 * 功能说明：
	 * - 当 bUseFixedSeed 为 True 时使用此值作为随机种子
	 * - 相同种子会产生相同的抽卡顺序
	 * - 默认值 1337（常见的测试种子）
	 * 使用方式：
	 * - 开发测试：使用固定值（如 1337）
	 * - 复现 Bug：使用出问题时的种子值
	 * - 竞技模式：双方使用相同种子
	 * 注意事项：
	 * - 只有当 bUseFixedSeed 为 True 时此值才会生效
	 * - 种子值可以是任意整数
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Random Seed", 
		meta = (DisplayName = "固定种子值", EditCondition = "bUseFixedSeed", EditConditionHides))
	int32 FixedRNGSeed = 1337;

	
	// ========== 卡牌配置 ==========
	
	/**
	 * @brief 可用卡牌列表
	 * @details
	 * 功能说明：
	 * - 定义本次游戏中可以抽到的所有卡牌
	 * - 每个槽位包含卡牌引用和高级配置（权重、保底等）
	 * 使用方式：
	 * 1. 点击 + 号添加新槽位
	 * 2. 选择卡牌数据资产
	 * 3. 调整权重和保底参数
	 * 4. 设置是否保证初始手牌
	 * 注意事项：
	 * - 不要添加空引用
	 * - 确保至少有一张卡牌
	 * - 权重为 0 的卡牌不会被抽到
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Card Pool", 
		meta = (DisplayName = "可用卡牌列表"))
	TArray<FSGCardConfigSlot> AllowedCards;

	// 🔧 MODIFIED - 移除旧的简单数组，改用 FSGCardConfigSlot
	// UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	// TArray<TObjectPtr<USG_CardDataBase>> AllowedCards;

	

	// ✨ NEW - 获取所有卡牌数据（用于兼容旧代码）
	/**
	 * @brief 获取所有卡牌数据资产
	 * @return 卡牌数据数组
	 * @details
	 * 功能说明：
	 * - 从配置槽位中提取所有卡牌数据
	 * - 用于兼容旧代码中直接访问 AllowedCards 的地方
	 * 注意事项：
	 * - 返回的数组可能包含空指针，使用前需要检查
	 */
	UFUNCTION(BlueprintCallable, Category = "Deck Config")
	TArray<USG_CardDataBase*> GetAllCardData() const;

	// ✨ NEW - 验证配置有效性
	/**
	 * @brief 验证卡组配置是否有效
	 * @return 是否有效
	 * @details
	 * 功能说明：
	 * - 检查配置是否合理
	 * - 在编辑器中保存时自动调用
	 * 检查项：
	 * - 至少有一张卡牌
	 * - 所有卡牌引用有效
	 * - 保证初始手牌的卡牌数量不超过初始手牌数
	 * - 权重配置合理
	 */
	UFUNCTION(BlueprintCallable, Category = "Deck Config")
	bool ValidateConfig(FString& OutErrorMessage) const;

	// ✨ NEW - 获取实际使用的随机种子
	/**
	 * @brief 获取实际使用的随机种子
	 * @return 随机种子值
	 * @details
	 * 功能说明：
	 * - 如果 bUseFixedSeed 为 True，返回 FixedRNGSeed
	 * - 如果 bUseFixedSeed 为 False，返回基于当前时间的随机种子
	 * 使用场景：
	 * - 卡组初始化时调用，获取用于初始化随机数生成器的种子
	 * - 日志记录，便于复现问题
	 * 注意事项：
	 * - 自动随机种子基于当前时间，每次调用结果不同
	 * - 固定种子每次调用结果相同
	 */
	UFUNCTION(BlueprintCallable, Category = "Random Seed")
	int32 GetEffectiveRNGSeed() const;

	// ✨ NEW - 生成随机种子（基于时间）
	/**
	 * @brief 生成基于时间的随机种子
	 * @return 随机种子值
	 * @details
	 * 功能说明：
	 * - 基于当前系统时间生成随机种子
	 * - 使用高精度时间戳，确保随机性
	 * 算法说明：
	 * - 获取当前时间戳（秒）
	 * - 获取当前时间戳（毫秒）
	 * - 组合两者生成种子：(秒 * 1000) ^ 毫秒
	 * 注意事项：
	 * - 此函数为静态函数，可以在任何地方调用
	 * - 每次调用结果不同（除非在同一毫秒内调用）
	 */
	UFUNCTION(BlueprintCallable, Category = "Random Seed")
	static int32 GenerateRandomSeed();


	
#if WITH_EDITOR
	/**
	 * @brief 编辑器中属性修改后回调
	 * @details
	 * 功能说明：
	 * - 在编辑器中修改属性后自动验证配置
	 * - 如果配置无效，输出警告信息
	 */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	
	/**
	 * @brief 覆写 Primary Asset ID 生成逻辑
	 * @return FPrimaryAssetId 对应的卡组资产 ID
	 */
	// 指定卡组数据的主资产类型为 Deck，方便 AssetManager 分类
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
