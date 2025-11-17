// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "SG_Player.generated.h"

/**
 * @brief 玩家摄像机 Pawn 声明
 * @details
 * 功能说明：
 * - 使用增强输入进行 WASD 平移和鼠标滚轮缩放。
 * - 广播确认/取消事件供蓝图订阅。
 * 详细流程：
 * 1. 构造函数创建组件并设置初始值。
 * 2. `BeginPlay` 裁剪目标相机距离。
 * 3. `SetupPlayerInputComponent` 绑定增强输入。
 * 4. `Tick` 中插值镜头距离与执行移动。
 * 注意事项：
 * - 蓝图需指定输入动作与映射上下文。
 */

struct FInputActionValue;
class UCameraComponent;
class USpringArmComponent;
class UFloatingPawnMovement;
class UInputAction;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSGPlayerSimpleInputSignature);

UCLASS()
class SGUO_API ASG_Player : public APawn
{
	GENERATED_BODY()

public:
	/** @brief 构造函数 */
	ASG_Player();

protected:
	/** @brief 生命周期开始 */
	virtual void BeginPlay() override;

public:	
	/** @brief 每帧更新 */
	virtual void Tick(float DeltaTime) override;
	/** @brief 绑定增强输入组件 */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

public:
	// 确认事件广播（左键）
	UPROPERTY(BlueprintAssignable, Category = "Input")
	FSGPlayerSimpleInputSignature OnConfirmInput;

	// 取消事件广播（右键）
	UPROPERTY(BlueprintAssignable, Category = "Input")
	FSGPlayerSimpleInputSignature OnCancelInput;

protected:
	// 摄像机臂组件（充当根组件）
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> CameraBoom;

	// 顶视摄像机组件
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> TopDownCamera;

	// 平面移动组件
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UFloatingPawnMovement> FloatingMovement;

	// 相机最小距离
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	float MinCameraDistance = 600.0f;

	// 相机最大距离
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	float MaxCameraDistance = 2000.0f;

	// 缩放步长
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	float CameraZoomStep = 120.0f;

	// 缩放插值速度
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	float CameraZoomInterpSpeed = 10.0f;

	// 增强输入动作：移动
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Enhanced", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> MoveAction;

	// 增强输入动作：缩放
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Enhanced", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> ZoomAction;

	
	// 🔧 MODIFIED - 确认输入（左键点击）
	/**
	 * @brief 确认输入动作（左键点击）
	 * @details
	 * 使用场景：
	 * - 确认卡牌放置
	 * - 确认目标选择
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Enhanced", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> ConfirmAction;

	// 🔧 MODIFIED - 取消输入（右键点击）
	/**
	 * @brief 取消输入动作（右键点击）
	 * @details
	 * 使用场景：
	 * - 取消卡牌放置
	 * - 取消选中
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Enhanced", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> CancelAction;


	// 增强输入动作：重置相机距离
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Enhanced", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> ResetCameraAction;

private:
	/** @brief 处理移动输入 */
	void HandleMoveInput(const FInputActionValue& Value);
	/** @brief 移动输入结束 */
	void HandleMoveCompleted(const FInputActionValue& Value);
	/** @brief 处理缩放输入 */
	void HandleZoomInput(const FInputActionValue& Value);
	/** @brief 处理确认输入 */
	void HandleConfirmInput(const FInputActionValue& Value);
	/** @brief 处理取消输入 */
	void HandleCancelInput(const FInputActionValue& Value);
	/** @brief 处理相机重置输入 */
	void HandleResetCameraInput(const FInputActionValue& Value);

	/** @brief 应用移动 */
	void ApplyMovement(float DeltaTime);
	/** @brief 更新缩放距离 */
	void UpdateCameraZoom(float ScrollDelta);

private:
	// 缓存的平面移动输入
	FVector2D CachedMoveInput = FVector2D::ZeroVector;
	// 默认相机距离
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	float DefaultCameraDistance = 1000.0f;
	// 当前目标相机距离
	float TargetCameraDistance = 1000.0f;
	// 初始角色位置
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	FVector DefaultActorLocation = FVector::ZeroVector;
};
