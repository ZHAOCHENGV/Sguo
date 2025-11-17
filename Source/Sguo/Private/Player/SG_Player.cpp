// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/SG_Player.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/PawnMovementComponent.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"

/**
 * @brief 构造函数
 * @details
 * 功能说明：创建必需组件并设置默认参数。
 * 详细流程：
 * 1. 启用 Tick。
 * 2. 创建摄像机臂并设为根组件。
 * 3. 配置摄像机、移动组件与控制器旋转选项。
 * 注意事项：
 * - 摄像机臂禁用碰撞，避免缩放时被阻挡。
 */
ASG_Player::ASG_Player()
{
	// 启用 Tick 以便处理持续输入
	PrimaryActorTick.bCanEverTick = true;
	// 初始化目标距离为默认值并裁剪
	TargetCameraDistance = FMath::Clamp(DefaultCameraDistance, MinCameraDistance, MaxCameraDistance);
	// 同步裁剪结果回默认值
	DefaultCameraDistance = TargetCameraDistance;

	// 创建摄像机臂并作为根组件
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	// 将摄像机臂设置为根组件
	RootComponent = CameraBoom;
	// 固定俯视角度
	CameraBoom->SetUsingAbsoluteRotation(true);
	// 初始化臂长为目标距离
	CameraBoom->TargetArmLength = TargetCameraDistance;
	// 设定俯视角
	CameraBoom->SetRelativeRotation(FRotator(-60.f, 0.f, 0.f));
	// 禁用碰撞检测以免缩放被阻挡
	CameraBoom->bDoCollisionTest = false;

	// 创建俯视摄像机
	TopDownCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));
	// 将摄像机附着到摄像机臂末端
	TopDownCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	// 禁止摄像机跟随控制器旋转
	TopDownCamera->bUsePawnControlRotation = false;

	// 创建平面移动组件
	FloatingMovement = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("FloatingMovement"));
	// 限制移动在水平面
	FloatingMovement->bConstrainToPlane = true;
	// 将平面法线设为 Z 轴
	FloatingMovement->SetPlaneConstraintAxisSetting(EPlaneConstraintAxisSetting::Z);
	// 设置默认最大速度
	FloatingMovement->MaxSpeed = 1200.0f;

	// 禁止控制器直接控制 Pawn 旋转
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	bUseControllerRotationYaw = false;
}

/**
 * @brief Pawn 生命周期开始
 * @details
 * 功能说明：将目标相机距离裁剪在可配置范围，并同步到摄像机臂。
 * 注意事项：确保配置在编辑器中已正确设置。
 */
void ASG_Player::BeginPlay()
{
	// 调用父类 BeginPlay
	Super::BeginPlay();
	// 限制目标距离在可用范围内
	TargetCameraDistance = FMath::Clamp(DefaultCameraDistance, MinCameraDistance, MaxCameraDistance);
	// 同步默认距离到内部缓存
	DefaultCameraDistance = TargetCameraDistance;
	// 将摄像机臂长度同步到目标值
	CameraBoom->TargetArmLength = TargetCameraDistance;
	// 记录初始位置用于重置
	DefaultActorLocation = GetActorLocation();
}

/**
 * @brief 帧更新
 * @param DeltaTime 帧间隔
 * @details
 * 功能说明：插值更新摄像机距离并根据缓存输入执行移动。
 */
void ASG_Player::Tick(float DeltaTime)
{
	// 调用父类 Tick
	Super::Tick(DeltaTime);
	// 平滑插值摄像机臂长度
	const float NewLength = FMath::FInterpTo(CameraBoom->TargetArmLength, TargetCameraDistance, DeltaTime, CameraZoomInterpSpeed);
	// 写回摄像机臂长度
	CameraBoom->TargetArmLength = NewLength;
	// 应用移动输入
	ApplyMovement(DeltaTime);
}

/**
 * @brief 绑定增强输入组件
 * @param PlayerInputComponent 输入组件
 * @details
 * 功能说明：将增强输入动作映射到处理函数。
 * 注意事项：仅在组件成功转换为 `UEnhancedInputComponent` 后绑定。
 */
void ASG_Player::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// 先执行父类的绑定
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	// 尝试转换为增强输入组件
	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// 绑定移动输入触发
		if (MoveAction)
		{
			// 持续触发时更新移动向量
			EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ASG_Player::HandleMoveInput);
			// 结束或取消时清空向量
			EnhancedInput->BindAction(MoveAction, ETriggerEvent::Completed, this, &ASG_Player::HandleMoveCompleted);
			EnhancedInput->BindAction(MoveAction, ETriggerEvent::Canceled, this, &ASG_Player::HandleMoveCompleted);
		}
		// 绑定缩放输入
		if (ZoomAction)
		{
			EnhancedInput->BindAction(ZoomAction, ETriggerEvent::Triggered, this, &ASG_Player::HandleZoomInput);
		}
		// 绑定确认输入
		if (ConfirmAction)
		{
			EnhancedInput->BindAction(ConfirmAction, ETriggerEvent::Started, this, &ASG_Player::HandleConfirmInput);
		}
		// 绑定取消输入
		if (CancelAction)
		{
			EnhancedInput->BindAction(CancelAction, ETriggerEvent::Started, this, &ASG_Player::HandleCancelInput);
		}
		// 绑定相机重置输入
		if (ResetCameraAction)
		{
			EnhancedInput->BindAction(ResetCameraAction, ETriggerEvent::Started, this, &ASG_Player::HandleResetCameraInput);
		}
	}
}

/**
 * @brief 处理移动输入
 * @param Value 输入值
 */
void ASG_Player::HandleMoveInput(const FInputActionValue& Value)
{
	// 缓存二维移动输入
	CachedMoveInput = Value.Get<FVector2D>();
}

/**
 * @brief 移动输入结束
 * @param Value 输入值
 */
void ASG_Player::HandleMoveCompleted(const FInputActionValue& Value)
{
	// 清空移动输入
	CachedMoveInput = FVector2D::ZeroVector;
}

/**
 * @brief 处理缩放输入
 * @param Value 输入值
 */
void ASG_Player::HandleZoomInput(const FInputActionValue& Value)
{
	// 读取滚轮增量
	const float ScrollDelta = Value.Get<float>();
	// 更新相机缩放
	UpdateCameraZoom(ScrollDelta);
}

/**
 * @brief 处理确认输入
 * @param Value 输入值
 */
void ASG_Player::HandleConfirmInput(const FInputActionValue& Value)
{
	// 广播确认事件
	OnConfirmInput.Broadcast();
}

/**
 * @brief 处理取消输入
 * @param Value 输入值
 */
void ASG_Player::HandleCancelInput(const FInputActionValue& Value)
{
	// 广播取消事件
	OnCancelInput.Broadcast();
}

/**
 * @brief 处理相机重置输入
 * @param Value 输入值
 */
void ASG_Player::HandleResetCameraInput(const FInputActionValue& Value)
{
	// 将目标距离重置为默认距离
	TargetCameraDistance = DefaultCameraDistance;
	// 恢复角色到初始位置
	SetActorLocation(DefaultActorLocation);
}

/**
 * @brief 应用平面移动（根据摄像机朝向）
 * @param DeltaTime 帧间隔
 * @details
 * 功能说明：
 * - 🔧 修改 - 根据摄像机的 Yaw 旋转计算移动方向
 * - 将输入向量转换到摄像机坐标系
 * 详细流程：
 * 1. 检查移动组件和输入有效性
 * 2. 获取摄像机的 Yaw 旋转角度
 * 3. 构建旋转矩阵，只考虑 Yaw（水平旋转）
 * 4. 将输入向量转换到摄像机坐标系
 * 5. 应用移动输入
 * 注意事项：
 * - 只使用 Yaw 旋转，忽略 Pitch 和 Roll
 * - 确保移动始终在水平面上
 */
void ASG_Player::ApplyMovement(float DeltaTime)
{
	// 检查移动组件是否有效
	if (!FloatingMovement)
	{
		return;
	}
	
	// 检查输入是否接近零
	if (CachedMoveInput.IsNearlyZero())
	{
		return;
	}

	// 🔧 修改 - 获取摄像机的旋转（只取 Yaw，忽略 Pitch 和 Roll）
	// 获取摄像机臂的世界旋转
	FRotator CameraRotation = CameraBoom->GetComponentRotation();
	// 只保留 Yaw（水平旋转），清除 Pitch 和 Roll
	FRotator CameraYawRotation(0.0f, CameraRotation.Yaw, 0.0f);

	// 🔧 修改 - 计算相对于摄像机的前向和右向
	// 获取摄像机的前向向量（基于 Yaw）
	FVector CameraForward = FRotationMatrix(CameraYawRotation).GetUnitAxis(EAxis::X);
	// 获取摄像机的右向向量（基于 Yaw）
	FVector CameraRight = FRotationMatrix(CameraYawRotation).GetUnitAxis(EAxis::Y);

	// 🔧 修改 - 根据摄像机方向应用移动输入
	// 前后移动（W/S）：沿摄像机前向
	AddMovementInput(CameraForward, CachedMoveInput.Y);
	// 左右移动（A/D）：沿摄像机右向
	AddMovementInput(CameraRight, CachedMoveInput.X);
}

/**
 * @brief 更新相机缩放
 * @param ScrollDelta 鼠标滚轮增量
 */
void ASG_Player::UpdateCameraZoom(float ScrollDelta)
{
	// 滚轮接近零时忽略
	if (FMath::IsNearlyZero(ScrollDelta))
	{
		return;
	}
	// 根据滚轮计算目标距离并裁剪
	TargetCameraDistance = FMath::Clamp(TargetCameraDistance - ScrollDelta * CameraZoomStep, MinCameraDistance, MaxCameraDistance);
}
