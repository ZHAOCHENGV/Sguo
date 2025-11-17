// ✨ NEW FILE - 日志系统优化
// Copyright notice placeholder
/**
 * @file SG_LogCategories.h
 * @brief 游戏日志分类定义
 * @details
 * 功能说明：
 * - 定义游戏各模块的日志类别，便于过滤和调试
 * - 支持运行时通过命令行调整日志级别
 * 使用方式：
 * - 在代码中使用：UE_LOG(LogSGCard, Log, TEXT("消息"))
 * - 命令行过滤：Log LogSGCard Verbose
 * - 配置文件设置：DefaultEngine.ini -> [Core.Log]
 * 注意事项：
 * - 所有模块都应使用对应的日志类别，避免使用 LogTemp
 */
#pragma once

#include "CoreMinimal.h"

// ✨ NEW - 卡牌系统日志类别
// 用于卡牌抽取、使用、选中等相关日志
DECLARE_LOG_CATEGORY_EXTERN(LogSGCard, Log, All);

// ✨ NEW - 资产管理日志类别
// 用于资产加载、缓存、卸载等相关日志
DECLARE_LOG_CATEGORY_EXTERN(LogSGAsset, Log, All);

// ✨ NEW - UI 系统日志类别
// 用于 UI 初始化、ViewModel 更新、事件绑定等相关日志
DECLARE_LOG_CATEGORY_EXTERN(LogSGUI, Log, All);

// ✨ NEW - 游戏玩法日志类别
// 用于单位生成、战斗、技能等相关日志
DECLARE_LOG_CATEGORY_EXTERN(LogSGGameplay, Log, All);
