// 🔧 修改 - SG_AIControllerBase.cpp
/**
 * @file SG_AIControllerBase.cpp
 * @brief AI 控制器基类实现
 */

#include "AI/SG_AIControllerBase.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Units/SG_UnitsBase.h"
#include "Kismet/GameplayStatics.h"
#include "Buildings/SG_MainCityBase.h"
#include "Debug/SG_LogCategories.h"

// ========== 黑板键名称定义 ==========
const FName ASG_AIControllerBase::BB_CurrentTarget = TEXT("CurrentTarget");
const FName ASG_AIControllerBase::BB_IsInAttackRange = TEXT("IsInAttackRange");
const FName ASG_AIControllerBase::BB_IsTargetLocked = TEXT("IsTargetLocked");
const FName ASG_AIControllerBase::BB_IsTargetMainCity = TEXT("IsTargetMainCity");

// ========== 构造函数 ==========

/**
 * @brief 构造函数
 * @details
 * 功能说明：
 * - 初始化 AI 组件
 * - 配置默认参数
 */
ASG_AIControllerBase::ASG_AIControllerBase()
{
	// 启用 Tick
	PrimaryActorTick.bCanEverTick = true;
	
	// 不需要 PlayerState
	bWantsPlayerState = false;
	
	// 不从 Pawn 获取控制旋转
	bSetControlRotationFromPawnOrientation = false;
}

// ========== 生命周期 ==========

/**
 * @brief 开始游戏时调用
 * @details
 * 功能说明：
 * - 启动行为树
 * - 初始化黑板数据
 */
void ASG_AIControllerBase::BeginPlay()
{
	Super::BeginPlay();
	
	// 如果配置了行为树，启动它
	if (BehaviorTreeAsset)
	{
		RunBehaviorTree(BehaviorTreeAsset);
		UE_LOG(LogSGGameplay, Log, TEXT("✓ AI 控制器启动行为树：%s"), *BehaviorTreeAsset->GetName());
	}
	else
	{
		UE_LOG(LogSGGameplay, Warning, TEXT("⚠️ AI 控制器未配置行为树"));
	}
}

/**
 * @brief 控制 Pawn 时调用
 * @param InPawn 被控制的 Pawn
 * @details
 * 功能说明：
 * - 初始化 AI 逻辑
 * - 启动行为树
 */
void ASG_AIControllerBase::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	
	// 获取黑板组件
	UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
	if (!BlackboardComp)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("❌ AI 控制器没有黑板组件"));
		return;
	}
	
	// 初始化黑板数据
	BlackboardComp->SetValueAsBool(BB_IsTargetLocked, false);
	BlackboardComp->SetValueAsBool(BB_IsInAttackRange, false);
	BlackboardComp->SetValueAsBool(BB_IsTargetMainCity, false);
	
	UE_LOG(LogSGGameplay, Log, TEXT("✓ AI 控制器接管 Pawn：%s"), *InPawn->GetName());
}

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
AActor* ASG_AIControllerBase::FindNearestTarget()
{
	// 获取控制的单位
	ASG_UnitsBase* ControlledUnit = Cast<ASG_UnitsBase>(GetPawn());
	if (!ControlledUnit)
	{
		return nullptr;
	}
	
	// 获取单位的阵营标签
	FGameplayTag MyFaction = ControlledUnit->FactionTag;
	
	// 获取所有单位
	TArray<AActor*> AllUnits;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASG_UnitsBase::StaticClass(), AllUnits);
	
	// 查找最近的敌方单位
	AActor* NearestEnemy = nullptr;
	float MinDistance = FLT_MAX;
	
	for (AActor* Actor : AllUnits)
	{
		// 排除自己
		if (Actor == ControlledUnit)
		{
			continue;
		}
		
		// 转换为单位类型
		ASG_UnitsBase* Unit = Cast<ASG_UnitsBase>(Actor);
		if (!Unit)
		{
			continue;
		}
		
		// 检查阵营（不同阵营才是敌人）
		if (Unit->FactionTag != MyFaction)
		{
			// 检查是否已死亡
			if (Unit->bIsDead)
			{
				continue;
			}
			
			// 计算距离
			float Distance = FVector::Dist(ControlledUnit->GetActorLocation(), Unit->GetActorLocation());
			
			// 更新最近敌人
			if (Distance < MinDistance)
			{
				MinDistance = Distance;
				NearestEnemy = Unit;
			}
		}
	}
	
	// 如果找到敌方单位，返回
	if (NearestEnemy)
	{
		UE_LOG(LogSGGameplay, Verbose, TEXT("%s 找到最近的敌方单位：%s (距离: %.0f)"), 
			*ControlledUnit->GetName(), *NearestEnemy->GetName(), MinDistance);
		return NearestEnemy;
	}
	
	// ✨ 新增 - 如果没有敌方单位，查找敌方主城
	UE_LOG(LogSGGameplay, Verbose, TEXT("%s 未找到敌方单位，尝试查找敌方主城"), *ControlledUnit->GetName());
	
	// 获取所有主城
	TArray<AActor*> AllMainCities;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASG_MainCityBase::StaticClass(), AllMainCities);
	
	// 查找敌方主城
	for (AActor* Actor : AllMainCities)
	{
		// 转换为主城类型
		ASG_MainCityBase* MainCity = Cast<ASG_MainCityBase>(Actor);
		if (!MainCity)
		{
			continue;
		}
		
		// 检查阵营（不同阵营才是敌人）
		if (MainCity->FactionTag != MyFaction)
		{
			// 检查主城是否已被摧毁
			if (MainCity->GetCurrentHealth() <= 0.0f)
			{
				continue;
			}
			
			// 找到敌方主城
			UE_LOG(LogSGGameplay, Log, TEXT("%s 找到敌方主城：%s"), 
				*ControlledUnit->GetName(), *MainCity->GetName());
			return MainCity;
		}
	}
	
	// 如果连主城都没找到
	UE_LOG(LogSGGameplay, Warning, TEXT("%s 未找到任何目标（单位或主城）"), *ControlledUnit->GetName());
	return nullptr;
}

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
bool ASG_AIControllerBase::DetectNearbyThreats(float DetectionRadius)
{
	// 获取控制的单位
	ASG_UnitsBase* ControlledUnit = Cast<ASG_UnitsBase>(GetPawn());
	if (!ControlledUnit)
	{
		return false;
	}
	
	// 获取当前目标
	AActor* CurrentTarget = GetCurrentTarget();
	
	// 🔧 修改 - 只有当前目标是主城时才检测周边威胁
	UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
	if (BlackboardComp && !BlackboardComp->GetValueAsBool(BB_IsTargetMainCity))
	{
		// 当前目标不是主城，不需要检测
		return false;
	}
	
	// 获取单位的阵营标签
	FGameplayTag MyFaction = ControlledUnit->FactionTag;
	
	// 获取所有单位
	TArray<AActor*> AllUnits;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASG_UnitsBase::StaticClass(), AllUnits);
	
	// 查找检测范围内的敌方单位
	for (AActor* Actor : AllUnits)
	{
		// 排除自己和当前目标
		if (Actor == ControlledUnit || Actor == CurrentTarget)
		{
			continue;
		}
		
		// 转换为单位类型
		ASG_UnitsBase* Unit = Cast<ASG_UnitsBase>(Actor);
		if (!Unit)
		{
			continue;
		}
		
		// 检查阵营（不同阵营才是敌人）
		if (Unit->FactionTag != MyFaction)
		{
			// 检查是否已死亡
			if (Unit->bIsDead)
			{
				continue;
			}
			
			// 计算距离
			float Distance = FVector::Dist(ControlledUnit->GetActorLocation(), Unit->GetActorLocation());
			
			// 如果在检测范围内，转移仇恨
			if (Distance <= DetectionRadius)
			{
				SetCurrentTarget(Unit);
				UE_LOG(LogSGGameplay, Log, TEXT("🔄 %s 检测到周边威胁，从主城转移目标到单位：%s"), 
					*ControlledUnit->GetName(), *Unit->GetName());
				return true;
			}
		}
	}
	
	return false;
}

/**
 * @brief 设置当前目标
 * @param NewTarget 新目标
 * @details
 * 功能说明：
 * - 更新黑板中的目标数据
 * - 通知行为树目标已改变
 */
void ASG_AIControllerBase::SetCurrentTarget(AActor* NewTarget)
{
	UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
	if (!BlackboardComp)
	{
		return;
	}
	
	// 更新黑板
	BlackboardComp->SetValueAsObject(BB_CurrentTarget, NewTarget);
	
	// 🔧 修改 - 使用 ASG_MainCityBase 类型判断目标是否为主城
	bool bTargetIsMainCity = false;
	if (NewTarget)
	{
		// 检查目标是否为主城类型
		bTargetIsMainCity = NewTarget->IsA(ASG_MainCityBase::StaticClass());
	}
	BlackboardComp->SetValueAsBool(BB_IsTargetMainCity, bTargetIsMainCity);
	
	// 锁定目标（只有在目标死亡后才会切换）
	BlackboardComp->SetValueAsBool(BB_IsTargetLocked, NewTarget != nullptr);
	
	// 更新单位的目标（用于 GAS 攻击）
	if (ASG_UnitsBase* ControlledUnit = Cast<ASG_UnitsBase>(GetPawn()))
	{
		ControlledUnit->SetTarget(NewTarget);
	}
	
	if (NewTarget)
	{
		UE_LOG(LogSGGameplay, Verbose, TEXT("🎯 设置目标：%s%s"), 
			*NewTarget->GetName(),
			bTargetIsMainCity ? TEXT(" (主城)") : TEXT(""));
	}
	else
	{
		UE_LOG(LogSGGameplay, Verbose, TEXT("🎯 清空目标"));
	}
}

/**
 * @brief 获取当前目标
 * @return 当前目标
 * @details
 * 功能说明：
 * - 从黑板读取当前目标
 */
AActor* ASG_AIControllerBase::GetCurrentTarget() const
{
	// 🔧 修复 - 使用 const 限定符
	const UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
	if (!BlackboardComp)
	{
		return nullptr;
	}
	
	return Cast<AActor>(BlackboardComp->GetValueAsObject(BB_CurrentTarget));
}

/**
 * @brief 检查目标是否有效
 * @return 目标是否有效
 * @details
 * 功能说明：
 * - 检查目标是否存在、是否存活
 * - 用于行为树装饰器
 */
bool ASG_AIControllerBase::IsTargetValid() const
{
	AActor* CurrentTarget = GetCurrentTarget();
	if (!CurrentTarget)
	{
		return false;
	}
	
	// 检查目标是否已死亡
	ASG_UnitsBase* TargetUnit = Cast<ASG_UnitsBase>(CurrentTarget);
	if (TargetUnit && TargetUnit->bIsDead)
	{
		return false;
	}
	
	return true;
}

// ========== 主城特殊逻辑 ==========

/**
 * @brief 打断主城攻击
 * @details
 * 功能说明：
 * - 火矢计施放时调用
 * - 停止当前攻击行为
 */
void ASG_AIControllerBase::InterruptAttack()
{
	if (!bIsMainCity)
	{
		return;
	}
	
	bAttackInterrupted = true;
	
	// 更新黑板
	UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
	if (BlackboardComp)
	{
		BlackboardComp->SetValueAsBool(TEXT("AttackInterrupted"), true);
	}
	
	UE_LOG(LogSGGameplay, Log, TEXT("🛑 主城攻击被打断（火矢计）"));
}

/**
 * @brief 恢复主城攻击
 * @details
 * 功能说明：
 * - 火矢计结束时调用
 * - 恢复攻击行为
 */
void ASG_AIControllerBase::ResumeAttack()
{
	if (!bIsMainCity)
	{
		return;
	}
	
	bAttackInterrupted = false;
	
	// 更新黑板
	UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
	if (BlackboardComp)
	{
		BlackboardComp->SetValueAsBool(TEXT("AttackInterrupted"), false);
	}
	
	UE_LOG(LogSGGameplay, Log, TEXT("▶️ 主城恢复攻击"));
}
