// 📄 文件：Source/Sguo/Public/AI/SG_AITypes.h
// 🔧 修改 - 添加一个使用枚举的属性，强制注册

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SG_AITypes.generated.h"

/**
 * @brief 目标锁定状态
 * @details
 * - Searching: 正在寻找目标
 * - Moving: 正在移动到目标
 * - Engaged: 已进入战斗状态（锁定）
 * - Blocked: 被阻挡，无法到达目标
 */
UENUM(BlueprintType)
enum class ESGTargetEngagementState : uint8
{
	Searching   UMETA(DisplayName = "搜索中"),
	Moving      UMETA(DisplayName = "移动中"),
	Engaged     UMETA(DisplayName = "战斗中"),
	Blocked     UMETA(DisplayName = "被阻挡")
};

/**
 * @brief AI 类型辅助类
 * @details 用于确保枚举类型被正确注册到反射系统
 */
UCLASS()
class SGUO_API USG_AITypes : public UObject
{
	GENERATED_BODY()

public:
	// ✨ 新增 - 添加一个使用枚举的属性，确保枚举被反射系统注册
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Types", meta = (DisplayName = "目标状态（占位）"))
	ESGTargetEngagementState DummyState = ESGTargetEngagementState::Searching;
};
