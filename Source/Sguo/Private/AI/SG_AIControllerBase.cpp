	// 🔧 修改 - SG_AIControllerBase.cpp
/**
 * @file SG_AIControllerBase.cpp
 * @brief AI 控制器基类实现
 */

#include "AI/SG_AIControllerBase.h"

#include "AbilitySystem/SG_AttributeSet.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Units/SG_UnitsBase.h"
#include "Kismet/GameplayStatics.h"
#include "Buildings/SG_MainCityBase.h"
#include "Debug/SG_LogCategories.h"
#include "Components/BoxComponent.h"  // ✨ 新增
#include "Actors/SG_FrontLineManager.h" // ✨ 引入前线管理器用于判断左右
// ✨ 新增 - 行为树组件头文件
#include "BehaviorTree/BehaviorTreeComponent.h"

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
	// 🔧 修改 - 不在这里启动行为树，等待 OnPossess
	UE_LOG(LogSGGameplay, Log, TEXT("✓ AI 控制器 BeginPlay 完成"));
	
}

/**
 * @brief 控制 Pawn 时调用
 * @param InPawn 被控制的 Pawn
 * @details
 * 功能说明：
 * - 初始化 AI 逻辑
 * - 🔧 修改：检测单位是否有自定义行为树
 * - 如果单位有自定义行为树，使用单位的行为树
 * - 否则使用控制器默认的行为树
 */
void ASG_AIControllerBase::OnPossess(APawn* InPawn)
{
	// 调用父类（这会创建默认的黑板和行为树组件）
	Super::OnPossess(InPawn);
    
	UE_LOG(LogSGGameplay, Log, TEXT("========== AI 控制器 OnPossess =========="));
	UE_LOG(LogSGGameplay, Log, TEXT("  控制的 Pawn：%s"), *InPawn->GetName());
    
	// ========== 步骤1：确定要使用的行为树 ==========
	UBehaviorTree* BehaviorTreeToUse = nullptr;
    
	// 优先检查单位是否有自定义行为树
	ASG_UnitsBase* ControlledUnit = Cast<ASG_UnitsBase>(InPawn);
	if (ControlledUnit)
	{
		BehaviorTreeToUse = ControlledUnit->GetUnitBehaviorTree();
        
		if (BehaviorTreeToUse)
		{
			UE_LOG(LogSGGameplay, Log, TEXT("  📋 使用单位自定义行为树：%s"), *BehaviorTreeToUse->GetName());
		}
	}
    
	// 如果单位没有自定义行为树，使用控制器默认的
	if (!BehaviorTreeToUse && DefaultBehaviorTree)
	{
		BehaviorTreeToUse = DefaultBehaviorTree;
		UE_LOG(LogSGGameplay, Log, TEXT("  📋 使用控制器默认行为树：%s"), *BehaviorTreeToUse->GetName());
	}
    
	// 如果都没有，输出警告
	if (!BehaviorTreeToUse)
	{
		UE_LOG(LogSGGameplay, Warning, TEXT("  ⚠️ 没有可用的行为树！"));
		UE_LOG(LogSGGameplay, Warning, TEXT("    请在单位蓝图中设置 UnitBehaviorTree"));
		UE_LOG(LogSGGameplay, Warning, TEXT("    或在 AI 控制器中设置 DefaultBehaviorTree"));
		return;
	}
    
	// ========== 步骤2：启动行为树 ==========
	bool bSuccess = StartBehaviorTree(BehaviorTreeToUse);
    
	if (bSuccess)
	{
		UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 行为树启动成功"));
	}
	else
	{
		UE_LOG(LogSGGameplay, Error, TEXT("  ❌ 行为树启动失败"));
	}
    
	UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
}



// ✨ 新增 - 初始化黑板
/**
 * @brief 初始化黑板组件
 * @param BehaviorTreeToUse 要使用的行为树
 * @return 是否成功初始化
 */
bool ASG_AIControllerBase::InitializeBlackboard(UBehaviorTree* BehaviorTreeToUse)
{
    if (!BehaviorTreeToUse)
    {
        UE_LOG(LogSGGameplay, Error, TEXT("  ❌ InitializeBlackboard：行为树为空"));
        return false;
    }
    
    // 获取行为树的黑板资产
    UBlackboardData* BlackboardAsset = BehaviorTreeToUse->BlackboardAsset;
    if (!BlackboardAsset)
    {
        UE_LOG(LogSGGameplay, Error, TEXT("  ❌ 行为树没有关联的黑板资产"));
        return false;
    }
    
    // 使用黑板资产初始化黑板组件
    bool bSuccess = UseBlackboard(BlackboardAsset, Blackboard);
    
    if (bSuccess && Blackboard)
    {
        // 初始化黑板数据
        Blackboard->SetValueAsBool(BB_IsTargetLocked, false);
        Blackboard->SetValueAsBool(BB_IsInAttackRange, false);
        Blackboard->SetValueAsBool(BB_IsTargetMainCity, false);
        
        UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 黑板初始化成功"));
        return true;
    }
    else
    {
        UE_LOG(LogSGGameplay, Error, TEXT("  ❌ 黑板初始化失败"));
        return false;
    }
}

// ✨ 新增 - 启动行为树
/**
 * @brief 启动指定的行为树
 * @param BehaviorTreeToRun 要运行的行为树
 * @return 是否成功启动
 */
bool ASG_AIControllerBase::StartBehaviorTree(UBehaviorTree* BehaviorTreeToRun)
{
    if (!BehaviorTreeToRun)
    {
        UE_LOG(LogSGGameplay, Error, TEXT("  ❌ StartBehaviorTree：行为树为空"));
        return false;
    }
    
    // 停止当前行为树（如果有）
    if (UBehaviorTreeComponent* BTComp = Cast<UBehaviorTreeComponent>(BrainComponent))
    {
        if (BTComp->IsRunning())
        {
            BTComp->StopTree(EBTStopMode::Safe);
            UE_LOG(LogSGGameplay, Verbose, TEXT("  🛑 停止当前行为树"));
        }
    }
    
    // 初始化黑板
    if (!InitializeBlackboard(BehaviorTreeToRun))
    {
        UE_LOG(LogSGGameplay, Error, TEXT("  ❌ 黑板初始化失败，无法启动行为树"));
        return false;
    }
    
    // 运行行为树
    bool bSuccess = RunBehaviorTree(BehaviorTreeToRun);
    
    if (bSuccess)
    {
        CurrentBehaviorTree = BehaviorTreeToRun;
        UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 行为树启动成功：%s"), *BehaviorTreeToRun->GetName());
    }
    else
    {
        UE_LOG(LogSGGameplay, Error, TEXT("  ❌ RunBehaviorTree 失败：%s"), *BehaviorTreeToRun->GetName());
    }
    
    return bSuccess;
}

// ========== OnUnPossess ==========
void ASG_AIControllerBase::OnUnPossess()
{
    // 解绑目标死亡事件
    if (CurrentListenedTarget.IsValid())
    {
        UnbindTargetDeathEvent(CurrentListenedTarget.Get());
        CurrentListenedTarget = nullptr;
    }
    
    // 停止行为树
    if (UBehaviorTreeComponent* BTComp = Cast<UBehaviorTreeComponent>(BrainComponent))
    {
        if (BTComp->IsRunning())
        {
            BTComp->StopTree(EBTStopMode::Safe);
        }
    }
    
    // 清空当前行为树引用
    CurrentBehaviorTree = nullptr;
    
    Super::OnUnPossess();
}

// ========== FreezeAI ==========
void ASG_AIControllerBase::FreezeAI()
{
    // 1. 停止行为树
    if (UBehaviorTreeComponent* BTComp = Cast<UBehaviorTreeComponent>(BrainComponent))
    {
        BTComp->StopTree(EBTStopMode::Safe);
    }
    
    // 2. 停止移动
    StopMovement();
    
    // 3. 解绑目标死亡事件
    if (CurrentListenedTarget.IsValid())
    {
        UnbindTargetDeathEvent(CurrentListenedTarget.Get());
        CurrentListenedTarget = nullptr;
    }
    
    // 4. 清除目标
    SetCurrentTarget(nullptr);
    
    // 5. 停止所有逻辑更新
    SetActorTickEnabled(false);
    
    UE_LOG(LogSGGameplay, Log, TEXT("🥶 AI 已冻结：%s"), 
        GetPawn() ? *GetPawn()->GetName() : TEXT("None"));
}


// ✨ 新增 - 解除控制时调用
	/**
	* @brief 解除控制时调用
	* @details
	* 功能说明：
	* - 清理目标死亡监听
	* - 停止行为树
*/
void ASG_AIControllerBase::OnUnPossess()
	{
	// 解绑目标死亡事件
	if (CurrentListenedTarget.IsValid())
	{
		UnbindTargetDeathEvent(CurrentListenedTarget.Get());
		CurrentListenedTarget = nullptr;
	}
    
		// 调用父类
		Super::OnUnPossess();
}
/**
 * @brief 运行指定的行为树
 * @param NewBehaviorTree 要运行的行为树
 * @return 是否成功启动
 */
bool ASG_AIControllerBase::RunBehaviorTreeAsset(UBehaviorTree* NewBehaviorTree)
{
	if (!NewBehaviorTree)
	{
		UE_LOG(LogSGGameplay, Warning, TEXT("⚠️ RunBehaviorTreeAsset：行为树为空"));
		return false;
	}
    
	// 停止当前行为树（如果有）
	if (UBehaviorTreeComponent* BTComp = Cast<UBehaviorTreeComponent>(BrainComponent))
	{
		if (BTComp->IsRunning())
		{
			BTComp->StopTree(EBTStopMode::Safe);
			UE_LOG(LogSGGameplay, Log, TEXT("  🛑 停止当前行为树"));
		}
	}
    
	// 运行新的行为树
	bool bSuccess = RunBehaviorTree(NewBehaviorTree);
    
	if (bSuccess)
	{
		UE_LOG(LogSGGameplay, Log, TEXT("✓ 成功启动行为树：%s"), *NewBehaviorTree->GetName());
	}
	else
	{
		UE_LOG(LogSGGameplay, Error, TEXT("❌ 启动行为树失败：%s"), *NewBehaviorTree->GetName());
	}
    
	return bSuccess;
}

	/**
 * @brief 冻结 AI
 * @details
 * 功能说明：
 * - 🔧 修改 - 增加解绑目标死亡事件
 */
void ASG_AIControllerBase::FreezeAI()
{
	// 1. 停止行为树
	if (UBehaviorTreeComponent* BTComp = Cast<UBehaviorTreeComponent>(BrainComponent))
	{
		BTComp->StopTree(EBTStopMode::Safe);
	}
    
	// 2. 停止移动
	StopMovement();
    
	// ✨ 新增 - 3. 解绑目标死亡事件
	if (CurrentListenedTarget.IsValid())
	{
		UnbindTargetDeathEvent(CurrentListenedTarget.Get());
		CurrentListenedTarget = nullptr;
	}
    
	// 4. 清除目标
	SetCurrentTarget(nullptr);
    
	// 5. 停止所有逻辑更新
	SetActorTickEnabled(false);
    
	UE_LOG(LogSGGameplay, Log, TEXT("🥶 AI 已冻结：%s"), 
		GetPawn() ? *GetPawn()->GetName() : TEXT("None"));
}

	// ========== 目标管理 ==========

/**
 * @brief 查找最近的目标
 * @return 最近的敌方单位或主城
 * @details
 * 功能说明：
 * - 🔧 修改 - 正方形寻敌范围使用 DetectionRange
 * - 🔧 修改 - 排除已死亡的单位
 * 🔧 修改 - 确保在没有敌方单位时能找到主城
 */
AActor* ASG_AIControllerBase::FindNearestTarget()
{

    // 1. 获取控制的单位
    ASG_UnitsBase* ControlledUnit = Cast<ASG_UnitsBase>(GetPawn());
    if (!ControlledUnit) 
    {
        UE_LOG(LogSGGameplay, Error, TEXT("FindNearestTarget: 控制的单位为空"));
        return nullptr;
    }

    FGameplayTag MyFaction = ControlledUnit->FactionTag;
    FVector MyLoc = ControlledUnit->GetActorLocation();
    
    // 获取寻敌配置
    float DetectionRadius = ControlledUnit->GetDetectionRange();
    ESGTargetSearchShape SearchShape = ControlledUnit->TargetSearchShape;
    bool bPrioritizeFrontmost = ControlledUnit->bPrioritizeFrontmost;

    UE_LOG(LogSGGameplay, Verbose, TEXT("FindNearestTarget: %s 开始寻找目标"), *ControlledUnit->GetName());
    UE_LOG(LogSGGameplay, Verbose, TEXT("  我方阵营：%s"), *MyFaction.ToString());
    UE_LOG(LogSGGameplay, Verbose, TEXT("  寻敌范围：%.0f"), DetectionRadius);

    // 2. 准备候选列表 - 分开处理单位和主城
    TArray<AActor*> AllUnits;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASG_UnitsBase::StaticClass(), AllUnits);
    
    TArray<AActor*> AllMainCities;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASG_MainCityBase::StaticClass(), AllMainCities);

    UE_LOG(LogSGGameplay, Verbose, TEXT("  场上单位数量：%d"), AllUnits.Num());
    UE_LOG(LogSGGameplay, Verbose, TEXT("  场上主城数量：%d"), AllMainCities.Num());

    // 3. 筛选有效的敌方单位
    TArray<AActor*> ValidEnemyUnits;
    
    for (AActor* Actor : AllUnits)
    {
        if (Actor == ControlledUnit) continue;

        ASG_UnitsBase* Unit = Cast<ASG_UnitsBase>(Actor);
        if (!Unit) continue;
        
        // 排除已死亡的单位
        if (Unit->bIsDead) continue;
        
        // 排除同阵营
        if (Unit->FactionTag == MyFaction) continue;

        // 🔧 修改 - 添加可被选为目标的检查
        // 排除不可被选为目标的单位（如某些站桩单位）
        if (!Unit->CanBeTargeted())
        {
            UE_LOG(LogSGGameplay, Verbose, TEXT("    跳过不可选中单位：%s"), *Unit->GetName());
            continue;
        }

        // 范围检查
        FVector TargetLoc = Unit->GetActorLocation();
        bool bInRange = false;
        
        if (SearchShape == ESGTargetSearchShape::Square)
        {
            float DiffX = FMath::Abs(TargetLoc.X - MyLoc.X);
            float DiffY = FMath::Abs(TargetLoc.Y - MyLoc.Y);
            bInRange = (DiffX <= DetectionRadius && DiffY <= DetectionRadius);
        }
        else
        {
            bInRange = (FVector::DistSquared(TargetLoc, MyLoc) <= (DetectionRadius * DetectionRadius));
        }

        if (bInRange)
        {
            ValidEnemyUnits.Add(Unit);
            UE_LOG(LogSGGameplay, Verbose, TEXT("    找到敌方单位：%s"), *Unit->GetName());
        }
    }

    // 4. 如果有敌方单位，选择最佳目标
    if (ValidEnemyUnits.Num() > 0)
    {
        AActor* BestTarget = nullptr;
        
        if (bPrioritizeFrontmost)
        {
            // 最前排优先（X轴最近）
            float BestXDiff = FLT_MAX;
            for (AActor* Target : ValidEnemyUnits)
            {
                float DistX = FMath::Abs(Target->GetActorLocation().X - MyLoc.X);
                if (DistX < BestXDiff)
                {
                    BestXDiff = DistX;
                    BestTarget = Target;
                }
            }
        }
        else
        {
            // 距离优先
            float BestDistSq = FLT_MAX;
            for (AActor* Target : ValidEnemyUnits)
            {
                float DistSq = FVector::DistSquared(Target->GetActorLocation(), MyLoc);
                if (DistSq < BestDistSq)
                {
                    BestDistSq = DistSq;
                    BestTarget = Target;
                }
            }
        }
        
        if (BestTarget)
        {
            UE_LOG(LogSGGameplay, Log, TEXT("FindNearestTarget: 选中敌方单位 %s"), *BestTarget->GetName());
            return BestTarget;
        }
    }

    // ✨ 新增 - 5. 如果没有敌方单位，查找敌方主城（无视距离限制）
    UE_LOG(LogSGGameplay, Log, TEXT("FindNearestTarget: 没有敌方单位，查找敌方主城"));
    
    AActor* NearestMainCity = nullptr;
    float NearestMainCityDist = FLT_MAX;
    
    for (AActor* Actor : AllMainCities)
    {
        ASG_MainCityBase* City = Cast<ASG_MainCityBase>(Actor);
        if (!City) continue;
        
        // 排除已摧毁的主城
        if (!City->IsAlive())
        {
            UE_LOG(LogSGGameplay, Verbose, TEXT("    跳过已摧毁的主城：%s"), *City->GetName());
            continue;
        }
        
        // 排除同阵营
        if (City->FactionTag == MyFaction)
        {
            UE_LOG(LogSGGameplay, Verbose, TEXT("    跳过同阵营主城：%s (阵营: %s)"), 
                *City->GetName(), *City->FactionTag.ToString());
            continue;
        }
        
        // 计算距离
        float Dist = FVector::Dist(MyLoc, City->GetActorLocation());
        UE_LOG(LogSGGameplay, Verbose, TEXT("    找到敌方主城：%s (距离: %.0f)"), *City->GetName(), Dist);
        
        if (Dist < NearestMainCityDist)
        {
            NearestMainCityDist = Dist;
            NearestMainCity = City;
        }
    }
    
    if (NearestMainCity)
    {
        UE_LOG(LogSGGameplay, Log, TEXT("FindNearestTarget: 选中敌方主城 %s"), *NearestMainCity->GetName());
        return NearestMainCity;
    }

    UE_LOG(LogSGGameplay, Warning, TEXT("FindNearestTarget: 未找到任何敌方目标"));
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
 * - 🔧 修改 - 增加 CanBeTargeted 检查
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
            
            // ✨ 新增 - 检查是否可被选为目标
            if (!Unit->CanBeTargeted())
            {
                UE_LOG(LogSGGameplay, Verbose, TEXT("  DetectNearbyThreats: 跳过不可选中单位：%s"), *Unit->GetName());
                continue;
            }
            
            // 计算距离
            float Distance = FVector::Dist(ControlledUnit->GetActorLocation(), Unit->GetActorLocation());
            
            // 如果在检测范围内，转移仇恨
            if (Distance <= DetectionRadius)
            {
                SetCurrentTarget(Unit);
                
                // 立即停止当前移动，强迫行为树重新评估
                StopMovement();

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
 * - 🔧 修改 - 增加目标死亡事件监听
 */
void ASG_AIControllerBase::SetCurrentTarget(AActor* NewTarget)
{
	UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
	if (!BlackboardComp)
	{
		return;
	}
    
	// ✨ 新增 - 解绑旧目标的死亡事件
	if (CurrentListenedTarget.IsValid())
	{
		UnbindTargetDeathEvent(CurrentListenedTarget.Get());
		CurrentListenedTarget = nullptr;
	}
    
	// 更新黑板
	BlackboardComp->SetValueAsObject(BB_CurrentTarget, NewTarget);
    
	// 检查目标是否为主城
	bool bTargetIsMainCity = false;
	if (NewTarget)
	{
		bTargetIsMainCity = NewTarget->IsA(ASG_MainCityBase::StaticClass());
	}
	BlackboardComp->SetValueAsBool(BB_IsTargetMainCity, bTargetIsMainCity);
    
	// 锁定目标
	BlackboardComp->SetValueAsBool(BB_IsTargetLocked, NewTarget != nullptr);
    
	// 更新单位的目标
	if (ASG_UnitsBase* ControlledUnit = Cast<ASG_UnitsBase>(GetPawn()))
	{
		ControlledUnit->SetTarget(NewTarget);
	}
    
	// ✨ 新增 - 绑定新目标的死亡事件
	if (NewTarget)
	{
		if (ASG_UnitsBase* TargetUnit = Cast<ASG_UnitsBase>(NewTarget))
		{
			BindTargetDeathEvent(TargetUnit);
			CurrentListenedTarget = TargetUnit;
		}
        
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
 * - 🔧 修改 - 增强死亡检测
 */
bool ASG_AIControllerBase::IsTargetValid() const
{
	AActor* CurrentTarget = GetCurrentTarget();
	if (!CurrentTarget)
	{
		return false;
	}
    
	// 检查单位是否已死亡或不可被选中
	ASG_UnitsBase* TargetUnit = Cast<ASG_UnitsBase>(CurrentTarget);
	if (TargetUnit)
	{
		// 检查死亡标记
		if (TargetUnit->bIsDead)
		{
			UE_LOG(LogSGGameplay, Verbose, TEXT("  目标单位已死亡（bIsDead）：%s"), *TargetUnit->GetName());
			return false;
		}
        
		// 检查生命值
		if (TargetUnit->AttributeSet && TargetUnit->AttributeSet->GetHealth() <= 0.0f)
		{
			UE_LOG(LogSGGameplay, Verbose, TEXT("  目标单位生命值为 0：%s"), *TargetUnit->GetName());
			return false;
		}
        
		// ✨ 新增 - 检查是否可被选为目标
		if (!TargetUnit->CanBeTargeted())
		{
			UE_LOG(LogSGGameplay, Verbose, TEXT("  目标单位不可被选中：%s"), *TargetUnit->GetName());
			return false;
		}
	}
    
	// 检查主城是否被摧毁
	ASG_MainCityBase* TargetMainCity = Cast<ASG_MainCityBase>(CurrentTarget);
	if (TargetMainCity)
	{
		float MainCityHealth = TargetMainCity->GetCurrentHealth();
		if (MainCityHealth <= 0.0f)
		{
			UE_LOG(LogSGGameplay, Log, TEXT("  ✗ 目标主城已被摧毁：%s（生命值：%.0f）"), 
				*TargetMainCity->GetName(), MainCityHealth);
			return false;
		}
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


// ✨ 新增 - 目标死亡回调
/**
 * @brief 目标死亡回调
 * @param DeadUnit 死亡的单位
 * @details
 * 功能说明：
 * - 当锁定的目标死亡时触发
 * - 清除当前目标
 * - 立即寻找新目标
 * 详细流程：
 * 1. 验证死亡的单位是当前目标
 * 2. 清除当前目标
 * 3. 立即寻找新目标
 * 4. 如果找到新目标，更新黑板
 */
void ASG_AIControllerBase::OnTargetDeath(ASG_UnitsBase* DeadUnit)
{
	// 验证死亡的单位是当前目标
	AActor* CurrentTarget = GetCurrentTarget();
	if (CurrentTarget != DeadUnit)
	{
		return;
	}
    
	UE_LOG(LogSGGameplay, Log, TEXT("🎯 目标死亡，需要重新寻找目标"));
	UE_LOG(LogSGGameplay, Log, TEXT("  死亡目标：%s"), *DeadUnit->GetName());
    
	// 清除监听引用
	CurrentListenedTarget = nullptr;
    
	// 清除当前目标（不触发解绑，因为目标已死亡）
	UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
	if (BlackboardComp)
	{
		BlackboardComp->SetValueAsObject(BB_CurrentTarget, nullptr);
		BlackboardComp->SetValueAsBool(BB_IsTargetLocked, false);
		BlackboardComp->SetValueAsBool(BB_IsTargetMainCity, false);
	}
    
	// 更新单位的目标
	if (ASG_UnitsBase* ControlledUnit = Cast<ASG_UnitsBase>(GetPawn()))
	{
		ControlledUnit->SetTarget(nullptr);
	}
    
	// 立即寻找新目标
	AActor* NewTarget = FindNearestTarget();
	if (NewTarget)
	{
		SetCurrentTarget(NewTarget);
		UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 找到新目标：%s"), *NewTarget->GetName());
	}
	else
	{
		UE_LOG(LogSGGameplay, Log, TEXT("  ⚠️ 未找到新目标"));
	}
}

// ✨ 新增 - 绑定目标死亡事件
/**
 * @brief 绑定目标死亡事件
 * @param Target 目标单位
 */
void ASG_AIControllerBase::BindTargetDeathEvent(ASG_UnitsBase* Target)
{
	if (!Target)
	{
		return;
	}
    
	// 绑定死亡事件
	Target->OnUnitDeathEvent.AddDynamic(this, &ASG_AIControllerBase::OnTargetDeath);
    
	UE_LOG(LogSGGameplay, Verbose, TEXT("  ✓ 绑定目标死亡事件：%s"), *Target->GetName());
}

// ✨ 新增 - 解绑目标死亡事件
/**
 * @brief 解绑目标死亡事件
 * @param Target 目标单位
 */
void ASG_AIControllerBase::UnbindTargetDeathEvent(ASG_UnitsBase* Target)
{
	if (!Target)
	{
		return;
	}
    
	// 解绑死亡事件
	Target->OnUnitDeathEvent.RemoveDynamic(this, &ASG_AIControllerBase::OnTargetDeath);
    
	UE_LOG(LogSGGameplay, Verbose, TEXT("  ✓ 解绑目标死亡事件：%s"), *Target->GetName());
}

