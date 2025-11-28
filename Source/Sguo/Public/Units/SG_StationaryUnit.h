// 📄 文件：Source/Sguo/Public/Units/SG_StationaryUnit.h
// ✨ 新增 - 站桩单位类

#pragma once

#include "CoreMinimal.h"
#include "Units/SG_UnitsBase.h"
#include "SG_StationaryUnit.generated.h"

/**
 * @brief 站桩单位类
 * @details
 * 功能说明：
 * - 继承自 SG_UnitsBase，复用所有基础功能
 * - 支持浮空或站立在地面
 * - 禁止移动（通过禁用移动组件实现）
 * - 可配置是否可被敌方选为目标
 * - 使用相同的数据配置和角色卡系统
 * 
 * 使用场景：
 * - 召唤物：诸葛亮的雷电网、吕布的旋转飞刀
 * - 固定防御塔
 * - 陷阱类单位
 * - 特效展示单位
 * 
 * 详细流程：
 * 1. BeginPlay 时根据配置调整位置和移动能力
 * 2. 禁用 CharacterMovement 组件
 * 3. AI 寻敌时会检查 CanBeTargeted 标记
 * 4. 其他战斗逻辑与普通单位完全相同
 * 
 * 注意事项：
 * - 浮空单位需要设置正确的碰撞响应
 * - 站桩单位不会参与前线推进
 * - 如果不可被选为目标，敌人会无视它继续前进
 */
UCLASS(BlueprintType, Blueprintable)
class SGUO_API ASG_StationaryUnit : public ASG_UnitsBase
{
	GENERATED_BODY()

public:
	/**
	 * @brief 构造函数
	 * @details
	 * 功能说明：
	 * - 初始化站桩单位的默认配置
	 * - 设置默认的浮空高度
	 */
	ASG_StationaryUnit();

protected:
	/**
	 * @brief 游戏开始时调用
	 * @details
	 * 功能说明：
	 * - 调用父类的 BeginPlay
	 * - 禁用移动能力
	 * - 根据配置调整位置（浮空或站立）
	 * 详细流程：
	 * 1. 调用 Super::BeginPlay()
	 * 2. 禁用 CharacterMovement 组件
	 * 3. 如果启用浮空，将单位提升到指定高度
	 * 4. 如果禁用重力，关闭物理重力
	 * 注意事项：
	 * - 必须在父类初始化完成后执行
	 * - 浮空单位需要正确的碰撞设置
	 */
	virtual void BeginPlay() override;

public:
	// ========== 站桩配置 ==========

	/**
	 * @brief 是否启用浮空模式
	 * @details
	 * 功能说明：
	 * - True：单位将悬浮在空中（例如召唤物、特效）
	 * - False：单位正常站立在地面上（例如固定防御塔）
	 * 使用场景：
	 * - 诸葛亮的雷电网：启用浮空，贴近地面
	 * - 吕布的旋转飞刀：启用浮空，角色高度
	 * - 陷阱：禁用浮空，站立地面
	 * 注意事项：
	 * - 浮空单位需要配合 HoverHeight 使用
	 * - 浮空单位建议禁用重力
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stationary Unit", 
		meta = (DisplayName = "启用浮空模式"))
	bool bEnableHover = false;

	/**
	 * @brief 浮空高度（厘米）
	 * @details
	 * 功能说明：
	 * - 单位相对于初始生成位置的垂直偏移
	 * - 正值：向上浮空
	 * - 负值：向下沉降（较少使用）
	 * - 0：保持原始高度
	 * 使用示例：
	 * - 雷电网：50-100 cm（贴近地面）
	 * - 旋转飞刀：100-150 cm（角色腰部高度）
	 * - 漂浮特效：200-300 cm（明显悬浮）
	 * 注意事项：
	 * - 只有 bEnableHover 为 true 时生效
	 * - 高度基于生成位置，不是绝对世界坐标
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stationary Unit", 
		meta = (DisplayName = "浮空高度(厘米)", EditCondition = "bEnableHover", EditConditionHides, ClampMin = "-500.0", ClampMax = "1000.0"))
	float HoverHeight = 100.0f;

	/**
	 * @brief 是否禁用重力
	 * @details
	 * 功能说明：
	 * - True：单位不受重力影响，固定在空中
	 * - False：单位受重力影响（站桩但有物理效果）
	 * 使用场景：
	 * - 浮空单位：建议启用（防止掉落）
	 * - 地面单位：建议禁用（保持物理真实感）
	 * 注意事项：
	 * - 禁用重力的单位不会被击飞
	 * - 配合 bEnableHover 使用效果最佳
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stationary Unit", 
		meta = (DisplayName = "禁用重力"))
	bool bDisableGravity = true;

	/**
	 * @brief 是否可被选为目标
	 * @details
	 * 功能说明：
	 * - True：敌方AI可以选择此单位为攻击目标（默认）
	 * - False：敌方AI会忽略此单位，直接穿过或绕过
	 * 使用场景：
	 * - 可攻击的召唤物（如旋转飞刀）：True
	 * - 纯视觉特效（如装饰物）：False
	 * - 陷阱（希望敌人踩上）：False
	 * - 固定防御塔：True
	 * 战术意义：
	 * - False：敌人会忽略并继续推进，适合区域控制技能
	 * - True：敌人会停下攻击，可以拖延敌军推进
	 * 注意事项：
	 * - 不影响技能的AOE伤害判定
	 * - 只影响AI的单体目标选择
	 * - 玩家手动点击仍然可以攻击
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stationary Unit", 
		meta = (DisplayName = "可被选为目标"))
	bool bCanBeTargeted = true;

	/**
	 * @brief 是否禁用移动
	 * @details
	 * 功能说明：
	 * - True：完全禁用移动能力（默认，推荐）
	 * - False：保留移动能力（特殊用途）
	 * 使用场景：
	 * - 普通站桩单位：True
	 * - 可移动的特殊召唤物：False
	 * 技术说明：
	 * - True：禁用 CharacterMovement 组件
	 * - False：保留移动能力但不建议使用
	 * 注意事项：
	 * - 即使禁用移动，单位仍然可以播放动画
	 * - AI 不会尝试移动站桩单位
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stationary Unit", 
		meta = (DisplayName = "禁用移动"))
	bool bDisableMovement = true;

	// ========== 查询接口 ==========

	/**
	 * @brief 检查单位是否可被选为目标
	 * @return 是否可被选为目标
	 * @details
	 * 功能说明：
	 * - 返回当前单位是否允许被AI选为攻击目标
	 * - AI 寻敌时会调用此函数过滤目标
	 * 使用场景：
	 * - AI 寻找最近敌人时
	 * - 技能选择目标时
	 * 注意事项：
	 * - 此函数为虚函数，可以在子类中重写
	 * - 可以添加额外的逻辑（如：受到伤害后变为可选中）
	 */
	virtual bool CanBeTargeted() const;

	/**
	 * @brief 检查单位是否在浮空模式
	 * @return 是否启用浮空
	 * @details
	 * 功能说明：
	 * - 返回单位当前是否处于浮空模式
	 * 使用场景：
	 * - 特效系统判断播放位置
	 * - 音效系统判断播放类型
	 */
	UFUNCTION(BlueprintPure, Category = "Stationary Unit", meta = (DisplayName = "是否浮空"))
	bool IsHovering() const { return bEnableHover; }

	/**
	 * @brief 获取浮空高度
	 * @return 浮空高度（厘米）
	 * @details
	 * 功能说明：
	 * - 返回单位的浮空高度配置值
	 * 使用场景：
	 * - 特效系统调整位置
	 * - 调试显示
	 */
	UFUNCTION(BlueprintPure, Category = "Stationary Unit", meta = (DisplayName = "获取浮空高度"))
	float GetHoverHeight() const { return HoverHeight; }

protected:
	/**
	 * @brief 应用站桩配置
	 * @details
	 * 功能说明：
	 * - 根据配置禁用移动和重力
	 * - 调整单位位置（浮空）
	 * 详细流程：
	 * 1. 禁用 CharacterMovement 组件（如果配置要求）
	 * 2. 禁用重力（如果配置要求）
	 * 3. 调整单位高度（如果启用浮空）
	 * 4. 更新碰撞设置
	 * 注意事项：
	 * - 在 BeginPlay 中调用
	 * - 必须在组件完全初始化后执行
	 */
	void ApplyStationarySettings();

	/**
	 * @brief 禁用移动能力
	 * @details
	 * 功能说明：
	 * - 禁用 CharacterMovement 组件
	 * - 设置移动速度为 0
	 * 详细流程：
	 * 1. 获取 CharacterMovement 组件
	 * 2. 设置 MaxWalkSpeed = 0
	 * 3. 禁用移动组件（可选）
	 * 注意事项：
	 * - 只有 bDisableMovement 为 true 时调用
	 */
	void DisableMovementCapability();

	/**
	 * @brief 应用浮空效果
	 * @details
	 * 功能说明：
	 * - 将单位提升到指定高度
	 * - 调整碰撞和物理设置
	 * 详细流程：
	 * 1. 获取当前位置
	 * 2. 计算新的 Z 坐标（当前 Z + HoverHeight）
	 * 3. 设置新位置
	 * 4. 禁用重力（如果配置要求）
	 * 注意事项：
	 * - 只有 bEnableHover 为 true 时调用
	 * - 浮空后位置不会再改变
	 */
	void ApplyHoverEffect();
};
