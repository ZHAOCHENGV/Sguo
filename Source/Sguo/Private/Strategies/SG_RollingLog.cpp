// Copyright (C) 2024 Sguo Project. All Rights Reserved.

#include "Strategies/SG_RollingLog.h"

// ============================================================================
// 引擎核心头文件
// ============================================================================
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/AudioComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

// ============================================================================
// GAS相关头文件
// ============================================================================
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "GameplayEffect.h"

// ============================================================================
// 特效与音效头文件
// ============================================================================
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Kismet/GameplayStatics.h"

// ============================================================================
// 项目头文件
// ============================================================================
#include "Units/SG_UnitsBase.h"
#include "Buildings/SG_MainCityBase.h"

// ============================================================================
// 构造函数
// ============================================================================

/**
 * @brief 默认构造函数
 * 
 * @details 详细流程:
 *          1. 创建根场景组件
 *          2. 创建碰撞检测球体组件
 *          3. 创建木桩静态网格组件
 *          4. 配置碰撞响应设置
 *          5. 启用Tick
 */
ASG_RollingLog::ASG_RollingLog()
{
	// ✨ 新增: 启用Tick用于位置更新和滚动动画
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	// ✨ 新增: 创建根场景组件
	RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootSceneComponent"));
	// 设置为根组件
	RootComponent = RootSceneComponent;

	// ✨ 新增: 创建碰撞检测球体
	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	// 附加到根组件
	CollisionSphere->SetupAttachment(RootComponent);
	// 设置碰撞球体半径
	CollisionSphere->SetSphereRadius(CollisionRadius);
	// 设置碰撞预设为OverlapAllDynamic
	CollisionSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	// 启用生成重叠事件
	CollisionSphere->SetGenerateOverlapEvents(true);

	// ✨ 新增: 创建木桩网格组件
	LogMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LogMesh"));
	// 附加到碰撞球体
	LogMesh->SetupAttachment(CollisionSphere);
	// 禁用网格体碰撞（使用球体碰撞）
	LogMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

// ============================================================================
// 生命周期函数
// ============================================================================

/**
 * @brief Actor开始播放时调用
 * 
 * @details 详细流程:
 *          1. 调用父类BeginPlay
 *          2. 设置最大存活时间
 *          3. 绑定碰撞重叠事件
 *          4. 播放生成音效
 *          5. 启动滚动循环音效
 */
void ASG_RollingLog::BeginPlay()
{
	// 调用父类实现
	Super::BeginPlay();

	// ✨ 新增: 设置Actor最大存活时间，防止遗留在场景中
	SetLifeSpan(MaxLifeTime);

	// ✨ 新增: 绑定碰撞重叠事件
	if (CollisionSphere)
	{
		// 绑定OnComponentBeginOverlap事件到OnSphereOverlap函数
		CollisionSphere->OnComponentBeginOverlap.AddDynamic(this, &ASG_RollingLog::OnSphereOverlap);
	}

	// ✨ 新增: 播放生成音效
	if (SpawnSound)
	{
		// 在木桩位置播放一次性音效
		UGameplayStatics::PlaySoundAtLocation(this, SpawnSound, GetActorLocation());
	}

	// ✨ 新增: 启动滚动循环音效
	if (RollSound)
	{
		// 创建附加的音效组件用于循环播放
		RollSoundComponent = UGameplayStatics::SpawnSoundAttached(
			RollSound,                    // 音效资源
			RootSceneComponent,           // 附加目标
			NAME_None,                    // Socket名称
			FVector::ZeroVector,          // 位置偏移
			EAttachLocation::KeepRelativeOffset,
			false,                        // 不自动销毁
			1.0f,                         // 音量
			1.0f,                         // 音调
			0.0f,                         // 起始时间
			nullptr,                      // 衰减设置
			nullptr,                      // 并发设置
			true                          // 自动激活
		);
	}

	// ✨ 新增: 记录调试日志
	UE_LOG(LogTemp, Log, TEXT("[RollingLog] 木桩生成于位置: %s, 移动方向: %s"), 
		*GetActorLocation().ToString(), 
		*MoveDirection.ToString());
}

/**
 * @brief Actor结束播放时调用
 * @param EndPlayReason - 结束原因
 * 
 * @details 详细流程:
 *          1. 停止滚动音效
 *          2. 播放销毁特效（如果不是被正常销毁）
 *          3. 调用父类EndPlay
 */
void ASG_RollingLog::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// ✨ 新增: 停止滚动音效
	if (RollSoundComponent && RollSoundComponent->IsPlaying())
	{
		// 停止音效播放
		RollSoundComponent->Stop();
	}

	// 调用父类实现
	Super::EndPlay(EndPlayReason);
}

// ============================================================================
// Tick更新
// ============================================================================

/**
 * @brief 每帧更新
 * @param DeltaTime - 帧间隔时间
 * 
 * @details 详细流程:
 *          1. 如果正在销毁则跳过更新
 *          2. 计算本帧移动距离
 *          3. 更新Actor位置
 *          4. 累计行进距离
 *          5. 更新滚动动画（网格体旋转）
 *          6. 绘制调试信息
 */
void ASG_RollingLog::Tick(float DeltaTime)
{
	// 调用父类Tick
	Super::Tick(DeltaTime);

	// ✨ 新增: 如果正在销毁，跳过所有更新
	if (bIsDestroying)
	{
		return;
	}

	// ✨ 新增: 计算本帧移动增量
	// 移动距离 = 速度 * 时间
	const float MoveDelta = RollSpeed * DeltaTime;
	
	// ✨ 新增: 计算新位置
	// 新位置 = 当前位置 + 移动方向 * 移动距离
	const FVector NewLocation = GetActorLocation() + MoveDirection * MoveDelta;
	
	// ✨ 新增: 更新Actor位置
	SetActorLocation(NewLocation);
	
	// ✨ 新增: 累计行进距离
	TraveledDistance += MoveDelta;

	// ✨ 新增: 更新滚动动画
	UpdateRollingAnimation(DeltaTime);

	// ✨ 新增: 绘制调试信息
	if (bShowDebug)
	{
		DrawDebugInfo();
	}
}

// ============================================================================
// 初始化接口
// ============================================================================

/**
 * @brief 初始化木桩参数
 * 
 * @details 详细流程:
 *          1. 保存移动方向（确保归一化）
 *          2. 缓存发起者引用
 *          3. 保存阵营标签
 *          4. 设置伤害和击退效果类
 *          5. 根据移动方向设置初始旋转
 */
void ASG_RollingLog::InitializeLog(
	const FVector& InMoveDirection,
	AActor* InInstigator,
	const FGameplayTag& InInstigatorFactionTag,
	TSubclassOf<UGameplayEffect> InDamageEffect,
	TSubclassOf<UGameplayEffect> InKnockbackEffect)
{
	// ✨ 新增: 保存并归一化移动方向
	// 确保方向向量长度为1，避免速度异常
	MoveDirection = InMoveDirection.GetSafeNormal();
	
	// ✨ 新增: 使用弱引用缓存发起者
	// 弱引用不会阻止发起者被GC回收
	EffectInstigator = InInstigator;
	
	// ✨ 新增: 保存发起者阵营标签
	// 用于判断碰撞目标是否为敌方
	InstigatorFactionTag = InInstigatorFactionTag;
	
	// ✨ 新增: 设置伤害效果类
	if (InDamageEffect)
	{
		DamageEffectClass = InDamageEffect;
	}
	
	// ✨ 新增: 设置击退效果类（可选）
	if (InKnockbackEffect)
	{
		KnockbackEffectClass = InKnockbackEffect;
	}

	// ✨ 新增: 根据移动方向设置Actor旋转
	// 使木桩朝向移动方向
	if (!MoveDirection.IsNearlyZero())
	{
		// 计算朝向移动方向的旋转
		const FRotator LookAtRotation = MoveDirection.Rotation();
		// 应用旋转
		SetActorRotation(LookAtRotation);
	}

	// ✨ 新增: 记录初始化日志
	UE_LOG(LogTemp, Log, TEXT("[RollingLog] 初始化完成 - 方向: %s, 发起者: %s, 阵营: %s"),
		*MoveDirection.ToString(),
		InInstigator ? *InInstigator->GetName() : TEXT("None"),
		*InstigatorFactionTag.ToString());
}

// ============================================================================
// 公共接口
// ============================================================================

/**
 * @brief 手动销毁木桩
 * 
 * @details 详细流程:
 *          1. 设置销毁标志防止重复调用
 *          2. 广播销毁事件
 *          3. 禁用碰撞
 *          4. 播放破碎特效
 *          5. 延迟销毁Actor（等待特效播放完成）
 */
void ASG_RollingLog::DestroyLog()
{
	// ✨ 新增: 防止重复销毁
	if (bIsDestroying)
	{
		return;
	}
	
	// ✨ 新增: 设置销毁标志
	bIsDestroying = true;

	// ✨ 新增: 广播销毁事件
	OnRollingLogDestroyed.Broadcast(this);

	// ✨ 新增: 立即禁用碰撞，防止销毁过程中再次触发碰撞
	if (CollisionSphere)
	{
		CollisionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		CollisionSphere->SetGenerateOverlapEvents(false);
	}

	// ✨ 新增: 隐藏网格体
	if (LogMesh)
	{
		LogMesh->SetVisibility(false);
	}

	// ✨ 新增: 播放破碎特效和音效
	PlayDestroyEffects();

	// ✨ 新增: 延迟销毁Actor（给特效播放时间）
	// 设置0.5秒后自动销毁
	SetLifeSpan(0.5f);

	// ✨ 新增: 记录销毁日志
	UE_LOG(LogTemp, Log, TEXT("[RollingLog] 木桩销毁 - 行进距离: %.2f cm"), TraveledDistance);
}

// ============================================================================
// 碰撞处理
// ============================================================================

/**
 * @brief 碰撞球体重叠事件回调
 * 
 * @details 详细流程:
 *          1. 检查是否正在销毁
 *          2. 忽略无效Actor和自身
 *          3. 忽略发起者
 *          4. 检查目标是否为敌方
 *          5. 处理击中目标
 *          6. 销毁木桩
 */
void ASG_RollingLog::OnSphereOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	// ✨ 新增: 如果正在销毁，忽略碰撞
	if (bIsDestroying)
	{
		return;
	}

	// ✨ 新增: 忽略无效Actor
	if (!OtherActor)
	{
		return;
	}

	// ✨ 新增: 忽略自身
	if (OtherActor == this)
	{
		return;
	}

	// ✨ 新增: 忽略发起者
	if (EffectInstigator.IsValid() && OtherActor == EffectInstigator.Get())
	{
		return;
	}

	// ✨ 新增: 检查是否为敌方目标
	if (!IsEnemyTarget(OtherActor))
	{
		return;
	}

	// ✨ 新增: 记录碰撞日志
	UE_LOG(LogTemp, Log, TEXT("[RollingLog] 击中敌方目标: %s"), *OtherActor->GetName());

	// ✨ 新增: 处理击中目标（伤害和击退）
	HandleHitTarget(OtherActor);

	// ✨ 新增: 销毁木桩（击中一个目标后破碎）
	DestroyLog();
}

/**
 * @brief 检查目标是否为敌方
 * @param TargetActor - 目标Actor
 * @return true表示是敌方
 * 
 * @details 详细流程:
 *          1. 尝试转换为单位基类
 *          2. 比较阵营标签
 *          3. 如果标签不同则为敌方
 */
bool ASG_RollingLog::IsEnemyTarget(AActor* TargetActor) const
{
	// ✨ 新增: 空指针检查
	if (!TargetActor)
	{
		return false;
	}

	// ✨ 新增: 检查是否为单位
	if (const ASG_UnitsBase* Unit = Cast<ASG_UnitsBase>(TargetActor))
	{
		// 获取单位的阵营标签
		const FGameplayTag& UnitFaction = Unit->FactionTag;
		
		// 如果阵营标签有效且与发起者不同，则为敌方
		if (UnitFaction.IsValid() && InstigatorFactionTag.IsValid())
		{
			// 阵营标签不匹配则为敌方
			return !UnitFaction.MatchesTagExact(InstigatorFactionTag);
		}
	}

	// ✨ 新增: 检查是否为主城
	if (const ASG_MainCityBase* MainCity = Cast<ASG_MainCityBase>(TargetActor))
	{
		// 获取主城的阵营标签
		const FGameplayTag& CityFaction = MainCity->FactionTag;
		
		// 如果阵营标签有效且与发起者不同，则为敌方主城
		if (CityFaction.IsValid() && InstigatorFactionTag.IsValid())
		{
			// 阵营标签不匹配则为敌方
			return !CityFaction.MatchesTagExact(InstigatorFactionTag);
		}
	}

	// ✨ 新增: 默认不是敌方
	return false;
}

// ============================================================================
// 伤害处理
// ============================================================================

/**
 * @brief 处理击中目标
 * @param HitTarget - 被击中的目标
 * 
 * @details 详细流程:
 *          1. 获取目标的AbilitySystemComponent
 *          2. 创建并应用伤害GameplayEffect
 *          3. 应用击退效果
 *          4. 播放击中特效
 */
void ASG_RollingLog::HandleHitTarget(AActor* HitTarget)
{
	// ✨ 新增: 空指针检查
	if (!HitTarget)
	{
		return;
	}

	// ✨ 新增: 获取目标的AbilitySystemComponent
	IAbilitySystemInterface* ASCInterface = Cast<IAbilitySystemInterface>(HitTarget);
	UAbilitySystemComponent* TargetASC = ASCInterface ? ASCInterface->GetAbilitySystemComponent() : nullptr;

	// ✨ 新增: 应用伤害效果
	if (TargetASC && DamageEffectClass)
	{
		// 创建效果上下文
		FGameplayEffectContextHandle ContextHandle = TargetASC->MakeEffectContext();
		// 设置效果发起者
		ContextHandle.AddInstigator(EffectInstigator.Get(), this);
		
		// 创建效果Spec
		FGameplayEffectSpecHandle SpecHandle = TargetASC->MakeOutgoingSpec(
			DamageEffectClass, 
			1.0f,  // 等级
			ContextHandle
		);

		// 设置伤害倍率（通过SetByCallerMagnitude）
		if (SpecHandle.IsValid())
		{
			// 应用伤害倍率
			// 注意：这里假设DamageEffectClass使用SetByCaller来设置伤害值
			// 如果使用其他方式，需要相应修改
			SpecHandle.Data->SetSetByCallerMagnitude(
				FGameplayTag::RequestGameplayTag(TEXT("Data.Damage")), 
				DamageMultiplier
			);
			
			// 应用效果到目标
			TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
			
			// 记录伤害日志
			UE_LOG(LogTemp, Log, TEXT("[RollingLog] 对目标 %s 造成伤害，倍率: %.2f"), 
				*HitTarget->GetName(), DamageMultiplier);
		}
	}

	// ✨ 新增: 应用击退效果
	ApplyKnockbackToTarget(HitTarget);

	// ✨ 新增: 播放击中特效
	PlayHitEffects(HitTarget->GetActorLocation());
}

/**
 * @brief 应用击退效果到目标
 * @param Target - 目标Actor
 * 
 * @details 详细流程:
 *          1. 获取目标的移动组件
 *          2. 计算击退方向（木桩移动方向）
 *          3. 计算击退速度
 *          4. 应用冲量或启动击退Timeline
 */
void ASG_RollingLog::ApplyKnockbackToTarget(AActor* Target)
{
	// ✨ 新增: 空指针检查
	if (!Target)
	{
		return;
	}

	// ✨ 新增: 检查击退距离是否有效
	if (KnockbackDistance <= 0.0f)
	{
		return;
	}

	// ✨ 新增: 尝试获取CharacterMovementComponent
	UCharacterMovementComponent* MovementComp = Target->FindComponentByClass<UCharacterMovementComponent>();
	
	if (MovementComp)
	{
		// 计算击退方向（使用木桩移动方向）
		FVector KnockbackDir = MoveDirection;
		// 确保击退方向在水平面上
		KnockbackDir.Z = 0.0f;
		KnockbackDir.Normalize();
		
		// 计算击退速度 = 距离 / 时间
		const float KnockbackSpeed = KnockbackDistance / FMath::Max(KnockbackDuration, 0.1f);
		
		// 计算击退冲量
		const FVector KnockbackImpulse = KnockbackDir * KnockbackSpeed;
		
		// 应用冲量实现击退
		// 注意：这里使用Launch方式，如果需要更精确的控制
		// 可以考虑使用GameplayEffect配合GameplayAbility
		MovementComp->Launch(KnockbackImpulse);
		
		// 记录击退日志
		UE_LOG(LogTemp, Log, TEXT("[RollingLog] 对目标 %s 应用击退 - 方向: %s, 速度: %.2f"), 
			*Target->GetName(), 
			*KnockbackDir.ToString(), 
			KnockbackSpeed);
	}
	else
	{
		// ✨ 新增: 如果没有MovementComponent，尝试直接移动Actor
		// 这种情况适用于非Character类型的Actor
		const FVector CurrentLocation = Target->GetActorLocation();
		const FVector TargetLocation = CurrentLocation + MoveDirection * KnockbackDistance;
		
		// 设置新位置
		Target->SetActorLocation(TargetLocation);
		
		// 记录日志
		UE_LOG(LogTemp, Log, TEXT("[RollingLog] 对目标 %s 应用位置偏移击退"), *Target->GetName());
	}
}

// ============================================================================
// 特效播放
// ============================================================================

/**
 * @brief 播放击中特效
 * @param HitLocation - 击中位置
 */
void ASG_RollingLog::PlayHitEffects(const FVector& HitLocation)
{
	// ✨ 新增: 播放击中粒子特效
	if (HitEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this,
			HitEffect,
			HitLocation,
			FRotator::ZeroRotator,
			FVector(1.0f),
			true,   // 自动销毁
			true,   // 自动激活
			ENCPoolMethod::None
		);
	}

	// ✨ 新增: 播放击中音效
	if (HitSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, HitSound, HitLocation);
	}
}

/**
 * @brief 播放破碎特效
 */
void ASG_RollingLog::PlayDestroyEffects()
{
	// 获取当前位置
	const FVector Location = GetActorLocation();
	const FRotator Rotation = GetActorRotation();

	// ✨ 新增: 播放破碎粒子特效
	if (DestroyEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this,
			DestroyEffect,
			Location,
			Rotation,
			FVector(1.0f),
			true,   // 自动销毁
			true,   // 自动激活
			ENCPoolMethod::None
		);
	}

	// ✨ 新增: 播放破碎音效
	if (DestroySound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, DestroySound, Location);
	}
}

// ============================================================================
// 动画更新
// ============================================================================

/**
 * @brief 更新滚动动画
 * @param DeltaTime - 帧间隔
 * 
 * @details 详细流程:
 *          1. 计算本帧旋转增量
 *          2. 绕滚动轴旋转网格体
 *          3. 应用旋转到网格组件
 */
void ASG_RollingLog::UpdateRollingAnimation(float DeltaTime)
{
	// ✨ 新增: 空指针检查
	if (!LogMesh)
	{
		return;
	}

	// ✨ 新增: 计算旋转增量
	// 旋转量 = 旋转速度 * 时间
	const float RotationDelta = RotationSpeed * DeltaTime;

	// ✨ 新增: 获取当前网格旋转
	FRotator CurrentRotation = LogMesh->GetRelativeRotation();
	
	// ✨ 新增: 计算滚动轴
	// 木桩沿X轴移动时，绕Y轴滚动（Pitch）
	// 实际滚动轴取决于木桩模型的朝向
	// 这里假设木桩是沿长轴放置的圆柱体
	CurrentRotation.Roll += RotationDelta;
	
	// ✨ 新增: 应用旋转
	LogMesh->SetRelativeRotation(CurrentRotation);
}

// ============================================================================
// 调试绘制
// ============================================================================

/**
 * @brief 绘制调试信息
 */
void ASG_RollingLog::DrawDebugInfo()
{
#if ENABLE_DRAW_DEBUG
	// ✨ 新增: 获取当前位置
	const FVector Location = GetActorLocation();

	// ✨ 新增: 绘制碰撞球体
	DrawDebugSphere(
		GetWorld(),
		Location,
		CollisionRadius,
		12,
		DebugColor,
		false,
		-1.0f,
		0,
		2.0f
	);

	// ✨ 新增: 绘制移动方向箭头
	DrawDebugDirectionalArrow(
		GetWorld(),
		Location,
		Location + MoveDirection * 200.0f,
		50.0f,
		FColor::Green,
		false,
		-1.0f,
		0,
		3.0f
	);

	// ✨ 新增: 绘制状态文本
	FString DebugText = FString::Printf(
		TEXT("RollingLog\nSpeed: %.1f\nTraveled: %.1f cm"),
		RollSpeed,
		TraveledDistance
	);
	DrawDebugString(
		GetWorld(),
		Location + FVector(0.0f, 0.0f, 100.0f),
		DebugText,
		nullptr,
		FColor::White,
		0.0f,
		true
	);
#endif
}
