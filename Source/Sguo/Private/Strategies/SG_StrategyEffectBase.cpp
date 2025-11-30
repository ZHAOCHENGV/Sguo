// 📄 文件：Source/Sguo/Private/Strategies/SG_StrategyEffectBase.cpp
// 🔧 修改 - 添加预览和交互接口实现

#include "Strategies/SG_StrategyEffectBase.h"
#include "Data/SG_StrategyCardData.h"
#include "Units/SG_UnitsBase.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Kismet/GameplayStatics.h"
#include "Debug/SG_LogCategories.h"

ASG_StrategyEffectBase::ASG_StrategyEffectBase()
{
	// ✨ 新增 - 启用 Tick（子类可能需要）
	PrimaryActorTick.bCanEverTick = true;
	// 默认禁用，子类按需启用
	PrimaryActorTick.bStartWithTickEnabled = false;
}

void ASG_StrategyEffectBase::BeginPlay()
{
	Super::BeginPlay();
}

void ASG_StrategyEffectBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	// 基类不做任何事，子类按需实现
}

void ASG_StrategyEffectBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

void ASG_StrategyEffectBase::InitializeEffect(
	USG_StrategyCardData* InCardData,
	AActor* InEffectInstigator,
	const FVector& InTargetLocation)
{
	UE_LOG(LogSGGameplay, Log, TEXT("========== 初始化计谋效果 =========="));
	
	// 缓存数据
	CardData = InCardData;
	EffectInstigator = InEffectInstigator;
	TargetLocation = InTargetLocation;
	
	// 从卡牌数据读取持续时间
	if (CardData)
	{
		EffectDuration = CardData->Duration;
		UE_LOG(LogSGGameplay, Log, TEXT("  卡牌：%s"), *CardData->CardName.ToString());
		UE_LOG(LogSGGameplay, Log, TEXT("  持续时间：%.1f 秒"), EffectDuration);
	}
	
	// 确定施放者阵营
	InstigatorFactionTag = FGameplayTag::RequestGameplayTag(FName("Unit.Faction.Player"), false);
	
	if (ASG_UnitsBase* InstigatorUnit = Cast<ASG_UnitsBase>(InEffectInstigator))
	{
		InstigatorFactionTag = InstigatorUnit->FactionTag;
	}
	
	UE_LOG(LogSGGameplay, Log, TEXT("  施放者阵营：%s"), *InstigatorFactionTag.ToString());
	UE_LOG(LogSGGameplay, Log, TEXT("  目标位置：%s"), *TargetLocation.ToString());
	
	// 标记已初始化
	bIsInitialized = true;
	
	UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
}

// ========== ✨ 新增 - 目标选择接口实现 ==========

bool ASG_StrategyEffectBase::RequiresTargetSelection_Implementation() const
{
	// 根据放置类型判断
	if (CardData)
	{
		// Global 类型不需要目标选择
		if (CardData->PlacementType == ESGPlacementType::Global)
		{
			return false;
		}
		// Area 和 Single 类型需要目标选择
		return true;
	}
	
	// 默认需要
	return true;
}

bool ASG_StrategyEffectBase::CanExecute_Implementation() const
{
	// 基类默认返回 true
	// 子类应该重写以实现特定检查
	return true;
}

FText ASG_StrategyEffectBase::GetCannotExecuteReason_Implementation() const
{
	// 基类返回空文本
	return FText::GetEmpty();
}

bool ASG_StrategyEffectBase::StartTargetSelection_Implementation()
{
	UE_LOG(LogSGGameplay, Log, TEXT("========== 开始目标选择 =========="));
	
	// 检查是否已初始化
	if (!bIsInitialized)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("  ❌ 效果未初始化！"));
		return false;
	}
	
	// 检查是否可以执行
	if (!CanExecute())
	{
		FText Reason = GetCannotExecuteReason();
		UE_LOG(LogSGGameplay, Warning, TEXT("  ⚠️ 无法执行：%s"), *Reason.ToString());
		return false;
	}
	
	// 设置状态
	SetState(ESGStrategyEffectState::WaitingForTarget);
	
	UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 目标选择已开始"));
	UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
	
	return true;
}

void ASG_StrategyEffectBase::UpdateTargetLocation_Implementation(const FVector& NewLocation)
{
	// 只在等待目标状态下更新
	if (CurrentState != ESGStrategyEffectState::WaitingForTarget)
	{
		return;
	}
	
	// 更新目标位置
	TargetLocation = NewLocation;
	
	// 更新 Actor 位置
	SetActorLocation(NewLocation);
}

bool ASG_StrategyEffectBase::ConfirmTarget_Implementation()
{
	UE_LOG(LogSGGameplay, Log, TEXT("========== 确认目标 =========="));
	
	// 检查状态
	if (CurrentState != ESGStrategyEffectState::WaitingForTarget)
	{
		UE_LOG(LogSGGameplay, Warning, TEXT("  ⚠️ 当前不在目标选择状态"));
		return false;
	}
	
	// 检查是否可以执行
	if (!CanExecute())
	{
		FText Reason = GetCannotExecuteReason();
		UE_LOG(LogSGGameplay, Warning, TEXT("  ⚠️ 无法执行：%s"), *Reason.ToString());
		return false;
	}
	
	UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 目标确认成功"));
	UE_LOG(LogSGGameplay, Log, TEXT("    位置：%s"), *TargetLocation.ToString());
	
	// 执行效果
	ExecuteEffect();
	
	UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
	
	return true;
}

void ASG_StrategyEffectBase::CancelEffect_Implementation()
{
	UE_LOG(LogSGGameplay, Log, TEXT("========== 取消计谋效果 =========="));
	
	// 设置状态
	SetState(ESGStrategyEffectState::Cancelled);
	
	// 广播完成事件（失败）
	OnEffectFinished.Broadcast(this, false);
	
	// 销毁 Actor
	Destroy();
	
	UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
}

void ASG_StrategyEffectBase::InterruptEffect_Implementation()
{
	UE_LOG(LogSGGameplay, Warning, TEXT("========== 计谋效果被打断 =========="));
	
	// 设置状态
	SetState(ESGStrategyEffectState::Interrupted);
	
	// 广播完成事件（失败）
	OnEffectFinished.Broadcast(this, false);
	
	// 销毁 Actor
	Destroy();
	
	UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
}

void ASG_StrategyEffectBase::ExecuteEffect_Implementation()
{
	UE_LOG(LogSGGameplay, Log, TEXT("========== 执行计谋效果 =========="));
	
	// 设置状态
	SetState(ESGStrategyEffectState::Executing);
	
	// 基类不执行任何效果
	// 子类需要重写此函数
	UE_LOG(LogSGGameplay, Warning, TEXT("  ⚠️ ASG_StrategyEffectBase::ExecuteEffect_Implementation 被调用"));
	UE_LOG(LogSGGameplay, Warning, TEXT("     子类应该重写此函数！"));
	
	UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
}

void ASG_StrategyEffectBase::EndEffect()
{
	UE_LOG(LogSGGameplay, Log, TEXT("========== 结束计谋效果 =========="));
	
	// 设置状态
	SetState(ESGStrategyEffectState::Completed);
	
	if (CardData)
	{
		UE_LOG(LogSGGameplay, Log, TEXT("  卡牌：%s"), *CardData->CardName.ToString());
	}
	
	// 广播完成事件（成功）
	OnEffectFinished.Broadcast(this, true);
	
	// 销毁 Actor
	Destroy();
	
	UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
}

void ASG_StrategyEffectBase::SetState(ESGStrategyEffectState NewState)
{
	ESGStrategyEffectState OldState = CurrentState;
	CurrentState = NewState;
	
	UE_LOG(LogSGGameplay, Verbose, TEXT("  状态变化：%d -> %d"), 
		static_cast<int32>(OldState), 
		static_cast<int32>(NewState));
}

// ========== 辅助函数（保持不变）==========

void ASG_StrategyEffectBase::GetAllUnitsOfFaction(FGameplayTag FactionTag, TArray<AActor*>& OutUnits)
{
	OutUnits.Empty();
	
	TArray<AActor*> AllUnits;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASG_UnitsBase::StaticClass(), AllUnits);
	
	for (AActor* Actor : AllUnits)
	{
		ASG_UnitsBase* Unit = Cast<ASG_UnitsBase>(Actor);
		if (Unit && !Unit->bIsDead && Unit->FactionTag.MatchesTag(FactionTag))
		{
			OutUnits.Add(Unit);
		}
	}
	
	UE_LOG(LogSGGameplay, Verbose, TEXT("  找到 %d 个 %s 阵营的单位"), 
		OutUnits.Num(), *FactionTag.ToString());
}

void ASG_StrategyEffectBase::GetUnitsInRadius(
	const FVector& Center,
	float Radius,
	FGameplayTag FactionTag,
	TArray<AActor*>& OutUnits)
{
	OutUnits.Empty();
	
	TArray<AActor*> AllUnits;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASG_UnitsBase::StaticClass(), AllUnits);
	
	for (AActor* Actor : AllUnits)
	{
		ASG_UnitsBase* Unit = Cast<ASG_UnitsBase>(Actor);
		if (!Unit || Unit->bIsDead)
		{
			continue;
		}
		
		if (FactionTag.IsValid() && !Unit->FactionTag.MatchesTag(FactionTag))
		{
			continue;
		}
		
		float Distance = FVector::Dist(Center, Unit->GetActorLocation());
		if (Distance <= Radius)
		{
			OutUnits.Add(Unit);
		}
	}
	
	UE_LOG(LogSGGameplay, Verbose, TEXT("  在半径 %.0f 内找到 %d 个单位"), 
		Radius, OutUnits.Num());
}

bool ASG_StrategyEffectBase::ApplyGameplayEffectToTarget(
	AActor* TargetActor,
	TSubclassOf<UGameplayEffect> EffectClass,
	float Level)
{
	if (!TargetActor || !EffectClass)
	{
		UE_LOG(LogSGGameplay, Warning, TEXT("  ⚠️ ApplyGameplayEffectToTarget：参数无效"));
		return false;
	}
	
	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor);
	if (!TargetASC)
	{
		UE_LOG(LogSGGameplay, Warning, TEXT("  ⚠️ 目标 %s 没有 ASC"), *TargetActor->GetName());
		return false;
	}
	
	FGameplayEffectContextHandle ContextHandle = TargetASC->MakeEffectContext();
	ContextHandle.AddInstigator(EffectInstigator, EffectInstigator);
	
	FGameplayEffectSpecHandle SpecHandle = TargetASC->MakeOutgoingSpec(EffectClass, Level, ContextHandle);
	if (!SpecHandle.IsValid())
	{
		UE_LOG(LogSGGameplay, Warning, TEXT("  ⚠️ 无法创建 GE 规格"));
		return false;
	}
	
	FActiveGameplayEffectHandle ActiveHandle = TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	
	if (ActiveHandle.IsValid())
	{
		UE_LOG(LogSGGameplay, Verbose, TEXT("  ✓ 对 %s 应用 %s 成功"), 
			*TargetActor->GetName(), *EffectClass->GetName());
		return true;
	}
	
	return false;
}

FGameplayTag ASG_StrategyEffectBase::GetInstigatorFactionTag() const
{
	return InstigatorFactionTag;
}