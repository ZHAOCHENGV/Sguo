// 📄 文件：Source/Sguo/Private/AI/SG_CombatTargetManager.cpp
// 🔧 修改 - 添加三色调试可视化和基于标签的槽位占用控制
// ✅ 这是完整文件

#include "AI/SG_CombatTargetManager.h"
#include "Units/SG_UnitsBase.h"
#include "Buildings/SG_MainCityBase.h"
#include "Debug/SG_LogCategories.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "Engine/OverlapResult.h"
#include "TimerManager.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
// ✨ 新增 - 调试绘制相关头文件
#include "DrawDebugHelpers.h"
#include "AI/SG_TargetingSubsystem.h"
#include "Engine/Engine.h"

// ========== FSGAttackSlot 结构体实现 ==========

/**
 * @brief 检查槽位是否被占据
 * @return 是否有有效的、存活的单位占据此槽位
 */
bool FSGAttackSlot::IsOccupied() const
{
    // 检查弱引用是否有效，且单位未死亡
    return OccupyingUnit.IsValid() && !OccupyingUnit->bIsDead;
}

// ✨ 新增 - 获取槽位状态
/**
 * @brief 获取槽位当前状态
 * @param Target 目标 Actor
 * @param TargetRadius 目标碰撞半径
 * @param ArrivalThreshold 到达判定阈值
 * @return 槽位状态枚举值
 * @details
 * 详细流程：
 * 1. 如果未被占用，返回 Available
 * 2. 如果被占用，计算攻击者到槽位的距离
 * 3. 距离小于阈值返回 Occupied，否则返回 Reserved
 */
ESGAttackSlotState FSGAttackSlot::GetSlotState(AActor* Target, float TargetRadius, float ArrivalThreshold) const
{
    // 没有被占用 - 空闲状态
    if (!IsOccupied())
    {
        return ESGAttackSlotState::Available;
    }

    // 被占用，检查攻击者是否已到达槽位
    ASG_UnitsBase* Occupier = OccupyingUnit.Get();
    if (!Occupier || !Target)
    {
        return ESGAttackSlotState::Reserved;
    }

    // 计算攻击者到槽位的距离
    float AttackerAttackRange = Occupier->GetAttackRangeForAI();
    FVector SlotPosition = GetWorldPosition(Target, AttackerAttackRange, TargetRadius);
    float DistanceToSlot = FVector::Dist(Occupier->GetActorLocation(), SlotPosition);

    // 如果距离小于阈值，认为已到达
    if (DistanceToSlot <= ArrivalThreshold)
    {
        return ESGAttackSlotState::Occupied;
    }

    // 否则是预约状态（正在移动中）
    return ESGAttackSlotState::Reserved;
}

/**
 * @brief 获取槽位的世界坐标
 * @param Target 目标 Actor
 * @param AttackerAttackRange 攻击者的攻击范围
 * @param TargetRadius 目标碰撞半径
 * @return 槽位在世界空间中的位置
 * @details
 * 计算公式：
 * - 槽位距离 = 目标半径 + 攻击范围 * 0.8
 * - 槽位位置 = 目标位置 + 方向向量 * 槽位距离
 */
FVector FSGAttackSlot::GetWorldPosition(AActor* Target, float AttackerAttackRange, float TargetRadius) const
{
    if (!Target)
    {
        return FVector::ZeroVector;
    }

    // 计算槽位距离：目标半径 + 攻击范围的 80%
    float SlotDistance = TargetRadius + (AttackerAttackRange * 0.8f);

    // 根据角度计算偏移
    float Radians = FMath::DegreesToRadians(Angle);
    FVector Offset;
    Offset.X = FMath::Cos(Radians) * SlotDistance;
    Offset.Y = FMath::Sin(Radians) * SlotDistance;
    Offset.Z = 0.0f;

    return Target->GetActorLocation() + Offset;
}

// ✨ 新增 - 使用默认攻击范围获取槽位位置
/**
 * @brief 获取槽位的世界坐标（使用默认攻击范围）
 * @param Target 目标 Actor
 * @param TargetRadius 目标碰撞半径
 * @param DefaultAttackRange 默认攻击范围
 * @return 槽位在世界空间中的位置
 * @details
 * 功能说明：
 * - 用于调试绘制，当槽位为空时无法获取攻击者的攻击范围
 * - 使用配置的默认值进行计算
 */
FVector FSGAttackSlot::GetWorldPositionWithDefault(AActor* Target, float TargetRadius, float DefaultAttackRange) const
{
    if (!Target)
    {
        return FVector::ZeroVector;
    }

    // 计算槽位距离：目标半径 + 默认攻击范围的 80%
    float SlotDistance = TargetRadius + (DefaultAttackRange * 0.8f);

    // 根据角度计算偏移
    float Radians = FMath::DegreesToRadians(Angle);
    FVector Offset;
    Offset.X = FMath::Cos(Radians) * SlotDistance;
    Offset.Y = FMath::Sin(Radians) * SlotDistance;
    Offset.Z = 0.0f;

    return Target->GetActorLocation() + Offset;
}

// ========== FSGTargetCombatInfo 结构体实现 ==========

/**
 * @brief 获取可用槽位数量
 * @return 未被占用的槽位数量
 */
int32 FSGTargetCombatInfo::GetAvailableSlotCount() const
{
    int32 Count = 0;
    for (const FSGAttackSlot& Slot : AttackSlots)
    {
        if (!Slot.IsOccupied())
        {
            Count++;
        }
    }
    return Count;
}

/**
 * @brief 获取已占用槽位数量
 * @return 被占用的槽位数量
 */
int32 FSGTargetCombatInfo::GetOccupiedSlotCount() const
{
    return AttackSlots.Num() - GetAvailableSlotCount();
}

// ✨ 新增 - 获取各状态槽位数量
/**
 * @brief 获取各状态槽位数量
 * @param Target 目标 Actor
 * @param OutAvailable 输出：空闲槽位数
 * @param OutReserved 输出：预约槽位数
 * @param OutOccupied 输出：已到达槽位数
 * @param ArrivalThreshold 到达判定阈值
 */
void FSGTargetCombatInfo::GetSlotStateCounts(AActor* Target, int32& OutAvailable, int32& OutReserved, int32& OutOccupied, float ArrivalThreshold) const
{
    OutAvailable = 0;
    OutReserved = 0;
    OutOccupied = 0;

    // 遍历所有槽位，统计各状态数量
    for (const FSGAttackSlot& Slot : AttackSlots)
    {
        ESGAttackSlotState State = Slot.GetSlotState(Target, TargetRadius, ArrivalThreshold);
        switch (State)
        {
        case ESGAttackSlotState::Available:
            OutAvailable++;
            break;
        case ESGAttackSlotState::Reserved:
            OutReserved++;
            break;
        case ESGAttackSlotState::Occupied:
            OutOccupied++;
            break;
        }
    }
}

// ========== USG_CombatTargetManager 实现 ==========

/**
 * @brief 子系统初始化
 * @param Collection 子系统集合
 */
void USG_CombatTargetManager::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // 定期清理无效数据（每3秒）
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            CleanupTimerHandle,
            this,
            &USG_CombatTargetManager::CleanupInvalidData,
            3.0f,
            true
        );
    }

    UE_LOG(LogSGGameplay, Log, TEXT("✓ 战斗目标管理器初始化完成"));
}

/**
 * @brief 子系统销毁
 */
void USG_CombatTargetManager::Deinitialize()
{
    // 清理计时器
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(CleanupTimerHandle);
    }
    
    // 清空数据
    TargetCombatInfoMap.Empty();
    
    Super::Deinitialize();
}

// ========== ✨ 新增 - Tick 和调试可视化 ==========

/**
 * @brief 每帧 Tick（用于绘制调试信息）
 * @param DeltaTime 帧间隔时间
 * @details
 * 功能说明：
 * - 只有开启调试可视化时才会执行绘制
 * - 绘制所有目标的槽位信息
 * - 可选绘制图例
 */
void USG_CombatTargetManager::Tick(float DeltaTime)
{
    // 调试可视化开启时绘制
    if (bShowDebugVisualization)
    {
        DrawDebugSlots();
        
        // 绘制图例（屏幕信息）
        if (bShowLegend)
        {
            DrawDebugLegend();
        }
    }
}

/**
 * @brief 切换调试可视化显示
 */
void USG_CombatTargetManager::ToggleDebugVisualization()
{
    bShowDebugVisualization = !bShowDebugVisualization;
    UE_LOG(LogSGGameplay, Log, TEXT("🔧 攻击槽位调试显示：%s"), 
        bShowDebugVisualization ? TEXT("开启") : TEXT("关闭"));
}

/**
 * @brief 设置调试可视化显示状态
 * @param bEnable 是否启用
 */
void USG_CombatTargetManager::SetDebugVisualization(bool bEnable)
{
    bShowDebugVisualization = bEnable;
    UE_LOG(LogSGGameplay, Log, TEXT("🔧 攻击槽位调试显示：%s"), 
        bEnable ? TEXT("开启") : TEXT("关闭"));
}

/**
 * @brief 绘制调试图例（屏幕信息）
 * @details
 * 功能说明：
 * - 在屏幕左上角显示颜色图例
 * - 帮助理解槽位状态的含义
 */
void USG_CombatTargetManager::DrawDebugLegend()
{
    if (GEngine)
    {
        // 构建图例文本
        FString LegendText = FString::Printf(
            TEXT("=== 攻击槽位状态 ===\n")
            TEXT("● 绿色: 空闲\n")
            TEXT("● 蓝色: 已预约(移动中)\n")
            TEXT("● 红色: 已到达(攻击中)")
        );

        // 在屏幕上显示（持续 0 秒表示每帧刷新）
        GEngine->AddOnScreenDebugMessage(
            -1,         // Key，-1 表示不覆盖
            0.0f,       // 显示时间，0 表示单帧
            FColor::White,
            LegendText
        );
    }
}

/**
 * @brief 绘制所有目标的调试槽位
 * @details
 * 详细流程：
 * 1. 遍历所有已注册的目标
 * 2. 跳过无效或已死亡的目标
 * 3. 为每个有效目标绘制槽位
 */
void USG_CombatTargetManager::DrawDebugSlots()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    // 遍历所有目标
    for (auto& Pair : TargetCombatInfoMap)
    {
        AActor* Target = Pair.Key.Get();
        if (!Target)
        {
            continue;
        }

        // 检查单位目标是否已死亡
        if (ASG_UnitsBase* TargetUnit = Cast<ASG_UnitsBase>(Target))
        {
            if (TargetUnit->bIsDead)
            {
                continue;
            }
        }

        // 绘制此目标的槽位
        DrawDebugSlotsForTarget(Target, Pair.Value);
    }
}

/**
 * @brief 绘制单个目标的调试槽位
 * @param Target 目标 Actor
 * @param CombatInfo 目标的战斗信息
 * @details
 * 功能说明：
 * - 绘制目标周围的圆圈
 * - 根据槽位状态使用不同颜色绘制槽位球体
 * - 绘制攻击者到槽位的连线
 * - 显示槽位编号和占用者名称
 */
void USG_CombatTargetManager::DrawDebugSlotsForTarget(AActor* Target, const FSGTargetCombatInfo& CombatInfo)
{
    UWorld* World = GetWorld();
    if (!World || !Target)
    {
        return;
    }

    FVector TargetLocation = Target->GetActorLocation();
    float TargetRadius = CombatInfo.TargetRadius;

    // 获取各状态槽位数量
    int32 AvailableCount = 0;
    int32 ReservedCount = 0;
    int32 OccupiedCount = 0;
    CombatInfo.GetSlotStateCounts(Target, AvailableCount, ReservedCount, OccupiedCount, DebugArrivalThreshold);

    int32 TotalSlots = CombatInfo.AttackSlots.Num();
    bool bIsFull = (AvailableCount == 0);

    // ========== 绘制目标周围的圆圈 ==========
    FColor CircleColor = bIsFull ? DebugColorFull : DebugColorAvailable;
    float CircleRadius = TargetRadius + (DebugDefaultAttackRange * SlotDistanceRatio);
    
    // 使用 DrawDebugCircle 绘制水平圆圈
    DrawDebugCircle(
        World,
        TargetLocation + FVector(0, 0, 50.0f),  // 稍微抬高，避免与地面重叠
        CircleRadius,
        32,             // 分段数
        CircleColor,
        false,          // bPersistentLines
        -1.0f,          // LifeTime（-1 表示单帧）
        0,              // DepthPriority
        2.0f,           // Thickness
        FVector(0, 1, 0),   // Y 轴
        FVector(1, 0, 0),   // Z 轴（水平圆）
        false           // bDrawAxis
    );

    // ========== 显示目标状态信息 ==========
    if (bShowTargetStatus)
    {
        FString StatusText = FString::Printf(TEXT("%s\n空闲:%d 预约:%d 到达:%d"),
            *Target->GetName(),
            AvailableCount,
            ReservedCount,
            OccupiedCount);

        FColor TextColor = bIsFull ? DebugColorFull : FColor::White;

        DrawDebugString(
            World,
            TargetLocation + FVector(0, 0, 180.0f),
            StatusText,
            nullptr,
            TextColor,
            0.0f,       // Duration
            true,       // bDrawShadow
            1.2f        // FontScale
        );
    }

    // ========== 绘制每个槽位 ==========
    for (int32 i = 0; i < CombatInfo.AttackSlots.Num(); ++i)
    {
        const FSGAttackSlot& Slot = CombatInfo.AttackSlots[i];

        // 获取槽位状态
        ESGAttackSlotState SlotState = Slot.GetSlotState(Target, TargetRadius, DebugArrivalThreshold);

        // 计算槽位位置和颜色
        FVector SlotPosition;
        FColor SlotColor;
        FColor LineColor;
        FString SlotText;

        switch (SlotState)
        {
        case ESGAttackSlotState::Available:
            // ========== 空闲 - 绿色 ==========
            SlotPosition = Slot.GetWorldPositionWithDefault(Target, TargetRadius, DebugDefaultAttackRange);
            SlotColor = DebugColorAvailable;
            LineColor = DebugColorAvailable;
            if (bShowSlotNumbers)
            {
                SlotText = FString::Printf(TEXT("#%d"), i);
            }
            break;

        case ESGAttackSlotState::Reserved:
            // ========== 预约中 - 蓝色 ==========
            {
                ASG_UnitsBase* Occupier = Slot.OccupyingUnit.Get();
                float OccupierAttackRange = Occupier ? Occupier->GetAttackRangeForAI() : DebugDefaultAttackRange;
                SlotPosition = Slot.GetWorldPosition(Target, OccupierAttackRange, TargetRadius);
                SlotColor = DebugColorReserved;
                LineColor = DebugColorReservedLine;

                // 槽位文本
                if (bShowSlotNumbers && bShowOccupierNames && Occupier)
                {
                    SlotText = FString::Printf(TEXT("#%d\n%s\n[移动中]"), i, *Occupier->GetName());
                }
                else if (bShowSlotNumbers)
                {
                    SlotText = FString::Printf(TEXT("#%d [移动中]"), i);
                }
                else if (bShowOccupierNames && Occupier)
                {
                    SlotText = FString::Printf(TEXT("%s\n[移动中]"), *Occupier->GetName());
                }

                // 绘制攻击者到槽位的连线（虚线效果）
                if (bShowAttackerLines && Occupier)
                {
                    FVector OccupierLocation = Occupier->GetActorLocation();
                    
                    // 绘制虚线
                    FVector Start = OccupierLocation + FVector(0, 0, 50.0f);
                    FVector End = SlotPosition + FVector(0, 0, 50.0f);
                    FVector Direction = (End - Start).GetSafeNormal();
                    float TotalDistance = FVector::Dist(Start, End);
                    float DashLength = 30.0f;
                    float GapLength = 20.0f;
                    
                    float CurrentDist = 0.0f;
                    while (CurrentDist < TotalDistance)
                    {
                        FVector DashStart = Start + Direction * CurrentDist;
                        float DashEnd = FMath::Min(CurrentDist + DashLength, TotalDistance);
                        FVector DashEndPos = Start + Direction * DashEnd;
                        
                        DrawDebugLine(
                            World,
                            DashStart,
                            DashEndPos,
                            LineColor,
                            false,
                            -1.0f,
                            0,
                            2.0f
                        );
                        
                        CurrentDist += DashLength + GapLength;
                    }

                    // 绘制箭头指向槽位
                    DrawDebugDirectionalArrow(
                        World,
                        End - Direction * 50.0f,
                        End,
                        30.0f,
                        LineColor,
                        false,
                        -1.0f,
                        0,
                        3.0f
                    );
                }
            }
            break;

        case ESGAttackSlotState::Occupied:
            // ========== 已到达 - 红色 ==========
            {
                ASG_UnitsBase* Occupier = Slot.OccupyingUnit.Get();
                float OccupierAttackRange = Occupier ? Occupier->GetAttackRangeForAI() : DebugDefaultAttackRange;
                SlotPosition = Slot.GetWorldPosition(Target, OccupierAttackRange, TargetRadius);
                SlotColor = DebugColorOccupied;
                LineColor = DebugColorOccupiedLine;

                // 槽位文本
                if (bShowSlotNumbers && bShowOccupierNames && Occupier)
                {
                    SlotText = FString::Printf(TEXT("#%d\n%s\n[攻击中]"), i, *Occupier->GetName());
                }
                else if (bShowSlotNumbers)
                {
                    SlotText = FString::Printf(TEXT("#%d [攻击中]"), i);
                }
                else if (bShowOccupierNames && Occupier)
                {
                    SlotText = FString::Printf(TEXT("%s\n[攻击中]"), *Occupier->GetName());
                }

                // 绘制攻击者到槽位的连线（实线）
                if (bShowAttackerLines && Occupier)
                {
                    FVector OccupierLocation = Occupier->GetActorLocation();
                    
                    DrawDebugLine(
                        World,
                        OccupierLocation + FVector(0, 0, 50.0f),
                        SlotPosition + FVector(0, 0, 50.0f),
                        LineColor,
                        false,
                        -1.0f,
                        0,
                        3.0f
                    );
                }
            }
            break;
        }

        // 绘制槽位球体
        DrawDebugSphere(
            World,
            SlotPosition + FVector(0, 0, 50.0f),
            DebugSlotSphereSize,
            12,
            SlotColor,
            false,
            -1.0f,
            0,
            2.0f
        );

        // 绘制槽位文本
        if (!SlotText.IsEmpty())
        {
            DrawDebugString(
                World,
                SlotPosition + FVector(0, 0, 100.0f),
                SlotText,
                nullptr,
                SlotColor,
                0.0f,
                true,
                1.0f
            );
        }

        // 绘制从目标中心到槽位的线（半透明辅助线）
        FVector StartPos = TargetLocation + FVector(0, 0, 50.0f);
        FVector EndPos = SlotPosition + FVector(0, 0, 50.0f);
        
        DrawDebugLine(
            World,
            StartPos,
            EndPos,
            FColor(SlotColor.R, SlotColor.G, SlotColor.B, 80),
            false,
            -1.0f,
            0,
            1.0f
        );
    }
}

// ========== ✨ 新增 - 槽位占用标签检查 ==========

/**
 * @brief 检查单位是否需要占用攻击槽位
 * @param Unit 要检查的单位
 * @return 是否需要占用槽位
 * @details
 * 功能说明：
 * - 如果 SlotOccupyingUnitTypes 为空，默认所有单位都占用槽位（向后兼容）
 * - 如果单位的 UnitTypeTag 在配置的标签容器中，需要占用槽位
 * - 否则不占用槽位（如远程单位）
 */
bool USG_CombatTargetManager::ShouldUnitOccupySlot(const ASG_UnitsBase* Unit) const
{
    if (!Unit)
    {
        return false;
    }

    // 如果没有配置任何标签，默认所有单位都占用槽位（向后兼容）
    if (SlotOccupyingUnitTypes.IsEmpty())
    {
        return true;
    }

    // 检查单位的类型标签是否在配置的标签容器中
    return SlotOccupyingUnitTypes.HasTag(Unit->UnitTypeTag);
}

// ========== 辅助函数 ==========

/**
 * @brief 获取目标的碰撞半径
 * @param Target 目标 Actor
 * @return 碰撞半径
 * @details
 * 功能说明：
 * - 主城使用检测盒的最大边长
 * - 单位使用胶囊体半径
 * - 其他 Actor 尝试查找胶囊体组件
 * - 默认返回 50.0
 */
float USG_CombatTargetManager::GetTargetCollisionRadius(AActor* Target) const
{
    if (!Target)
    {
        return 50.0f;
    }

    // 主城特殊处理
    if (ASG_MainCityBase* MainCity = Cast<ASG_MainCityBase>(Target))
    {
        if (MainCity->GetAttackDetectionBox())
        {
            FVector BoxExtent = MainCity->GetAttackDetectionBox()->GetScaledBoxExtent();
            return FMath::Max(BoxExtent.X, BoxExtent.Y);
        }
        return 800.0f;
    }

    // 单位使用胶囊体半径
    if (ASG_UnitsBase* Unit = Cast<ASG_UnitsBase>(Target))
    {
        if (UCapsuleComponent* Capsule = Unit->GetCapsuleComponent())
        {
            return Capsule->GetScaledCapsuleRadius();
        }
    }

    // 通用：尝试查找胶囊体组件
    if (UCapsuleComponent* Capsule = Target->FindComponentByClass<UCapsuleComponent>())
    {
        return Capsule->GetScaledCapsuleRadius();
    }

    return 50.0f;
}

/**
 * @brief 为单位查找最佳目标（带槽位检查）
 * @param Querier 查询单位
 * @return 最佳目标 Actor
 * @details
 * 🔧 核心修改：
 * - 如果视野内没有敌方单位，自动回退到敌方主城
 * - 使用 TargetingSubsystem 进行目标查找
 * - 保留槽位检查逻辑
 */
AActor* USG_CombatTargetManager::FindBestTargetWithSlot(ASG_UnitsBase* Querier)
{
    if (!Querier)
    {
        return nullptr;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return nullptr;
    }

    FVector QuerierLocation = Querier->GetActorLocation();
    FGameplayTag QuerierFaction = Querier->FactionTag;
    float SearchRadius = Querier->GetDetectionRange();

    // 检查查询者是否需要占用槽位
    bool bQuerierNeedsSlot = ShouldUnitOccupySlot(Querier);

    // ========== 步骤1：使用球形检测获取范围内的敌方单位 ==========
    TArray<AActor*> NearbyEnemies;
    QueryEnemiesInRange(Querier, SearchRadius, NearbyEnemies);

    // ========== 步骤2：如果有敌方单位，进行评分和槽位检查 ==========
    if (NearbyEnemies.Num() > 0)
    {
        // 候选列表
        struct FCandidateInfo
        {
            AActor* Actor;
            float Distance;
            int32 OccupiedSlots;
            float Score;
            bool bHasAvailableSlot;
        };
        TArray<FCandidateInfo> UnitCandidates;

        // 筛选并评分敌方单位
        for (AActor* Actor : NearbyEnemies)
        {
            ASG_UnitsBase* Unit = Cast<ASG_UnitsBase>(Actor);
            if (!Unit)
            {
                continue;
            }

            float Distance = FVector::Dist(QuerierLocation, Unit->GetActorLocation());

            // 只有需要占用槽位的单位才检查槽位
            bool bHasSlot = true;
            int32 Slots = 0;

            if (bQuerierNeedsSlot)
            {
                bHasSlot = HasAvailableSlot(Unit);
                Slots = GetOccupiedSlotCount(Unit);
            }

            // 计算评分
            float DistanceScore = FMath::Max(0.0f, 1.0f - (Distance / SearchRadius));
            float CrowdingPenalty = 1.0f + (Slots * 0.5f);
            float SlotBonus = bHasSlot ? 1.0f : 0.3f;
            float Score = (DistanceScore * 100.0f * SlotBonus) / CrowdingPenalty;

            FCandidateInfo Candidate;
            Candidate.Actor = Unit;
            Candidate.Distance = Distance;
            Candidate.OccupiedSlots = Slots;
            Candidate.Score = Score;
            Candidate.bHasAvailableSlot = bHasSlot;

            UnitCandidates.Add(Candidate);
        }

        // 按评分排序并选择最佳目标
        if (UnitCandidates.Num() > 0)
        {
            UnitCandidates.Sort([](const FCandidateInfo& A, const FCandidateInfo& B)
            {
                return A.Score > B.Score;
            });

            // 优先选择有槽位的目标
            if (bQuerierNeedsSlot)
            {
                for (const FCandidateInfo& Candidate : UnitCandidates)
                {
                    if (Candidate.bHasAvailableSlot)
                    {
                        UE_LOG(LogSGGameplay, Log, TEXT("🎯 %s 选中敌方单位：%s (距离: %.0f, 槽位: %d, 评分: %.2f)"),
                            *Querier->GetName(),
                            *Candidate.Actor->GetName(),
                            Candidate.Distance,
                            Candidate.OccupiedSlots,
                            Candidate.Score);
                        return Candidate.Actor;
                    }
                }
            }

            // 返回评分最高的
            UE_LOG(LogSGGameplay, Log, TEXT("🎯 %s 选中敌方单位：%s (距离: %.0f, 评分: %.2f)"),
                *Querier->GetName(),
                *UnitCandidates[0].Actor->GetName(),
                UnitCandidates[0].Distance,
                UnitCandidates[0].Score);
            return UnitCandidates[0].Actor;
        }
    }

    // ========== 🔧 修改 - 步骤3：没有敌方单位，回退到敌方主城 ==========
    UE_LOG(LogSGGameplay, Log, TEXT("📍 %s 视野内无敌方单位，查找敌方主城..."), *Querier->GetName());

    // 使用 TargetingSubsystem 查找敌方主城
    if (USG_TargetingSubsystem* TargetingSys = World->GetSubsystem<USG_TargetingSubsystem>())
    {
        ASG_MainCityBase* EnemyCity = TargetingSys->FindEnemyMainCity(Querier);
        if (EnemyCity)
        {
            UE_LOG(LogSGGameplay, Log, TEXT("🏰 %s 回退到敌方主城：%s"),
                *Querier->GetName(), *EnemyCity->GetName());
            return EnemyCity;
        }
    }
    else
    {
        // 备选：直接查找主城
        TArray<AActor*> AllMainCities;
        UGameplayStatics::GetAllActorsOfClass(World, ASG_MainCityBase::StaticClass(), AllMainCities);

        ASG_MainCityBase* NearestEnemyCity = nullptr;
        float NearestDistance = FLT_MAX;

        for (AActor* Actor : AllMainCities)
        {
            ASG_MainCityBase* City = Cast<ASG_MainCityBase>(Actor);
            if (!City)
            {
                continue;
            }

            // 跳过同阵营
            if (City->FactionTag == QuerierFaction)
            {
                continue;
            }

            // 跳过已摧毁的
            if (!City->IsAlive())
            {
                continue;
            }

            float Distance = FVector::Dist(QuerierLocation, City->GetActorLocation());
            if (Distance < NearestDistance)
            {
                NearestDistance = Distance;
                NearestEnemyCity = City;
            }
        }

        if (NearestEnemyCity)
        {
            UE_LOG(LogSGGameplay, Log, TEXT("🏰 %s 回退到敌方主城：%s（备选路径）"),
                *Querier->GetName(), *NearestEnemyCity->GetName());
            return NearestEnemyCity;
        }
    }

    UE_LOG(LogSGGameplay, Warning, TEXT("⚠️ %s 未找到任何敌方目标"),
        *Querier->GetName());

    return nullptr;
}

/**
 * @brief 尝试预约目标的攻击槽位
 * @param Attacker 攻击单位
 * @param Target 目标 Actor
 * @param OutSlotPosition 输出：预约成功后的槽位世界坐标
 * @return 是否预约成功
 * @details
 * 功能说明：
 * - ✨ 新增 - 检查攻击者是否需要占用槽位
 * - 远程单位不占用槽位，计算攻击范围边缘位置
 */
bool USG_CombatTargetManager::TryReserveAttackSlot(ASG_UnitsBase* Attacker, AActor* Target, FVector& OutSlotPosition)
{
    if (!Attacker || !Target)
    {
        return false;
    }

    // ✨ 新增 - 检查攻击者是否需要占用槽位
    if (!ShouldUnitOccupySlot(Attacker))
    {
        // 远程单位不占用槽位，计算攻击范围边缘位置
        float AttackRange = Attacker->GetAttackRangeForAI();
        FVector AttackerLocation = Attacker->GetActorLocation();
        FVector TargetLocation = Target->GetActorLocation();
        FVector Direction = (TargetLocation - AttackerLocation).GetSafeNormal();
        
        // 目标位置 - 攻击范围 * 0.9（留一点余量）
        OutSlotPosition = TargetLocation - Direction * (AttackRange * 0.9f);
        OutSlotPosition.Z = AttackerLocation.Z;
        
        UE_LOG(LogSGGameplay, Verbose, TEXT("🏹 %s 是远程单位，不占用槽位"),
            *Attacker->GetName());
        return true;
    }

    float AttackerAttackRange = Attacker->GetAttackRangeForAI();

    // 主城特殊处理
    if (ASG_MainCityBase* MainCity = Cast<ASG_MainCityBase>(Target))
    {
        FVector CityLocation = MainCity->GetActorLocation();
        FVector AttackerLocation = Attacker->GetActorLocation();
        FVector Direction = (AttackerLocation - CityLocation).GetSafeNormal();

        float CityRadius = GetTargetCollisionRadius(MainCity);
        float StandDistance = CityRadius + (AttackerAttackRange * SlotDistanceRatio);

        OutSlotPosition = CityLocation + (Direction * StandDistance);
        OutSlotPosition.Z = AttackerLocation.Z;

        UE_LOG(LogSGGameplay, Verbose, TEXT("🏰 %s 主城槽位：攻击范围=%.0f, 主城半径=%.0f, 站位距离=%.0f"),
            *Attacker->GetName(), AttackerAttackRange, CityRadius, StandDistance);

        return true;
    }

    // 普通单位的槽位逻辑
    FSGTargetCombatInfo& CombatInfo = GetOrCreateCombatInfo(Target);

    // 检查是否已经预约了槽位
    for (FSGAttackSlot& Slot : CombatInfo.AttackSlots)
    {
        if (Slot.OccupyingUnit.Get() == Attacker)
        {
            OutSlotPosition = Slot.GetWorldPosition(Target, AttackerAttackRange, CombatInfo.TargetRadius);
            return true;
        }
    }

    // 查找最近的可用槽位
    int32 SlotIndex = FindNearestAvailableSlot(Target, Attacker->GetActorLocation(), AttackerAttackRange);
    if (SlotIndex == INDEX_NONE)
    {
        UE_LOG(LogSGGameplay, Warning, TEXT("❌ %s 无法预约 %s 的槽位：已满"),
            *Attacker->GetName(), *Target->GetName());
        return false;
    }

    // 预约槽位
    CombatInfo.AttackSlots[SlotIndex].OccupyingUnit = Attacker;
    OutSlotPosition = CombatInfo.AttackSlots[SlotIndex].GetWorldPosition(Target, AttackerAttackRange, CombatInfo.TargetRadius);

    UE_LOG(LogSGGameplay, Verbose, TEXT("✅ %s 预约了 %s 的槽位 #%d (攻击范围: %.0f, 目标半径: %.0f)"),
        *Attacker->GetName(), *Target->GetName(), SlotIndex, AttackerAttackRange, CombatInfo.TargetRadius);

    return true;
}

/**
 * @brief 释放攻击槽位
 * @param Attacker 攻击单位
 * @param Target 目标 Actor
 */
void USG_CombatTargetManager::ReleaseAttackSlot(ASG_UnitsBase* Attacker, AActor* Target)
{
    if (!Attacker || !Target)
    {
        return;
    }

    // ✨ 新增 - 远程单位没有占用槽位，不需要释放
    if (!ShouldUnitOccupySlot(Attacker))
    {
        return;
    }

    // 主城不使用槽位系统
    if (Target->IsA(ASG_MainCityBase::StaticClass()))
    {
        return;
    }

    FSGTargetCombatInfo* CombatInfo = TargetCombatInfoMap.Find(Target);
    if (!CombatInfo)
    {
        return;
    }

    // 查找并释放槽位
    for (FSGAttackSlot& Slot : CombatInfo->AttackSlots)
    {
        if (Slot.OccupyingUnit.Get() == Attacker)
        {
            Slot.OccupyingUnit = nullptr;
            UE_LOG(LogSGGameplay, Verbose, TEXT("🔓 %s 释放了 %s 的槽位"),
                *Attacker->GetName(), *Target->GetName());
            return;
        }
    }
}

/**
 * @brief 释放单位的所有槽位
 * @param Attacker 攻击单位
 */
void USG_CombatTargetManager::ReleaseAllSlots(ASG_UnitsBase* Attacker)
{
    if (!Attacker)
    {
        return;
    }

    // ✨ 新增 - 远程单位没有占用槽位
    if (!ShouldUnitOccupySlot(Attacker))
    {
        return;
    }

    // 遍历所有目标，释放此单位占用的槽位
    for (auto& Pair : TargetCombatInfoMap)
    {
        // 跳过主城
        if (AActor* TargetActor = Pair.Key.Get())
        {
            if (TargetActor->IsA(ASG_MainCityBase::StaticClass())) continue;
        }

        for (FSGAttackSlot& Slot : Pair.Value.AttackSlots)
        {
            if (Slot.OccupyingUnit.Get() == Attacker)
            {
                Slot.OccupyingUnit = nullptr;
            }
        }
    }
}

/**
 * @brief 检查目标是否有可用槽位
 * @param Target 目标 Actor
 * @return 是否有可用槽位
 */
bool USG_CombatTargetManager::HasAvailableSlot(AActor* Target) const
{
    // 主城总是有可用位置
    if (Target && Target->IsA(ASG_MainCityBase::StaticClass()))
    {
        return true;
    }

    const FSGTargetCombatInfo* CombatInfo = TargetCombatInfoMap.Find(Target);
    if (!CombatInfo)
    {
        return true;  // 未初始化意味着还没人攻击
    }
    return CombatInfo->GetAvailableSlotCount() > 0;
}

/**
 * @brief 获取目标的已占用槽位数量
 * @param Target 目标 Actor
 * @return 已占用的槽位数量
 */
int32 USG_CombatTargetManager::GetOccupiedSlotCount(AActor* Target) const
{
    // 主城不使用槽位系统
    if (Target && Target->IsA(ASG_MainCityBase::StaticClass()))
    {
        return 0;
    }

    const FSGTargetCombatInfo* CombatInfo = TargetCombatInfoMap.Find(Target);
    if (!CombatInfo)
    {
        return 0;
    }
    return CombatInfo->GetOccupiedSlotCount();
}

/**
 * @brief 获取单位当前预约的槽位位置
 * @param Attacker 攻击单位
 * @param Target 目标 Actor
 * @param OutPosition 输出：槽位世界坐标
 * @return 是否找到预约的槽位
 */
bool USG_CombatTargetManager::GetReservedSlotPosition(ASG_UnitsBase* Attacker, AActor* Target, FVector& OutPosition) const
{
    if (!Attacker || !Target)
    {
        return false;
    }

    // ✨ 新增 - 远程单位不使用槽位，计算攻击范围边缘
    if (!ShouldUnitOccupySlot(Attacker))
    {
        float AttackRange = Attacker->GetAttackRangeForAI();
        FVector AttackerLocation = Attacker->GetActorLocation();
        FVector TargetLocation = Target->GetActorLocation();
        FVector Direction = (TargetLocation - AttackerLocation).GetSafeNormal();
        
        OutPosition = TargetLocation - Direction * (AttackRange * 0.9f);
        OutPosition.Z = AttackerLocation.Z;
        return true;
    }

    float AttackerAttackRange = Attacker->GetAttackRangeForAI();

    // 主城逻辑
    if (ASG_MainCityBase* MainCity = Cast<ASG_MainCityBase>(Target))
    {
        FVector CityLocation = MainCity->GetActorLocation();
        FVector AttackerLocation = Attacker->GetActorLocation();
        FVector Direction = (AttackerLocation - CityLocation).GetSafeNormal();

        float CityRadius = GetTargetCollisionRadius(MainCity);
        float StandDistance = CityRadius + (AttackerAttackRange * SlotDistanceRatio);

        OutPosition = CityLocation + (Direction * StandDistance);
        OutPosition.Z = AttackerLocation.Z;
        return true;
    }

    // 普通单位逻辑
    const FSGTargetCombatInfo* CombatInfo = TargetCombatInfoMap.Find(Target);
    if (!CombatInfo)
    {
        return false;
    }

    for (const FSGAttackSlot& Slot : CombatInfo->AttackSlots)
    {
        if (Slot.OccupyingUnit.Get() == Attacker)
        {
            OutPosition = Slot.GetWorldPosition(Target, AttackerAttackRange, CombatInfo->TargetRadius);
            return true;
        }
    }

    return false;
}

/**
 * @brief 为目标初始化攻击槽位
 * @param Target 目标 Actor
 */
void USG_CombatTargetManager::InitializeSlotsForTarget(AActor* Target)
{
    if (!Target)
    {
        return;
    }

    // 主城不使用槽位系统
    if (Target->IsA(ASG_MainCityBase::StaticClass())) return;

    FSGTargetCombatInfo& CombatInfo = TargetCombatInfoMap.FindOrAdd(Target);
    if (CombatInfo.AttackSlots.Num() > 0) return;  // 已初始化

    // 缓存目标半径
    CombatInfo.TargetRadius = GetTargetCollisionRadius(Target);

    int32 NumSlots = UnitSlotCount;

    // 创建槽位
    CombatInfo.AttackSlots.SetNum(NumSlots);

    // 均匀分布槽位角度
    for (int32 i = 0; i < NumSlots; ++i)
    {
        CombatInfo.AttackSlots[i].Angle = (360.0f / NumSlots) * i;
        CombatInfo.AttackSlots[i].OccupyingUnit = nullptr;
    }

    UE_LOG(LogSGGameplay, Log, TEXT("📍 为 %s 初始化了 %d 个攻击槽位（目标半径: %.0f）"),
        *Target->GetName(), NumSlots, CombatInfo.TargetRadius);
}

/**
 * @brief 获取或创建目标的战斗信息
 * @param Target 目标 Actor
 * @return 目标的战斗信息引用
 */
FSGTargetCombatInfo& USG_CombatTargetManager::GetOrCreateCombatInfo(AActor* Target)
{
    FSGTargetCombatInfo* Existing = TargetCombatInfoMap.Find(Target);
    if (Existing && Existing->AttackSlots.Num() > 0)
    {
        return *Existing;
    }

    InitializeSlotsForTarget(Target);
    return TargetCombatInfoMap.FindOrAdd(Target);
}

/**
 * @brief 查找最近的可用槽位
 * @param Target 目标 Actor
 * @param AttackerLocation 攻击者位置
 * @param AttackerAttackRange 攻击者攻击范围
 * @return 槽位索引，如果没有可用槽位返回 INDEX_NONE
 */
int32 USG_CombatTargetManager::FindNearestAvailableSlot(AActor* Target, const FVector& AttackerLocation, float AttackerAttackRange)
{
    FSGTargetCombatInfo& CombatInfo = GetOrCreateCombatInfo(Target);

    int32 BestIndex = INDEX_NONE;
    float BestDistSq = FLT_MAX;

    // 遍历所有槽位，找到最近的可用槽位
    for (int32 i = 0; i < CombatInfo.AttackSlots.Num(); ++i)
    {
        const FSGAttackSlot& Slot = CombatInfo.AttackSlots[i];

        // 跳过已占用的槽位
        if (Slot.IsOccupied()) continue;

        // 计算槽位位置
        FVector SlotWorldPos = Slot.GetWorldPosition(Target, AttackerAttackRange, CombatInfo.TargetRadius);
        float DistSq = FVector::DistSquared(AttackerLocation, SlotWorldPos);

        // 更新最佳槽位
        if (DistSq < BestDistSq)
        {
            BestDistSq = DistSq;
            BestIndex = i;
        }
    }

    return BestIndex;
}

/**
 * @brief 使用球形检测获取范围内的敌方单位
 * @param Querier 查询单位
 * @param Range 检测范围
 * @param OutEnemies 输出：敌方单位列表
 */
void USG_CombatTargetManager::QueryEnemiesInRange(ASG_UnitsBase* Querier, float Range, TArray<AActor*>& OutEnemies)
{
    OutEnemies.Empty();

    if (!Querier)
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    FVector Center = Querier->GetActorLocation();
    FGameplayTag QuerierFaction = Querier->FactionTag;

    // 设置查询参数
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(Querier);
    QueryParams.bTraceComplex = false;
    QueryParams.bReturnPhysicalMaterial = false;

    FCollisionObjectQueryParams ObjectParams;
    ObjectParams.AddObjectTypesToQuery(ECC_Pawn);

    FCollisionShape SphereShape = FCollisionShape::MakeSphere(Range);

    // 执行球形重叠检测
    TArray<FOverlapResult> Overlaps;
    bool bHasOverlap = World->OverlapMultiByObjectType(
        Overlaps,
        Center,
        FQuat::Identity,
        ObjectParams,
        SphereShape,
        QueryParams
    );

    if (!bHasOverlap)
    {
        UE_LOG(LogSGGameplay, Verbose, TEXT("球形检测：%s 范围 %.0f 内无 Pawn"),
            *Querier->GetName(), Range);
        return;
    }

    // 过滤敌方单位
    for (const FOverlapResult& Overlap : Overlaps)
    {
        AActor* Actor = Overlap.GetActor();
        if (!Actor)
        {
            continue;
        }

        ASG_UnitsBase* Unit = Cast<ASG_UnitsBase>(Actor);
        if (!Unit)
        {
            continue;
        }

        // 跳过同阵营
        if (Unit->FactionTag == QuerierFaction)
        {
            continue;
        }

        // 跳过死亡单位
        if (Unit->bIsDead)
        {
            continue;
        }

        // 跳过不可被选为目标的单位
        if (!Unit->CanBeTargeted())
        {
            continue;
        }

        OutEnemies.Add(Unit);
    }

    UE_LOG(LogSGGameplay, Verbose, TEXT("球形检测：%s 范围 %.0f 内找到 %d 个敌方单位"),
        *Querier->GetName(), Range, OutEnemies.Num());
}

/**
 * @brief 清理无效数据
 * @details
 * 功能说明：
 * - 定期清理无效的目标引用
 * - 清理已死亡单位占用的槽位
 */
void USG_CombatTargetManager::CleanupInvalidData()
{
    for (auto It = TargetCombatInfoMap.CreateIterator(); It; ++It)
    {
        // 目标无效，移除整个记录
        if (!It.Key().IsValid())
        {
            It.RemoveCurrent();
            continue;
        }

        // 清理无效的槽位占用者
        for (FSGAttackSlot& Slot : It.Value().AttackSlots)
        {
            if (Slot.OccupyingUnit.IsValid() && Slot.OccupyingUnit->bIsDead)
            {
                Slot.OccupyingUnit = nullptr;
            }
        }
    }
}
