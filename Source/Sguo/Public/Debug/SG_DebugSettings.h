// ========== SG_DebugSettings.h ==========
// Copyright Epic Games, Inc. All Rights Reserved.

/**
 * @file SG_DebugSettings.h
 * @brief 调试系统配置类
 * @details
 * 功能说明：
 * - 在项目设置中配置调试相关参数
 * - 支持保存到配置文件（DefaultGame.ini）
 * - 提供全局访问接口
 * 使用方式：
 * - Edit → Project Settings → Game → 调试系统
 * - 在蓝图或 C++ 中通过 USG_DebugSettings::Get() 获取配置
 * 注意事项：
 * - 继承自 UDeveloperSettings 才能显示在项目设置中
 * - 使用 TSoftClassPtr 避免硬引用
 * - 配置会自动序列化和加载
 */

#pragma once

#include "CoreMinimal.h"
#include "SG_UnitDebugWidget.h"
#include "Engine/DeveloperSettings.h"
#include "SG_DebugSettings.generated.h"

// 前向声明


/**
 * @brief 调试系统配置
 * @details
 * 功能说明：
 * - 管理所有调试相关的配置参数
 * - 自动显示在项目设置中
 * - 支持配置文件持久化
 * 配置项说明：
 * - DebugWidgetClass：用于显示单位属性的 Widget 类
 * - WidgetHeightOffset：Widget 在单位头顶的偏移高度
 * - WidgetDrawSize：Widget 的显示尺寸
 * - bAutoEnableOnBeginPlay：游戏开始时是否自动启用调试显示
 * - bAutoAddToNewUnits：是否自动为新生成的单位添加调试显示
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="调试系统配置"))
class SGUO_API USG_DebugSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/**
	 * @brief 获取配置单例
	 * @return 配置实例
	 * @details
	 * 功能说明：
	 * - 提供全局访问配置的接口
	 * - 返回默认配置对象
	 * 使用示例：
	 * @code
	 * const USG_DebugSettings* Settings = USG_DebugSettings::Get();
	 * if (Settings)
	 * {
	 *     float Height = Settings->WidgetHeightOffset;
	 * }
	 * @endcode
	 */
	static const USG_DebugSettings* Get()
	{
		return GetDefault<USG_DebugSettings>();
	}

	// ========== 调试显示配置 ==========
	
	/**
	 * @brief 调试 Widget 类
	 * @details
	 * 功能说明：
	 * - 指定用于显示单位属性的 Widget 类
	 * - 必须继承自 USG_UnitDebugWidget
	 * 推荐设置：
	 * - /Game/BP/UI/Debug/WBP_UnitDebugWidget
	 * 注意事项：
	 * - 使用 TSoftClassPtr 避免硬引用
	 * - 需要在使用时调用 LoadSynchronous() 加载
	 */
	UPROPERTY(Config, EditAnywhere, Category = "调试显示", 
		meta = (DisplayName = "调试 Widget 类", 
		AllowedClasses = "/Script/UMG.UserWidget" ))
	TSoftClassPtr<USG_UnitDebugWidget> DebugWidgetClass;

	/**
	 * @brief Widget 在单位头顶的偏移高度
	 * @details
	 * 功能说明：
	 * - 控制 Widget 相对于单位根组件的 Z 轴偏移
	 * - 单位：虚幻单位（厘米）
	 * 推荐值：
	 * - 小型单位：100.0
	 * - 中型单位：150.0
	 * - 大型单位：200.0
	 * 注意事项：
	 * - 值越大，Widget 显示位置越高
	 * - 需要根据单位实际高度调整
	 */
	UPROPERTY(Config, EditAnywhere, Category = "调试显示", 
		meta = (DisplayName = "头顶偏移高度", 
		ClampMin = "0.0", UIMin = "0.0", UIMax = "500.0"))
	float WidgetHeightOffset = 150.0f;

	/**
	 * @brief Widget 缩放大小
	 * @details
	 * 功能说明：
	 * - 控制 Widget 的显示尺寸
	 * - X：宽度，Y：高度
	 * 推荐值：
	 * - 简洁模式：(250.0, 80.0)
	 * - 标准模式：(300.0, 100.0)
	 * - 详细模式：(400.0, 150.0)
	 * 注意事项：
	 * - 尺寸过大会遮挡游戏画面
	 * - 尺寸过小会导致文字难以阅读
	 */
	UPROPERTY(Config, EditAnywhere, Category = "调试显示", 
		meta = (DisplayName = "Widget 缩放"))
	FVector2D WidgetDrawSize = FVector2D(300.0f, 100.0f);

	/**
	 * @brief 是否在游戏开始时自动启用
	 * @details
	 * 功能说明：
	 * - 控制游戏开始时是否自动启用调试显示
	 * - True：游戏启动后自动显示所有单位的调试信息
	 * - False：需要手动调用 EnableUnitDebugDisplay() 启用
	 * 使用场景：
	 * - 开发阶段：建议启用，方便实时查看单位状态
	 * - 测试阶段：根据需要选择
	 * - 发布版本：必须禁用
	 * 注意事项：
	 * - 会在游戏开始后延迟 0.1 秒启用（确保所有单位已生成）
	 */
	UPROPERTY(Config, EditAnywhere, Category = "调试显示", 
		meta = (DisplayName = "自动启用调试显示"))
	bool bAutoEnableOnBeginPlay = true;

	/**
	 * @brief 是否自动为新生成的单位添加调试 Widget
	 * @details
	 * 功能说明：
	 * - 控制是否监听单位生成事件
	 * - True：自动为新生成的单位添加调试显示（推荐）
	 * - False：只为初始单位添加，不监听新单位
	 * 使用场景：
	 * - 游戏中会动态生成单位：必须启用
	 * - 所有单位在游戏开始时已存在：可以禁用
	 * 技术细节：
	 * - 通过监听 World 的 OnActorSpawned 事件实现
	 * - 会自动过滤非单位类型的 Actor
	 * 性能影响：
	 * - 启用后会轻微增加 Actor 生成的开销
	 * - 对游戏性能影响可忽略不计
	 */
	UPROPERTY(Config, EditAnywhere, Category = "调试显示", 
		meta = (DisplayName = "自动监听新单位"))
	bool bAutoAddToNewUnits = true;

public:
	// ========== UDeveloperSettings 接口 ==========
	
	/**
	 * @brief 获取配置分类名称
	 * @return 分类名称
	 * @details
	 * 功能说明：
	 * - 指定配置在项目设置中的分类
	 * - 返回 "Game" 表示显示在游戏相关设置中
	 */
	virtual FName GetCategoryName() const override
	{
		return FName(TEXT("Game"));
	}

#if WITH_EDITOR
	/**
	 * @brief 获取配置节标题
	 * @return 节标题文本
	 * @details
	 * 功能说明：
	 * - 指定配置在项目设置中的显示名称
	 * - 显示为 "调试系统"
	 */
	virtual FText GetSectionText() const override
	{
		return FText::FromString(TEXT("调试系统"));
	}

	/**
	 * @brief 获取配置节描述
	 * @return 节描述文本
	 * @details
	 * 功能说明：
	 * - 提供配置的详细说明
	 * - 显示在配置界面的顶部
	 */
	virtual FText GetSectionDescription() const override
	{
		return FText::FromString(TEXT("配置单位属性调试显示相关参数"));
	}
#endif
};
