// 📄 文件：GameplayMechanics/SG_FrontLineManager.cpp

#include "Actors/SG_FrontLineManager.h"
#include "Components/SplineComponent.h"
#include "Components/BillboardComponent.h"
// ✨ 新增 - 静态网格体组件头文件
#include "Components/StaticMeshComponent.h"
#include "Units/SG_UnitsBase.h"
#include "Buildings/SG_MainCityBase.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Debug/SG_LogCategories.h"

/**
 * @brief 构造函数
 * @details 
 * 初始化流程：
 * 1. 启用 Tick 功能，用于每帧更新前线位置
 * 2. 创建根组件
 * 3. 创建双方前线样条线组件
 * 4. ✨ 新增 - 创建玩家前线可视化网格体组件
 * 5. 创建编辑器图标组件
 * 6. 设置样条线初始点位
 */
ASG_FrontLineManager::ASG_FrontLineManager()
{
    // 启用 Tick，每帧更新前线位置
    // 用于实时读取最前方单位位置，实现零延迟跟随
    PrimaryActorTick.bCanEverTick = true;

    // 创建根组件
    // 作为所有子组件的父级，提供统一的坐标系
    RootComp = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
    RootComponent = RootComp;

    // 创建玩家前线样条线
    // 用于在编辑器和运行时可视化玩家前线位置
    PlayerFrontLineSpline = CreateDefaultSubobject<USplineComponent>(TEXT("PlayerFrontLineSpline"));
    PlayerFrontLineSpline->SetupAttachment(RootComp);
    // 清空默认点
    PlayerFrontLineSpline->ClearSplinePoints();
    // 添加初始的两个端点（形成一条竖线）
    // 左端点：Y = -2500
    PlayerFrontLineSpline->AddSplinePoint(FVector(0.0f, -2500.0f, 10.0f), ESplineCoordinateSpace::Local);
    // 右端点：Y = 2500
    PlayerFrontLineSpline->AddSplinePoint(FVector(0.0f, 2500.0f, 10.0f), ESplineCoordinateSpace::Local);

    // 创建敌人前线样条线
    // 用于在编辑器和运行时可视化敌人前线位置
    EnemyFrontLineSpline = CreateDefaultSubobject<USplineComponent>(TEXT("EnemyFrontLineSpline"));
    EnemyFrontLineSpline->SetupAttachment(RootComp);
    // 清空默认点
    EnemyFrontLineSpline->ClearSplinePoints();
    // 添加初始的两个端点（形成一条竖线）
    // 左端点：Y = -2500
    EnemyFrontLineSpline->AddSplinePoint(FVector(0.0f, -2500.0f, 10.0f), ESplineCoordinateSpace::Local);
    // 右端点：Y = 2500
    EnemyFrontLineSpline->AddSplinePoint(FVector(0.0f, 2500.0f, 10.0f), ESplineCoordinateSpace::Local);

    // ✨ 新增 - 创建玩家前线可视化网格体
    // 用于在游戏运行时显示玩家前线的3D模型
    PlayerFrontLineMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlayerFrontLineMesh"));
    PlayerFrontLineMesh->SetupAttachment(RootComp);
    // 设置初始位置（在根组件位置）
    PlayerFrontLineMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));
    // 设置初始旋转（面向 Y 轴）
    PlayerFrontLineMesh->SetRelativeRotation(FRotator(0.0f, 0.0f, 0.0f));
    // 设置碰撞（不参与碰撞）
    PlayerFrontLineMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    // 设置为可见
    PlayerFrontLineMesh->SetVisibility(true);
    // 默认不投射阴影（可根据需要调整）
    PlayerFrontLineMesh->SetCastShadow(false);

    // 创建 Actor 广告牌
    // 在编辑器中显示图标，方便在场景中定位该 Actor
    ActorBillboard = CreateDefaultSubobject<UBillboardComponent>(TEXT("ActorBillboard"));
    ActorBillboard->SetupAttachment(RootComp);
    // 设置图标位置（在根组件上方 300 单位）
    ActorBillboard->SetRelativeLocation(FVector(0.0f, 0.0f, 300.0f));
    // 设置图标缩放（3倍大小，更容易看到）
    ActorBillboard->SetRelativeScale3D(FVector(3.0f, 3.0f, 3.0f));
    // 仅在编辑器中显示
    ActorBillboard->bIsEditorOnly = true;
    
    // 初始化前线位置为初始值
    CurrentPlayerFrontLineX = InitialFrontLineX;
    CurrentEnemyFrontLineX = InitialFrontLineX;
}

/**
 * @brief BeginPlay 生命周期函数
 * @details 
 * 初始化流程：
 * 1. 查找并缓存双方主城位置
 * 2. 确定玩家和敌人的方向（左/右）
 * 3. 打印初始化日志
 * 4. 设置前线初始位置
 * 5. 立即扫描一次最前方单位
 * 6. 更新可视化（样条线 + 网格体）
 * 7. 启动定时重新扫描
 */
void ASG_FrontLineManager::BeginPlay()
{
    Super::BeginPlay();
    
    // 查找并缓存主城位置
    // 这一步必须最先执行，因为需要根据主城位置确定玩家方向
    FindAndCacheMainCities();
    
    // 打印初始化信息到日志
    UE_LOG(LogSGGameplay, Log, TEXT("========== 前线管理器初始化 =========="));
    UE_LOG(LogSGGameplay, Log, TEXT("  玩家主城：X = %.0f"), PlayerMainCityX);
    UE_LOG(LogSGGameplay, Log, TEXT("  敌人主城：X = %.0f"), EnemyMainCityX);
    UE_LOG(LogSGGameplay, Log, TEXT("  玩家在左侧：%s"), bPlayerOnLeftSide ? TEXT("是") : TEXT("否"));
    UE_LOG(LogSGGameplay, Log, TEXT("  重新扫描间隔：%.2f 秒"), RescanInterval);
    UE_LOG(LogSGGameplay, Log, TEXT("  显示前线网格体：%s"), bShowPlayerFrontLineMesh ? TEXT("是") : TEXT("否"));
    UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
    
    // 设置初始位置
    // 游戏开始时，双方前线都在中间位置
    CurrentPlayerFrontLineX = InitialFrontLineX;
    CurrentEnemyFrontLineX = InitialFrontLineX;
    
    // 立即扫描一次，找到初始的最前方单位
    // 这样可以确保游戏开始时就有正确的前线位置
    RescanFrontmostUnits();
    
    // 更新可视化
    // 根据当前前线位置更新样条线和网格体
    UpdateFrontLineVisualization();
    
    // 启动定时重新扫描（定期查找新的最前方单位）
    // 使用定时器，每隔 RescanInterval 秒执行一次 RescanFrontmostUnits
    GetWorld()->GetTimerManager().SetTimer(
        RescanTimerHandle,                          // 定时器句柄
        this,                                       // 调用对象
        &ASG_FrontLineManager::RescanFrontmostUnits,// 回调函数
        RescanInterval,                             // 间隔时间
        true                                        // 是否循环
    );
}

/**
 * @brief 每帧更新（实时跟随版）
 * @param DeltaTime 距离上一帧的时间间隔（秒）
 * @details 
 * 执行流程：
 * 1. 调用父类 Tick
 * 2. 更新前线位置（从缓存单位读取实时位置）
 * 3. 绘制调试信息
 * 
 * 性能说明：
 * - 每帧只读取2个单位的位置，性能开销极小
 * - 不需要遍历所有单位，复杂度为 O(1)
 */
void ASG_FrontLineManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    // 每帧更新前线位置（实时跟随单位）
    // 直接从缓存的最前方单位读取位置，实现零延迟跟随
    UpdateFrontLinePositionRealtime();
    
    // 绘制调试信息
    // 如果启用了调试绘制，显示前线、区域和单位标记
    if (bEnableDebugDraw)
    {
        DrawDebugInfo();
    }
}

/**
 * @brief 更新前线位置（每帧调用，实时跟随）
 * @details 
 * 核心逻辑：
 * 1. 从缓存的最前方单位读取实时位置（无需遍历所有单位）
 * 2. 检查单位是否越过初始线（根据 bOnlyTrackCrossedUnits 配置）
 * 3. 计算新的前线位置（单位位置 + 偏移量）
 * 4. 直接设置前线位置（无插值，零延迟）
 * 5. 调整前线间距，防止重叠
 * 6. 更新可视化（样条线 + 网格体）
 * 
 * 性能优化：
 * - 只读取2个单位的位置，O(1) 复杂度
 * - 无插值计算，直接赋值
 * - 只在位置改变时更新可视化
 * 
 * 注意事项：
 * - 需要确保缓存的单位有效且未死亡
 * - 根据玩家方向（左/右）计算前线位置
 */
void ASG_FrontLineManager::UpdateFrontLinePositionRealtime()
{
    // 记录是否有变化
    // 只有在前线位置改变时才更新可视化，避免不必要的计算
    bool bChanged = false;
    
    // ========== 更新玩家前线 ==========
    
    // 默认前线位置为初始值
    float NewPlayerFrontLineX = InitialFrontLineX;
    
    // 检查缓存的玩家最前方单位是否有效
    if (CachedPlayerFrontmostUnit &&          // 单位存在
        !CachedPlayerFrontmostUnit->bIsDead &&// 单位未死亡
        IsValid(CachedPlayerFrontmostUnit))   // 单位对象有效
    {
        // 获取单位当前位置的 X 坐标
        float UnitX = CachedPlayerFrontmostUnit->GetActorLocation().X;
        
        // 检查是否越过初始线
        // 根据玩家方向判断：
        // - 玩家在左侧：单位 X > 初始线 X 表示越线
        // - 玩家在右侧：单位 X < 初始线 X 表示越线
        bool bCrossedLine = bPlayerOnLeftSide ? 
            (UnitX > InitialFrontLineX) : 
            (UnitX < InitialFrontLineX);
        
        // 如果单位越过初始线，更新前线位置
        if (bCrossedLine)
        {
            // 计算前线位置
            if (bPlayerOnLeftSide)
            {
                // 玩家在左侧，前线在单位右侧
                // 前线位置 = 单位位置 + 偏移量
                NewPlayerFrontLineX = UnitX + FrontLineOffset;
            }
            else
            {
                // 玩家在右侧，前线在单位左侧
                // 前线位置 = 单位位置 - 偏移量
                NewPlayerFrontLineX = UnitX - FrontLineOffset;
            }
        }
    }

    // ========== 更新敌人前线 ==========
    
    // 默认前线位置为初始值
    float NewEnemyFrontLineX = InitialFrontLineX;
    
    // 检查缓存的敌人最前方单位是否有效
    if (CachedEnemyFrontmostUnit &&          // 单位存在
        !CachedEnemyFrontmostUnit->bIsDead &&// 单位未死亡
        IsValid(CachedEnemyFrontmostUnit))   // 单位对象有效
    {
        // 获取单位当前位置的 X 坐标
        float UnitX = CachedEnemyFrontmostUnit->GetActorLocation().X;
        
        // 检查是否越过初始线
        // 根据玩家方向判断：
        // - 玩家在左侧（敌人在右侧）：单位 X < 初始线 X 表示越线
        // - 玩家在右侧（敌人在左侧）：单位 X > 初始线 X 表示越线
        bool bCrossedLine = bPlayerOnLeftSide ? 
            (UnitX < InitialFrontLineX) : 
            (UnitX > InitialFrontLineX);
        
        // 如果单位越过初始线，更新前线位置
        if (bCrossedLine)
        {
            // 计算前线位置
            if (bPlayerOnLeftSide)
            {
                // 敌人在右侧，前线在单位左侧
                // 前线位置 = 单位位置 - 偏移量
                NewEnemyFrontLineX = UnitX - FrontLineOffset;
            }
            else
            {
                // 敌人在左侧，前线在单位右侧
                // 前线位置 = 单位位置 + 偏移量
                NewEnemyFrontLineX = UnitX + FrontLineOffset;
            }
        }
    }

    // ========== 应用新位置 ==========
    
    // 直接设置新位置（无插值）
    // 检查玩家前线是否改变（允许1个单位的误差）
    if (!FMath::IsNearlyEqual(CurrentPlayerFrontLineX, NewPlayerFrontLineX, 1.0f))
    {
        // 直接赋值，无插值，实现零延迟跟随
        CurrentPlayerFrontLineX = NewPlayerFrontLineX;
        bChanged = true;
    }

    // 检查敌人前线是否改变（允许1个单位的误差）
    if (!FMath::IsNearlyEqual(CurrentEnemyFrontLineX, NewEnemyFrontLineX, 1.0f))
    {
        // 直接赋值，无插值，实现零延迟跟随
        CurrentEnemyFrontLineX = NewEnemyFrontLineX;
        bChanged = true;
    }

    // 只在位置改变时更新可视化
    // 避免不必要的计算和绘制
    if (bChanged)
    {
        // 调整前线间距
        // 确保双方前线不会过于接近
        AdjustFrontLineDistance();
        
        // 更新可视化
        // 更新样条线和网格体位置
        UpdateFrontLineVisualization();
    }
}

/**
 * @brief 重新扫描最前方单位（定时调用）
 * @details 
 * 执行流程：
 * 1. 获取场景中所有单位
 * 2. 根据阵营标签区分玩家和敌人单位
 * 3. 根据位置和方向找到最前方的单位
 * 4. 更新缓存的最前方单位
 * 5. 解绑旧单位的死亡事件
 * 6. 绑定新单位的死亡事件
 * 
 * 调用时机：
 * - BeginPlay 时立即调用一次
 * - 之后每隔 RescanInterval 秒调用一次
 * - 最前方单位死亡时立即调用
 * 
 * 性能说明：
 * - 需要遍历所有单位，复杂度为 O(n)
 * - 通过定时调用而非每帧调用来平衡性能
 * - 在两次扫描之间，直接读取缓存单位位置（O(1)）
 */
void ASG_FrontLineManager::RescanFrontmostUnits()
{
    // 打印日志，标记开始重新扫描
    UE_LOG(LogSGGameplay, Verbose, TEXT("========== 重新扫描最前方单位 =========="));
    
    // 获取场景中所有单位
    // 查找所有 ASG_UnitsBase 类型的 Actor
    TArray<AActor*> AllUnits;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASG_UnitsBase::StaticClass(), AllUnits);
    
    // 定义阵营标签
    // 用于区分玩家和敌人单位
    FGameplayTag PlayerFactionTag = FGameplayTag::RequestGameplayTag(TEXT("Unit.Faction.Player"));
    FGameplayTag EnemyFactionTag = FGameplayTag::RequestGameplayTag(TEXT("Unit.Faction.Enemy"));
    
    // ========== 查找玩家最前方单位 ==========
    
    // 初始化玩家极值位置为初始线
    float PlayerExtremumX = InitialFrontLineX;
    // 初始化玩家最前方单位为空
    ASG_UnitsBase* PlayerFrontmost = nullptr;
    
    // ========== 查找敌人最前方单位 ==========
    
    // 初始化敌人极值位置为初始线
    float EnemyExtremumX = InitialFrontLineX;
    // 初始化敌人最前方单位为空
    ASG_UnitsBase* EnemyFrontmost = nullptr;
    
    // 一次遍历同时查找双方最前方单位
    // 避免分两次遍历，提高性能
    for (AActor* Actor : AllUnits)
    {
        // 转换为单位类型
        ASG_UnitsBase* Unit = Cast<ASG_UnitsBase>(Actor);
        
        // 跳过无效或已死亡的单位
        if (!Unit || Unit->bIsDead)
        {
            continue;
        }
        
        // 获取单位的 X 坐标
        float UnitX = Unit->GetActorLocation().X;
        
        // ========== 检查玩家单位 ==========
        if (Unit->FactionTag.MatchesTag(PlayerFactionTag))
        {
            // 判断是否是最前方单位
            bool bIsFrontmost = false;
            
            if (bPlayerOnLeftSide)
            {
                // 玩家在左侧，找最右边的单位
                if (bOnlyTrackCrossedUnits)
                {
                    // 只追踪越过初始线的单位
                    // 条件：单位 X > 初始线 X 且 单位 X > 当前极值 X
                    bIsFrontmost = (UnitX > InitialFrontLineX) && (UnitX > PlayerExtremumX);
                }
                else
                {
                    // 追踪所有单位
                    // 条件：单位 X > 当前极值 X
                    bIsFrontmost = (UnitX > PlayerExtremumX);
                }
            }
            else
            {
                // 玩家在右侧，找最左边的单位
                if (bOnlyTrackCrossedUnits)
                {
                    // 只追踪越过初始线的单位
                    // 条件：单位 X < 初始线 X 且 单位 X < 当前极值 X
                    bIsFrontmost = (UnitX < InitialFrontLineX) && (UnitX < PlayerExtremumX);
                }
                else
                {
                    // 追踪所有单位
                    // 条件：单位 X < 当前极值 X
                    bIsFrontmost = (UnitX < PlayerExtremumX);
                }
            }
            
            // 如果是最前方单位，更新极值和缓存
            if (bIsFrontmost)
            {
                PlayerExtremumX = UnitX;
                PlayerFrontmost = Unit;
            }
        }
        // ========== 检查敌人单位 ==========
        else if (Unit->FactionTag.MatchesTag(EnemyFactionTag))
        {
            // 判断是否是最前方单位
            bool bIsFrontmost = false;
            
            if (bPlayerOnLeftSide)
            {
                // 敌人在右侧，找最左边的单位
                if (bOnlyTrackCrossedUnits)
                {
                    // 只追踪越过初始线的单位
                    // 条件：单位 X < 初始线 X 且 单位 X < 当前极值 X
                    bIsFrontmost = (UnitX < InitialFrontLineX) && (UnitX < EnemyExtremumX);
                }
                else
                {
                    // 追踪所有单位
                    // 条件：单位 X < 当前极值 X
                    bIsFrontmost = (UnitX < EnemyExtremumX);
                }
            }
            else
            {
                // 敌人在左侧，找最右边的单位
                if (bOnlyTrackCrossedUnits)
                {
                    // 只追踪越过初始线的单位
                    // 条件：单位 X > 初始线 X 且 单位 X > 当前极值 X
                    bIsFrontmost = (UnitX > InitialFrontLineX) && (UnitX > EnemyExtremumX);
                }
                else
                {
                    // 追踪所有单位
                    // 条件：单位 X > 当前极值 X
                    bIsFrontmost = (UnitX > EnemyExtremumX);
                }
            }
            
            // 如果是最前方单位，更新极值和缓存
            if (bIsFrontmost)
            {
                EnemyExtremumX = UnitX;
                EnemyFrontmost = Unit;
            }
        }
    }
    
    // ========== 更新玩家最前方单位缓存 ==========
    
    // 检查最前方单位是否改变
    if (PlayerFrontmost != CachedPlayerFrontmostUnit)
    {
        // 解绑旧单位的死亡事件
        if (CachedPlayerFrontmostUnit)
        {
            UnbindUnitDeathEvent(CachedPlayerFrontmostUnit);
        }
        
        // 绑定新单位的死亡事件
        if (PlayerFrontmost)
        {
            BindUnitDeathEvent(PlayerFrontmost);
            // 打印日志，记录新的最前方单位
            UE_LOG(LogSGGameplay, Log, TEXT("✓ 玩家最前方单位更新：%s (X = %.0f)"), 
                *PlayerFrontmost->GetName(), PlayerExtremumX);
        }
        else
        {
            // 没有找到越过初始线的单位
            UE_LOG(LogSGGameplay, Log, TEXT("玩家无越过初始线的单位"));
        }
        
        // 更新缓存
        CachedPlayerFrontmostUnit = PlayerFrontmost;
    }
    
    // ========== 更新敌人最前方单位缓存 ==========
    
    // 检查最前方单位是否改变
    if (EnemyFrontmost != CachedEnemyFrontmostUnit)
    {
        // 解绑旧单位的死亡事件
        if (CachedEnemyFrontmostUnit)
        {
            UnbindUnitDeathEvent(CachedEnemyFrontmostUnit);
        }
        
        // 绑定新单位的死亡事件
        if (EnemyFrontmost)
        {
            BindUnitDeathEvent(EnemyFrontmost);
            // 打印日志，记录新的最前方单位
            UE_LOG(LogSGGameplay, Log, TEXT("✓ 敌人最前方单位更新：%s (X = %.0f)"), 
                *EnemyFrontmost->GetName(), EnemyExtremumX);
        }
        else
        {
            // 没有找到越过初始线的单位
            UE_LOG(LogSGGameplay, Log, TEXT("敌人无越过初始线的单位"));
        }
        
        // 更新缓存
        CachedEnemyFrontmostUnit = EnemyFrontmost;
    }
    
    // 打印日志，标记扫描结束
    UE_LOG(LogSGGameplay, Verbose, TEXT("========================================"));
}

/**
 * @brief 更新前线可视化
 * @details 
 * 功能说明：
 * - 更新样条线组件的位置，使其显示当前前线
 * - ✨ 新增 - 更新玩家前线网格体的位置、旋转和缩放
 * 
 * 更新内容：
 * 1. 更新样条线位置（编辑器可视化）
 * 2. 更新玩家前线网格体位置（运行时可视化）
 * 3. 根据 FrontLineWidth 自动调整网格体 Y 轴缩放
 */
void ASG_FrontLineManager::UpdateFrontLineVisualization()
{
    // ========== 更新玩家前线样条线 ==========
    if (PlayerFrontLineSpline)
    {
        // 计算样条线起点（左端点）
        // X = 当前玩家前线位置
        // Y = 前线宽度的一半（负值，表示左侧）
        // Z = 前线高度
        FVector StartPoint = FVector(CurrentPlayerFrontLineX, -FrontLineWidth / 2.0f, FrontLineHeight);
        
        // 计算样条线终点（右端点）
        // X = 当前玩家前线位置
        // Y = 前线宽度的一半（正值，表示右侧）
        // Z = 前线高度
        FVector EndPoint = FVector(CurrentPlayerFrontLineX, FrontLineWidth / 2.0f, FrontLineHeight);
        
        // 更新样条线第0个点的位置（起点）
        PlayerFrontLineSpline->SetLocationAtSplinePoint(0, StartPoint, ESplineCoordinateSpace::World);
        // 更新样条线第1个点的位置（终点）
        PlayerFrontLineSpline->SetLocationAtSplinePoint(1, EndPoint, ESplineCoordinateSpace::World);
    }

    // ========== 更新敌人前线样条线 ==========
    if (EnemyFrontLineSpline)
    {
        // 计算样条线起点（左端点）
        // X = 当前敌人前线位置
        // Y = 前线宽度的一半（负值，表示左侧）
        // Z = 前线高度
        FVector StartPoint = FVector(CurrentEnemyFrontLineX, -FrontLineWidth / 2.0f, FrontLineHeight);
        
        // 计算样条线终点（右端点）
        // X = 当前敌人前线位置
        // Y = 前线宽度的一半（正值，表示右侧）
        // Z = 前线高度
        FVector EndPoint = FVector(CurrentEnemyFrontLineX, FrontLineWidth / 2.0f, FrontLineHeight);
        
        // 更新样条线第0个点的位置（起点）
        EnemyFrontLineSpline->SetLocationAtSplinePoint(0, StartPoint, ESplineCoordinateSpace::World);
        // 更新样条线第1个点的位置（终点）
        EnemyFrontLineSpline->SetLocationAtSplinePoint(1, EndPoint, ESplineCoordinateSpace::World);
    }

    // ✨ 新增 - 更新玩家前线网格体
    if (PlayerFrontLineMesh)
    {
        // 根据配置决定是否显示网格体
        PlayerFrontLineMesh->SetVisibility(bShowPlayerFrontLineMesh);
        
        if (bShowPlayerFrontLineMesh)
        {
            // 计算网格体位置
            // X = 当前玩家前线位置
            // Y = 0（中心位置）
            // Z = 前线高度
            FVector MeshLocation = FVector(CurrentPlayerFrontLineX, 0.0f, FrontLineHeight);
            
            // 设置网格体世界位置
            PlayerFrontLineMesh->SetWorldLocation(MeshLocation);
            
            // 设置网格体旋转
            // 面向 Y 轴（前线是一条竖线）
            FRotator MeshRotation = FRotator(0.0f, 0.0f, 0.0f);
            PlayerFrontLineMesh->SetWorldRotation(MeshRotation);
            
            // 计算网格体缩放
            // X 和 Z 使用配置的缩放值
            // Y 根据前线宽度自动计算（假设网格体原始宽度为100单位）
            FVector MeshScale = FrontLineMeshScale;
            // 根据前线宽度调整 Y 轴缩放
            // 假设网格体原始宽度为100单位，缩放到 FrontLineWidth
            MeshScale.Y = (FrontLineWidth / 100.0f) * FrontLineMeshScale.Y;
            
            // 设置网格体世界缩放
            PlayerFrontLineMesh->SetWorldScale3D(MeshScale);
        }
    }
}

/**
 * @brief 绘制调试信息
 * @details 
 * 绘制内容：
 * 1. 玩家前线（蓝色实线）
 * 2. 敌人前线（红色实线）
 * 3. 中立区中线（黄色虚线）
 * 4. 前线位置文字
 * 5. 最前方单位标记（球体 + 文字）
 * 
 * 注意事项：
 * - 仅在 bEnableDebugDraw 为 true 时绘制
 * - 使用 DrawDebug 系列函数，仅在编辑器和开发版本中显示
 */
void ASG_FrontLineManager::DrawDebugInfo()
{
    // 检查世界对象是否有效
    if (!GetWorld())
    {
        return;
    }
    
    // ========== 绘制玩家前线（蓝色） ==========
    
    // 计算前线起点（左端点）
    FVector PlayerLineStart = FVector(CurrentPlayerFrontLineX, -FrontLineWidth / 2.0f, 0.0f);
    // 计算前线终点（右端点）
    FVector PlayerLineEnd = FVector(CurrentPlayerFrontLineX, FrontLineWidth / 2.0f, 0.0f);
    // 绘制蓝色线条
    DrawDebugLine(GetWorld(), PlayerLineStart, PlayerLineEnd, FColor::Blue, false, -1.0f, 0, FrontLineThickness);

    // ========== 绘制敌人前线（红色） ==========
    
    // 计算前线起点（左端点）
    FVector EnemyLineStart = FVector(CurrentEnemyFrontLineX, -FrontLineWidth / 2.0f, 0.0f);
    // 计算前线终点（右端点）
    FVector EnemyLineEnd = FVector(CurrentEnemyFrontLineX, FrontLineWidth / 2.0f, 0.0f);
    // 绘制红色线条
    DrawDebugLine(GetWorld(), EnemyLineStart, EnemyLineEnd, FColor::Red, false, -1.0f, 0, FrontLineThickness);

    // ========== 绘制中立区中线（黄色虚线） ==========
    
    // 计算中立区中线的 X 坐标（双方前线的中点）
    float MidX = (CurrentPlayerFrontLineX + CurrentEnemyFrontLineX) / 2.0f;
    
    // 虚线段数（偶数，用于绘制虚线效果）
    int32 SegmentCount = 20;
    // 每段的长度
    float SegmentLength = FrontLineWidth / SegmentCount;
    
    // 绘制虚线（每隔一段绘制一次）
    for (int32 i = 0; i < SegmentCount; i += 2)
    {
        // 计算当前段的起点
        FVector SegmentStart = FVector(MidX, -FrontLineWidth / 2.0f + i * SegmentLength, 0.0f);
        // 计算当前段的终点
        FVector SegmentEnd = FVector(MidX, -FrontLineWidth / 2.0f + (i + 1) * SegmentLength, 0.0f);
        // 绘制黄色线条（粗细为前线粗细的一半）
        DrawDebugLine(GetWorld(), SegmentStart, SegmentEnd, FColor::Yellow, false, -1.0f, 0, FrontLineThickness / 2.0f);
    }
    
    // ========== 绘制文字信息 ==========
    
    // 绘制玩家前线位置文字（蓝色）
    DrawDebugString(GetWorld(), FVector(CurrentPlayerFrontLineX, 0.0f, 200.0f), 
        FString::Printf(TEXT("玩家前线: %.0f"), CurrentPlayerFrontLineX),
        nullptr, FColor::Blue, 0.0f, true, 1.5f);

    // 绘制敌人前线位置文字（红色）
    DrawDebugString(GetWorld(), FVector(CurrentEnemyFrontLineX, 0.0f, 200.0f), 
        FString::Printf(TEXT("敌人前线: %.0f"), CurrentEnemyFrontLineX),
        nullptr, FColor::Red, 0.0f, true, 1.5f);

    // ========== 绘制最前方单位指示 ==========
    
    // 绘制玩家最前方单位标记
    if (CachedPlayerFrontmostUnit && !CachedPlayerFrontmostUnit->bIsDead)
    {
        // 获取单位位置
        FVector UnitLoc = CachedPlayerFrontmostUnit->GetActorLocation();
        // 绘制青色球体（半径100）
        DrawDebugSphere(GetWorld(), UnitLoc, 100.0f, 12, FColor::Cyan, false, -1.0f, 0, 5.0f);
        // 绘制文字标签
        DrawDebugString(GetWorld(), UnitLoc + FVector(0.0f, 0.0f, 150.0f), 
            TEXT("玩家最前方"), nullptr, FColor::Cyan, 0.0f, true, 1.2f);
    }

    // 绘制敌人最前方单位标记
    if (CachedEnemyFrontmostUnit && !CachedEnemyFrontmostUnit->bIsDead)
    {
        // 获取单位位置
        FVector UnitLoc = CachedEnemyFrontmostUnit->GetActorLocation();
        // 绘制橙色球体（半径100）
        DrawDebugSphere(GetWorld(), UnitLoc, 100.0f, 12, FColor::Orange, false, -1.0f, 0, 5.0f);
        // 绘制文字标签
        DrawDebugString(GetWorld(), UnitLoc + FVector(0.0f, 0.0f, 150.0f), 
            TEXT("敌人最前方"), nullptr, FColor::Orange, 0.0f, true, 1.2f);
    }
}

/**
 * @brief 调整前线间距
 * @details 
 * 功能说明：
 * - 确保双方前线不会过于接近
 * - 当前线间距小于 MinFrontLineDistance 时，将双方前线向外推开
 * - 保持前线间距至少为 MinFrontLineDistance
 * 
 * 调整策略：
 * - 计算当前双方前线的距离
 * - 如果距离小于最小间距，计算需要调整的距离
 * - 将双方前线各向外推开一半的调整距离
 * - 根据玩家方向（左/右）决定推开的方向
 */
void ASG_FrontLineManager::AdjustFrontLineDistance()
{
    // 计算当前双方前线的距离（绝对值）
    float CurrentDistance = FMath::Abs(CurrentEnemyFrontLineX - CurrentPlayerFrontLineX);
    
    // 如果当前距离小于最小间距，需要调整
    if (CurrentDistance < MinFrontLineDistance)
    {
        // 计算需要调整的距离（总共需要增加的距离）
        // 除以2是因为双方各向外推开一半
        float AdjustDistance = (MinFrontLineDistance - CurrentDistance) / 2.0f;
        
        // 根据玩家方向调整前线位置
        if (bPlayerOnLeftSide)
        {
            // 玩家在左侧
            // 玩家前线向左推（减小 X）
            CurrentPlayerFrontLineX -= AdjustDistance;
            // 敌人前线向右推（增大 X）
            CurrentEnemyFrontLineX += AdjustDistance;
        }
        else
        {
            // 玩家在右侧
            // 玩家前线向右推（增大 X）
            CurrentPlayerFrontLineX += AdjustDistance;
            // 敌人前线向左推（减小 X）
            CurrentEnemyFrontLineX -= AdjustDistance;
        }
    }
}

/**
 * @brief 查找并缓存主城位置
 * @details 
 * 执行流程：
 * 1. 获取场景中所有主城
 * 2. 根据 Faction 标签区分玩家和敌人主城
 * 3. 缓存主城引用和位置
 * 4. 根据主城位置确定玩家方向（左/右）
 * 
 * 注意事项：
 * - 需要主城正确设置 Faction 标签
 * - 玩家方向由主城位置决定：玩家主城 X < 敌人主城 X 则玩家在左侧
 */
void ASG_FrontLineManager::FindAndCacheMainCities()
{
    // 打印日志，开始查找主城
    UE_LOG(LogSGGameplay, Log, TEXT("查找主城..."));
    
    // 获取场景中所有主城
    TArray<AActor*> FoundMainCities;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASG_MainCityBase::StaticClass(), FoundMainCities);
    
    // 定义阵营标签
    FGameplayTag PlayerFactionTag = FGameplayTag::RequestGameplayTag(TEXT("Unit.Faction.Player"));
    FGameplayTag EnemyFactionTag = FGameplayTag::RequestGameplayTag(TEXT("Unit.Faction.Enemy"));
    
    // 遍历所有主城
    for (AActor* Actor : FoundMainCities)
    {
        // 转换为主城类型
        ASG_MainCityBase* MainCity = Cast<ASG_MainCityBase>(Actor);
        if (!MainCity)
        {
            continue;
        }
        
        // 检查是否是玩家主城
        if (MainCity->FactionTag.MatchesTag(PlayerFactionTag))
        {
            // 缓存玩家主城引用
            CachedPlayerMainCity = MainCity;
            // 缓存玩家主城 X 坐标
            PlayerMainCityX = MainCity->GetActorLocation().X;
            // 打印日志
            UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 玩家主城：X = %.0f"), PlayerMainCityX);
        }
        // 检查是否是敌人主城
        else if (MainCity->FactionTag.MatchesTag(EnemyFactionTag))
        {
            // 缓存敌人主城引用
            CachedEnemyMainCity = MainCity;
            // 缓存敌人主城 X 坐标
            EnemyMainCityX = MainCity->GetActorLocation().X;
            // 打印日志
            UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 敌人主城：X = %.0f"), EnemyMainCityX);
        }
    }
    
    // 如果双方主城都找到了，确定玩家方向
    if (CachedPlayerMainCity && CachedEnemyMainCity)
    {
        // 玩家主城 X < 敌人主城 X，则玩家在左侧
        bPlayerOnLeftSide = (PlayerMainCityX < EnemyMainCityX);
    }
}

/**
 * @brief 获取位置所属区域
 * @param Location 要查询的世界坐标位置
 * @return 该位置所属的区域类型（玩家区域/中立区域/敌人区域）
 * @details 
 * 判断逻辑：
 * - 根据玩家方向（左/右）和位置 X 坐标判断
 * - 玩家在左侧时：
 *   - X < 玩家前线 → 玩家区域
 *   - 玩家前线 < X < 敌人前线 → 中立区域
 *   - X > 敌人前线 → 敌人区域
 * - 玩家在右侧时：
 *   - X > 玩家前线 → 玩家区域
 *   - 敌人前线 < X < 玩家前线 → 中立区域
 *   - X < 敌人前线 → 敌人区域
 */
ESGFrontLineZone ASG_FrontLineManager::GetZoneAtLocation(const FVector& Location) const
{
    // 获取位置的 X 坐标
    float LocationX = Location.X;
    
    // 根据玩家方向判断
    if (bPlayerOnLeftSide)
    {
        // 玩家在左侧
        if (LocationX < CurrentPlayerFrontLineX)
        {
            // 位置在玩家前线左侧 → 玩家区域
            return ESGFrontLineZone::PlayerZone;
        }
        else if (LocationX > CurrentEnemyFrontLineX)
        {
            // 位置在敌人前线右侧 → 敌人区域
            return ESGFrontLineZone::EnemyZone;
        }
        else
        {
            // 位置在双方前线之间 → 中立区域
            return ESGFrontLineZone::NeutralZone;
        }
    }
    else
    {
        // 玩家在右侧
        if (LocationX > CurrentPlayerFrontLineX)
        {
            // 位置在玩家前线右侧 → 玩家区域
            return ESGFrontLineZone::PlayerZone;
        }
        else if (LocationX < CurrentEnemyFrontLineX)
        {
            // 位置在敌人前线左侧 → 敌人区域
            return ESGFrontLineZone::EnemyZone;
        }
        else
        {
            // 位置在双方前线之间 → 中立区域
            return ESGFrontLineZone::NeutralZone;
        }
    }
}

/**
 * @brief 判断位置是否在玩家区域
 * @param Location 要查询的世界坐标位置
 * @return true：在玩家区域；false：不在玩家区域
 */
bool ASG_FrontLineManager::IsInPlayerZone(const FVector& Location) const
{
    return GetZoneAtLocation(Location) == ESGFrontLineZone::PlayerZone;
}

/**
 * @brief 判断位置是否在敌人区域
 * @param Location 要查询的世界坐标位置
 * @return true：在敌人区域；false：不在敌人区域
 */
bool ASG_FrontLineManager::IsInEnemyZone(const FVector& Location) const
{
    return GetZoneAtLocation(Location) == ESGFrontLineZone::EnemyZone;
}

/**
 * @brief 判断位置是否在中立区域
 * @param Location 要查询的世界坐标位置
 * @return true：在中立区域；false：不在中立区域
 */
bool ASG_FrontLineManager::IsInNeutralZone(const FVector& Location) const
{
    return GetZoneAtLocation(Location) == ESGFrontLineZone::NeutralZone;
}

/**
 * @brief 单位死亡回调
 * @param DeadUnit 死亡的单位
 * @details 
 * 功能说明：
 * - 当最前方单位死亡时触发
 * - 立即清除缓存并重新扫描
 * - 确保前线始终跟踪有效单位
 * 
 * 执行流程：
 * 1. 检查死亡单位是否是缓存的最前方单位
 * 2. 如果是，打印警告日志
 * 3. 清除缓存
 * 4. 立即调用 RescanFrontmostUnits 重新扫描
 */
void ASG_FrontLineManager::OnUnitDeath(ASG_UnitsBase* DeadUnit)
{
    // 检查是否是玩家最前方单位死亡
    if (DeadUnit == CachedPlayerFrontmostUnit)
    {
        // 打印警告日志
        UE_LOG(LogSGGameplay, Warning, TEXT("⚠️ 玩家最前方单位死亡，立即重新扫描"));
        // 清除缓存
        CachedPlayerFrontmostUnit = nullptr;
        // 立即重新扫描
        RescanFrontmostUnits();
    }
    // 检查是否是敌人最前方单位死亡
    else if (DeadUnit == CachedEnemyFrontmostUnit)
    {
        // 打印警告日志
        UE_LOG(LogSGGameplay, Warning, TEXT("⚠️ 敌人最前方单位死亡，立即重新扫描"));
        // 清除缓存
        CachedEnemyFrontmostUnit = nullptr;
        // 立即重新扫描
        RescanFrontmostUnits();
    }
}

/**
 * @brief 绑定单位死亡事件
 * @param Unit 要绑定的单位
 * @details 将 OnUnitDeath 函数绑定到单位的死亡委托
 */
void ASG_FrontLineManager::BindUnitDeathEvent(ASG_UnitsBase* Unit)
{
    if (Unit)
    {
        // 将 OnUnitDeath 函数添加到单位的死亡事件委托
        Unit->OnUnitDeathEvent.AddDynamic(this, &ASG_FrontLineManager::OnUnitDeath);
    }
}

/**
 * @brief 解绑单位死亡事件
 * @param Unit 要解绑的单位
 * @details 从单位的死亡委托中移除 OnUnitDeath 函数
 */
void ASG_FrontLineManager::UnbindUnitDeathEvent(ASG_UnitsBase* Unit)
{
    if (Unit)
    {
        // 从单位的死亡事件委托中移除 OnUnitDeath 函数
        Unit->OnUnitDeathEvent.RemoveDynamic(this, &ASG_FrontLineManager::OnUnitDeath);
    }
}

/**
 * @brief 获取前线管理器单例
 * @param WorldContextObject 世界上下文对象
 * @return 前线管理器实例指针，如果不存在则返回 nullptr
 * @details 
 * 功能说明：
 * - 使用静态缓存优化查询性能
 * - 首次查询时遍历场景查找
 * - 后续查询直接返回缓存的实例
 * 
 * 注意事项：
 * - 场景中只应存在一个前线管理器实例
 * - 使用弱指针避免内存泄漏
 */
ASG_FrontLineManager* ASG_FrontLineManager::GetFrontLineManager(UObject* WorldContextObject)
{
    // 检查世界上下文对象是否有效
    if (!WorldContextObject)
    {
        return nullptr;
    }
    
    // 获取世界对象
    UWorld* World = WorldContextObject->GetWorld();
    if (!World)
    {
        return nullptr;
    }
    
    // 静态缓存，存储前线管理器实例
    // 使用弱指针避免内存泄漏
    static TWeakObjectPtr<ASG_FrontLineManager> CachedManager;
    
    // 如果缓存有效，直接返回
    if (CachedManager.IsValid())
    {
        return CachedManager.Get();
    }
    
    // 缓存无效，需要重新查找
    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsOfClass(World, ASG_FrontLineManager::StaticClass(), FoundActors);
    
    // 如果找到了前线管理器
    if (FoundActors.Num() > 0)
    {
        // 转换为前线管理器类型
        ASG_FrontLineManager* Manager = Cast<ASG_FrontLineManager>(FoundActors[0]);
        // 更新缓存
        CachedManager = Manager;
        return Manager;
    }
    
    // 没有找到前线管理器
    return nullptr;
}
