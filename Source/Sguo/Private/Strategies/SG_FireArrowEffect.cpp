// 📄 文件：Source/Sguo/Private/Strategies/SG_FireArrowEffect.cpp
// 🔧 修改 - 完整修复版本

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

ASG_FireArrowEffect::ASG_FireArrowEffect()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	PreviewDecal = CreateDefaultSubobject<UDecalComponent>(TEXT("PreviewDecal"));
	RootComponent = PreviewDecal;
	PreviewDecal->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
	PreviewDecal->SetVisibility(false);
}

void ASG_FireArrowEffect::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogSGGameplay, Log, TEXT("========== 火矢计效果生成 =========="));
}

void ASG_FireArrowEffect::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (CurrentState == ESGStrategyEffectState::Executing)
	{
		ParticipatingArchers.RemoveAll([](const TWeakObjectPtr<ASG_StationaryUnit>& Archer)
		{
			if (!Archer.IsValid())
			{
				return true;
			}
			if (Archer->bIsDead)
			{
				return true;
			}
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
	UE_LOG(LogSGGameplay, Log, TEXT("    区域半径：%.1f"), FireArrowCardData->AreaRadius);
	UE_LOG(LogSGGameplay, Log, TEXT("    持续时间：%.1f 秒"), FireArrowCardData->SkillDuration);
	UE_LOG(LogSGGameplay, Log, TEXT("    射击间隔：%.1f 秒"), FireArrowCardData->FireInterval);
	UE_LOG(LogSGGameplay, Log, TEXT("    每轮火箭数：%d"), FireArrowCardData->ArrowsPerArcherPerRound);

	FindParticipatingArchers();
	UE_LOG(LogSGGameplay, Log, TEXT("    可用弓手数：%d"), ParticipatingArchers.Num());

	CreatePreviewDecal();
	SetActorLocation(InTargetLocation);

	UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
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

	UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 火矢计目标选择已开始"));
	UE_LOG(LogSGGameplay, Log, TEXT("    可用弓手：%d"), ParticipatingArchers.Num());
	UE_LOG(LogSGGameplay, Log, TEXT("========================================"));

	return bResult;
}

void ASG_FireArrowEffect::UpdateTargetLocation_Implementation(const FVector& NewLocation)
{
	if (CurrentState != ESGStrategyEffectState::WaitingForTarget)
	{
		return;
	}

	TargetLocation = NewLocation;
	SetActorLocation(NewLocation);
	UpdatePreviewDecal();
}

bool ASG_FireArrowEffect::ConfirmTarget_Implementation()
{
	UE_LOG(LogSGGameplay, Log, TEXT("========== 火矢计确认目标 =========="));

	if (CurrentState != ESGStrategyEffectState::WaitingForTarget)
	{
		UE_LOG(LogSGGameplay, Warning, TEXT("  ⚠️ 当前不在目标选择状态"));
		return false;
	}

	FindParticipatingArchers();

	if (!CanExecute())
	{
		FText Reason = GetCannotExecuteReason();
		UE_LOG(LogSGGameplay, Warning, TEXT("  ⚠️ %s"), *Reason.ToString());
		return false;
	}

	UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 目标确认：%s"), *TargetLocation.ToString());

	HidePreviewDecal();
	ExecuteEffect();

	UE_LOG(LogSGGameplay, Log, TEXT("========================================"));

	return true;
}

void ASG_FireArrowEffect::CancelEffect_Implementation()
{
	UE_LOG(LogSGGameplay, Log, TEXT("  火矢计被取消"));
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

void ASG_FireArrowEffect::ExecuteEffect_Implementation()
{
	UE_LOG(LogSGGameplay, Log, TEXT("========== 执行火矢计 =========="));

	if (ParticipatingArchers.Num() == 0)
	{
		UE_LOG(LogSGGameplay, Warning, TEXT("  ⚠️ 没有可用的弓手，火矢计无法执行"));
		EndEffect();
		return;
	}

	SetState(ESGStrategyEffectState::Executing);
	SkillStartTime = GetWorld()->GetTimeSeconds();

	// ✨ 通知弓手开始火矢技能
	NotifyArchersStartFireArrow();

	// 立即执行第一轮射击
	ExecuteFireRound();

	float FireInterval = FireArrowCardData ? FireArrowCardData->FireInterval : 0.3f;
	float SkillDuration = FireArrowCardData ? FireArrowCardData->SkillDuration : 5.0f;

	GetWorld()->GetTimerManager().SetTimer(
		FireTimerHandle,
		this,
		&ASG_FireArrowEffect::OnFireTimerTick,
		FireInterval,
		true
	);

	GetWorld()->GetTimerManager().SetTimer(
		DurationTimerHandle,
		this,
		&ASG_FireArrowEffect::OnSkillDurationEnd,
		SkillDuration,
		false
	);

	UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 火矢计开始执行"));
	UE_LOG(LogSGGameplay, Log, TEXT("    参与弓手：%d"), ParticipatingArchers.Num());
	UE_LOG(LogSGGameplay, Log, TEXT("    持续时间：%.1f 秒"), SkillDuration);
	UE_LOG(LogSGGameplay, Log, TEXT("    射击间隔：%.1f 秒"), FireInterval);
	UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
}

void ASG_FireArrowEffect::FindParticipatingArchers()
{
	ParticipatingArchers.Empty();

	TArray<AActor*> AllActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASG_StationaryUnit::StaticClass(), AllActors);

	for (AActor* Actor : AllActors)
	{
		ASG_StationaryUnit* StationaryUnit = Cast<ASG_StationaryUnit>(Actor);
		if (!StationaryUnit)
		{
			continue;
		}

		if (StationaryUnit->bIsDead)
		{
			continue;
		}

		if (StationaryUnit->FactionTag != InstigatorFactionTag)
		{
			continue;
		}

		if (!StationaryUnit->IsHovering())
		{
			continue;
		}

		ParticipatingArchers.Add(StationaryUnit);
		UE_LOG(LogSGGameplay, Verbose, TEXT("    找到弓手：%s"), *StationaryUnit->GetName());
	}
}

void ASG_FireArrowEffect::ExecuteFireRound()
{
	UE_LOG(LogSGGameplay, Log, TEXT("  🏹 执行火矢计第 %d 轮射击"), FiredRounds + 1);

	int32 ValidArcherCount = 0;
	int32 ArrowCount = FireArrowCardData ? FireArrowCardData->ArrowsPerArcherPerRound : 3;

	for (const TWeakObjectPtr<ASG_StationaryUnit>& ArcherPtr : ParticipatingArchers)
	{
		if (!ArcherPtr.IsValid())
		{
			continue;
		}

		ASG_StationaryUnit* Archer = ArcherPtr.Get();

		if (Archer->bIsDead)
		{
			continue;
		}

		FireArrowsFromArcher(Archer, ArrowCount);
		ValidArcherCount++;
	}

	K2_OnFireRoundStarted(FiredRounds, ValidArcherCount);
	FiredRounds++;

	UE_LOG(LogSGGameplay, Log, TEXT("    参与弓手：%d，每人发射：%d 支"), ValidArcherCount, ArrowCount);
}

void ASG_FireArrowEffect::FireArrowsFromArcher(ASG_StationaryUnit* Archer, int32 ArrowCount)
{
	if (!Archer || ArrowCount <= 0)
	{
		return;
	}

	// 获取火箭类
	TSubclassOf<AActor> ProjectileClass = nullptr;
	if (FireArrowCardData && FireArrowCardData->FireArrowProjectileClass)
	{
		ProjectileClass = FireArrowCardData->FireArrowProjectileClass;
	}
	else
	{
		ProjectileClass = Archer->GetFireArrowProjectileClass();
	}

	// 发射多支火箭
	for (int32 i = 0; i < ArrowCount; ++i)
	{
		FVector RandomTarget = GenerateRandomTargetInArea();

		// 使用弓手的 FireArrow 方法
		AActor* SpawnedProjectile = Archer->FireArrow(RandomTarget, ProjectileClass);

		// 设置额外参数
		if (ASG_Projectile* Projectile = Cast<ASG_Projectile>(SpawnedProjectile))
		{
			if (FireArrowCardData)
			{
				Projectile->SetFlightSpeed(FireArrowCardData->ArrowSpeed);
				Projectile->ArcHeight = FireArrowCardData->ArrowArcHeight;
				Projectile->DamageMultiplier = FireArrowCardData->ArrowDamageMultiplier;
			}
		}

		K2_OnArrowFired(Archer, RandomTarget);
	}

	UE_LOG(LogSGGameplay, Verbose, TEXT("    弓手 %s 发射了 %d 支火矢"), 
		*Archer->GetName(), ArrowCount);
}

FVector ASG_FireArrowEffect::GenerateRandomTargetInArea() const
{
	float Radius = FireArrowCardData ? FireArrowCardData->AreaRadius : 800.0f;
	float RandomAngle = FMath::FRandRange(0.0f, 360.0f);
	float RandomRadius = FMath::Sqrt(FMath::FRand()) * Radius;

	FVector Offset;
	Offset.X = RandomRadius * FMath::Cos(FMath::DegreesToRadians(RandomAngle));
	Offset.Y = RandomRadius * FMath::Sin(FMath::DegreesToRadians(RandomAngle));
	Offset.Z = 0.0f;

	return TargetLocation + Offset;
}

void ASG_FireArrowEffect::CreatePreviewDecal()
{
	if (!PreviewDecal)
	{
		return;
	}

	float Radius = FireArrowCardData ? FireArrowCardData->AreaRadius : 800.0f;
	PreviewDecal->DecalSize = FVector(1000.0f, Radius, Radius);

	if (FireArrowCardData && FireArrowCardData->PreviewAreaMaterial)
	{
		UMaterialInstanceDynamic* DynamicMaterial = UMaterialInstanceDynamic::Create(
			FireArrowCardData->PreviewAreaMaterial,
			this
		);

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
	// 贴花跟随 Actor 移动
}

void ASG_FireArrowEffect::HidePreviewDecal()
{
	if (PreviewDecal)
	{
		PreviewDecal->SetVisibility(false);
		bPreviewVisible = false;
	}
}

void ASG_FireArrowEffect::NotifyArchersStartFireArrow()
{
	UE_LOG(LogSGGameplay, Log, TEXT("  通知弓手开始火矢技能"));
	
	for (const TWeakObjectPtr<ASG_StationaryUnit>& ArcherPtr : ParticipatingArchers)
	{
		if (!ArcherPtr.IsValid())
		{
			continue;
		}

		ASG_StationaryUnit* Archer = ArcherPtr.Get();
		Archer->StartFireArrowSkill();

		UE_LOG(LogSGGameplay, Verbose, TEXT("    ✓ %s 开始火矢技能"), *Archer->GetName());
	}
}

void ASG_FireArrowEffect::NotifyArchersEndFireArrow()
{
	UE_LOG(LogSGGameplay, Log, TEXT("  通知弓手结束火矢技能"));
	
	for (const TWeakObjectPtr<ASG_StationaryUnit>& ArcherPtr : ParticipatingArchers)
	{
		if (!ArcherPtr.IsValid())
		{
			continue;
		}

		ASG_StationaryUnit* Archer = ArcherPtr.Get();
		Archer->EndFireArrowSkill();

		UE_LOG(LogSGGameplay, Verbose, TEXT("    ✓ %s 结束火矢技能"), *Archer->GetName());
	}
}

void ASG_FireArrowEffect::OnFireTimerTick()
{
	if (CurrentState != ESGStrategyEffectState::Executing)
	{
		return;
	}

	ExecuteFireRound();
}

void ASG_FireArrowEffect::OnSkillDurationEnd()
{
	UE_LOG(LogSGGameplay, Log, TEXT("========== 火矢计完成 =========="));
	UE_LOG(LogSGGameplay, Log, TEXT("  总轮数：%d"), FiredRounds);

	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(FireTimerHandle);
	}

	NotifyArchersEndFireArrow();
	K2_OnFireArrowCompleted(FiredRounds);
	EndEffect();

	UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
}
