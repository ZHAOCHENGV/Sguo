// ✨ 新增 - AI 控制器基类
/**
 * @file SG_AIControllerBase.h
 * @brief AI 控制器基类
 * @details
 * 功能说明：
 * - 管理 AI 单位的行为树和黑板
 * - 提供目标查找、仇恨管理等接口
 * - 处理主城特殊逻辑（火矢计打断）
 * 使用方式：
 * 1. 在单位 Blueprint 中设置 AI Controller Class 为此类
 * 2. 配置行为树资产
 * 3. AI 将自动运行
 */

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "SG_AIControllerBase.generated.h"

// 前置声明
class UBehaviorTree;
class UBlackboardComponent;
class ASG_UnitsBase;

/**
 * @brief AI 控制器基类
 * @details
 * 功能说明：
 * - 运行行为树控制 AI 行为
 * - 管理黑板数据
 * - 提供目标查找和仇恨管理接口
 */
UCLASS()
class SGUO_API ASG_AIControllerBase : public AAIController
{
	GENERATED_BODY()

public:
	// ========== 构造函数 ==========
	
	/**
	 * @brief 构造函数
	 * @details
	 * 功能说明：
	 * - 初始化 AI 组件
	 * - 配置默认参数
	 */
	ASG_AIControllerBase();

	// ========== 生命周期 ==========
	
	/**
	 * @brief 开始游戏时调用
	 * @details
	 * 功能说明：
	 * - 启动行为树
	 * - 初始化黑板数据
	 */
	virtual void BeginPlay() override;

	/**
	 * @brief 控制 Pawn 时调用
	 * @param InPawn 被控制的 Pawn
	 * @details
	 * 功能说明：
	 * - 初始化 AI 逻辑
	 * - 启动行为树
	 */
	virtual void OnPossess(APawn* InPawn) override;

	// ========== 行为树配置 ==========
	
	/**
	 * @brief 行为树资产
	 * @details
	 * 功能说明：
	 * - AI 的决策逻辑
	 * - 在 Blueprint 中配置不同单位的行为树
	 * 配置示例：
	 * - 步兵：BT_Infantry
	 * - 骑兵：BT_Cavalry
	 * - 弓兵：BT_Archer
	 * - 主城：BT_MainCity
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AI", meta = (DisplayName = "行为树"))
	TObjectPtr<UBehaviorTree> BehaviorTreeAsset;

	// ========== 目标管理 ==========
	
	/**
	 * @brief 查找最近的目标
	 * @return 最近的敌方单位或主城
	 * @details
	 * 功能说明：
	 * - 优先查找最近的敌方单位（人形或兵器）
	 * - 如果没有单位，查找敌方主城
	 * 详细流程：
	 * 1. 获取所有敌方单位
	 * 2. 计算距离，找到最近的
	 * 3. 如果没有单位，查找主城
	 * 注意事项：
	 * - 只查找不同阵营的目标
	 * - 排除已死亡的单位
	 */
	UFUNCTION(BlueprintCallable, Category = "AI")
	AActor* FindNearestTarget();

	/**
	 * @brief 检测周边威胁
	 * @param DetectionRadius 检测半径
	 * @return 是否发现新威胁
	 * @details
	 * 功能说明：
	 * - 在行军或攻击主城时，检测周边是否有新目标
	 * - 如果发现新目标，转移仇恨
	 * 详细流程：
	 * 1. 获取检测范围内的所有敌方单位
	 * 2. 排除当前目标
	 * 3. 如果有新目标，更新黑板
	 * 注意事项：
	 * - 只在攻击主城或移动时检测
	 * - 检测半径可配置
	 */
	UFUNCTION(BlueprintCallable, Category = "AI")
	bool DetectNearbyThreats(float DetectionRadius = 800.0f);

	/**
	 * @brief 设置当前目标
	 * @param NewTarget 新目标
	 * @details
	 * 功能说明：
	 * - 更新黑板中的目标数据
	 * - 通知行为树目标已改变
	 */
	UFUNCTION(BlueprintCallable, Category = "AI")
	void SetCurrentTarget(AActor* NewTarget);

	/**
	 * @brief 获取当前目标
	 * @return 当前目标
	 * @details
	 * 功能说明：
	 * - 从黑板读取当前目标
	 */
	UFUNCTION(BlueprintPure, Category = "AI")
	AActor* GetCurrentTarget() const;

	/**
	 * @brief 检查目标是否有效
	 * @return 目标是否有效
	 * @details
	 * 功能说明：
	 * - 检查目标是否存在、是否存活
	 * - 用于行为树装饰器
	 */
	UFUNCTION(BlueprintPure, Category = "AI")
	bool IsTargetValid() const;

	// ========== 主城特殊逻辑 ==========
	
	/**
	 * @brief 是否为主城 AI
	 * @details
	 * 功能说明：
	 * - 标识此 AI 是否控制主城
	 * - 主城有特殊的攻击逻辑（弓箭手、火矢计）
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AI|Main City", meta = (DisplayName = "是否为主城"))
	bool bIsMainCity = false;

	/**
	 * @brief 主城攻击被打断
	 * @details
	 * 功能说明：
	 * - 火矢计施放时，打断主城的攻击
	 * - 技能结束后恢复攻击
	 */
	UPROPERTY(BlueprintReadWrite, Category = "AI|Main City", meta = (DisplayName = "攻击被打断"))
	bool bAttackInterrupted = false;

	/**
	 * @brief 打断主城攻击
	 * @details
	 * 功能说明：
	 * - 火矢计施放时调用
	 * - 停止当前攻击行为
	 */
	UFUNCTION(BlueprintCallable, Category = "AI|Main City")
	void InterruptAttack();

	/**
	 * @brief 恢复主城攻击
	 * @details
	 * 功能说明：
	 * - 火矢计结束时调用
	 * - 恢复攻击行为
	 */
	UFUNCTION(BlueprintCallable, Category = "AI|Main City")
	void ResumeAttack();

protected:
	// ========== 黑板键名称（常量）==========
	
	/**
	 * @brief 黑板键：当前目标
	 * @details 存储当前攻击目标的 Actor 引用
	 */
	static const FName BB_CurrentTarget;

	/**
	 * @brief 黑板键：是否在攻击范围内
	 * @details 存储布尔值，表示是否在攻击范围内
	 */
	static const FName BB_IsInAttackRange;

	/**
	 * @brief 黑板键：是否锁定目标
	 * @details 存储布尔值，锁定后不会自动切换目标
	 */
	static const FName BB_IsTargetLocked;

	/**
	 * @brief 黑板键：目标是否为主城
	 * @details 存储布尔值，用于特殊逻辑判断
	 */
	static const FName BB_IsTargetMainCity;
};
