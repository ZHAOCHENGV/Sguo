/**
 * @file SG_AIControllerBase.cpp
 * @brief AI控制器基类实现
 */

#include "AI/SG_AIControllerBase.h"
#include "Units/SG_UnitsBase.h"
#include "Buildings/SG_MainCityBase.h"
#include "Navigation/PathFollowingComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Debug/SG_LogCategories.h"

// 构造函数
ASG_AIControllerBase::ASG_AIControllerBase()
{
	// 启用Tick
	PrimaryActorTick.bCanEverTick = true;
	
	// 启用导航寻路
	bWantsPlayerState = false;
	bSetControlRotationFromPawnOrientation = false;
}

// BeginPlay
void ASG_AIControllerBase::BeginPlay()
{
	Super::BeginPlay();
	
	UE_LOG(LogSGGameplay, Log, TEXT("🤖 AI Controller 已启动：%s"), *GetName());
}

// 当控制Pawn时调用
void ASG_AIControllerBase::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	
	if (ASG_UnitsBase* Unit = Cast<ASG_UnitsBase>(InPawn))
	{
		UE_LOG(LogSGGameplay, Log, TEXT("🤖 AI Controller 控制单位：%s"), *Unit->GetName());
	}
}

// ========== 目标管理 ==========

AActor* ASG_AIControllerBase::FindNearestEnemy(float SearchRadius)
{
	ASG_UnitsBase* ControlledUnit = GetControlledUnit();
	if (!ControlledUnit)
	{
		UE_LOG(LogSGGameplay, Warning, TEXT("❌ FindNearestEnemy: 没有控制的单位"));
		return nullptr;
	}
	
	// 获取单位阵营
	FGameplayTag MyFaction = GetUnitFactionTag();
	if (!MyFaction.IsValid())
	{
		UE_LOG(LogSGGameplay, Warning, TEXT("❌ FindNearestEnemy: 单位阵营标签无效"));
		return nullptr;
	}
	
	// 查找所有单位
	TArray<AActor*> FoundUnits;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASG_UnitsBase::StaticClass(), FoundUnits);
	
	AActor* NearestEnemy = nullptr;
	float NearestDistance = SearchRadius;
	FVector MyLocation = ControlledUnit->GetActorLocation();
	
	// 遍历所有单位，查找最近的敌人
	for (AActor* Actor : FoundUnits)
	{
		ASG_UnitsBase* OtherUnit = Cast<ASG_UnitsBase>(Actor);
		if (!OtherUnit || OtherUnit == ControlledUnit)
			continue;
		
		// 跳过已死亡的单位
		if (OtherUnit->bIsDead)
			continue;
		
		// 检查是否为敌人（阵营不同）
		if (OtherUnit->FactionTag == MyFaction)
			continue;
		
		// 计算距离
		float Distance = FVector::Dist(MyLocation, OtherUnit->GetActorLocation());
		if (Distance < NearestDistance)
		{
			NearestDistance = Distance;
			NearestEnemy = OtherUnit;
		}
	}
	
	if (NearestEnemy)
	{
		UE_LOG(LogSGGameplay, Log, TEXT("🎯 找到最近的敌人：%s，距离：%.1f"), 
			*NearestEnemy->GetName(), NearestDistance);
	}
	
	return NearestEnemy;
}

AActor* ASG_AIControllerBase::FindEnemyMainCity()
{
	ASG_UnitsBase* ControlledUnit = GetControlledUnit();
	if (!ControlledUnit)
	{
		return nullptr;
	}
	
	// 获取单位阵营
	FGameplayTag MyFaction = GetUnitFactionTag();
	if (!MyFaction.IsValid())
	{
		return nullptr;
	}
	
	// 查找所有主城
	TArray<AActor*> FoundMainCities;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASG_MainCityBase::StaticClass(), FoundMainCities);
	
	// 查找敌方主城
	for (AActor* Actor : FoundMainCities)
	{
		ASG_MainCityBase* MainCity = Cast<ASG_MainCityBase>(Actor);
		if (MainCity && MainCity->FactionTag != MyFaction)
		{
			UE_LOG(LogSGGameplay, Log, TEXT("🏰 找到敌方主城：%s"), *MainCity->GetName());
			return MainCity;
		}
	}
	
	return nullptr;
}

void ASG_AIControllerBase::SetCurrentTarget(AActor* NewTarget)
{
	if (CurrentTarget != NewTarget)
	{
		CurrentTarget = NewTarget;
		
		if (NewTarget)
		{
			UE_LOG(LogSGGameplay, Log, TEXT("🎯 设置新目标：%s"), *NewTarget->GetName());
		}
		else
		{
			UE_LOG(LogSGGameplay, Log, TEXT("🎯 清除目标"));
		}
	}
}

bool ASG_AIControllerBase::IsTargetValid() const
{
	if (!CurrentTarget)
	{
		return false;
	}
	
	// 检查目标是否为单位
	if (ASG_UnitsBase* TargetUnit = Cast<ASG_UnitsBase>(CurrentTarget))
	{
		// 检查是否已死亡
		if (TargetUnit->bIsDead)
		{
			return false;
		}
	}
	
	// 检查目标是否仍在搜索范围内
	ASG_UnitsBase* ControlledUnit = GetControlledUnit();
	if (ControlledUnit)
	{
		float Distance = FVector::Dist(ControlledUnit->GetActorLocation(), CurrentTarget->GetActorLocation());
		if (Distance > TargetSearchRadius * 1.5f) // 给予1.5倍容错
		{
			return false;
		}
	}
	
	return true;
}

// ========== 移动控制 ==========

bool ASG_AIControllerBase::MoveToTargetLocation(FVector TargetLocation, float AcceptanceRadius)
{
	EPathFollowingRequestResult::Type Result = MoveToLocation(TargetLocation, AcceptanceRadius);
	return Result == EPathFollowingRequestResult::RequestSuccessful;
}

bool ASG_AIControllerBase::MoveToTargetActor(AActor* TargetActor, float AcceptanceRadius)
{
	if (!TargetActor)
	{
		UE_LOG(LogSGGameplay, Warning, TEXT("❌ MoveToTargetActor: 目标为空"));
		return false;
	}
	
	EPathFollowingRequestResult::Type Result = MoveToActor(TargetActor, AcceptanceRadius);
	return Result == EPathFollowingRequestResult::RequestSuccessful;
}

void ASG_AIControllerBase::StopMovement()
{
	StopMovement();
	UE_LOG(LogSGGameplay, Log, TEXT("🛑 停止移动"));
}

// ========== 战斗控制 ==========

bool ASG_AIControllerBase::IsInAttackRange(AActor* Target, float AttackRange) const
{
	if (!Target)
	{
		return false;
	}
	
	ASG_UnitsBase* ControlledUnit = GetControlledUnit();
	if (!ControlledUnit)
	{
		return false;
	}
	
	// 如果没有指定攻击范围，使用单位的基础攻击范围
	if (AttackRange <= 0.0f)
	{
		AttackRange = ControlledUnit->BaseAttackRange;
	}
	
	// 计算距离
	float Distance = FVector::Dist(ControlledUnit->GetActorLocation(), Target->GetActorLocation());
	return Distance <= AttackRange;
}

void ASG_AIControllerBase::FaceTarget(AActor* Target)
{
	if (!Target)
	{
		return;
	}
	
	ASG_UnitsBase* ControlledUnit = GetControlledUnit();
	if (!ControlledUnit)
	{
		return;
	}
	
	// 计算朝向目标的旋转
	FVector Direction = Target->GetActorLocation() - ControlledUnit->GetActorLocation();
	Direction.Z = 0.0f; // 忽略Z轴
	
	if (!Direction.IsNearlyZero())
	{
		FRotator TargetRotation = Direction.Rotation();
		ControlledUnit->SetActorRotation(TargetRotation);
	}
}

bool ASG_AIControllerBase::PerformAttack()
{
	ASG_UnitsBase* ControlledUnit = GetControlledUnit();
	if (!ControlledUnit)
	{
		UE_LOG(LogSGGameplay, Warning, TEXT("❌ PerformAttack: 没有控制的单位"));
		return false;
	}
	
	// 调用单位的PerformAttack函数（触发GAS攻击能力）
	bool bSuccess = ControlledUnit->PerformAttack();
	
	if (bSuccess)
	{
		UE_LOG(LogSGGameplay, Log, TEXT("⚔️ AI触发攻击"));
	}
	else
	{
		UE_LOG(LogSGGameplay, Warning, TEXT("❌ AI触发攻击失败"));
	}
	
	return bSuccess;
}

// ========== 辅助函数 ==========

ASG_UnitsBase* ASG_AIControllerBase::GetControlledUnit() const
{
	return Cast<ASG_UnitsBase>(GetPawn());
}

FGameplayTag ASG_AIControllerBase::GetUnitFactionTag() const
{
	ASG_UnitsBase* ControlledUnit = GetControlledUnit();
	if (ControlledUnit)
	{
		return ControlledUnit->FactionTag;
	}
	return FGameplayTag::EmptyTag;
}
