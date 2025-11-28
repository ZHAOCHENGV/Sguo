// Copyright (C) 2024 Sguo Project. All Rights Reserved.

#include "Strategies/SG_RollingLogsEffect.h"

// ============================================================================
// 引擎核心头文件
// ============================================================================
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"

// ============================================================================
// 特效头文件
// ============================================================================
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"

// ============================================================================
// 项目头文件
// ============================================================================
#include "Strategies/SG_RollingLog.h"
#include "GameplayMechanics/SG_FrontLineManager.h"
#include "Buildings/SG_MainCityBase.h"

// ============================================================================
// 构造函数
// ============================================================================

/**
 * @brief 默认构造函数
 * 
 * @details 初始化默认效果持续时间为6秒
 */
ASG_RollingLogsEffect::ASG_RollingLogsEffect()
{
	// ✨ 新增: 设置默认效果持续时间为6秒
	EffectDuration = 6.0f;
}

// ============================================================================
// 生命周期函数
// ============================================================================

/**
 * @brief Actor开始播放时调用
 */
void ASG_RollingLogsEffect::BeginPlay()
{
	// 调用父类实现
	Super::BeginPlay();
}

/**
 * @brief Actor结束播放时调用
 * @param EndPlayReason - 结束原因
 * 
 * @details 详细流程:
 *          1. 确保效果已结束
 *          2. 清理所有定时器
 *          3. 调用父类EndPlay
 */
void ASG_RollingLogsEffect::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// ✨ 新增: 确保效果已结束
	if (bIsEffectRunning)
	{
		EndEffect();
	}

	// ✨ 新增: 清理定时器
	if (UWorld* World = GetWorld())
	{
		// 清理生成定时器
		World->GetTimerManager().ClearTimer(SpawnTimerHandle);
		// 清理效果结束定时器
		World->GetTimerManager().ClearTimer(EffectEndTimerHandle);
	}

	// 调用父类实现
	Super::EndPlay(EndPlayReason);
}

// ============================================================================
// 策略效果接口实现
// ============================================================================

/**
 * @brief 执行效果
 * 
 * @details 详细流程:
 *          1. 调用父类ExecuteEffect
 *          2. 查找并缓存必要引用
 *          3. 计算生成区域参数
 *          4. 播放效果开始视觉
 *          5. 启动生成定时器
 *          6. 设置效果结束定时器
 */
void ASG_RollingLogsEffect::ExecuteEffect()
{
	// ✨ 新增: 调用父类实现
	Super::ExecuteEffect();

	// ✨ 新增: 查找并缓存必要引用
	if (!FindAndCacheReferences())
	{
		// 找不到必要引用，提前终止效果
		UE_LOG(LogTemp, Warning, TEXT("[RollingLogsEffect] 无法找到必要的场景引用，效果终止"));
		EndEffect();
		return;
	}

	// ✨ 新增: 计算生成区域参数
	CalculateSpawnAreaParameters();

	// ✨ 新增: 标记效果开始运行
	bIsEffectRunning = true;
	TotalSpawnedCount = 0;

	// ✨ 新增: 播放效果开始视觉
	PlayEffectStartVisuals();

	// ✨ 新增: 获取World引用
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("[RollingLogsEffect] 无法获取World引用"));
		EndEffect();
		return;
	}

	// ✨ 新增: 启动木桩生成定时器
	// 设置循环定时器，按SpawnInterval间隔调用OnSpawnTimerTick
	World->GetTimerManager().SetTimer(
		SpawnTimerHandle,
		this,
		&ASG_RollingLogsEffect::OnSpawnTimerTick,
		SpawnInterval,
		true,           // 循环
		InitialDelay    // 首次延迟
	);

	// ✨ 新增: 设置效果结束定时器
	World->GetTimerManager().SetTimer(
		EffectEndTimerHandle,
		this,
		&ASG_RollingLogsEffect::EndEffect,
		EffectDuration,
		false           // 不循环
	);

	// ✨ 新增: 记录开始日志
	UE_LOG(LogTemp, Log, TEXT("[RollingLogsEffect] 流木计效果开始 - 持续时间: %.1f秒, 生成间隔: %.2f秒"), 
		EffectDuration, SpawnInterval);
}

/**
 * @brief 结束效果
 * 
 * @details 详细流程:
 *          1. 检查效果是否在运行
 *          2. 停止生成定时器
 *          3. 播放效果结束视觉
 *          4. 清理木桩引用
 *          5. 调用父类EndEffect
 */
void ASG_RollingLogsEffect::EndEffect()
{
	// ✨ 新增: 检查是否已经结束
	if (!bIsEffectRunning)
	{
		// 调用父类实现以确保正确清理
		Super::EndEffect();
		return;
	}

	// ✨ 新增: 标记效果结束
	bIsEffectRunning = false;

	// ✨ 新增: 获取World引用
	if (UWorld* World = GetWorld())
	{
		// 停止生成定时器
		World->GetTimerManager().ClearTimer(SpawnTimerHandle);
		// 停止效果结束定时器（以防被其他方式调用EndEffect）
		World->GetTimerManager().ClearTimer(EffectEndTimerHandle);
	}

	// ✨ 新增: 播放效果结束视觉
	PlayEffectEndVisuals();

	// ✨ 新增: 清理木桩引用列表
	// 注意：不主动销毁木桩，让它们自然消失
	ActiveLogs.Empty();

	// ✨ 新增: 记录结束日志
	UE_LOG(LogTemp, Log, TEXT("[RollingLogsEffect] 流木计效果结束 - 共生成木桩: %d个"), TotalSpawnedCount);

	// ✨ 新增: 调用父类实现
	Super::EndEffect();
}

// ============================================================================
// 引用查找与缓存
// ============================================================================

/**
 * @brief 查找并缓存必要的场景引用
 * @return true表示所有引用都已找到
 * 
 * @details 详细流程:
 *          1. 查找FrontLineManager
 *          2. 查找所有主城
 *          3. 根据阵营标签区分己方和敌方主城
 */
bool ASG_RollingLogsEffect::FindAndCacheReferences()
{
	// ✨ 新增: 获取World引用
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	// ✨ 新增: 查找FrontLineManager
	TArray<AActor*> FoundFrontLineManagers;
	UGameplayStatics::GetAllActorsOfClass(World, ASG_FrontLineManager::StaticClass(), FoundFrontLineManagers);
	
	if (FoundFrontLineManagers.Num() > 0)
	{
		// 缓存第一个找到的FrontLineManager
		CachedFrontLineManager = Cast<ASG_FrontLineManager>(FoundFrontLineManagers[0]);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[RollingLogsEffect] 未找到FrontLineManager"));
	}

	// ✨ 新增: 查找所有主城
	TArray<AActor*> FoundMainCities;
	UGameplayStatics::GetAllActorsOfClass(World, ASG_MainCityBase::StaticClass(), FoundMainCities);

	// ✨ 新增: 根据阵营标签区分主城
	for (AActor* Actor : FoundMainCities)
	{
		ASG_MainCityBase* MainCity = Cast<ASG_MainCityBase>(Actor);
		if (!MainCity)
		{
			continue;
		}

		// 检查主城阵营标签与发起者阵营的关系
		if (MainCity->FactionTag.IsValid() && InstigatorFactionTag.IsValid())
		{
			if (MainCity->FactionTag.MatchesTagExact(InstigatorFactionTag))
			{
				// 阵营标签匹配，这是己方主城
				CachedFriendlyMainCity = MainCity;
				UE_LOG(LogTemp, Log, TEXT("[RollingLogsEffect] 找到己方主城: %s"), *MainCity->GetName());
			}
			else
			{
				// 阵营标签不匹配，这是敌方主城
				CachedEnemyMainCity = MainCity;
				UE_LOG(LogTemp, Log, TEXT("[RollingLogsEffect] 找到敌方主城: %s"), *MainCity->GetName());
			}
		}
	}

	// ✨ 新增: 验证必要引用是否完整
	// 必须有己方主城和敌方主城才能正确计算方向
	const bool bHasFriendlyCity = CachedFriendlyMainCity.IsValid();
	const bool bHasEnemyCity = CachedEnemyMainCity.IsValid();

	if (!bHasFriendlyCity)
	{
		UE_LOG(LogTemp, Warning, TEXT("[RollingLogsEffect] 未找到己方主城"));
	}
	if (!bHasEnemyCity)
	{
		UE_LOG(LogTemp, Warning, TEXT("[RollingLogsEffect] 未找到敌方主城"));
	}

	// 返回是否找到了两个主城
	return bHasFriendlyCity && bHasEnemyCity;
}

/**
 * @brief 计算生成区域参数
 * 
 * @details 详细流程:
 *          1. 获取己方主城位置
 *          2. 计算从己方主城指向敌方主城的方向
 *          3. 计算生成X坐标（己方主城前方偏移处）
 */
void ASG_RollingLogsEffect::CalculateSpawnAreaParameters()
{
	// ✨ 新增: 检查缓存引用有效性
	if (!CachedFriendlyMainCity.IsValid() || !CachedEnemyMainCity.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[RollingLogsEffect] 主城引用无效，使用默认参数"));
		// 使用默认方向（正X方向）
		CalculatedMoveDirection = FVector::ForwardVector;
		CalculatedSpawnX = 0.0f;
		return;
	}

	// ✨ 新增: 获取两个主城的位置
	const FVector FriendlyLocation = CachedFriendlyMainCity->GetActorLocation();
	const FVector EnemyLocation = CachedEnemyMainCity->GetActorLocation();

	// ✨ 新增: 计算移动方向（从己方主城指向敌方主城）
	FVector DirectionToEnemy = EnemyLocation - FriendlyLocation;
	// 只考虑XY平面的方向（忽略高度差）
	DirectionToEnemy.Z = 0.0f;
	// 归一化方向向量
	CalculatedMoveDirection = DirectionToEnemy.GetSafeNormal();

	// ✨ 新增: 计算生成位置X坐标
	// 生成位置 = 己方主城位置 + 方向 * 偏移距离
	const FVector SpawnOrigin = FriendlyLocation + CalculatedMoveDirection * SpawnOffsetFromMainCity;
	CalculatedSpawnX = SpawnOrigin.X;

	// ✨ 新增: 记录计算结果
	UE_LOG(LogTemp, Log, TEXT("[RollingLogsEffect] 计算完成 - 生成X: %.1f, 方向: %s"), 
		CalculatedSpawnX, *CalculatedMoveDirection.ToString());

	// ✨ 新增: 调试绘制
	if (bShowDebug)
	{
		DrawDebugVisualization();
	}
}

// ============================================================================
// 木桩生成
// ============================================================================

/**
 * @brief 生成定时器回调
 * 
 * @details 详细流程:
 *          1. 检查效果是否仍在运行
 *          2. 清理无效的木桩引用
 *          3. 检查是否超过最大同时数量
 *          4. 按配置数量生成新木桩
 */
void ASG_RollingLogsEffect::OnSpawnTimerTick()
{
	// ✨ 新增: 检查效果是否仍在运行
	if (!bIsEffectRunning)
	{
		return;
	}

	// ✨ 新增: 清理无效的木桩引用
	CleanupInvalidLogs();

	// ✨ 新增: 检查最大同时数量限制
	if (MaxSimultaneousLogs > 0 && ActiveLogs.Num() >= MaxSimultaneousLogs)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[RollingLogsEffect] 达到最大同时数量限制: %d"), MaxSimultaneousLogs);
		return;
	}

	// ✨ 新增: 生成木桩
	for (int32 i = 0; i < LogsPerSpawn; ++i)
	{
		// 检查是否还有配额
		if (MaxSimultaneousLogs > 0 && ActiveLogs.Num() >= MaxSimultaneousLogs)
		{
			break;
		}

		// 生成单个木桩
		ASG_RollingLog* NewLog = SpawnSingleLog();
		if (NewLog)
		{
			TotalSpawnedCount++;
		}
	}
}

/**
 * @brief 生成单个木桩
 * @return 生成的木桩Actor
 * 
 * @details 详细流程:
 *          1. 验证木桩类是否有效
 *          2. 计算随机生成位置
 *          3. 设置生成参数
 *          4. 生成木桩Actor
 *          5. 初始化木桩
 *          6. 绑定销毁事件
 *          7. 添加到存活列表
 */
ASG_RollingLog* ASG_RollingLogsEffect::SpawnSingleLog()
{
	// ✨ 新增: 验证木桩类
	if (!RollingLogClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[RollingLogsEffect] 未设置RollingLogClass"));
		return nullptr;
	}

	// ✨ 新增: 获取World引用
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	// ✨ 新增: 计算随机生成位置
	const FVector SpawnLocation = CalculateSpawnPosition();
	// 计算朝向敌方的旋转
	const FRotator SpawnRotation = CalculatedMoveDirection.Rotation();

	// ✨ 新增: 设置生成参数
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = GetInstigator();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	// ✨ 新增: 生成木桩Actor
	ASG_RollingLog* NewLog = World->SpawnActor<ASG_RollingLog>(
		RollingLogClass,
		SpawnLocation,
		SpawnRotation,
		SpawnParams
	);

	if (!NewLog)
	{
		UE_LOG(LogTemp, Warning, TEXT("[RollingLogsEffect] 木桩生成失败"));
		return nullptr;
	}

	// ✨ 新增: 初始化木桩参数
	NewLog->InitializeLog(
		CalculatedMoveDirection,
		EffectInstigator.Get(),
		InstigatorFactionTag,
		DamageEffectClass,
		KnockbackEffectClass
	);

	// ✨ 新增: 绑定销毁事件
	NewLog->OnRollingLogDestroyed.AddDynamic(this, &ASG_RollingLogsEffect::OnRollingLogDestroyed);

	// ✨ 新增: 添加到存活列表
	ActiveLogs.Add(NewLog);

	// ✨ 新增: 播放生成特效
	if (SpawnVFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this,
			SpawnVFX,
			SpawnLocation,
			SpawnRotation,
			FVector(1.0f),
			true,
			true,
			ENCPoolMethod::None
		);
	}

	// ✨ 新增: 记录生成日志
	UE_LOG(LogTemp, Verbose, TEXT("[RollingLogsEffect] 生成木桩 #%d 于位置: %s"), 
		TotalSpawnedCount + 1, *SpawnLocation.ToString());

	return NewLog;
}

/**
 * @brief 计算随机生成位置
 * @return 生成位置世界坐标
 * 
 * @details 详细流程:
 *          1. X坐标使用预计算的固定值
 *          2. Y坐标在范围内随机
 *          3. Z坐标通过射线检测获取地面高度
 */
FVector ASG_RollingLogsEffect::CalculateSpawnPosition() const
{
	// ✨ 新增: 计算Y坐标
	// 在[-SpawnAreaHalfWidth, SpawnAreaHalfWidth]范围内随机
	const float RandomY = FMath::FRandRange(-SpawnAreaHalfWidth, SpawnAreaHalfWidth);
	
	// ✨ 新增: 添加Y轴抖动
	const float JitteredY = RandomY + FMath::FRandRange(-SpawnYJitter, SpawnYJitter);

	// ✨ 新增: 获取地面高度
	const float GroundZ = GetGroundHeight(FVector2D(CalculatedSpawnX, JitteredY));
	
	// ✨ 新增: 计算最终Z坐标
	const float FinalZ = GroundZ + SpawnHeightOffset;

	// ✨ 新增: 返回最终位置
	return FVector(CalculatedSpawnX, JitteredY, FinalZ);
}

/**
 * @brief 获取地面高度
 * @param XYPosition - XY坐标
 * @return 地面Z坐标
 */
float ASG_RollingLogsEffect::GetGroundHeight(const FVector2D& XYPosition) const
{
	// ✨ 新增: 获取World引用
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0f;
	}

	// ✨ 新增: 设置射线检测参数
	const FVector TraceStart(XYPosition.X, XYPosition.Y, 10000.0f);
	const FVector TraceEnd(XYPosition.X, XYPosition.Y, -10000.0f);

	// ✨ 新增: 执行射线检测
	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	// 检测地面层
	const bool bHit = World->LineTraceSingleByChannel(
		HitResult,
		TraceStart,
		TraceEnd,
		ECC_WorldStatic,
		QueryParams
	);

	// ✨ 新增: 返回检测结果
	if (bHit)
	{
		return HitResult.Location.Z;
	}

	// 默认返回0
	return 0.0f;
}

// ============================================================================
// 事件处理
// ============================================================================

/**
 * @brief 木桩销毁回调
 * @param DestroyedLog - 被销毁的木桩
 */
void ASG_RollingLogsEffect::OnRollingLogDestroyed(ASG_RollingLog* DestroyedLog)
{
	// ✨ 新增: 从存活列表中移除
	if (DestroyedLog)
	{
		// 使用Lambda表达式查找并移除
		ActiveLogs.RemoveAll([DestroyedLog](const TWeakObjectPtr<ASG_RollingLog>& LogPtr)
		{
			return !LogPtr.IsValid() || LogPtr.Get() == DestroyedLog;
		});
	}
}

/**
 * @brief 清理无效的木桩引用
 */
void ASG_RollingLogsEffect::CleanupInvalidLogs()
{
	// ✨ 新增: 移除所有无效的弱引用
	ActiveLogs.RemoveAll([](const TWeakObjectPtr<ASG_RollingLog>& LogPtr)
	{
		return !LogPtr.IsValid();
	});
}

// ============================================================================
// 视觉效果
// ============================================================================

/**
 * @brief 播放效果开始视觉
 */
void ASG_RollingLogsEffect::PlayEffectStartVisuals()
{
	// ✨ 新增: 播放开始特效
	if (EffectStartVFX && CachedFriendlyMainCity.IsValid())
	{
		const FVector Location = CachedFriendlyMainCity->GetActorLocation();
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this,
			EffectStartVFX,
			Location,
			FRotator::ZeroRotator,
			FVector(1.0f),
			true,
			true,
			ENCPoolMethod::None
		);
	}

	// ✨ 新增: 播放开始音效
	if (EffectStartSound)
	{
		UGameplayStatics::PlaySound2D(this, EffectStartSound);
	}
}

/**
 * @brief 播放效果结束视觉
 */
void ASG_RollingLogsEffect::PlayEffectEndVisuals()
{
	// ✨ 新增: 播放结束特效
	if (EffectEndVFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this,
			EffectEndVFX,
			GetActorLocation(),
			FRotator::ZeroRotator,
			FVector(1.0f),
			true,
			true,
			ENCPoolMethod::None
		);
	}

	// ✨ 新增: 播放结束音效
	if (EffectEndSound)
	{
		UGameplayStatics::PlaySound2D(this, EffectEndSound);
	}
}

/**
 * @brief 绘制调试信息
 */
void ASG_RollingLogsEffect::DrawDebugVisualization()
{
#if ENABLE_DRAW_DEBUG
	// ✨ 新增: 获取World引用
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// ✨ 新增: 绘制生成区域
	// 绘制生成线（沿Y轴）
	const FVector LineStart(CalculatedSpawnX, -SpawnAreaHalfWidth, 100.0f);
	const FVector LineEnd(CalculatedSpawnX, SpawnAreaHalfWidth, 100.0f);
	DrawDebugLine(World, LineStart, LineEnd, DebugColor, false, EffectDuration, 0, 5.0f);

	// ✨ 新增: 绘制移动方向
	const FVector DirectionStart(CalculatedSpawnX, 0.0f, 100.0f);
	const FVector DirectionEnd = DirectionStart + CalculatedMoveDirection * 500.0f;
	DrawDebugDirectionalArrow(World, DirectionStart, DirectionEnd, 100.0f, FColor::Green, false, EffectDuration, 0, 5.0f);

	// ✨ 新增: 绘制主城位置
	if (CachedFriendlyMainCity.IsValid())
	{
		DrawDebugSphere(World, CachedFriendlyMainCity->GetActorLocation(), 100.0f, 12, FColor::Blue, false, EffectDuration);
	}
	if (CachedEnemyMainCity.IsValid())
	{
		DrawDebugSphere(World, CachedEnemyMainCity->GetActorLocation(), 100.0f, 12, FColor::Red, false, EffectDuration);
	}
#endif
}
