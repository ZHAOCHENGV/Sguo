// 📄 文件：Source/Sguo/Private/Units/SG_StationaryUnit.cpp
// ✨ 新增 - 站桩单位类实现

#include "Units/SG_StationaryUnit.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Debug/SG_LogCategories.h"

/**
 * @brief 构造函数
 * @details
 * 功能说明：
 * - 初始化站桩单位的默认配置
 * - 设置默认值
 */
ASG_StationaryUnit::ASG_StationaryUnit()
{
	// 设置默认的站桩配置
	// 默认不浮空，站立在地面
	bEnableHover = false;
	
	// 默认浮空高度 0 厘米
	HoverHeight = 0;
	
	// 默认禁用重力（浮空单位需要）
	bDisableGravity = true;
	
	// 默认可以被选为目标
	bCanBeTargeted = true;
	
	// 默认禁用移动
	bDisableMovement = true;
}

/**
 * @brief 游戏开始时调用
 * @details
 * 功能说明：
 * - 调用父类的 BeginPlay
 * - 应用站桩配置
 */
void ASG_StationaryUnit::BeginPlay()
{
	// 调用父类的 BeginPlay，初始化 GAS、属性等
	Super::BeginPlay();

	// 应用站桩单位的特殊配置
	ApplyStationarySettings();

	// 打印调试日志
	UE_LOG(LogSGUnit, Log, TEXT("[站桩单位] %s 初始化完成 | 浮空:%s | 高度:%.1f | 可被选中:%s | 禁用移动:%s"),
		*GetName(),
		bEnableHover ? TEXT("是") : TEXT("否"),
		HoverHeight,
		bCanBeTargeted ? TEXT("是") : TEXT("否"),
		bDisableMovement ? TEXT("是") : TEXT("否")
	);
}

/**
 * @brief 检查单位是否可被选为目标
 * @return 是否可被选为目标
 * @details
 * 功能说明：
 * - 返回 bCanBeTargeted 配置值
 * - 子类可以重写此函数添加额外逻辑
 */
bool ASG_StationaryUnit::CanBeTargeted() const
{
	// 返回配置的可被选中状态
	return bCanBeTargeted;
}

/**
 * @brief 应用站桩配置
 * @details
 * 功能说明：
 * - 根据配置禁用移动和重力
 * - 调整单位位置（浮空）
 * 详细流程：
 * 1. 禁用移动能力（如果配置要求）
 * 2. 应用浮空效果（如果配置要求）
 */
void ASG_StationaryUnit::ApplyStationarySettings()
{
	// 步骤1：禁用移动能力
	if (bDisableMovement)
	{
		DisableMovementCapability();
	}

	// 步骤2：应用浮空效果
	if (bEnableHover)
	{
		ApplyHoverEffect();
	}
}

/**
 * @brief 禁用移动能力
 * @details
 * 功能说明：
 * - 禁用 CharacterMovement 组件
 * - 设置移动速度为 0
 * 详细流程：
 * 1. 获取 CharacterMovement 组件
 * 2. 检查组件是否有效
 * 3. 设置移动速度为 0
 * 4. 禁用移动组件（可选）
 */
void ASG_StationaryUnit::DisableMovementCapability()
{
	// 获取角色移动组件
	UCharacterMovementComponent* MovementComp = GetCharacterMovement();
    
	// 检查组件是否有效
	if (!MovementComp)
	{
		UE_LOG(LogSGUnit, Warning, TEXT("[站桩单位] %s 的 CharacterMovement 组件无效，无法禁用移动"), *GetName());
		return;
	}

	// 设置最大移动速度为 0（完全禁止移动）
	MovementComp->MaxWalkSpeed = 0.0f;
	MovementComp->MaxAcceleration = 0.0f;
	
    
	// ✨ 新增 - 如果启用浮空，设置为 Flying 模式
	if (bEnableHover || bDisableGravity)
	{
		MovementComp->SetMovementMode(MOVE_Flying);
		MovementComp->GravityScale = 0.0f;
	}
	else
	{
		// 保持 Walking 模式，但速度为 0
		MovementComp->SetMovementMode(MOVE_Walking);
	}
    
	// 禁用导航代理（AI 不会尝试移动此单位）
	MovementComp->bUseRVOAvoidance = false;

	// 打印调试日志
	UE_LOG(LogSGUnit, Verbose, TEXT("[站桩单位] %s 移动能力已禁用（速度=0，模式=%s）"), 
		*GetName(),
		(bEnableHover || bDisableGravity) ? TEXT("Flying") : TEXT("Walking"));
}

/**
 * @brief 应用浮空效果
 * @details
 * 功能说明：
 * - 将单位提升到指定高度
 * - 调整碰撞和物理设置
 * 详细流程：
 * 1. 获取当前位置
 * 2. 计算新的 Z 坐标
 * 3. 设置新位置
 * 4. 禁用重力（如果配置要求）
 */
void ASG_StationaryUnit::ApplyHoverEffect()
{
	// 步骤1：获取当前世界位置
	FVector CurrentLocation = GetActorLocation();
	
	// 步骤2：计算新的 Z 坐标（当前高度 + 浮空高度）
	FVector NewLocation = CurrentLocation;
	NewLocation.Z += HoverHeight;
	
	// 步骤3：设置新位置
	SetActorLocation(NewLocation, false, nullptr, ETeleportType::TeleportPhysics);
	
	// 步骤4：禁用重力（如果配置要求）
	if (bDisableGravity)
	{
		// 获取角色移动组件
		UCharacterMovementComponent* MovementComp = GetCharacterMovement();
		
		// 检查组件是否有效
		if (MovementComp)
		{
			// 禁用重力
			MovementComp->GravityScale = 0.0f;
			
			// 设置移动模式为飞行（浮空）
			MovementComp->SetMovementMode(MOVE_Flying);
		}
	}

	// 打印调试日志
	UE_LOG(LogSGUnit, Verbose, TEXT("[站桩单位] %s 浮空效果已应用 | 原始高度:%.1f | 新高度:%.1f | 偏移:%.1f"),
		*GetName(),
		CurrentLocation.Z,
		NewLocation.Z,
		HoverHeight
	);
}
