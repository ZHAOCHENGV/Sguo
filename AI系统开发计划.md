# AI系统开发完整计划

## 📋 目录
1. [系统概述](#系统概述)
2. [技术选型](#技术选型)
3. [架构设计](#架构设计)
4. [开发路线图](#开发路线图)
5. [详细实现步骤](#详细实现步骤)
6. [测试计划](#测试计划)

---

## 🎯 系统概述

### 核心功能需求
1. **寻路系统** - 单位自动寻找到达目标的路径
2. **目标查找** - 自动查找最近的敌人/主城
3. **战斗AI** - 自动进攻敌人并使用攻击能力
4. **攻城AI** - 自动进攻敌方主城
5. **状态管理** - 巡逻、追击、攻击、撤退等状态切换

### 设计目标
- ✅ 简单易维护
- ✅ 与GAS攻击系统无缝集成
- ✅ 支持多人游戏（网络复制）
- ✅ 高性能（支持大量单位）
- ✅ 可扩展（易于添加新行为）

---

## 🔧 技术选型

### 方案对比

| 方案 | 优点 | 缺点 | 推荐度 |
|------|------|------|--------|
| **StateTree** | UE5原生，轻量高效，可视化编辑 | 需要学习新系统 | ⭐⭐⭐⭐⭐ |
| **Behavior Tree** | 成熟稳定，教程多 | 性能较低，复杂 | ⭐⭐⭐ |
| **纯C++状态机** | 完全可控，性能最高 | 开发量大，难维护 | ⭐⭐ |

### 最终选择：**StateTree** ✅

**理由：**
1. UE5.6原生支持，官方推荐
2. 比Behavior Tree性能高10倍以上
3. 可视化编辑，调试方便
4. 轻量级，适合RTS游戏的大量单位

---

## 🏗️ 架构设计

### 核心类结构

```
SG_AIControllerBase (AIController)
    ├── StateTree Component (状态树组件)
    ├── Navigation Invoker (导航调用器)
    └── AI Perception (感知组件 - 可选)

SG_UnitsBase (Character)
    ├── Ability System Component (GAS)
    ├── Character Movement Component (移动组件)
    ├── Target Actor (当前目标)
    └── AI Controller Reference
```

### StateTree状态设计

```
Root State: 单位AI
│
├── State: 空闲 (Idle)
│   └── Transition: 发现敌人 → 追击
│
├── State: 追击 (Chase)
│   ├── Task: 查找最近敌人
│   ├── Task: 移动到目标
│   └── Transition: 进入攻击范围 → 攻击
│
├── State: 攻击 (Attack)
│   ├── Task: 检查目标有效性
│   ├── Task: 面向目标
│   ├── Task: 执行攻击
│   └── Transition: 目标死亡/逃离 → 追击
│
└── State: 攻城 (AttackMainCity)
    ├── Task: 查找敌方主城
    ├── Task: 移动到主城
    └── Task: 攻击主城
```

### 数据流设计

```
AI Tick → StateTree Update
    ↓
查找目标 (FindTarget)
    ↓
移动到目标 (MoveToTarget)
    ↓
检查攻击范围 (CheckAttackRange)
    ↓
触发GAS攻击能力 (PerformAttack)
    ↓
应用伤害 (ApplyDamage via GameplayEffect)
```

---

## 🚀 开发路线图

### 优先级排序

| 序号 | 任务 | 优先级 | 预计时间 | 依赖 |
|------|------|--------|----------|------|
| 1 | 创建AIController基类 | ⭐⭐⭐⭐⭐ | 30分钟 | 无 |
| 2 | 实现导航系统集成 | ⭐⭐⭐⭐⭐ | 1小时 | 任务1 |
| 3 | 实现目标查找系统 | ⭐⭐⭐⭐⭐ | 1小时 | 任务1 |
| 4 | 创建StateTree Tasks | ⭐⭐⭐⭐⭐ | 2-3小时 | 任务2,3 |
| 5 | 集成GAS攻击系统 | ⭐⭐⭐⭐⭐ | 1小时 | 任务4 |
| 6 | 实现攻城逻辑 | ⭐⭐⭐⭐ | 1小时 | 任务5 |
| 7 | 创建StateTree资产 | ⭐⭐⭐⭐ | 1小时 | 任务4-6 |
| 8 | 测试和调试 | ⭐⭐⭐⭐ | 2小时 | 全部 |

**总预计时间：9-11小时**

---

## 📝 详细实现步骤

### 任务1：创建AIController基类（30分钟）

#### 1.1 创建 SG_AIControllerBase.h

```cpp
#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "SG_AIControllerBase.generated.h"

/**
 * @brief AI控制器基类
 * @details
 * 功能说明：
 * - 管理单位的AI行为
 * - 集成StateTree系统
 * - 提供目标查找和导航功能
 */
UCLASS()
class SGUO_API ASG_AIControllerBase : public AAIController
{
    GENERATED_BODY()

public:
    // 构造函数
    ASG_AIControllerBase();

protected:
    // 生命周期函数
    virtual void BeginPlay() override;
    virtual void OnPossess(APawn* InPawn) override;

public:
    // ========== 目标管理 ==========
    
    /**
     * @brief 查找最近的敌人
     * @param SearchRadius 搜索半径
     * @return 找到的目标Actor，如果没有返回nullptr
     */
    UFUNCTION(BlueprintCallable, Category = "AI|Target")
    AActor* FindNearestEnemy(float SearchRadius = 2000.0f);
    
    /**
     * @brief 查找敌方主城
     * @return 敌方主城Actor，如果没有返回nullptr
     */
    UFUNCTION(BlueprintCallable, Category = "AI|Target")
    AActor* FindEnemyMainCity();
    
    /**
     * @brief 设置当前目标
     * @param NewTarget 新的目标Actor
     */
    UFUNCTION(BlueprintCallable, Category = "AI|Target")
    void SetCurrentTarget(AActor* NewTarget);
    
    /**
     * @brief 获取当前目标
     * @return 当前目标Actor
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "AI|Target")
    AActor* GetCurrentTarget() const { return CurrentTarget; }
    
    /**
     * @brief 检查当前目标是否有效
     * @return 目标是否有效（存在、存活、在范围内）
     */
    UFUNCTION(BlueprintCallable, Category = "AI|Target")
    bool IsTargetValid() const;

    // ========== 移动控制 ==========
    
    /**
     * @brief 移动到目标位置
     * @param TargetLocation 目标位置
     * @param AcceptanceRadius 接受半径（到达此距离即认为成功）
     * @return 是否成功开始移动
     */
    UFUNCTION(BlueprintCallable, Category = "AI|Movement")
    bool MoveToLocation(FVector TargetLocation, float AcceptanceRadius = 50.0f);
    
    /**
     * @brief 移动到目标Actor
     * @param TargetActor 目标Actor
     * @param AcceptanceRadius 接受半径
     * @return 是否成功开始移动
     */
    UFUNCTION(BlueprintCallable, Category = "AI|Movement")
    bool MoveToTargetActor(AActor* TargetActor, float AcceptanceRadius = 150.0f);
    
    /**
     * @brief 停止移动
     */
    UFUNCTION(BlueprintCallable, Category = "AI|Movement")
    void StopMovement();

    // ========== 战斗控制 ==========
    
    /**
     * @brief 检查是否在攻击范围内
     * @param Target 目标Actor
     * @param AttackRange 攻击范围（如果为0则使用单位的BaseAttackRange）
     * @return 是否在攻击范围内
     */
    UFUNCTION(BlueprintCallable, Category = "AI|Combat")
    bool IsInAttackRange(AActor* Target, float AttackRange = 0.0f) const;
    
    /**
     * @brief 面向目标
     * @param Target 目标Actor
     */
    UFUNCTION(BlueprintCallable, Category = "AI|Combat")
    void FaceTarget(AActor* Target);
    
    /**
     * @brief 执行攻击
     * @return 是否成功触发攻击
     */
    UFUNCTION(BlueprintCallable, Category = "AI|Combat")
    bool PerformAttack();

protected:
    // ========== 属性 ==========
    
    /** 当前目标Actor */
    UPROPERTY(BlueprintReadOnly, Category = "AI")
    TObjectPtr<AActor> CurrentTarget;
    
    /** 目标搜索半径 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Config")
    float TargetSearchRadius = 2000.0f;
    
    /** 是否自动查找目标 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Config")
    bool bAutoFindTarget = true;
    
    /** 是否优先攻击主城 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Config")
    bool bPrioritizeMainCity = false;

private:
    // ========== 辅助函数 ==========
    
    /**
     * @brief 获取控制的单位
     * @return 单位Character指针
     */
    class ASG_UnitsBase* GetControlledUnit() const;
    
    /**
     * @brief 获取单位的阵营标签
     * @return 阵营标签
     */
    FGameplayTag GetUnitFactionTag() const;
};
```

#### 1.2 创建 SG_AIControllerBase.cpp

```cpp
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

bool ASG_AIControllerBase::MoveToLocation(FVector TargetLocation, float AcceptanceRadius)
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
```

### 任务2：实现导航系统集成（1小时）

#### 2.1 在SG_UnitsBase中添加AI支持

**修改 SG_UnitsBase.h：**

```cpp
// ========== AI 相关 ==========

/** AI控制器类 */
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
TSubclassOf<AAIController> AIControllerClass;

/** 是否自动生成AI控制器 */
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
bool bUseAIController = true;
```

**修改 SG_UnitsBase.cpp BeginPlay：**

```cpp
void ASG_UnitsBase::BeginPlay()
{
    Super::BeginPlay();
    
    // ... 现有代码 ...
    
    // ✨ 新增 - 自动生成AI控制器
    if (bUseAIController && !Controller)
    {
        if (AIControllerClass)
        {
            SpawnDefaultController();
            UE_LOG(LogSGGameplay, Log, TEXT("✅ 自动生成AI控制器：%s"), *AIControllerClass->GetName());
        }
    }
}
```

#### 2.2 配置导航网格

**关卡设置：**
1. 在关卡中添加 Nav Mesh Bounds Volume
2. 调整大小覆盖整个战场区域
3. 按 `P` 键查看导航网格（绿色区域）

**项目设置：**
```
Project Settings → Navigation Mesh
- Runtime Generation: Dynamic (支持动态生成)
- Cell Size: 19.0
- Cell Height: 10.0
- Agent Radius: 34.0
- Agent Height: 144.0
```

### 任务3：实现StateTree Tasks（2-3小时）

#### 3.1 创建 StateTree Task: FindTarget

**文件：SG_StateTreeTask_FindTarget.h**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "SG_StateTreeTask_FindTarget.generated.h"

/**
 * @brief StateTree任务：查找目标
 * @details
 * 功能说明：
 * - 查找最近的敌人或敌方主城
 * - 将找到的目标保存到AI Controller
 */
USTRUCT()
struct SGUO_API FSG_StateTreeTask_FindTarget : public FStateTreeTaskBase
{
    GENERATED_BODY()

    using FInstanceDataType = FSG_StateTreeTask_FindTargetInstanceData;

    FSG_StateTreeTask_FindTarget() = default;

    virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
    virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};

/**
 * @brief FindTarget任务实例数据
 */
USTRUCT()
struct SGUO_API FSG_StateTreeTask_FindTargetInstanceData
{
    GENERATED_BODY()

    /** 搜索半径 */
    UPROPERTY(EditAnywhere, Category = "Parameter")
    float SearchRadius = 2000.0f;

    /** 是否优先查找主城 */
    UPROPERTY(EditAnywhere, Category = "Parameter")
    bool bPrioritizeMainCity = false;

    /** 找到的目标（输出） */
    UPROPERTY(EditAnywhere, Category = "Output")
    TObjectPtr<AActor> FoundTarget = nullptr;
};
```

**文件：SG_StateTreeTask_FindTarget.cpp**

```cpp
#include "AI/StateTree/SG_StateTreeTask_FindTarget.h"
#include "AI/SG_AIControllerBase.h"
#include "StateTreeExecutionContext.h"
#include "Debug/SG_LogCategories.h"

EStateTreeRunStatus FSG_StateTreeTask_FindTarget::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
    return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FSG_StateTreeTask_FindTarget::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
    FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

    // 获取AI Controller
    AAIController* AIController = Cast<AAIController>(Context.GetOwner());
    if (!AIController)
    {
        UE_LOG(LogSGGameplay, Warning, TEXT("❌ FindTarget: 无法获取AI Controller"));
        return EStateTreeRunStatus::Failed;
    }

    ASG_AIControllerBase* SGAIController = Cast<ASG_AIControllerBase>(AIController);
    if (!SGAIController)
    {
        UE_LOG(LogSGGameplay, Warning, TEXT("❌ FindTarget: 不是SG_AIControllerBase"));
        return EStateTreeRunStatus::Failed;
    }

    // 优先查找主城（如果设置）
    if (InstanceData.bPrioritizeMainCity)
    {
        AActor* MainCity = SGAIController->FindEnemyMainCity();
        if (MainCity)
        {
            InstanceData.FoundTarget = MainCity;
            SGAIController->SetCurrentTarget(MainCity);
            UE_LOG(LogSGGameplay, Log, TEXT("✅ 找到目标主城：%s"), *MainCity->GetName());
            return EStateTreeRunStatus::Succeeded;
        }
    }

    // 查找最近的敌人
    AActor* Enemy = SGAIController->FindNearestEnemy(InstanceData.SearchRadius);
    if (Enemy)
    {
        InstanceData.FoundTarget = Enemy;
        SGAIController->SetCurrentTarget(Enemy);
        UE_LOG(LogSGGameplay, Log, TEXT("✅ 找到目标敌人：%s"), *Enemy->GetName());
        return EStateTreeRunStatus::Succeeded;
    }

    // 没有找到目标
    UE_LOG(LogSGGameplay, Log, TEXT("❌ 未找到目标"));
    return EStateTreeRunStatus::Failed;
}
```

#### 3.2 创建其他StateTree Tasks

**需要创建的任务列表：**

| 任务名称 | 文件名 | 功能描述 |
|---------|--------|---------|
| FindTarget | SG_StateTreeTask_FindTarget | 查找最近的敌人/主城 |
| MoveToTarget | SG_StateTreeTask_MoveToTarget | 移动到目标位置 |
| CheckAttackRange | SG_StateTreeTask_CheckAttackRange | 检查是否在攻击范围内 |
| PerformAttack | SG_StateTreeTask_PerformAttack | 执行攻击（触发GAS） |
| FaceTarget | SG_StateTreeTask_FaceTarget | 面向目标 |
| CheckTargetValid | SG_StateTreeTask_CheckTargetValid | 检查目标是否有效 |

### 任务4：集成GAS攻击系统（1小时）

已在前面实现，主要是：
1. AI Controller调用单位的`PerformAttack()`
2. `PerformAttack()`内部触发GAS攻击能力
3. GAS攻击能力执行伤害计算

### 任务5：创建StateTree资产（1小时）

**在UE编辑器中创建：**

```
1. Content Browser → 右键 → AI → State Tree
2. 命名：ST_UnitAI
3. 打开StateTree编辑器
4. 构建状态树（见架构设计）
5. 配置Task参数
6. 保存
```

### 任务6：测试和调试（2小时）

**测试关卡设置：**

```
1. 创建测试关卡：TestMap_AI
2. 添加Nav Mesh Bounds Volume
3. 放置玩家单位（设置FactionTag: Unit.Faction.Player）
4. 放置敌方单位（设置FactionTag: Unit.Faction.Enemy）
5. 配置单位：
   - bUseAIController: true
   - AIControllerClass: BP_AIController
6. 运行测试
7. 观察日志（LogSGGameplay）
```

---

## 🧪 测试计划

### 测试用例

| 测试项 | 预期结果 | 验证方法 |
|--------|---------|---------|
| 单位自动寻路 | 单位沿导航网格移动 | 视觉观察 + 日志 |
| 自动查找敌人 | 找到最近的敌人 | 日志输出 |
| 自动攻击敌人 | 播放攻击动画并造成伤害 | 视觉观察 + 日志 |
| 目标死亡后重新查找 | 切换到新目标 | 日志输出 |
| 攻城逻辑 | 移动到主城并攻击 | 视觉观察 + 日志 |
| 超出范围停止追击 | 返回巡逻点 | 视觉观察 |

### 调试技巧

1. **启用StateTree调试：**
   ```
   - 运行游戏时按 ` 键打开控制台
   - 输入：statetree.debug 1
   - 显示当前状态和转换
   ```

2. **导航网格可视化：**
   ```
   - 按 P 键显示导航网格
   - 绿色区域：可通行
   - 红色区域：不可通行
   ```

3. **日志过滤：**
   ```
   - Output Log → 过滤：LogSGGameplay
   - 只显示AI相关日志
   ```

---

## 📚 参考资料

### UE官方文档
- [StateTree 官方文档](https://docs.unrealengine.com/5.6/state-tree-in-unreal-engine/)
- [AI Controller](https://docs.unrealengine.com/5.6/ai-controllers-in-unreal-engine/)
- [Navigation System](https://docs.unrealengine.com/5.6/navigation-system-in-unreal-engine/)

### 项目内部参考
- `SG_UnitsBase.h` - 单位基类
- `SG_GameplayAbility_Attack.h` - 攻击能力基类
- `战斗系统实现进度.md` - 战斗系统进度

---

## ✅ 完成检查清单

### C++代码
- [ ] ASG_AIControllerBase 类
- [ ] StateTree Task: FindTarget
- [ ] StateTree Task: MoveToTarget
- [ ] StateTree Task: CheckAttackRange
- [ ] StateTree Task: PerformAttack
- [ ] StateTree Task: FaceTarget
- [ ] StateTree Task: CheckTargetValid

### 蓝图资产
- [ ] BP_AIController（基于SG_AIControllerBase）
- [ ] ST_UnitAI（StateTree资产）

### 测试
- [ ] 导航网格配置
- [ ] AI自动寻路测试
- [ ] AI自动攻击测试
- [ ] 攻城逻辑测试

---

## 🚀 下一步行动

**准备开始？请回答以下问题：**

1. ❓ 是否立即开始实现AI Controller基类？
2. ❓ 是否需要我先创建所有StateTree Task的完整代码？
3. ❓ 是否需要蓝图资产创建指南？
4. ❓ 其他特殊需求或优先级调整？

**我会根据你的选择开始实现！** 💪
