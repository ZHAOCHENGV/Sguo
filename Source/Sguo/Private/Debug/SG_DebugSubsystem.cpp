// ========== SG_DebugSubsystem.cpp ==========
// Copyright Epic Games, Inc. All Rights Reserved.

/**
 * @file SG_DebugSubsystem.cpp
 * @brief 调试子系统实现（支持动态生成单位）
 * @details
 * 功能说明：
 * - 实现调试子系统的所有功能
 * - 从配置类读取参数
 * - 管理单位调试 Widget 的生命周期
 */

#include "Debug/SG_DebugSubsystem.h"
#include "Debug/SG_UnitDebugWidget.h"
#include "Debug/SG_DebugSettings.h"
#include "Units/SG_UnitsBase.h"
#include "Components/WidgetComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Debug/SG_LogCategories.h"
#include "Engine/World.h"

// ========== 公共接口实现 ==========

/**
 * @brief 获取调试子系统单例
 * @param WorldContextObject 世界上下文对象
 * @return 调试子系统实例
 * @details
 * 功能说明：
 * - 从世界上下文获取世界对象
 * - 从世界获取子系统实例
 * 错误处理：
 * - 如果世界上下文无效，返回 nullptr
 * - 如果世界对象无效，返回 nullptr
 */
USG_DebugSubsystem* USG_DebugSubsystem::Get(const UObject* WorldContextObject)
{
	// 检查世界上下文是否有效
	if (!WorldContextObject)
	{
		return nullptr;
	}
	
	// 获取世界对象
	UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return nullptr;
	}
	
	// 获取子系统实例
	return World->GetSubsystem<USG_DebugSubsystem>();
}

// ========== 生命周期实现 ==========

/**
 * @brief 子系统初始化
 * @param Collection 子系统集合
 * @details
 * 功能说明：
 * - 在世界创建时自动调用
 * - 从配置类读取所有设置参数
 * - 输出配置信息到日志
 * - 如果配置了自动启用，延迟启用调试显示
 * 执行流程：
 * 1. 调用父类初始化
 * 2. 输出初始化日志
 * 3. 获取配置实例
 * 4. 检查配置有效性
 * 5. 输出配置参数
 * 6. 如果启用自动启用，设置延迟定时器
 * 错误处理：
 * - 如果配置无效，输出错误并返回
 */
void USG_DebugSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	// 调用父类实现
	Super::Initialize(Collection);
	
	// 输出日志
	UE_LOG(LogSGGameplay, Log, TEXT("========== 调试子系统初始化 =========="));

	// 从配置读取设置
	const USG_DebugSettings* Settings = GetDebugSettings();
	if (!Settings)
	{
		// 输出错误
		UE_LOG(LogSGGameplay, Error, TEXT("❌ 无法获取调试配置！"));
		UE_LOG(LogSGGameplay, Error, TEXT("  请检查 USG_DebugSettings 是否正确配置"));
		UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
		return;
	}

	// 检查 Widget 类配置
	if (Settings->DebugWidgetClass.IsNull())
	{
		// 输出警告
		UE_LOG(LogSGGameplay, Warning, TEXT("⚠️ DebugWidgetClass 未设置"));
		UE_LOG(LogSGGameplay, Warning, TEXT("  请在项目设置中配置："));
		UE_LOG(LogSGGameplay, Warning, TEXT("  Edit → Project Settings → Game → 调试系统"));
	}
	else
	{
		// 输出日志
		UE_LOG(LogSGGameplay, Log, TEXT("  ✓ DebugWidgetClass：%s"), 
			*Settings->DebugWidgetClass.ToString());
	}

	// 输出配置信息
	UE_LOG(LogSGGameplay, Log, TEXT("  自动启用：%s"), 
		Settings->bAutoEnableOnBeginPlay ? TEXT("是") : TEXT("否"));
	UE_LOG(LogSGGameplay, Log, TEXT("  自动监听新单位：%s"), 
		Settings->bAutoAddToNewUnits ? TEXT("是") : TEXT("否"));
	UE_LOG(LogSGGameplay, Log, TEXT("  偏移高度：%.0f"), Settings->WidgetHeightOffset);
	UE_LOG(LogSGGameplay, Log, TEXT("  Widget 大小：[%.0f, %.0f]"), 
		Settings->WidgetDrawSize.X, Settings->WidgetDrawSize.Y);

	// 如果配置了自动启用
	if (Settings->bAutoEnableOnBeginPlay)
	{
		// 延迟1帧后启用（确保所有单位已生成）
		// 使用定时器延迟执行，避免在初始化阶段就开始查找单位
		FTimerHandle DelayHandle;
		GetWorld()->GetTimerManager().SetTimer(
			DelayHandle,
			this,
			&USG_DebugSubsystem::EnableUnitDebugDisplay,
			0.1f,  // 延迟 0.1 秒
			false  // 不循环
		);
		
		// 输出日志
		UE_LOG(LogSGGameplay, Log, TEXT("  ⏰ 将在 0.1 秒后自动启用调试显示"));
	}
	
	UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
}

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
 * 3. 调用父类反初始化
 * 4. 输出日志
 */
void USG_DebugSubsystem::Deinitialize()
{
	// 停止监听单位生成
	StopListeningForUnitSpawns();
	
	// 移除所有调试 Widget
	RemoveAllDebugWidgets();
	
	// 调用父类实现
	Super::Deinitialize();
	
	// 输出日志
	UE_LOG(LogSGGameplay, Log, TEXT("调试子系统已销毁"));
}

// ========== 调试显示控制实现 ==========

/**
 * @brief 启用单位属性显示
 * @details
 * 功能说明：
 * - 为场景中所有现有单位添加调试 Widget
 * - 开始监听新单位生成（如果配置中启用）
 * - 标记调试显示为已启用状态
 * 执行流程：
 * 1. 检查是否已启用
 * 2. 标记为已启用
 * 3. 为所有现有单位添加 Widget
 * 4. 如果配置启用，开始监听新单位
 * 5. 输出统计信息
 */
void USG_DebugSubsystem::EnableUnitDebugDisplay()
{
	// 检查是否已启用
	if (bDebugDisplayEnabled)
	{
		// 输出警告
		UE_LOG(LogSGGameplay, Warning, TEXT("调试显示已启用，跳过"));
		return;
	}
	
	// 输出日志
	UE_LOG(LogSGGameplay, Log, TEXT("========== 启用单位属性调试显示 =========="));
	
	// 标记为已启用
	bDebugDisplayEnabled = true;
	
	// 为所有现有单位添加调试 Widget
	AddDebugWidgetToAllUnits();
	
	// 从配置读取是否自动监听新单位
	const USG_DebugSettings* Settings = GetDebugSettings();
	if (Settings && Settings->bAutoAddToNewUnits)
	{
		// 开始监听新单位生成
		StartListeningForUnitSpawns();
		UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 已开始监听新单位生成"));
	}
	
	// 输出日志
	UE_LOG(LogSGGameplay, Log, TEXT("✓ 已为 %d 个单位添加调试显示"), UnitWidgetMap.Num());
	UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
}

/**
 * @brief 禁用单位属性显示
 * @details
 * 功能说明：
 * - 移除所有单位的调试 Widget
 * - 停止监听新单位生成
 * - 标记调试显示为已禁用状态
 * 执行流程：
 * 1. 检查是否已禁用
 * 2. 标记为已禁用
 * 3. 停止监听单位生成
 * 4. 移除所有 Widget
 * 5. 输出日志
 */
void USG_DebugSubsystem::DisableUnitDebugDisplay()
{
	// 检查是否已禁用
	if (!bDebugDisplayEnabled)
	{
		// 输出警告
		UE_LOG(LogSGGameplay, Warning, TEXT("调试显示已禁用，跳过"));
		return;
	}
	
	// 输出日志
	UE_LOG(LogSGGameplay, Log, TEXT("========== 禁用单位属性调试显示 =========="));
	
	// 标记为已禁用
	bDebugDisplayEnabled = false;
	
	// 停止监听新单位生成
	StopListeningForUnitSpawns();
	UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 已停止监听新单位生成"));
	
	// 移除所有调试 Widget
	RemoveAllDebugWidgets();
	
	// 输出日志
	UE_LOG(LogSGGameplay, Log, TEXT("✓ 已移除所有调试显示"));
	UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
}

/**
 * @brief 切换单位属性显示
 * @details
 * 功能说明：
 * - 根据当前状态自动切换启用/禁用
 * 实现逻辑：
 * - 如果已启用则调用 DisableUnitDebugDisplay()
 * - 如果已禁用则调用 EnableUnitDebugDisplay()
 */
void USG_DebugSubsystem::ToggleUnitDebugDisplay()
{
	// 根据当前状态切换
	if (bDebugDisplayEnabled)
	{
		// 禁用
		DisableUnitDebugDisplay();
	}
	else
	{
		// 启用
		EnableUnitDebugDisplay();
	}
}

// ========== 单位 Widget 管理实现 ==========

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
 * 2. 检查是否已添加
 * 3. 从配置读取参数
 * 4. 加载 Widget 类
 * 5. 创建 WidgetComponent
 * 6. 配置显示参数
 * 7. 附加到单位
 * 8. 创建并绑定 Widget
 * 9. 保存到映射表
 * 错误处理：
 * - 单位无效：静默返回
 * - 已添加：输出日志并跳过
 * - 配置无效：输出错误并返回
 * - Widget 类无效：输出错误并返回
 * - 组件创建失败：输出错误并返回
 */
void USG_DebugSubsystem::AddDebugWidgetToUnit(ASG_UnitsBase* Unit)
{
	// 检查单位是否有效
	if (!Unit || !IsValid(Unit))
	{
		return;
	}
	
	// 检查是否已添加
	if (UnitWidgetMap.Contains(Unit))
	{
		// 输出日志
		UE_LOG(LogSGGameplay, Verbose, TEXT("单位 %s 已有调试显示，跳过"), *Unit->GetName());
		return;
	}
	
	// 从配置读取设置
	const USG_DebugSettings* Settings = GetDebugSettings();
	if (!Settings)
	{
		// 输出错误
		UE_LOG(LogSGGameplay, Error, TEXT("❌ 无法获取调试配置！"));
		return;
	}

	// 加载 Widget 类
	TSubclassOf<USG_UnitDebugWidget> WidgetClass = Settings->DebugWidgetClass.LoadSynchronous();
	if (!WidgetClass)
	{
		// 输出错误
		UE_LOG(LogSGGameplay, Error, TEXT("❌ 无法加载 DebugWidgetClass！"));
		UE_LOG(LogSGGameplay, Error, TEXT("  路径：%s"), *Settings->DebugWidgetClass.ToString());
		UE_LOG(LogSGGameplay, Error, TEXT("  请在项目设置中检查配置"));
		return;
	}
	
	// 创建 WidgetComponent
	UWidgetComponent* WidgetComp = NewObject<UWidgetComponent>(Unit, UWidgetComponent::StaticClass());
	if (!WidgetComp)
	{
		// 输出错误
		UE_LOG(LogSGGameplay, Error, TEXT("❌ 创建 WidgetComponent 失败"));
		return;
	}
	
	// 注册组件
	WidgetComp->RegisterComponent();
	
	// 设置 Widget 类
	WidgetComp->SetWidgetClass(WidgetClass);
	
	// 设置显示模式为屏幕空间（始终面向摄像机）
	WidgetComp->SetWidgetSpace(EWidgetSpace::Screen);
	
	// 设置绘制大小（从配置读取）
	WidgetComp->SetDrawSize(Settings->WidgetDrawSize);
	
	// 附加到单位根组件
	WidgetComp->AttachToComponent(
		Unit->GetRootComponent(),
		FAttachmentTransformRules::KeepRelativeTransform
	);
	
	// 设置相对位置（头顶偏移，从配置读取）
	WidgetComp->SetRelativeLocation(FVector(0.0f, 0.0f, Settings->WidgetHeightOffset));
	
	// 获取 Widget 实例
	USG_UnitDebugWidget* DebugWidget = Cast<USG_UnitDebugWidget>(WidgetComp->GetWidget());
	if (DebugWidget)
	{
		// 绑定到单位
		DebugWidget->BindToUnit(Unit);
		
		// 输出日志
		UE_LOG(LogSGGameplay, Verbose, TEXT("✓ 为单位 %s 添加调试显示"), *Unit->GetName());
	}
	else
	{
		// 输出警告
		UE_LOG(LogSGGameplay, Warning, TEXT("⚠️ Widget 实例创建失败或类型不匹配"));
	}
	
	// 保存到映射表
	UnitWidgetMap.Add(Unit, WidgetComp);
}

/**
 * @brief 移除单位的调试 Widget
 * @param Unit 目标单位
 * @details
 * 功能说明：
 * - 销毁单位的调试 Widget 组件
 * - 从映射表中移除记录
 * 执行流程：
 * 1. 检查单位有效性
 * 2. 从映射表查找 Widget
 * 3. 销毁组件
 * 4. 从映射表移除
 * 5. 输出日志
 * 错误处理：
 * - 单位无效：静默返回
 * - 未找到 Widget：静默返回
 */
void USG_DebugSubsystem::RemoveDebugWidgetFromUnit(ASG_UnitsBase* Unit)
{
	// 检查单位是否有效
	if (!Unit)
	{
		return;
	}
	
	// 查找 Widget 组件
	TObjectPtr<UWidgetComponent>* FoundWidget = UnitWidgetMap.Find(Unit);
	if (!FoundWidget || !(*FoundWidget))
	{
		return;
	}
	
	// 销毁组件
	(*FoundWidget)->DestroyComponent();
	
	// 从映射表中移除
	UnitWidgetMap.Remove(Unit);
	
	// 输出日志
	UE_LOG(LogSGGameplay, Verbose, TEXT("✓ 已移除单位 %s 的调试显示"), *Unit->GetName());
}

// ========== 事件回调实现 ==========

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
 * 5. 输出日志
 * 性能优化：
 * - 尽早返回，避免不必要的类型转换
 * - 使用 Cast 而不是 IsA，一次完成检查和转换
 */
void USG_DebugSubsystem::OnActorSpawned(AActor* Actor)
{
	// 检查 Actor 是否有效
	if (!Actor || !IsValid(Actor))
	{
		return;
	}
	
	// 检查调试显示是否启用
	if (!bDebugDisplayEnabled)
	{
		return;
	}
	
	// 尝试转换为单位类型
	ASG_UnitsBase* Unit = Cast<ASG_UnitsBase>(Actor);
	
	// 如果是单位类型
	if (Unit)
	{
		// 输出日志
		UE_LOG(LogSGGameplay, Log, TEXT("🆕 检测到新单位生成：%s"), *Unit->GetName());
		
		// 为新单位添加调试 Widget
		AddDebugWidgetToUnit(Unit);
	}
}

// ========== 内部辅助函数实现 ==========

/**
 * @brief 获取调试配置
 * @return 配置实例
 * @details
 * 功能说明：
 * - 提供快捷访问配置的接口
 * - 封装配置获取逻辑
 * 实现细节：
 * - 调用 USG_DebugSettings::Get() 获取单例
 */
const USG_DebugSettings* USG_DebugSubsystem::GetDebugSettings() const
{
	return USG_DebugSettings::Get();
}

/**
 * @brief 为所有现有单位添加调试 Widget
 * @details
 * 功能说明：
 * - 查找场景中所有单位
 * - 遍历并为每个单位添加调试 Widget
 * 执行流程：
 * 1. 获取世界对象
 * 2. 使用 UGameplayStatics::GetAllActorsOfClass() 查找所有单位
 * 3. 输出找到的单位数量
 * 4. 遍历单位列表
 * 5. 为每个单位调用 AddDebugWidgetToUnit()
 * 性能考虑：
 * - GetAllActorsOfClass() 会遍历所有 Actor，有一定开销
 * - 只在启用调试显示时调用一次
 * - 后续新生成的单位通过事件监听处理
 */
void USG_DebugSubsystem::AddDebugWidgetToAllUnits()
{
	// 获取世界对象
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	
	// 获取场景中所有单位
	TArray<AActor*> AllUnits;
	UGameplayStatics::GetAllActorsOfClass(World, ASG_UnitsBase::StaticClass(), AllUnits);
	
	// 输出日志
	UE_LOG(LogSGGameplay, Log, TEXT("找到 %d 个现有单位"), AllUnits.Num());
	
	// 遍历所有单位
	for (AActor* Actor : AllUnits)
	{
		// 转换为单位类型
		ASG_UnitsBase* Unit = Cast<ASG_UnitsBase>(Actor);
		if (Unit)
		{
			// 为单位添加调试 Widget
			AddDebugWidgetToUnit(Unit);
		}
	}
}

/**
 * @brief 移除所有调试 Widget
 * @details
 * 功能说明：
 * - 遍历所有已创建的 Widget
 * - 销毁 Widget 组件
 * - 清空映射表
 * 执行流程：
 * 1. 遍历 UnitWidgetMap
 * 2. 检查 Widget 组件有效性
 * 3. 调用 DestroyComponent() 销毁
 * 4. 清空 UnitWidgetMap
 * 5. 输出日志
 * 注意事项：
 * - 必须检查组件有效性，避免访问已销毁的对象
 * - 使用 IsValid() 检查对象是否有效
 */
void USG_DebugSubsystem::RemoveAllDebugWidgets()
{
	// 遍历所有 Widget 组件
	for (auto& Pair : UnitWidgetMap)
	{
		// 获取 Widget 组件
		UWidgetComponent* WidgetComp = Pair.Value;
		if (WidgetComp && IsValid(WidgetComp))
		{
			// 销毁组件
			WidgetComp->DestroyComponent();
		}
	}
	
	// 清空映射表
	UnitWidgetMap.Empty();
	
	// 输出日志
	UE_LOG(LogSGGameplay, Verbose, TEXT("✓ 已清理所有调试 Widget"));
}

/**
 * @brief 开始监听世界中的单位生成事件
 * @details
 * 功能说明：
 * - 绑定到世界的 OnActorSpawned 委托
 * - 自动为新生成的单位添加调试 Widget
 * 执行流程：
 * 1. 获取世界对象
 * 2. 检查世界有效性
 * 3. 检查是否已绑定（避免重复）
 * 4. 创建委托并绑定到 OnActorSpawned 函数
 * 5. 保存委托句柄
 * 6. 输出日志
 * 技术细节：
 * - 使用 World->AddOnActorSpawnedHandler() 绑定
 * - 使用 FDelegate::CreateUObject() 创建委托
 * - 委托句柄用于后续解绑
 * 错误处理：
 * - 世界无效：输出错误并返回
 * - 已绑定：输出警告并跳过
 */
void USG_DebugSubsystem::StartListeningForUnitSpawns()
{
	// 获取世界对象
	UWorld* World = GetWorld();
	if (!World)
	{
		// 输出错误
		UE_LOG(LogSGGameplay, Error, TEXT("❌ 无法获取 World，监听失败"));
		return;
	}
	
	// 检查是否已绑定
	if (ActorSpawnedDelegateHandle.IsValid())
	{
		// 输出警告
		UE_LOG(LogSGGameplay, Warning, TEXT("⚠️ 已在监听单位生成，跳过重复绑定"));
		return;
	}
	
	// 绑定到世界的 OnActorSpawned 委托
	// 当任何 Actor 生成时，都会调用 OnActorSpawned 函数
	ActorSpawnedDelegateHandle = World->AddOnActorSpawnedHandler(
		FOnActorSpawned::FDelegate::CreateUObject(this, &USG_DebugSubsystem::OnActorSpawned)
	);
	
	// 输出日志
	UE_LOG(LogSGGameplay, Log, TEXT("✓ 开始监听单位生成事件"));
}

/**
 * @brief 停止监听世界中的单位生成事件
 * @details
 * 功能说明：
 * - 解绑 OnActorSpawned 委托
 * - 清理委托句柄
 * 执行流程：
 * 1. 获取世界对象
 * 2. 检查世界有效性
 * 3. 检查委托句柄有效性
 * 4. 调用 World->RemoveOnActorSpawnedHandler() 解绑
 * 5. 重置委托句柄
 * 6. 输出日志
 * 注意事项：
 * - 必须在世界销毁前解绑
 * - 使用 Reset() 清空句柄，避免悬空引用
 */
void USG_DebugSubsystem::StopListeningForUnitSpawns()
{
	// 获取世界对象
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	
	// 检查是否已绑定
	if (!ActorSpawnedDelegateHandle.IsValid())
	{
		return;
	}
	
	// 解绑委托
	World->RemoveOnActorSpawnedHandler(ActorSpawnedDelegateHandle);
	
	// 清空句柄
	ActorSpawnedDelegateHandle.Reset();
	
	// 输出日志
	UE_LOG(LogSGGameplay, Verbose, TEXT("✓ 已停止监听单位生成"));
}
