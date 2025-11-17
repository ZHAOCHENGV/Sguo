// 🔧 MODIFIED FILE - 放置预览 Actor
// Copyright notice placeholder
/**
 * @file SG_PlacementPreview.h
 * @brief 卡牌放置预览 Actor
 * @details
 * 功能说明：
 * - 显示卡牌放置的预览效果
 * - 跟随鼠标移动并紧贴地面
 * - 根据是否可放置显示不同颜色
 * 详细流程：
 * 1. 生成时创建预览网格体
 * 2. Tick 中更新位置（射线检测地面）
 * 3. 检测放置有效性（前线、碰撞等）
 * 4. 根据卡牌类型显示不同预览（单点、区域）
 * 注意事项：
 * - 预览 Actor 不参与碰撞
 * - 使用半透明材质显示预览
 */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SG_PlacementPreview.generated.h"

// 前向声明
class UStaticMeshComponent;
class UDecalComponent;
class USG_CardDataBase;
enum class ESGPlacementType : uint8;
class ASG_FrontLineManager;


// ✨ NEW - 碰撞检测方式枚举
/**
 * @brief 碰撞检测方式
 * @details
 * 功能说明：
 * - 定义不同的碰撞检测策略
 * 使用场景：
 * - 在蓝图中选择最适合的检测方式
 */
UENUM(BlueprintType)
enum class ESGCollisionCheckMethod : uint8
{
	// 通道查询（检测特定碰撞通道）
	ByChannel       UMETA(DisplayName = "By Channel"),
	
	// 对象类型查询（检测特定对象类型）
	ByObjectType    UMETA(DisplayName = "By Object Type"),
	
	// 类查询（检测特定 Actor 类）
	ByActorClass    UMETA(DisplayName = "By Actor Class"),
	
	// 距离查询（简单距离计算）
	ByDistance      UMETA(DisplayName = "By Distance")
};



UCLASS()
class SGUO_API ASG_PlacementPreview : public AActor
{
	GENERATED_BODY()
	
public:	
	/**
	 * @brief 构造函数
	 * @details
	 * 功能说明：
	 * - 创建预览组件
	 * - 设置默认属性
	 */
	ASG_PlacementPreview();

protected:
	/**
	 * @brief 生命周期开始
	 * @details
	 * 功能说明：
	 * - 初始化预览材质
	 * - 设置碰撞属性
	 */
	virtual void BeginPlay() override;

public:
	

	
	/**
	 * @brief 每帧更新
	 * @param DeltaTime 帧间隔时间
	 * @details
	 * 功能说明：
	 * - 更新预览位置（跟随鼠标）
	 * - 检测放置有效性
	 * - 更新预览颜色
	 */
	virtual void Tick(float DeltaTime) override;

	// ========== 初始化函数 ==========
	
	/**
	 * @brief 初始化预览 Actor
	 * @param InCardData 卡牌数据
	 * @param InPlayerController 玩家控制器
	 * @details
	 * 功能说明：
	 * - 设置卡牌数据和控制器引用
	 * - 根据卡牌类型创建对应的预览效果
	 * 详细流程：
	 * 1. 保存卡牌数据和控制器引用
	 * 2. 根据放置类型创建预览（单点/区域）
	 * 3. 设置预览材质和颜色
	 */
	UFUNCTION(BlueprintCallable, Category = "Placement")
	void InitializePreview(USG_CardDataBase* InCardData, APlayerController* InPlayerController);

	/**
	 * @brief 检查当前位置是否可以放置
	 * @return 是否可以放置
	 * @details
	 * 功能说明：
	 * - 检测放置位置是否有效
	 * 检查项：
	 * 1. 是否在前线范围内（如果需要）
	 * 2. 是否与其他单位重叠
	 * 3. 是否在可导航区域
	 */
	UFUNCTION(BlueprintCallable, Category = "Placement")
	bool CanPlaceAtCurrentLocation() const;

	/**
	 * @brief 获取当前预览位置
	 * @return 预览位置
	 */
	UFUNCTION(BlueprintCallable, Category = "Placement")
	FVector GetPreviewLocation() const { return PreviewLocation; }

	/**
	 * @brief 获取预览旋转
	 * @return 预览旋转
	 */
	UFUNCTION(BlueprintCallable, Category = "Placement")
	FRotator GetPreviewRotation() const { return PreviewRotation; }

	/**
	 * @brief 是否可以放置
	 */
	bool bCanPlace;
protected:
	// ========== 组件 ==========
	
	/**
	 * @brief 根组件
	 * @details 作为场景组件的根节点
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* RootComp;

	/**
	 * @brief 预览网格体（用于单点放置）
	 * @details 显示单位的预览模型
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* PreviewMesh;

	/**
	 * @brief 区域指示器（用于区域放置）
	 * @details 显示放置区域的范围
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UDecalComponent* AreaIndicator;

	// ========== 预览设置 ==========
	
	/**
	 * @brief 可放置颜色（绿色）
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Preview")
	FLinearColor ValidPlacementColor = FLinearColor::Green;

	/**
	 * @brief 不可放置颜色（红色）
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Preview")
	FLinearColor InvalidPlacementColor = FLinearColor::Red;

	/**
	 * @brief 预览透明度
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Preview")
	float PreviewOpacity = 0.5f;

	/**
	 * @brief 射线检测距离
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Preview")
	float RaycastDistance = 10000.0f;

	/**
	 * @brief 地面偏移高度（避免 Z-Fighting）
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Preview")
	float GroundOffset = 5.0f;

	// ========== 碰撞检测配置 ==========
	
	// ✨ NEW - 碰撞检测方式
	/**
	 * @brief 碰撞检测方式
	 * @details
	 * 功能说明：
	 * - 选择碰撞检测策略
	 * 选项说明：
	 * - By Channel：使用碰撞通道检测（最常用）
	 * - By Object Type：使用对象类型检测（推荐）
	 * - By Actor Class：检测特定 Actor 类（最精确）
	 * - By Distance：简单距离计算（最快）
	 * 推荐设置：
	 * - 默认使用 By Object Type
	 * - 如果有问题改用 By Actor Class
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision Detection", 
		meta = (DisplayName = "检测方式"))
	ESGCollisionCheckMethod CollisionCheckMethod = ESGCollisionCheckMethod::ByObjectType;

	// ✨ NEW - 碰撞检测半径
	/**
	 * @brief 碰撞检测半径
	 * @details
	 * 功能说明：
	 * - 定义碰撞检测的球形范围
	 * - 单位：厘米
	 * 使用建议：
	 * - 单个英雄：50-100
	 * - 兵团：100-150
	 * - 根据实际单位大小调整
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision Detection", 
		meta = (DisplayName = "检测半径", ClampMin = "10.0", UIMin = "10.0", UIMax = "500.0"))
	float CollisionCheckRadius = 100.0f;

	// ✨ NEW - 碰撞通道（By Channel 模式使用）
	/**
	 * @brief 碰撞检测通道
	 * @details
	 * 功能说明：
	 * - 当检测方式为 By Channel 时使用
	 * - 定义检测哪个碰撞通道
	 * 常用通道：
	 * - ECC_Pawn：检测 Pawn
	 * - ECC_WorldDynamic：检测动态物体
	 * - ECC_GameTraceChannel1：自定义通道
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision Detection", 
		meta = (DisplayName = "检测通道", EditCondition = "CollisionCheckMethod == ESGCollisionCheckMethod::ByChannel", EditConditionHides))
	TEnumAsByte<ECollisionChannel> CollisionChannel = ECC_Pawn;

	// ✨ NEW - 对象类型（By Object Type 模式使用）
	/**
	 * @brief 检测的对象类型
	 * @details
	 * 功能说明：
	 * - 当检测方式为 By Object Type 时使用
	 * - 可以选择多个对象类型
	 * 常用类型：
	 * - Pawn：检测所有 Pawn
	 * - WorldDynamic：检测动态物体
	 * - PhysicsBody：检测物理对象
	 * 推荐设置：
	 * - 只勾选 Pawn
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision Detection", 
		meta = (DisplayName = "对象类型", EditCondition = "CollisionCheckMethod == ESGCollisionCheckMethod::ByObjectType", EditConditionHides))
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;

	// ✨ NEW - Actor 类过滤（By Actor Class 模式使用）
	/**
	 * @brief 要检测的 Actor 类
	 * @details
	 * 功能说明：
	 * - 当检测方式为 By Actor Class 时使用
	 * - 只检测指定类及其子类
	 * 使用建议：
	 * - 设置为您的单位基类（如 ASG_UnitsBase）
	 * - 可以添加多个类
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision Detection", 
		meta = (DisplayName = "检测的 Actor 类", EditCondition = "CollisionCheckMethod == ESGCollisionCheckMethod::ByActorClass", EditConditionHides))
	TArray<TSubclassOf<AActor>> ActorClassesToCheck;

	// ✨ NEW - 是否忽略预览 Actor
	/**
	 * @brief 是否忽略其他预览 Actor
	 * @details
	 * 功能说明：
	 * - True：不检测其他预览 Actor（推荐）
	 * - False：检测所有对象
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision Detection", 
		meta = (DisplayName = "忽略预览 Actor"))
	bool bIgnorePreviewActors = true;

	// ✨ NEW - 是否忽略死亡单位
	/**
	 * @brief 是否忽略死亡单位
	 * @details
	 * 功能说明：
	 * - True：不检测即将销毁的单位（推荐）
	 * - False：检测所有单位
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision Detection", 
		meta = (DisplayName = "忽略死亡单位"))
	bool bIgnoreDeadUnits = true;

	// ✨ NEW - 是否启用调试绘制
	/**
	 * @brief 是否启用碰撞检测的可视化调试
	 * @details
	 * 功能说明：
	 * - True：在场景中绘制检测范围
	 * - False：不绘制（性能更好）
	 * 使用场景：
	 * - 开发调试时启用
	 * - 正式发布时禁用
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision Detection", 
		meta = (DisplayName = "启用调试绘制"))
	bool bEnableDebugDraw = true;

	// ========== 运行时数据 ==========
	
	/**
	 * @brief 卡牌数据引用
	 */
	UPROPERTY(Transient)
	USG_CardDataBase* CardData;

	/**
	 * @brief 玩家控制器引用
	 */
	UPROPERTY(Transient)
	APlayerController* PlayerController;

	/**
	 * @brief 当前预览位置
	 */
	FVector PreviewLocation;

	/**
	 * @brief 当前预览旋转
	 */
	FRotator PreviewRotation;



	/**
	 * @brief 预览材质动态实例
	 */
	UPROPERTY(Transient)
	UMaterialInstanceDynamic* PreviewMaterialInstance;

private:
	// ========== 内部函数 ==========
	
	/**
	 * @brief 更新预览位置（射线检测）
	 * @details
	 * 功能说明：
	 * - 从鼠标位置发射射线
	 * - 检测地面位置
	 * - 更新预览 Actor 位置
	 */
	void UpdatePreviewLocation();

	/**
	 * @brief 更新预览颜色
	 * @details
	 * 功能说明：
	 * - 根据是否可放置更新颜色
	 * - 绿色：可放置
	 * - 红色：不可放置
	 */
	void UpdatePreviewColor();

	/**
	 * @brief 检测碰撞
	 * @return 是否有碰撞
	 * @details
	 * 功能说明：
	 * - 在预览位置进行球形检测
	 * - 检查是否与其他单位重叠
	 */
	// 🔧 MODIFIED - 碰撞检测函数（支持多种检测方式）
	bool CheckCollision() const;
	bool CheckCollisionByChannel() const;
	bool CheckCollisionByObjectType() const;
	bool CheckCollisionByActorClass() const;
	bool CheckCollisionByDistance() const;
	/**
	 * @brief 创建单点预览
	 * @details
	 * 功能说明：
	 * - 为单点放置卡牌创建预览网格体
	 */
	void CreateSinglePointPreview();

	/**
	 * @brief 创建区域预览
	 * @details
	 * 功能说明：
	 * - 为区域放置卡牌创建区域指示器
	 */
	void CreateAreaPreview();


	// ✨ NEW - 检查前线限制
	/**
	 * @brief 检查是否违反前线限制
	 * @return 是否违反（true = 违反，不能放置）
	 * @details
	 * 功能说明：
	 * - 检查当前位置是否在前线允许的范围内
	 * - 玩家单位只能在前线左侧（X < 前线 X）
	 * - 计谋卡不受前线限制
	 */
	bool CheckFrontLineViolation() const;
	// ✨ NEW - 缓存的前线管理器
	UPROPERTY(Transient)
	ASG_FrontLineManager* CachedFrontLineManager;



};
