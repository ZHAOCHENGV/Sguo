// 📄 文件：Source/Sguo/Public/Data/SG_FireArrowCardData.h
// ✨ 新增 - 火矢计卡牌数据资产

#pragma once

#include "CoreMinimal.h"
#include "Data/SG_StrategyCardData.h"
#include "SG_FireArrowCardData.generated.h"

/**
 * @brief 火矢计卡牌数据
 * @details
 * 功能说明：
 * - 继承自计谋卡数据基类
 * - 定义火矢计的特有参数（范围、持续时间、射击间隔等）
 * 详细流程：
 * 1. 玩家选择火矢计卡牌
 * 2. 显示圆形区域预览
 * 3. 玩家确认位置后，从浮空弓手发射火箭
 * 4. 持续一段时间，每隔一定时间发射一轮
 * 注意事项：
 * - 需要场上有浮空弓手单位才能发动
 * - 技能可被打断
 */
UCLASS(BlueprintType, Blueprintable)
class SGUO_API USG_FireArrowCardData : public USG_StrategyCardData
{
	GENERATED_BODY()

public:
	// ========== 区域配置 ==========
	
	/**
	 * @brief 打击区域半径（厘米）
	 * @details
	 * 功能说明：
	 * - 定义火箭覆盖的圆形区域半径
	 * - 影响预览显示和实际打击范围
	 * 建议值：
	 * - 500-800：小范围集中打击
	 * - 800-1200：中等范围
	 * - 1200+：大范围覆盖
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire Arrow Config", 
		meta = (DisplayName = "打击区域半径", ClampMin = "100.0", UIMin = "100.0", UIMax = "2000.0"))
	float AreaRadius = 800.0f;

	// ========== 持续时间配置 ==========
	
	/**
	 * @brief 技能持续时间（秒）
	 * @details
	 * 功能说明：
	 * - 从确认发射到技能结束的总时长
	 * - 期间会持续发射火箭
	 * 建议值：
	 * - 3-5 秒：短时间密集打击
	 * - 5-8 秒：中等持续时间
	 * - 8+ 秒：长时间压制
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire Arrow Config", 
		meta = (DisplayName = "技能持续时间(秒)", ClampMin = "1.0", UIMin = "1.0", UIMax = "15.0"))
	float SkillDuration = 5.0f;

	/**
	 * @brief 射击间隔时间（秒）
	 * @details
	 * 功能说明：
	 * - 每轮射击之间的时间间隔
	 * - 间隔越短，火箭密度越高
	 * 建议值：
	 * - 0.2-0.5：密集射击
	 * - 0.5-1.0：中等频率
	 * - 1.0+：稀疏射击
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire Arrow Config", 
		meta = (DisplayName = "射击间隔(秒)", ClampMin = "0.1", UIMin = "0.1", UIMax = "3.0"))
	float FireInterval = 0.3f;

	// ========== 火箭配置 ==========
	
	/**
	 * @brief 每轮每个弓手发射的火箭数量
	 * @details
	 * 功能说明：
	 * - 每个浮空弓手每轮发射的火箭数
	 * - 总火箭数 = 弓手数量 * 每轮火箭数
	 * 建议值：
	 * - 1-2：单发精准
	 * - 3-5：散射覆盖
	 * - 5+：火力压制
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire Arrow Config", 
		meta = (DisplayName = "每轮每弓手火箭数", ClampMin = "1", UIMin = "1", UIMax = "10"))
	int32 ArrowsPerArcherPerRound = 3;

	/**
	 * @brief 火箭投射物类
	 * @details
	 * 功能说明：
	 * - 指定使用的火箭投射物蓝图类
	 * - 如果为空，使用默认的 SG_Projectile
	 * 注意事项：
	 * - 火箭投射物应该有燃烧视觉效果
	 * - 可以配置落地后的 AOE 伤害
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire Arrow Config", 
		meta = (DisplayName = "火箭投射物类"))
	TSubclassOf<AActor> FireArrowProjectileClass;

	/**
	 * @brief 火箭弧度高度（厘米）
	 * @details
	 * 功能说明：
	 * - 火箭抛物线的最高点高度
	 * - 数值越大，火箭飞行弧度越高
	 * 建议值：
	 * - 200-400：低弧度快速打击
	 * - 400-600：中等弧度
	 * - 600+：高抛
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire Arrow Config", 
		meta = (DisplayName = "火箭弧度高度", ClampMin = "100.0", UIMin = "100.0", UIMax = "1000.0"))
	float ArrowArcHeight = 400.0f;

	/**
	 * @brief 火箭飞行速度
	 * @details
	 * 功能说明：
	 * - 火箭的飞行速度（厘米/秒）
	 * 建议值：
	 * - 2000-3000：较慢，视觉效果明显
	 * - 3000-4000：中等速度
	 * - 4000+：快速打击
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire Arrow Config", 
		meta = (DisplayName = "火箭飞行速度", ClampMin = "1000.0", UIMin = "1000.0", UIMax = "6000.0"))
	float ArrowSpeed = 3000.0f;

	// ========== 伤害配置 ==========
	
	/**
	 * @brief 单支火箭伤害倍率
	 * @details
	 * 功能说明：
	 * - 基于弓手攻击力的伤害倍率
	 * - 实际伤害 = 弓手攻击力 * 倍率
	 * 建议值：
	 * - 0.3-0.5：弱化单发伤害（火箭数量多时）
	 * - 0.5-1.0：标准伤害
	 * - 1.0+：强化单发伤害
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire Arrow Config", 
		meta = (DisplayName = "火箭伤害倍率", ClampMin = "0.1", UIMin = "0.1", UIMax = "3.0"))
	float ArrowDamageMultiplier = 0.5f;

	// ========== 视觉效果配置 ==========
	
	/**
	 * @brief 预览区域材质
	 * @details
	 * 功能说明：
	 * - 显示在地面的区域预览材质
	 * - 建议使用半透明红色材质表示危险区域
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire Arrow Visual", 
		meta = (DisplayName = "预览区域材质"))
	TObjectPtr<UMaterialInterface> PreviewAreaMaterial;

	/**
	 * @brief 预览区域颜色
	 * @details
	 * 功能说明：
	 * - 区域预览的颜色
	 * - 建议使用红色或橙色表示火焰效果
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire Arrow Visual", 
		meta = (DisplayName = "预览区域颜色"))
	FLinearColor PreviewAreaColor = FLinearColor(1.0f, 0.3f, 0.0f, 0.5f);

	// ========== 弓手筛选配置 ==========
	
	/**
	 * @brief 浮空弓手单位标签
	 * @details
	 * 功能说明：
	 * - 用于筛选参与火矢计的浮空单位
	 * - 只有匹配此标签的浮空单位才会发射火箭
	 * 注意事项：
	 * - 需要在弓手单位上设置对应的标签
	 * - 如果为空，则所有浮空单位都参与
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire Arrow Config", 
		meta = (DisplayName = "弓手单位标签", Categories = "Unit.Type"))
	FGameplayTag ArcherUnitTag;

public:
	/**
	 * @brief 构造函数
	 * @details
	 * 功能说明：
	 * - 设置默认的放置类型为区域放置
	 * - 设置默认的目标类型为敌方
	 */
	USG_FireArrowCardData()
	{
		// 设置为区域放置类型
		PlacementType = ESGPlacementType::Area;
		// 设置目标为敌方
		TargetType = ESGStrategyTargetType::Enemy;
	}
};
