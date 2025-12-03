// 📄 文件：Source/Sguo/Private/Strategies/SG_FireArrowEffect.cpp
// 🔧 修复 - 解决 LNK2019 链接错误，补全所有函数实现
// ✅ 这是完整文件，请直接覆盖

#include "Strategies/SG_FireArrowEffect.h"
#include "Data/SG_FireArrowCardData.h"
#include "Units/SG_StationaryUnit.h"
#include "Actors/SG_Projectile.h"
#include "Buildings/SG_MainCityBase.h"
#include "Components/DecalComponent.h"
#include "Kismet/GameplayStatics.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Debug/SG_LogCategories.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/PlayerController.h"

ASG_FireArrowEffect::ASG_FireArrowEffect()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	PreviewDecal = CreateDefaultSubobject<UDecalComponent>(TEXT("PreviewDecal"));
	RootComponent = PreviewDecal;
	PreviewDecal->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
	PreviewDecal->SetVisibility(false);
	
	// 默认开启强制贴地，只检测静态物体
	bForceGroundTrace = true;
	GroundTraceChannel = ECC_WorldStatic;
}

void ASG_FireArrowEffect::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogSGGameplay, Log, TEXT("========== 火矢计效果生成 =========="));
}

void ASG_FireArrowEffect::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 如果正在执行，检查弓手存活状态
	if (CurrentState == ESGStrategyEffectState::Executing)
	{
		ParticipatingArchers.RemoveAll([](const TWeakObjectPtr<ASG_StationaryUnit>& Archer)
		{
			if (!Archer.IsValid()) return true;
			if (Archer->bIsDead) return true;
			return false;
		});

		if (ParticipatingArchers.Num() == 0)
		{
			UE_LOG(LogSGGameplay, Warning, TEXT("  ⚠️ 所有弓手已死亡，火矢计提前结束"));
			OnSkillDurationEnd();
		}
	}
}

void ASG_FireArrowEffect::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(FireTimerHandle);
		GetWorld()->GetTimerManager().ClearTimer(DurationTimerHandle);
	}

	NotifyArchersEndFireArrow();

	Super::EndPlay(EndPlayReason);
}

void ASG_FireArrowEffect::InitializeEffect(
	USG_StrategyCardData* InCardData,
	AActor* InEffectInstigator,
	const FVector& InTargetLocation)
{
	Super::InitializeEffect(InCardData, InEffectInstigator, InTargetLocation);

	FireArrowCardData = Cast<USG_FireArrowCardData>(InCardData);
	if (!FireArrowCardData)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("❌ 火矢计初始化失败：卡牌数据类型错误！"));
		return;
	}

	UE_LOG(LogSGGameplay, Log, TEXT("  初始化火矢计"));
	if (FireArrowCardData)
	{
		UE_LOG(LogSGGameplay, Log, TEXT("    区域半径：%.1f"), FireArrowCardData->AreaRadius);
		UE_LOG(LogSGGameplay, Log, TEXT("    持续时间：%.1f 秒"), FireArrowCardData->SkillDuration);
	}

	FindParticipatingArchers();
	UE_LOG(LogSGGameplay, Log, TEXT("    可用弓手数：%d"), ParticipatingArchers.Num());

	CreatePreviewDecal();
	SetActorLocation(InTargetLocation);
}

bool ASG_FireArrowEffect::CanExecute_Implementation() const
{
	return ParticipatingArchers.Num() > 0;
}

FText ASG_FireArrowEffect::GetCannotExecuteReason_Implementation() const
{
	if (ParticipatingArchers.Num() == 0)
	{
		return FText::FromString(TEXT("没有可用的浮空弓手！"));
	}
	return FText::GetEmpty();
}

bool ASG_FireArrowEffect::StartTargetSelection_Implementation()
{
	UE_LOG(LogSGGameplay, Log, TEXT("========== 火矢计开始目标选择 =========="));

	FindParticipatingArchers();

	if (!CanExecute())
	{
		FText Reason = GetCannotExecuteReason();
		UE_LOG(LogSGGameplay, Warning, TEXT("  ⚠️ %s"), *Reason.ToString());
		return false;
	}

	if (PreviewDecal)
	{
		PreviewDecal->SetVisibility(true);
		bPreviewVisible = true;
	}

	bool bResult = Super::StartTargetSelection_Implementation();
	return bResult;
}

void ASG_FireArrowEffect::UpdateTargetLocation_Implementation(const FVector& NewLocation)
{
	if (CurrentState != ESGStrategyEffectState::WaitingForTarget)
	{
		return;
	}

	FVector FinalLocation = NewLocation;

	if (bForceGroundTrace)
	{
		APlayerController* PC = Cast<APlayerController>(GetOwner());
		if (PC)
		{
			FVector WorldLoc, WorldDir;
			if (PC->DeprojectMousePositionToWorld(WorldLoc, WorldDir))
			{
				FVector Start = WorldLoc;
				FVector End = Start + WorldDir * TraceDistance;

				FHitResult Hit;
				FCollisionQueryParams QueryParams;
				QueryParams.AddIgnoredActor(this);
				if (PC->GetPawn()) QueryParams.AddIgnoredActor(PC->GetPawn());

				FCollisionObjectQueryParams ObjectParams;
				ObjectParams.AddObjectTypesToQuery(GroundTraceChannel); 

				bool bHit = GetWorld()->LineTraceSingleByObjectType(
					Hit, Start, End, ObjectParams, QueryParams);

				if (bHit)
				{
					FinalLocation = Hit.ImpactPoint;
				}
			}
		}
	}

	TargetLocation = FinalLocation;
	SetActorLocation(FinalLocation);
	UpdatePreviewDecal();
}

bool ASG_FireArrowEffect::ConfirmTarget_Implementation()
{
	UE_LOG(LogSGGameplay, Log, TEXT("========== 火矢计确认目标 =========="));

	if (CurrentState != ESGStrategyEffectState::WaitingForTarget)
	{
		return false;
	}

	FindParticipatingArchers();

	if (!CanExecute())
	{
		return false;
	}

	HidePreviewDecal();
	ExecuteEffect();

	return true;
}

void ASG_FireArrowEffect::CancelEffect_Implementation()
{
	HidePreviewDecal();
	Super::CancelEffect_Implementation();
}

void ASG_FireArrowEffect::InterruptEffect_Implementation()
{
	if (CurrentState != ESGStrategyEffectState::Executing)
	{
		return;
	}

	UE_LOG(LogSGGameplay, Warning, TEXT("  ⚠️ 火矢计被打断！"));

	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(FireTimerHandle);
		GetWorld()->GetTimerManager().ClearTimer(DurationTimerHandle);
	}

	NotifyArchersEndFireArrow();
	Super::InterruptEffect_Implementation();
}

/**
 * @brief 执行效果（委托模式）
 * 🔧 核心逻辑：遍历弓手，调用 Unit 的 StartStrategySkill 接口
 */
void ASG_FireArrowEffect::ExecuteEffect_Implementation()
{
	UE_LOG(LogSGGameplay, Log, TEXT("========== 执行火矢计 (委托模式) =========="));

	if (ParticipatingArchers.Num() == 0)
	{
		UE_LOG(LogSGGameplay, Warning, TEXT("  ⚠️ 没有可用的弓手，火矢计无法执行"));
		EndEffect();
		return;
	}

	SetState(ESGStrategyEffectState::Executing);
	SkillStartTime = GetWorld()->GetTimeSeconds();

	// 获取配置数据
	float FireInterval = FireArrowCardData ? FireArrowCardData->FireInterval : 0.3f;
	float SkillDuration = FireArrowCardData ? FireArrowCardData->SkillDuration : 5.0f;
	int32 ArrowsPerRound = FireArrowCardData ? FireArrowCardData->ArrowsPerArcherPerRound : 1;
	float AreaRadius = FireArrowCardData ? FireArrowCardData->AreaRadius : 800.0f;
	
	// 获取数值参数
	float DmgMult = FireArrowCardData ? FireArrowCardData->ArrowDamageMultiplier : 1.0f;
	float Arc = FireArrowCardData ? FireArrowCardData->ArrowArcHeight : 0.5f;
	float Speed = FireArrowCardData ? FireArrowCardData->ArrowSpeed : 1500.0f;
	TSubclassOf<AActor> ProjClass = FireArrowCardData ? FireArrowCardData->FireArrowProjectileClass : nullptr;

	// 遍历所有弓手，启动他们的计谋模式
	for (const TWeakObjectPtr<ASG_StationaryUnit>& ArcherPtr : ParticipatingArchers)
	{
		if (!ArcherPtr.IsValid()) continue;
		ASG_StationaryUnit* Archer = ArcherPtr.Get();
		if (Archer->bIsDead) continue;

		// 获取弓手自己的蒙太奇
		UAnimMontage* MyMontage = Archer->FireArrowMontage;

		// 调用单位的新接口
		Archer->StartStrategySkill(
			TargetLocation,
			AreaRadius,
			SkillDuration,
			FireInterval,
			ArrowsPerRound,
			ProjClass,
			MyMontage,
			DmgMult,
			Arc,
			Speed
		);

		UE_LOG(LogSGGameplay, Verbose, TEXT("    -> 弓手 %s 开始自动射击"), *Archer->GetName());
	}

	// 设置结束定时器
	GetWorld()->GetTimerManager().SetTimer(
		DurationTimerHandle,
		this,
		&ASG_FireArrowEffect::OnSkillDurationEnd,
		SkillDuration,
		false
	);

	UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 火矢计指令已下达，等待结束"));
}

/**
 * @brief 查找参与的弓手
 * 🔧 修复：之前版本缺失此实现
 */
void ASG_FireArrowEffect::FindParticipatingArchers()
{
	ParticipatingArchers.Empty();

	TArray<AActor*> AllActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASG_StationaryUnit::StaticClass(), AllActors);

	for (AActor* Actor : AllActors)
	{
		ASG_StationaryUnit* StationaryUnit = Cast<ASG_StationaryUnit>(Actor);
		if (!StationaryUnit) continue;
		if (StationaryUnit->bIsDead) continue;
		if (StationaryUnit->FactionTag != InstigatorFactionTag) continue;
		if (!StationaryUnit->IsHovering()) continue;

		ParticipatingArchers.Add(StationaryUnit);
	}
}

/**
 * @brief 技能时间结束
 */
void ASG_FireArrowEffect::OnSkillDurationEnd()
{
	UE_LOG(LogSGGameplay, Log, TEXT("========== 火矢计时间结束 =========="));

	// 通知所有弓手停止
	NotifyArchersEndFireArrow();

	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(FireTimerHandle);
		GetWorld()->GetTimerManager().ClearTimer(DurationTimerHandle);
	}

	K2_OnFireArrowCompleted(0);
	EndEffect();
}

/**
 * @brief 通知弓手停止
 */
void ASG_FireArrowEffect::NotifyArchersEndFireArrow()
{
	for (const TWeakObjectPtr<ASG_StationaryUnit>& ArcherPtr : ParticipatingArchers)
	{
		if (ArcherPtr.IsValid())
		{
			ArcherPtr.Get()->StopStrategySkill();
		}
	}
}

/**
 * @brief 通知弓手开始
 * ⚠️ 空实现：新逻辑直接在 ExecuteEffect 中调用 StartStrategySkill
 */
void ASG_FireArrowEffect::NotifyArchersStartFireArrow()
{
	// Deprecated in new logic
}

/**
 * @brief 执行一轮射击
 * ⚠️ 空实现：射击逻辑已移交 Unit，此函数仅为满足链接器保留
 */
void ASG_FireArrowEffect::ExecuteFireRound()
{
	// Deprecated: Firing is handled by ASG_StationaryUnit::ExecuteStrategyFire
}

/**
 * @brief 射击定时器 Tick
 * ⚠️ 空实现：Effect 不再管理射击 Tick
 */
void ASG_FireArrowEffect::OnFireTimerTick()
{
	// Deprecated
}

/**
 * @brief 单个弓手发射
 * ⚠️ 空实现：链接器占位符
 */
void ASG_FireArrowEffect::FireArrowsFromArcher(ASG_StationaryUnit* Archer, int32 ArrowCount)
{
	// Deprecated
}

/**
 * @brief 生成随机目标
 * ⚠️ 空实现：链接器占位符，逻辑已移至 Unit
 */
FVector ASG_FireArrowEffect::GenerateRandomTargetInArea() const
{
	return FVector::ZeroVector;
}

// ========== 辅助函数 ==========

void ASG_FireArrowEffect::CreatePreviewDecal()
{
	if (!PreviewDecal) return;

	float Radius = FireArrowCardData ? FireArrowCardData->AreaRadius : 800.0f;
	PreviewDecal->DecalSize = FVector(1000.0f, Radius, Radius);

	if (FireArrowCardData && FireArrowCardData->PreviewAreaMaterial)
	{
		UMaterialInstanceDynamic* DynamicMaterial = UMaterialInstanceDynamic::Create(
			FireArrowCardData->PreviewAreaMaterial, this);

		if (DynamicMaterial)
		{
			DynamicMaterial->SetVectorParameterValue(FName("Color"), FireArrowCardData->PreviewAreaColor);
			PreviewDecal->SetDecalMaterial(DynamicMaterial);
		}
	}
	PreviewDecal->SortOrder = 10;
}

void ASG_FireArrowEffect::UpdatePreviewDecal()
{
	// Decal follows actor automatically
}

void ASG_FireArrowEffect::HidePreviewDecal()
{
	if (PreviewDecal)
	{
		PreviewDecal->SetVisibility(false);
		bPreviewVisible = false;
	}
}