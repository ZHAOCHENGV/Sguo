// ========== SG_DebugSubsystem.h ==========
// Copyright Epic Games, Inc. All Rights Reserved.

/**
 * @file SG_DebugSubsystem.h
 * @brief 调试子系统（全局管理 + 动态监听）
 * @details
 * 功能说明：
 * - 全局管理所有单位的调试显示
 * - 自动监听单位生成事件
 * - 为新生成的单位自动添加调试 Widget
 * 详细流程：
 * 1. 从配置类读取设置参数
 * 2. 监听世界中的 Actor 生成事件
 * 3. 检测是否是单位类型
 * 4. 自动创建并附加调试 Widget
 * 使用方式：
 * - 自动模式：在项目设置中配置，游戏启动后自动工作
 * - 手动模式：通过蓝图或代码调用 EnableUnitDebugDisplay()
 * 注意事项：
 * - 使用 WorldSubsystem，随世界创建和销毁
 * - 配置参数从 USG_DebugSettings 读取
 * - 可通过蓝图或控制台命令开关
 */

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SG_DebugSubsystem.generated.h"

// 前向声明
class USG_UnitDebugWidget;
class ASG_UnitsBase;
class UWidgetComponent;

/**
 * @brief 调试子系统
 * @details
 * 功能说明：
 * - 全局管理单位属性调试显示
 * - 动态监听并处理新生成的单位
 * - 提供统一的启用/禁用接口
 * 技术细节：
 * - 继承自 UWorldSubsystem，与世界生命周期绑定
 * - 使用 TMap 管理所有单位的 Widget 组件
 * - 通过委托监听 Actor 生成事件
 * 配置方式：
 * - Edit → Project Settings → Game → 调试系统
 */
UCLASS()
class SGUO_API USG_DebugSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// ========== 公共接口 ==========
	
	/**
	 * @brief 获取调试子系统单例
	 * @param WorldContextObject 世界上下文对象
	 * @return 调试子系统实例
	 * @details
	 * 功能说明：
	 * - 提供全局访问子系统的接口
	 * - 自动从世界上下文获取子系统实例
	 * 使用示例：
	 * @code
	 * USG_DebugSubsystem* DebugSys = USG_DebugSubsystem::Get(this);
	 * if (DebugSys)
	 * {
	 *     DebugSys->EnableUnitDebugDisplay();
	 * }
	 * @endcode
	 * 注意事项：
	 * - WorldContextObject 必须有效
	 * - 如果世界不存在，返回 nullptr
	 */
	UFUNCTION(BlueprintPure, Category = "Debug", 
		meta = (WorldContext = "WorldContextObject", DisplayName = "获取调试子系统"))
	static USG_DebugSubsystem* Get(const UObject* WorldContextObject);

	/**
	 * @brief 启用单位属性显示
	 * @details
	 * 功能说明：
	 * - 为场景中所有现有单位添加调试 Widget
	 * - 开始监听新单位生成（如果配置中启用）
	 * - 标记调试显示为已启用状态
	 * 执行流程：
	 * 1. 检查是否已启用（避免重复启用）
	 * 2. 遍历场景中所有单位
	 * 3. 为每个单位创建并附加调试 Widget
	 * 4. 如果配置启用自动监听，开始监听新单位生成
	 * 使用场景：
	 * - 游戏运行中需要查看单位状态时
	 * - 调试特定功能时临时启用
	 * 注意事项：
	 * - 可以在游戏运行中随时调用
	 * - 如果已启用会输出警告并跳过
	 */
	UFUNCTION(BlueprintCallable, Category = "Debug", 
		meta = (DisplayName = "启用单位属性显示"))
	void EnableUnitDebugDisplay();

	/**
	 * @brief 禁用单位属性显示
	 * @details
	 * 功能说明：
	 * - 移除所有单位的调试 Widget
	 * - 停止监听新单位生成
	 * - 标记调试显示为已禁用状态
	 * 执行流程：
	 * 1. 检查是否已禁用（避免重复禁用）
	 * 2. 停止监听单位生成事件
	 * 3. 遍历所有已创建的 Widget
	 * 4. 销毁 Widget 组件并清理映射表
	 * 使用场景：
	 * - 调试完成后清理界面
	 * - 性能测试时关闭调试显示
	 * 注意事项：
	 * - 可以在游戏运行中随时调用
	 * - 如果已禁用会输出警告并跳过
	 */
	UFUNCTION(BlueprintCallable, Category = "Debug", 
		meta = (DisplayName = "禁用单位属性显示"))
	void DisableUnitDebugDisplay();

	/**
	 * @brief 切换单位属性显示
	 * @details
	 * 功能说明：
	 * - 根据当前状态自动切换启用/禁用
	 * - 如果已启用则禁用
	 * - 如果已禁用则启用
	 * 使用场景：
	 * - 绑定到快捷键，方便快速切换
	 * - 在调试菜单中提供开关选项
	 * 使用示例：
	 * @code
	 * // 在玩家控制器中绑定快捷键
	 * void AMyPlayerController::SetupInputComponent()
	 * {
	 *     InputComponent->BindKey(EKeys::F1, IE_Pressed, this, 
	 *         &AMyPlayerController::ToggleDebugDisplay);
	 * }
	 * 
	 * void AMyPlayerController::ToggleDebugDisplay()
	 * {
	 *     USG_DebugSubsystem* DebugSys = USG_DebugSubsystem::Get(this);
	 *     if (DebugSys)
	 *     {
	 *         DebugSys->ToggleUnitDebugDisplay();
	 *     }
	 * }
	 * @endcode
	 */
	UFUNCTION(BlueprintCallable, Category = "Debug", 
		meta = (DisplayName = "切换单位属性显示"))
	void ToggleUnitDebugDisplay();

	/**
	 * @brief 为单位添加调试 Widget
	 * @param Unit 目标单位
	 * @details
	 * 功能说明：
	 * - 为指定单位创建调试 Widget
	 * - 附加到单位头顶
	 * - 绑定到单位的属性组件
	 * 执行流程：
	 * 1. 检查单位有效性
	 * 2. 检查是否已添加（避免重复）
	 * 3. 从配置读取 Widget 类和参数
	 * 4. 创建 WidgetComponent
	 * 5. 设置显示模式和尺寸
	 * 6. 附加到单位根组件
	 * 7. 创建 Widget 实例并绑定
	 * 8. 保存到映射表
	 * 使用场景：
	 * - 手动为特定单位添加调试显示
	 * - 在单位生成回调中调用
	 * 注意事项：
	 * - 如果单位已有调试 Widget，会跳过
	 * - 如果配置未设置 Widget 类，会输出错误
	 */
	UFUNCTION(BlueprintCallable, Category = "Debug", 
		meta = (DisplayName = "为单位添加调试显示"))
	void AddDebugWidgetToUnit(ASG_UnitsBase* Unit);

	/**
	 * @brief 移除单位的调试 Widget
	 * @param Unit 目标单位
	 * @details
	 * 功能说明：
	 * - 销毁单位的调试 Widget 组件
	 * - 从映射表中移除记录
	 * 执行流程：
	 * 1. 检查单位有效性
	 * 2. 从映射表查找 Widget 组件
	 * 3. 销毁组件
	 * 4. 从映射表移除
	 * 使用场景：
	 * - 手动移除特定单位的调试显示
	 * - 单位销毁前清理 Widget
	 * 注意事项：
	 * - 如果单位没有调试 Widget，会静默跳过
	 */
	UFUNCTION(BlueprintCallable, Category = "Debug", 
		meta = (DisplayName = "移除单位调试显示"))
	void RemoveDebugWidgetFromUnit(ASG_UnitsBase* Unit);

	/**
	 * @brief 是否启用调试显示
	 * @return 当前是否启用
	 * @details
	 * 功能说明：
	 * - 查询当前调试显示的启用状态
	 * - 用于 UI 显示或条件判断
	 * 使用示例：
	 * @code
	 * if (DebugSubsystem->IsDebugDisplayEnabled())
	 * {
	 *     // 显示调试菜单中的"禁用"按钮
	 * }
	 * else
	 * {
	 *     // 显示调试菜单中的"启用"按钮
	 * }
	 * @endcode
	 */
	UFUNCTION(BlueprintPure, Category = "Debug", 
		meta = (DisplayName = "是否启用调试显示"))
	bool IsDebugDisplayEnabled() const { return bDebugDisplayEnabled; }

protected:
	// ========== 生命周期 ==========
	
	/**
	 * @brief 子系统初始化
	 * @param Collection 子系统集合
	 * @details
	 * 功能说明：
	 * - 在世界创建时自动调用
	 * - 从配置类读取所有设置参数
	 * - 如果配置了自动启用，延迟启用调试显示
	 * 执行流程：
	 * 1. 调用父类初始化
	 * 2. 获取配置实例
	 * 3. 输出配置信息到日志
	 * 4. 如果启用自动启用，设置延迟定时器
	 * 注意事项：
	 * - 延迟 0.1 秒启用，确保所有初始单位已生成
	 * - 如果配置类无效，会输出错误
	 */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/**
	 * @brief 子系统反初始化
	 * @details
	 * 功能说明：
	 * - 在世界销毁时自动调用
	 * - 停止监听单位生成
	 * - 清理所有调试 Widget
	 * 执行流程：
	 * 1. 停止监听单位生成事件
	 * 2. 移除所有调试 Widget
	 * 3. 清空映射表
	 * 4. 调用父类反初始化
	 * 注意事项：
	 * - 确保所有资源被正确释放
	 * - 避免内存泄漏
	 */
	virtual void Deinitialize() override;

	// ========== 事件回调 ==========
	
	/**
	 * @brief 世界中 Actor 生成时调用
	 * @param Actor 新生成的 Actor
	 * @details
	 * 功能说明：
	 * - 监听世界中的 Actor 生成事件
	 * - 检测是否是单位类型
	 * - 自动为新单位添加调试 Widget
	 * 执行流程：
	 * 1. 检查 Actor 有效性
	 * 2. 检查调试显示是否启用
	 * 3. 尝试转换为 ASG_UnitsBase
	 * 4. 如果是单位，调用 AddDebugWidgetToUnit()
	 * 技术细节：
	 * - 通过 World 的 OnActorSpawned 委托触发
	 * - 会自动过滤非单位类型的 Actor
	 * 性能影响：
	 * - 每次 Actor 生成都会调用
	 * - 类型检查的开销很小
	 * 注意事项：
	 * - 只有在调试显示启用时才会添加 Widget
	 * - 如果单位已有 Widget，会在 AddDebugWidgetToUnit 中跳过
	 */
	UFUNCTION()
	void OnActorSpawned(AActor* Actor);

protected:
	// ========== 运行时数据 ==========
	
	/**
	 * @brief 是否启用调试显示
	 * @details
	 * 功能说明：
	 * - 标记当前调试显示的启用状态
	 * - 用于控制是否为新单位添加 Widget
	 * 注意事项：
	 * - Transient 标记，不会被序列化
	 * - 每次游戏启动都会重置
	 */
	UPROPERTY(Transient)
	bool bDebugDisplayEnabled = false;

	/**
	 * @brief 已创建的 Widget 组件映射
	 * @details
	 * 功能说明：
	 * - 管理所有单位的 Widget 组件
	 * - Key：单位指针
	 * - Value：Widget 组件指针
	 * 使用场景：
	 * - 快速查找单位的 Widget
	 * - 批量清理所有 Widget
	 * - 避免重复创建 Widget
	 * 注意事项：
	 * - Transient 标记，不会被序列化
	 * - 需要在单位销毁时清理对应的映射
	 */
	UPROPERTY(Transient)
	TMap<TObjectPtr<ASG_UnitsBase>, TObjectPtr<UWidgetComponent>> UnitWidgetMap;

	/**
	 * @brief Actor 生成事件委托句柄
	 * @details
	 * 功能说明：
	 * - 保存绑定到 World 的委托句柄
	 * - 用于在反初始化时解绑事件
	 * 技术细节：
	 * - 通过 World->AddOnActorSpawnedHandler() 获取
	 * - 通过 World->RemoveOnActorSpawnedHandler() 解绑
	 * 注意事项：
	 * - 必须在子系统销毁前解绑
	 * - 避免悬空指针导致崩溃
	 */
	FDelegateHandle ActorSpawnedDelegateHandle;

private:
	// ========== 内部辅助函数 ==========
	
	/**
	 * @brief 获取调试配置
	 * @return 配置实例
	 * @details
	 * 功能说明：
	 * - 提供快捷访问配置的接口
	 * - 封装配置获取逻辑
	 * 注意事项：
	 * - 如果配置无效，返回 nullptr
	 */
	const class USG_DebugSettings* GetDebugSettings() const;

	/**
	 * @brief 为所有现有单位添加调试 Widget
	 * @details
	 * 功能说明：
	 * - 查找场景中所有单位
	 * - 遍历并为每个单位添加调试 Widget
	 * 执行流程：
	 * 1. 使用 UGameplayStatics::GetAllActorsOfClass() 查找所有单位
	 * 2. 遍历单位列表
	 * 3. 为每个单位调用 AddDebugWidgetToUnit()
	 * 使用场景：
	 * - 在启用调试显示时调用
	 * - 处理游戏开始时已存在的单位
	 * 注意事项：
	 * - 只会添加 ASG_UnitsBase 及其子类
	 * - 如果单位已有 Widget，会自动跳过
	 */
	void AddDebugWidgetToAllUnits();

	/**
	 * @brief 移除所有调试 Widget
	 * @details
	 * 功能说明：
	 * - 遍历所有已创建的 Widget
	 * - 销毁 Widget 组件
	 * - 清空映射表
	 * 执行流程：
	 * 1. 遍历 UnitWidgetMap
	 * 2. 对每个 Widget 组件调用 DestroyComponent()
	 * 3. 清空 UnitWidgetMap
	 * 使用场景：
	 * - 在禁用调试显示时调用
	 * - 在子系统反初始化时调用
	 * 注意事项：
	 * - 确保所有组件都被正确销毁
	 * - 避免内存泄漏
	 */
	void RemoveAllDebugWidgets();

	/**
	 * @brief 开始监听世界中的单位生成事件
	 * @details
	 * 功能说明：
	 * - 绑定到世界的 OnActorSpawned 委托
	 * - 自动为新生成的单位添加调试 Widget
	 * 执行流程：
	 * 1. 获取世界对象
	 * 2. 检查是否已绑定（避免重复绑定）
	 * 3. 调用 World->AddOnActorSpawnedHandler() 绑定
	 * 4. 保存委托句柄
	 * 使用场景：
	 * - 在启用调试显示时调用（如果配置启用）
	 * - 确保动态生成的单位也有调试显示
	 * 注意事项：
	 * - 如果已绑定，会输出警告并跳过
	 * - 委托句柄必须保存，用于后续解绑
	 */
	void StartListeningForUnitSpawns();

	/**
	 * @brief 停止监听世界中的单位生成事件
	 * @details
	 * 功能说明：
	 * - 解绑 OnActorSpawned 委托
	 * - 清理委托句柄
	 * 执行流程：
	 * 1. 获取世界对象
	 * 2. 检查委托句柄是否有效
	 * 3. 调用 World->RemoveOnActorSpawnedHandler() 解绑
	 * 4. 重置委托句柄
	 * 使用场景：
	 * - 在禁用调试显示时调用
	 * - 在子系统反初始化时调用
	 * 注意事项：
	 * - 必须在世界销毁前解绑
	 * - 避免悬空指针导致崩溃
	 */
	void StopListeningForUnitSpawns();
};
