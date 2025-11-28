// Copyright (C) 2024 Sguo Project. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Strategies/SG_StrategyEffectBase.h"
#include "SG_RollingLogsEffect.generated.h"

// ============================================================================
// 前向声明
// ============================================================================
class ASG_RollingLog;
class ASG_FrontLineManager;
class ASG_MainCityBase;
class UNiagaraSystem;
class USoundBase;

/**
 * @class ASG_RollingLogsEffect
 * @brief 流木计策略效果 - 管理滚动木桩的持续生成
 * 
 * @details 功能说明:
 *          本类实现「流木计」策略卡的完整效果逻辑。
 *          激活后在指定持续时间内（默认6秒），按设定间隔持续生成滚动木桩。
 *          木桩从释放者主城方向生成，沿直线滚向敌方主城方向。
 *          生成位置在战场Y轴范围内随机分布。
 * 
 * @details 详细流程:
 *          1. ExecuteEffect() - 开始效果，启动木桩生成定时器
 *          2. SpawnRollingLog() - 定时调用，在随机位置生成木桩
 *          3. CalculateSpawnPosition() - 计算合法的随机生成位置
 *          4. CalculateMoveDirection() - 计算木桩移动方向（朝向敌方主城）
 *          5. EndEffect() - 效果结束，停止生成，清理资源
 * 
 * @note    使用注意:
 *          - 需要场景中存在 ASG_FrontLineManager
 *          - 需要双方主城正确配置FactionTag
 *          - 木桩生成数量受 SpawnInterval 和 EffectDuration 控制
 * 
 * @see     ASG_RollingLog
 * @see     ASG_StrategyEffectBase
 * @see     ASG_FrontLineManager
 */
UCLASS(Blueprintable, BlueprintType)
class SGUO_API ASG_RollingLogsEffect : public ASG_StrategyEffectBase
{
	GENERATED_BODY()
	
public:
	// ========================================================================
	// 构造函数
	// ========================================================================
	
	/**
	 * @brief 默认构造函数
	 */
	ASG_RollingLogsEffect();

protected:
	// ========================================================================
	// 生命周期重写
	// ========================================================================
	
	/**
	 * @brief Actor开始播放时调用
	 */
	virtual void BeginPlay() override;

	/**
	 * @brief Actor结束播放时调用
	 * @param EndPlayReason - 结束原因
	 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	// ========================================================================
	// 策略效果接口重写
	// ========================================================================
	
	/**
	 * @brief 执行效果
	 * 
	 * @details 详细流程:
	 *          1. 调用父类ExecuteEffect
	 *          2. 查找并缓存FrontLineManager和主城引用
	 *          3. 计算生成区域边界
	 *          4. 启动木桩生成定时器
	 *          5. 设置效果结束定时器
	 * 
	 * @note    如果找不到必要的管理器或主城，效果将提前终止
	 */
	virtual void ExecuteEffect() override;

	/**
	 * @brief 结束效果
	 * 
	 * @details 详细流程:
	 *          1. 停止生成定时器
	 *          2. 清理所有仍存活的木桩引用
	 *          3. 播放效果结束特效
	 *          4. 调用父类EndEffect
	 */
	virtual void EndEffect() override;

protected:
	// ========================================================================
	// 木桩配置
	// ========================================================================
	
	/** ✨ 新增: 木桩Actor类 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rolling Logs|Spawn", 
		meta = (DisplayName = "木桩类"))
	TSubclassOf<ASG_RollingLog> RollingLogClass;

	/** ✨ 新增: 木桩生成间隔（秒） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rolling Logs|Spawn", 
		meta = (DisplayName = "生成间隔", ClampMin = "0.1", ClampMax = "5.0"))
	float SpawnInterval = 0.5f;

	/** ✨ 新增: 首次生成延迟（秒） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rolling Logs|Spawn", 
		meta = (DisplayName = "首次延迟", ClampMin = "0.0", ClampMax = "2.0"))
	float InitialDelay = 0.0f;

	/** ✨ 新增: 每次生成的木桩数量 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rolling Logs|Spawn", 
		meta = (DisplayName = "每次生成数量", ClampMin = "1", ClampMax = "5"))
	int32 LogsPerSpawn = 1;

	/** ✨ 新增: 最大同时存在木桩数量（0表示不限制） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rolling Logs|Spawn", 
		meta = (DisplayName = "最大同时数量", ClampMin = "0", ClampMax = "50"))
	int32 MaxSimultaneousLogs = 20;

	// ========================================================================
	// 生成区域配置
	// ========================================================================
	
	/** ✨ 新增: 生成区域Y轴半宽度（战场宽度的一半） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rolling Logs|Area", 
		meta = (DisplayName = "区域Y半宽", ClampMin = "100.0", ClampMax = "5000.0"))
	float SpawnAreaHalfWidth = 1500.0f;

	/** ✨ 新增: 生成位置X轴偏移（相对于己方主城） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rolling Logs|Area", 
		meta = (DisplayName = "X轴偏移", ClampMin = "0.0", ClampMax = "2000.0"))
	float SpawnOffsetFromMainCity = 300.0f;

	/** ✨ 新增: 生成高度偏移（相对于地面） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rolling Logs|Area", 
		meta = (DisplayName = "高度偏移", ClampMin = "0.0", ClampMax = "500.0"))
	float SpawnHeightOffset = 50.0f;

	/** ✨ 新增: Y轴生成位置抖动范围（增加随机性） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rolling Logs|Area", 
		meta = (DisplayName = "Y轴抖动范围", ClampMin = "0.0", ClampMax = "500.0"))
	float SpawnYJitter = 100.0f;

	// ========================================================================
	// 伤害配置
	// ========================================================================
	
	/** ✨ 新增: 伤害GameplayEffect类 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rolling Logs|Damage", 
		meta = (DisplayName = "伤害效果类"))
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	/** ✨ 新增: 击退GameplayEffect类 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rolling Logs|Damage", 
		meta = (DisplayName = "击退效果类"))
	TSubclassOf<UGameplayEffect> KnockbackEffectClass;

	// ========================================================================
	// 视觉效果配置
	// ========================================================================
	
	/** ✨ 新增: 效果开始特效 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rolling Logs|VFX", 
		meta = (DisplayName = "开始特效"))
	UNiagaraSystem* EffectStartVFX;

	/** ✨ 新增: 效果结束特效 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rolling Logs|VFX", 
		meta = (DisplayName = "结束特效"))
	UNiagaraSystem* EffectEndVFX;

	/** ✨ 新增: 生成提示特效（每次生成木桩时播放） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rolling Logs|VFX", 
		meta = (DisplayName = "生成特效"))
	UNiagaraSystem* SpawnVFX;

	// ========================================================================
	// 音效配置
	// ========================================================================
	
	/** ✨ 新增: 效果开始音效 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rolling Logs|SFX", 
		meta = (DisplayName = "开始音效"))
	USoundBase* EffectStartSound;

	/** ✨ 新增: 效果结束音效 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rolling Logs|SFX", 
		meta = (DisplayName = "结束音效"))
	USoundBase* EffectEndSound;

	// ========================================================================
	// 调试配置
	// ========================================================================
	
	/** ✨ 新增: 是否显示调试信息 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rolling Logs|Debug", 
		meta = (DisplayName = "显示调试"))
	bool bShowDebug = false;

	/** ✨ 新增: 调试绘制颜色 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rolling Logs|Debug", 
		meta = (DisplayName = "调试颜色"))
	FColor DebugColor = FColor::Yellow;

	// ========================================================================
	// 运行时状态
	// ========================================================================
	
	/** 生成定时器句柄 */
	FTimerHandle SpawnTimerHandle;

	/** 效果结束定时器句柄 */
	FTimerHandle EffectEndTimerHandle;

	/** 缓存的FrontLineManager引用 */
	UPROPERTY()
	TWeakObjectPtr<ASG_FrontLineManager> CachedFrontLineManager;

	/** 缓存的己方主城引用 */
	UPROPERTY()
	TWeakObjectPtr<ASG_MainCityBase> CachedFriendlyMainCity;

	/** 缓存的敌方主城引用 */
	UPROPERTY()
	TWeakObjectPtr<ASG_MainCityBase> CachedEnemyMainCity;

	/** 当前存活的木桩列表 */
	UPROPERTY()
	TArray<TWeakObjectPtr<ASG_RollingLog>> ActiveLogs;

	/** 已生成的木桩总数 */
	int32 TotalSpawnedCount = 0;

	/** 效果是否正在运行 */
	bool bIsEffectRunning = false;

	/** 计算出的生成位置X坐标 */
	float CalculatedSpawnX = 0.0f;

	/** 计算出的移动方向 */
	FVector CalculatedMoveDirection = FVector::ForwardVector;

	// ========================================================================
	// 内部函数
	// ========================================================================
	
	/**
	 * @brief 查找并缓存必要的场景引用
	 * @return true表示所有引用都已找到
	 * 
	 * @details 详细流程:
	 *          1. 查找FrontLineManager
	 *          2. 查找己方主城（与发起者阵营相同）
	 *          3. 查找敌方主城（与发起者阵营不同）
	 */
	bool FindAndCacheReferences();

	/**
	 * @brief 计算生成区域参数
	 * 
	 * @details 详细流程:
	 *          1. 根据己方主城位置计算生成X坐标
	 *          2. 根据主城朝向计算移动方向
	 */
	void CalculateSpawnAreaParameters();

	/**
	 * @brief 生成木桩的定时器回调
	 * 
	 * @details 详细流程:
	 *          1. 检查是否超过最大同时数量
	 *          2. 清理已销毁的木桩引用
	 *          3. 按配置数量生成新木桩
	 */
	void OnSpawnTimerTick();

	/**
	 * @brief 生成单个木桩
	 * @return 生成的木桩Actor，失败返回nullptr
	 * 
	 * @details 详细流程:
	 *          1. 计算随机生成位置
	 *          2. 生成木桩Actor
	 *          3. 初始化木桩参数
	 *          4. 绑定销毁事件
	 *          5. 添加到存活列表
	 */
	ASG_RollingLog* SpawnSingleLog();

	/**
	 * @brief 计算随机生成位置
	 * @return 生成位置世界坐标
	 * 
	 * @details 详细流程:
	 *          1. X坐标使用计算好的固定值
	 *          2. Y坐标在[-SpawnAreaHalfWidth, SpawnAreaHalfWidth]范围内随机
	 *          3. Z坐标为地面高度+偏移
	 */
	FVector CalculateSpawnPosition() const;

	/**
	 * @brief 木桩销毁回调
	 * @param DestroyedLog - 被销毁的木桩
	 */
	UFUNCTION()
	void OnRollingLogDestroyed(ASG_RollingLog* DestroyedLog);

	/**
	 * @brief 清理无效的木桩引用
	 */
	void CleanupInvalidLogs();

	/**
	 * @brief 播放效果开始视觉
	 */
	void PlayEffectStartVisuals();

	/**
	 * @brief 播放效果结束视觉
	 */
	void PlayEffectEndVisuals();

	/**
	 * @brief 绘制调试信息
	 */
	void DrawDebugVisualization();

	/**
	 * @brief 获取地面高度
	 * @param XY位置 - X和Y坐标
	 * @return 地面Z坐标
	 */
	float GetGroundHeight(const FVector2D& XYPosition) const;
};
