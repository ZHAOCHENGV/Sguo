// 📄 文件：Source/Sguo/Private/Actors/SG_Projectile.cpp

#include "Actors/SG_Projectile.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Units/SG_UnitsBase.h"
#include "Buildings/SG_MainCityBase.h"
#include "Debug/SG_LogCategories.h"
#include "GameplayEffect.h"
#include "GameplayCueManager.h"
#include "DrawDebugHelpers.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"

// ✨ 新增 - 默认胶囊体尺寸常量命名空间
/**
 * @brief 投射物默认配置
 * @details 包含构造函数中使用的默认值
 */
namespace ProjectileDefaults
{
    /** 默认胶囊体半径（厘米） */
    constexpr float CapsuleRadius = 10.0f;
    
    /** 默认胶囊体半高（厘米） */
    constexpr float CapsuleHalfHeight = 30.0f;
}

/**
 * @brief 构造函数
 * @details 
 * 功能说明：
 * - 创建并配置所有组件
 * - 设置碰撞响应
 * - 绑定碰撞事件
 * 
 * 详细流程：
 * 1. 启用 Tick
 * 2. 创建场景根组件
 * 3. 创建胶囊体碰撞组件并配置
 * 4. 创建网格体组件
 * 5. 启用网络复制
 */
ASG_Projectile::ASG_Projectile()
{
    // 启用 Tick 以便每帧更新飞行位置
    PrimaryActorTick.bCanEverTick = true;

    // ========== 创建场景根组件 ==========
    // 作为根组件，允许其他组件自由调整位置和旋转
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    // 设置为根组件
    RootComponent = SceneRoot;

    // ========== 创建胶囊体碰撞组件 ==========
    CollisionCapsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CollisionCapsule"));
    // 附加到根组件，不作为根组件，可自由调整方向
    CollisionCapsule->SetupAttachment(RootComponent);
    
    // 🔧 修改 - 使用常量设置默认胶囊体尺寸（用户可在蓝图或实例中修改组件属性）
    CollisionCapsule->SetCapsuleRadius(ProjectileDefaults::CapsuleRadius);
    CollisionCapsule->SetCapsuleHalfHeight(ProjectileDefaults::CapsuleHalfHeight);
    // 设置碰撞体旋转偏移
    CollisionCapsule->SetRelativeRotation(CollisionRotationOffset);
    
    // 碰撞设置 - 仅查询，不进行物理模拟
    CollisionCapsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    // 设置为世界动态对象
    CollisionCapsule->SetCollisionObjectType(ECC_WorldDynamic);
    // 默认忽略所有通道
    CollisionCapsule->SetCollisionResponseToAllChannels(ECR_Ignore);
    // 与 Pawn 重叠（用于检测单位）
    CollisionCapsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    // 与世界静态物体阻挡（用于检测地面）
    CollisionCapsule->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
    // 与世界动态物体重叠（用于检测其他动态对象）
    CollisionCapsule->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
    // 确保能检测到 Overlap 事件
    CollisionCapsule->SetGenerateOverlapEvents(true);
    
    // 绑定碰撞事件
    CollisionCapsule->OnComponentBeginOverlap.AddDynamic(this, &ASG_Projectile::OnCapsuleOverlap);
    CollisionCapsule->OnComponentHit.AddDynamic(this, &ASG_Projectile::OnCapsuleHit);

    // ========== 创建网格体组件 ==========
    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
    // 附加到根组件
    MeshComponent->SetupAttachment(RootComponent);
    // 网格体不参与碰撞
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // 启用网络复制
    bReplicates = true;
}

/**
 * @brief BeginPlay 生命周期函数
 * @details 
 * 功能说明：
 * - 设置生存时间
 * - 应用碰撞体旋转偏移
 * - 设置延迟启用碰撞
 * - 激活飞行特效
 * 
 * 详细流程：
 * 1. 调用父类 BeginPlay
 * 2. 设置 Actor 生存时间
 * 3. 应用碰撞体旋转偏移
 * 4. 初始时禁用碰撞
 * 5. 设置延迟启用碰撞的定时器
 * 6. 激活飞行 GameplayCue
 * 7. 输出调试日志
 */
void ASG_Projectile::BeginPlay()
{
    // 调用父类实现
    Super::BeginPlay();

    // 设置生存时间
    SetLifeSpan(LifeSpan);

    // 🔧 修改 - 只应用旋转偏移，碰撞尺寸使用组件自身设置
    if (CollisionCapsule)
    {
        // 应用碰撞体旋转偏移
        CollisionCapsule->SetRelativeRotation(CollisionRotationOffset);
        
        // 初始时禁用碰撞，防止在友方建筑内部生成时立即碰撞
        CollisionCapsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        
        // 输出胶囊体配置信息（从组件读取实际尺寸）
        UE_LOG(LogSGGameplay, Verbose, TEXT("投射物 %s：碰撞胶囊体配置 - 半径:%.1f 半高:%.1f，%.2f 秒后启用碰撞"), 
            *GetName(), 
            CollisionCapsule->GetScaledCapsuleRadius(),
            CollisionCapsule->GetScaledCapsuleHalfHeight(),
            CollisionEnableDelay);
    }

    // 设置延迟启用碰撞的定时器
    if (CollisionEnableDelay > 0.0f)
    {
        // 延迟启用碰撞
        GetWorldTimerManager().SetTimer(
            CollisionEnableTimerHandle,
            this,
            &ASG_Projectile::EnableCollision,
            CollisionEnableDelay,
            false  // 不循环
        );
    }
    else
    {
        // 如果延迟为 0，立即启用
        EnableCollision();
    }

    // 激活飞行 GameplayCue（拖尾特效）
    ActivateTrailGameplayCue();

    // 输出调试日志
    UE_LOG(LogSGGameplay, Verbose, TEXT("投射物生成：%s"), *GetName());
    UE_LOG(LogSGGameplay, Verbose, TEXT("  飞行模式：%s"), 
        FlightMode == ESGProjectileFlightMode::Linear ? TEXT("直线") :
        FlightMode == ESGProjectileFlightMode::Parabolic ? TEXT("抛物线") : TEXT("归航"));
    UE_LOG(LogSGGameplay, Verbose, TEXT("  目标模式：%s"),
        TargetMode == ESGProjectileTargetMode::TargetActor ? TEXT("目标Actor") :
        TargetMode == ESGProjectileTargetMode::TargetLocation ? TEXT("指定位置") :
        TargetMode == ESGProjectileTargetMode::AreaCenter ? TEXT("区域中心") :
        TargetMode == ESGProjectileTargetMode::AreaRandom ? TEXT("区域随机点") : TEXT("目标周围随机点"));
    UE_LOG(LogSGGameplay, Verbose, TEXT("  飞行速度：%.1f"), FlightSpeed);
    UE_LOG(LogSGGameplay, Verbose, TEXT("  弧度高度：%.1f"), ArcHeight);
}

/**
 * @brief EndPlay 生命周期函数
 * @param EndPlayReason 结束原因
 * @details 
 * 功能说明：
 * - 清理碰撞启用定时器
 * - 移除飞行特效
 * - 执行销毁特效
 * - 广播销毁事件
 */
void ASG_Projectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 清理碰撞启用定时器
    if (GetWorldTimerManager().IsTimerActive(CollisionEnableTimerHandle))
    {
        GetWorldTimerManager().ClearTimer(CollisionEnableTimerHandle);
    }

    // 移除飞行 GameplayCue（拖尾特效）
    RemoveTrailGameplayCue();
    
    // 执行销毁 GameplayCue
    ExecuteDestroyGameplayCue();
    
    // 调用蓝图事件
    K2_OnProjectileDestroyed(GetActorLocation());
    
    // 构建销毁信息
    FSGProjectileHitInfo DestroyInfo;
    DestroyInfo.HitLocation = GetActorLocation();
    DestroyInfo.ProjectileDirection = CurrentVelocity.GetSafeNormal();
    DestroyInfo.ProjectileSpeed = CurrentVelocity.Size();
    
    // 广播销毁事件
    OnProjectileDestroyed.Broadcast(DestroyInfo);

    // 调用父类实现
    Super::EndPlay(EndPlayReason);
}

/**
 * @brief Tick 函数
 * @param DeltaTime 帧间隔时间
 * @details 
 * 功能说明：
 * - 根据飞行模式更新位置
 * - 更新投射物旋转
 * - 检查目标有效性（抛物线模式）
 * - 绘制调试信息
 */
void ASG_Projectile::Tick(float DeltaTime)
{
    // 调用父类实现
    Super::Tick(DeltaTime);

    // 未初始化则不处理
    if (!bIsInitialized)
    {
        return;
    }

    // 已落地则不再更新位置
    if (bHasLanded)
    {
        return;
    }

    // 根据飞行模式更新位置
    switch (FlightMode)
    {
    case ESGProjectileFlightMode::Linear:
        // 直线飞行
        UpdateLinearFlight(DeltaTime);
        break;

    case ESGProjectileFlightMode::Parabolic:
        // 抛物线飞行 - 检查目标是否仍然有效
        if (TargetMode == ESGProjectileTargetMode::TargetActor && !bTargetLost && !IsTargetValid())
        {
            // 目标丢失，切换到地面落点模式
            HandleTargetLost();
        }
        UpdateParabolicFlight(DeltaTime);
        break;

    case ESGProjectileFlightMode::Homing:
        // 归航飞行
        UpdateHomingFlight(DeltaTime);
        break;
    }

    // 更新旋转（朝向速度方向）
    UpdateRotation();

#if WITH_EDITOR
    // ========== 调试绘制 ==========
    if (bDrawDebugTrajectory)
    {
        // 绘制速度方向（红色箭头）
        DrawDebugLine(GetWorld(), GetActorLocation(), GetActorLocation() + CurrentVelocity.GetSafeNormal() * 100.0f, FColor::Red, false, -1.0f, 0, 2.0f);
        
        // 绘制抛物线轨迹（仅抛物线模式）
        if (FlightMode == ESGProjectileFlightMode::Parabolic)
        {
            // 按 5% 的步进绘制轨迹线段
            for (float t = 0.0f; t < 1.0f; t += 0.05f)
            {
                FVector P1, P2;
                if (bFlyToGround)
                {
                    // 飞向地面模式
                    P1 = CalculateParabolicPositionToGround(t);
                    P2 = CalculateParabolicPositionToGround(t + 0.05f);
                }
                else
                {
                    // 飞向目标模式
                    P1 = CalculateParabolicPosition(t);
                    P2 = CalculateParabolicPosition(t + 0.05f);
                }
                // 绘制绿色轨迹线
                DrawDebugLine(GetWorld(), P1, P2, FColor::Green, false, 0.1f, 0, 1.0f);
            }
        }
    }

    if (bDrawDebugGroundImpact)
    {
        // 绘制目标位置（黄色球）
        DrawDebugSphere(GetWorld(), TargetLocation, 20.0f, 8, FColor::Yellow, false, -1.0f, 0, 2.0f);
        // 绘制地面落点（橙色球）
        DrawDebugSphere(GetWorld(), GroundImpactLocation, 30.0f, 12, FColor::Orange, false, -1.0f, 0, 2.0f);
    }

    // 绘制区域范围
    if (bDrawDebugArea && (TargetMode == ESGProjectileTargetMode::AreaCenter || 
        TargetMode == ESGProjectileTargetMode::AreaRandom || 
        TargetMode == ESGProjectileTargetMode::TargetAreaRandom))
    {
        switch (AreaShape)
        {
        case ESGProjectileAreaShape::Circle:
            // 绘制圆形区域（青色）
            DrawDebugCircle(GetWorld(), AreaCenterLocation, AreaRadius, 32, FColor::Cyan, false, -1.0f, 0, 2.0f, FVector::ForwardVector, FVector::RightVector, false);
            // 绘制内圆（蓝色）
            if (AreaInnerRadius > 0.0f)
            {
                DrawDebugCircle(GetWorld(), AreaCenterLocation, AreaInnerRadius, 32, FColor::Blue, false, -1.0f, 0, 2.0f, FVector::ForwardVector, FVector::RightVector, false);
            }
            break;

        case ESGProjectileAreaShape::Rectangle:
            {
                // 计算矩形四个角
                FVector Forward = AreaRotation.RotateVector(FVector::ForwardVector);
                FVector Right = AreaRotation.RotateVector(FVector::RightVector);
                FVector HalfExtent = FVector(AreaSize.X * 0.5f, AreaSize.Y * 0.5f, 0.0f);
                
                FVector Corners[4];
                Corners[0] = AreaCenterLocation + Forward * HalfExtent.X + Right * HalfExtent.Y;
                Corners[1] = AreaCenterLocation + Forward * HalfExtent.X - Right * HalfExtent.Y;
                Corners[2] = AreaCenterLocation - Forward * HalfExtent.X - Right * HalfExtent.Y;
                Corners[3] = AreaCenterLocation - Forward * HalfExtent.X + Right * HalfExtent.Y;
                
                // 绘制矩形边（青色）
                for (int32 i = 0; i < 4; ++i)
                {
                    DrawDebugLine(GetWorld(), Corners[i], Corners[(i + 1) % 4], FColor::Cyan, false, -1.0f, 0, 2.0f);
                }
            }
            break;

        case ESGProjectileAreaShape::Sector:
            {
                // 获取扇形方向
                FVector Forward = AreaRotation.RotateVector(FVector::ForwardVector);
                
                // 绘制两条边（从中心到边缘）
                FVector LeftEdge = Forward.RotateAngleAxis(-SectorAngle * 0.5f, FVector::UpVector) * AreaRadius;
                FVector RightEdge = Forward.RotateAngleAxis(SectorAngle * 0.5f, FVector::UpVector) * AreaRadius;
                
                DrawDebugLine(GetWorld(), AreaCenterLocation, AreaCenterLocation + LeftEdge, FColor::Cyan, false, -1.0f, 0, 2.0f);
                DrawDebugLine(GetWorld(), AreaCenterLocation, AreaCenterLocation + RightEdge, FColor::Cyan, false, -1.0f, 0, 2.0f);
                
                // 绘制弧线
                int32 NumSegments = FMath::Max(8, FMath::CeilToInt(SectorAngle / 10.0f));
                float AngleStep = SectorAngle / NumSegments;
                for (int32 i = 0; i < NumSegments; ++i)
                {
                    float Angle1 = -SectorAngle * 0.5f + AngleStep * i;
                    float Angle2 = -SectorAngle * 0.5f + AngleStep * (i + 1);
                    FVector P1 = AreaCenterLocation + Forward.RotateAngleAxis(Angle1, FVector::UpVector) * AreaRadius;
                    FVector P2 = AreaCenterLocation + Forward.RotateAngleAxis(Angle2, FVector::UpVector) * AreaRadius;
                    DrawDebugLine(GetWorld(), P1, P2, FColor::Cyan, false, -1.0f, 0, 2.0f);
                }
            }
            break;
        }
    }
#endif
}

// ==================== ✨ 新增 - 胶囊体尺寸获取函数 ====================

/**
 * @brief 获取碰撞胶囊体的半径
 * @return 胶囊体半径，如果组件无效返回 0
 * @details 直接从 CollisionCapsule 组件读取缩放后的实际半径
 */
float ASG_Projectile::GetCapsuleRadius() const
{
    // 检查组件有效性
    if (CollisionCapsule)
    {
        // 返回缩放后的实际半径
        return CollisionCapsule->GetScaledCapsuleRadius();
    }
    // 组件无效返回 0
    return 0.0f;
}

/**
 * @brief 获取碰撞胶囊体的半高
 * @return 胶囊体半高，如果组件无效返回 0
 * @details 直接从 CollisionCapsule 组件读取缩放后的实际半高
 */
float ASG_Projectile::GetCapsuleHalfHeight() const
{
    // 检查组件有效性
    if (CollisionCapsule)
    {
        // 返回缩放后的实际半高
        return CollisionCapsule->GetScaledCapsuleHalfHeight();
    }
    // 组件无效返回 0
    return 0.0f;
}

// ==================== 初始化函数 ====================

/**
 * @brief 初始化投射物（目标为 Actor）
 * @param InInstigatorASC 攻击者 ASC
 * @param InFactionTag 攻击者阵营
 * @param InTarget 目标 Actor
 * @param InArcHeight 弧度高度（覆盖默认值，-1 表示使用默认）
 * @details 
 * 功能说明：
 * - 根据 TargetMode 决定目标位置
 * - TargetActor: 飞向目标中心
 * - TargetAreaRandom: 飞向目标周围随机点
 * 
 * 详细流程：
 * 1. 保存攻击者信息
 * 2. 设置忽略友方碰撞
 * 3. 重置状态标记
 * 4. 记录起始位置
 * 5. 根据目标模式计算目标位置
 * 6. 计算地面落点
 * 7. 初始化飞行参数
 */
void ASG_Projectile::InitializeProjectile(
    UAbilitySystemComponent* InInstigatorASC,
    FGameplayTag InFactionTag,
    AActor* InTarget,
    float InArcHeight
)
{
    // 保存攻击者 ASC
    InstigatorASC = InInstigatorASC;
    // 保存攻击者阵营
    InstigatorFactionTag = InFactionTag;
    // 保存目标 Actor
    CurrentTarget = InTarget;

    // 设置忽略友方碰撞
    if (CollisionCapsule)
    {
        // 忽略所有者
        if (AActor* OwnerActor = GetOwner())
        {
            CollisionCapsule->IgnoreActorWhenMoving(OwnerActor, true);
        }
        
        // 忽略施放者
        if (APawn* InstigatorPawn = GetInstigator())
        {
            CollisionCapsule->IgnoreActorWhenMoving(InstigatorPawn, true);
        }
        
        // 忽略所有友方主城
        TArray<AActor*> AllMainCities;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASG_MainCityBase::StaticClass(), AllMainCities);
        for (AActor* CityActor : AllMainCities)
        {
            // 转换为主城类
            if (ASG_MainCityBase* City = Cast<ASG_MainCityBase>(CityActor))
            {
                // 检查是否同阵营
                if (City->FactionTag == InstigatorFactionTag)
                {
                    CollisionCapsule->IgnoreActorWhenMoving(City, true);
                    UE_LOG(LogSGGameplay, Verbose, TEXT("  投射物忽略友方主城碰撞：%s"), *City->GetName());
                }
            }
        }
        
        // 忽略所有友方单位
        TArray<AActor*> AllUnits;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASG_UnitsBase::StaticClass(), AllUnits);
        for (AActor* UnitActor : AllUnits)
        {
            // 转换为单位类
            if (ASG_UnitsBase* Unit = Cast<ASG_UnitsBase>(UnitActor))
            {
                // 检查是否同阵营
                if (Unit->FactionTag == InstigatorFactionTag)
                {
                    CollisionCapsule->IgnoreActorWhenMoving(Unit, true);
                }
            }
        }
    }

    // 重置状态标记
    bTargetLost = false;
    bHasLanded = false;
    bFlyToGround = false;
    
    // 记录起始位置
    StartLocation = GetActorLocation();

    // 应用弧度高度覆盖（-1 表示使用默认值）
    if (InArcHeight >= 0.0f)
    {
        ArcHeight = InArcHeight;
    }

    // 根据目标模式计算目标位置
    if (InTarget)
    {
        switch (TargetMode)
        {
        case ESGProjectileTargetMode::TargetActor:
            // 飞向目标中心
            TargetLocation = CalculateTargetLocation(InTarget);
            AreaCenterLocation = InTarget->GetActorLocation();
            AreaRotation = InTarget->GetActorRotation();
            bFlyToGround = false;
            break;

        case ESGProjectileTargetMode::TargetAreaRandom:
            // 飞向目标周围随机点
            AreaCenterLocation = InTarget->GetActorLocation();
            AreaRotation = InTarget->GetActorRotation();
            // 生成随机点
            TargetLocation = GenerateRandomPointInArea(AreaCenterLocation, AreaRotation);
            bFlyToGround = true;
            break;

        default:
            // 其他模式使用目标位置
            TargetLocation = CalculateTargetLocation(InTarget);
            AreaCenterLocation = InTarget->GetActorLocation();
            AreaRotation = InTarget->GetActorRotation();
            bFlyToGround = false;
            break;
        }
    }
    else
    {
        // 如果没有目标，向前飞行 5000 厘米
        TargetLocation = StartLocation + GetActorForwardVector() * 5000.0f;
        AreaCenterLocation = TargetLocation;
        AreaRotation = GetActorRotation();
    }

    // 计算地面落点
    GroundImpactLocation = CalculateGroundImpactLocation(TargetLocation);

    // 计算飞行距离
    if (bFlyToGround)
    {
        // 飞向地面模式 - 使用地面落点计算距离
        TotalFlightDistance = FVector::Dist(StartLocation, GroundImpactLocation);
        TotalFlightDistanceToGround = TotalFlightDistance;
    }
    else
    {
        // 飞向目标模式 - 分别计算两种距离
        TotalFlightDistance = FVector::Dist(StartLocation, TargetLocation);
        TotalFlightDistanceToGround = FVector::Dist(StartLocation, GroundImpactLocation);
    }

    // 初始化速度向量
    FVector FinalTarget = bFlyToGround ? GroundImpactLocation : TargetLocation;
    FVector Direction = (FinalTarget - StartLocation).GetSafeNormal();
    CurrentVelocity = Direction * FlightSpeed;
    
    // 重置飞行进度
    FlightProgress = 0.0f;
    
    // 标记为已初始化
    bIsInitialized = true;

    // 输出日志
    UE_LOG(LogSGGameplay, Log, TEXT("========== 初始化投射物（Actor目标） =========="));
    UE_LOG(LogSGGameplay, Log, TEXT("  目标：%s"), InTarget ? *InTarget->GetName() : TEXT("无"));
    UE_LOG(LogSGGameplay, Log, TEXT("  起点：%s"), *StartLocation.ToString());
    UE_LOG(LogSGGameplay, Log, TEXT("  目标位置：%s"), *TargetLocation.ToString());
    UE_LOG(LogSGGameplay, Log, TEXT("  地面落点：%s"), *GroundImpactLocation.ToString());
    UE_LOG(LogSGGameplay, Log, TEXT("  距离：%.1f"), TotalFlightDistance);
    UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
}

/**
 * @brief 初始化投射物（目标为位置）
 * @param InInstigatorASC 攻击者 ASC
 * @param InFactionTag 攻击者阵营
 * @param InTargetLocation 目标位置
 * @param InArcHeight 弧度高度（覆盖默认值，-1 表示使用默认）
 * @details 
 * 功能说明：
 * - 根据 TargetMode 决定目标位置
 * - TargetLocation: 飞向指定位置
 * - AreaCenter: 飞向区域中心地面
 * - AreaRandom: 飞向区域内随机地面点
 */
void ASG_Projectile::InitializeProjectileToLocation(
    UAbilitySystemComponent* InInstigatorASC,
    FGameplayTag InFactionTag,
    FVector InTargetLocation,
    float InArcHeight
)
{
    // 保存攻击者 ASC
    InstigatorASC = InInstigatorASC;
    // 保存攻击者阵营
    InstigatorFactionTag = InFactionTag;
    // 清空目标 Actor
    CurrentTarget = nullptr;

    // 重置状态标记
    bTargetLost = false;
    bHasLanded = false;
    
    // 记录起始位置
    StartLocation = GetActorLocation();

    // 应用弧度高度覆盖
    if (InArcHeight >= 0.0f)
    {
        ArcHeight = InArcHeight;
    }

    // 设置区域中心和朝向
    AreaCenterLocation = InTargetLocation;
    AreaRotation = GetActorRotation();

    // 根据目标模式计算目标位置
    switch (TargetMode)
    {
    case ESGProjectileTargetMode::TargetLocation:
        // 飞向指定位置（应用偏移）
        TargetLocation = InTargetLocation + (bUseWorldSpaceOffset ? TargetLocationOffset : GetActorRotation().RotateVector(TargetLocationOffset));
        bFlyToGround = false;
        break;

    case ESGProjectileTargetMode::AreaCenter:
        // 飞向区域中心地面
        TargetLocation = InTargetLocation;
        bFlyToGround = true;
        break;

    case ESGProjectileTargetMode::AreaRandom:
        // 飞向区域内随机地面点
        TargetLocation = GenerateRandomPointInArea(InTargetLocation, AreaRotation);
        bFlyToGround = true;
        break;

    default:
        // 默认飞向指定位置
        TargetLocation = InTargetLocation;
        bFlyToGround = false;
        break;
    }

    // 计算地面落点
    GroundImpactLocation = CalculateGroundImpactLocation(TargetLocation);

    // 计算飞行距离
    if (bFlyToGround)
    {
        TotalFlightDistance = FVector::Dist(StartLocation, GroundImpactLocation);
        TotalFlightDistanceToGround = TotalFlightDistance;
    }
    else
    {
        TotalFlightDistance = FVector::Dist(StartLocation, TargetLocation);
        TotalFlightDistanceToGround = FVector::Dist(StartLocation, GroundImpactLocation);
    }

    // 初始化速度向量
    FVector FinalTarget = bFlyToGround ? GroundImpactLocation : TargetLocation;
    FVector Direction = (FinalTarget - StartLocation).GetSafeNormal();
    CurrentVelocity = Direction * FlightSpeed;
    
    // 重置飞行进度
    FlightProgress = 0.0f;
    
    // 标记为已初始化
    bIsInitialized = true;

    // 输出日志
    UE_LOG(LogSGGameplay, Log, TEXT("========== 初始化投射物（位置目标） =========="));
    UE_LOG(LogSGGameplay, Log, TEXT("  起点：%s"), *StartLocation.ToString());
    UE_LOG(LogSGGameplay, Log, TEXT("  目标位置：%s"), *TargetLocation.ToString());
    UE_LOG(LogSGGameplay, Log, TEXT("  地面落点：%s"), *GroundImpactLocation.ToString());
    UE_LOG(LogSGGameplay, Log, TEXT("  距离：%.1f"), TotalFlightDistance);
    UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
}

/**
 * @brief 初始化投射物（目标为区域）
 * @param InInstigatorASC 攻击者 ASC
 * @param InFactionTag 攻击者阵营
 * @param InAreaCenter 区域中心位置
 * @param InAreaRotation 区域朝向
 * @param InArcHeight 弧度高度（覆盖默认值，-1 表示使用默认）
 * @details 
 * 功能说明：
 * - 用于区域攻击
 * - 根据 TargetMode 决定飞向区域中心还是随机点
 * - 区域朝向用于扇形和矩形区域
 */
void ASG_Projectile::InitializeProjectileToArea(
    UAbilitySystemComponent* InInstigatorASC,
    FGameplayTag InFactionTag,
    FVector InAreaCenter,
    FRotator InAreaRotation,
    float InArcHeight
)
{
    // 保存攻击者 ASC
    InstigatorASC = InInstigatorASC;
    // 保存攻击者阵营
    InstigatorFactionTag = InFactionTag;
    // 清空目标 Actor
    CurrentTarget = nullptr;

    // 重置状态标记
    bTargetLost = false;
    bHasLanded = false;
    // 区域模式始终飞向地面
    bFlyToGround = true;
    
    // 记录起始位置
    StartLocation = GetActorLocation();

    // 应用弧度高度覆盖
    if (InArcHeight >= 0.0f)
    {
        ArcHeight = InArcHeight;
    }

    // 设置区域中心和朝向
    AreaCenterLocation = InAreaCenter;
    AreaRotation = InAreaRotation;

    // 根据目标模式计算目标位置
    switch (TargetMode)
    {
    case ESGProjectileTargetMode::AreaCenter:
        // 飞向区域中心
        TargetLocation = InAreaCenter;
        break;

    case ESGProjectileTargetMode::AreaRandom:
    case ESGProjectileTargetMode::TargetAreaRandom:
        // 飞向区域内随机点
        TargetLocation = GenerateRandomPointInArea(InAreaCenter, InAreaRotation);
        break;

    default:
        // 默认飞向区域中心
        TargetLocation = InAreaCenter;
        break;
    }

    // 计算地面落点
    GroundImpactLocation = CalculateGroundImpactLocation(TargetLocation);
    
    // 区域模式使用地面落点计算距离
    TotalFlightDistance = FVector::Dist(StartLocation, GroundImpactLocation);
    TotalFlightDistanceToGround = TotalFlightDistance;

    // 初始化速度向量
    FVector Direction = (GroundImpactLocation - StartLocation).GetSafeNormal();
    CurrentVelocity = Direction * FlightSpeed;
    
    // 重置飞行进度
    FlightProgress = 0.0f;
    
    // 标记为已初始化
    bIsInitialized = true;

    // 输出日志
    UE_LOG(LogSGGameplay, Log, TEXT("========== 初始化投射物（区域目标） =========="));
    UE_LOG(LogSGGameplay, Log, TEXT("  区域形状：%s"),
        AreaShape == ESGProjectileAreaShape::Circle ? TEXT("圆形") :
        AreaShape == ESGProjectileAreaShape::Rectangle ? TEXT("矩形") : TEXT("扇形"));
    UE_LOG(LogSGGameplay, Log, TEXT("  区域中心：%s"), *InAreaCenter.ToString());
    UE_LOG(LogSGGameplay, Log, TEXT("  目标位置：%s"), *TargetLocation.ToString());
    UE_LOG(LogSGGameplay, Log, TEXT("  地面落点：%s"), *GroundImpactLocation.ToString());
    UE_LOG(LogSGGameplay, Log, TEXT("  距离：%.1f"), TotalFlightDistance);
    UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
}

// ==================== 运行时设置函数 ====================

/**
 * @brief 设置飞行速度（运行时）
 * @param NewSpeed 新的飞行速度
 * @details 限制最小速度为 100，并同步更新速度向量
 */
void ASG_Projectile::SetFlightSpeed(float NewSpeed)
{
    // 限制最小速度为 100
    FlightSpeed = FMath::Max(100.0f, NewSpeed);
    
    // 更新当前速度向量的大小（保持方向不变）
    if (!CurrentVelocity.IsNearlyZero())
    {
        CurrentVelocity = CurrentVelocity.GetSafeNormal() * FlightSpeed;
    }
}

/**
 * @brief 设置目标位置偏移（运行时）
 * @param NewOffset 新的偏移向量
 * @param bWorldSpace 是否使用世界空间
 */
void ASG_Projectile::SetTargetLocationOffset(FVector NewOffset, bool bWorldSpace)
{
    TargetLocationOffset = NewOffset;
    bUseWorldSpaceOffset = bWorldSpace;
}

/**
 * @brief 设置区域参数（运行时）
 * @param InShape 区域形状
 * @param InRadius 区域半径（圆形/扇形）
 * @param InInnerRadius 区域内半径
 * @param InSize 区域尺寸（矩形）
 * @param InSectorAngle 扇形角度
 */
void ASG_Projectile::SetAreaParameters(
    ESGProjectileAreaShape InShape,
    float InRadius,
    float InInnerRadius,
    FVector2D InSize,
    float InSectorAngle
)
{
    AreaShape = InShape;
    AreaRadius = InRadius;
    AreaInnerRadius = InInnerRadius;
    AreaSize = InSize;
    SectorAngle = InSectorAngle;
}

// ==================== 飞行逻辑函数 ====================

/**
 * @brief 更新直线飞行
 * @param DeltaTime 帧间隔时间
 * @details 
 * 功能说明：
 * - 直接飞向目标位置
 * - 持续追踪目标（如果有）
 */
void ASG_Projectile::UpdateLinearFlight(float DeltaTime)
{
    // 如果有目标 Actor，动态更新目标位置
    if (CurrentTarget.IsValid() && TargetMode == ESGProjectileTargetMode::TargetActor)
    {
        TargetLocation = CalculateTargetLocation(CurrentTarget.Get());
    }

    // 计算本帧移动距离
    float MoveDistance = FlightSpeed * DeltaTime;
    
    // 获取当前位置
    FVector CurrentLocation = GetActorLocation();
    // 确定最终目标位置
    FVector FinalTarget = bFlyToGround ? GroundImpactLocation : TargetLocation;
    // 计算到目标的向量
    FVector ToTarget = FinalTarget - CurrentLocation;
    
    // 检查是否已到达目标
    if (ToTarget.Size() <= MoveDistance)
    {
        // 已到达目标位置
        SetActorLocation(FinalTarget);
        CurrentVelocity = ToTarget.GetSafeNormal() * FlightSpeed;
        
        // 如果飞向地面，触发落地
        if (bFlyToGround)
        {
            HandleGroundImpact();
        }
    }
    else
    {
        // 继续飞行
        FVector Direction = ToTarget.GetSafeNormal();
        CurrentVelocity = Direction * FlightSpeed;
        SetActorLocation(CurrentLocation + CurrentVelocity * DeltaTime);
    }
}

/**
 * @brief 更新抛物线飞行
 * @param DeltaTime 帧间隔时间
 * @details 
 * 功能说明：
 * - 沿抛物线轨迹飞行
 * - 根据 bFlyToGround 决定飞向目标中心还是地面落点
 */
void ASG_Projectile::UpdateParabolicFlight(float DeltaTime)
{
    // 选择使用的飞行距离
    float EffectiveFlightDistance = bFlyToGround ? TotalFlightDistanceToGround : TotalFlightDistance;

    // 防止除零
    if (EffectiveFlightDistance < KINDA_SMALL_NUMBER)
    {
        HandleGroundImpact();
        return;
    }

    // 计算本帧飞行距离
    float DistanceThisFrame = FlightSpeed * DeltaTime;
    // 更新飞行进度
    FlightProgress += DistanceThisFrame / EffectiveFlightDistance;
    // 限制进度在 [0, 1] 范围内
    FlightProgress = FMath::Clamp(FlightProgress, 0.0f, 1.0f);

    // 计算当前位置
    FVector NewLocation;
    if (bFlyToGround)
    {
        // 飞向地面落点
        NewLocation = CalculateParabolicPositionToGround(FlightProgress);
    }
    else
    {
        // 飞向目标中心
        NewLocation = CalculateParabolicPosition(FlightProgress);
    }
    
    // 计算速度向量（用于旋转）
    FVector PreviousLocation = GetActorLocation();
    if (DeltaTime > KINDA_SMALL_NUMBER)
    {
        CurrentVelocity = (NewLocation - PreviousLocation) / DeltaTime;
    }
    
    // 如果速度过小，使用方向估算
    if (CurrentVelocity.Size() < 1.0f)
    {
        float NextProgress = FMath::Clamp(FlightProgress + 0.01f, 0.0f, 1.0f);
        FVector NextLocation;
        if (bFlyToGround)
        {
            NextLocation = CalculateParabolicPositionToGround(NextProgress);
        }
        else
        {
            NextLocation = CalculateParabolicPosition(NextProgress);
        }
        CurrentVelocity = (NextLocation - NewLocation).GetSafeNormal() * FlightSpeed;
    }

    // 更新位置
    SetActorLocation(NewLocation);

    // 检查是否到达终点
    if (FlightProgress >= 1.0f)
    {
        if (bFlyToGround)
        {
            // 飞向地面模式，触发落地
            HandleGroundImpact();
        }
    }

    // 如果目标还活着且未丢失，动态更新目标位置
    if (!bFlyToGround && !bTargetLost && CurrentTarget.IsValid())
    {
        AActor* Target = CurrentTarget.Get();
        // 计算新的目标位置
        FVector NewTargetLocation = CalculateTargetLocation(Target);
        // 平滑更新目标位置（避免抖动）
        TargetLocation = FMath::VInterpTo(TargetLocation, NewTargetLocation, DeltaTime, 5.0f);
        // 更新总飞行距离
        TotalFlightDistance = FVector::Dist(StartLocation, TargetLocation);
        // 同时更新地面落点
        GroundImpactLocation = CalculateGroundImpactLocation(TargetLocation);
        TotalFlightDistanceToGround = FVector::Dist(StartLocation, GroundImpactLocation);
    }
}

/**
 * @brief 计算抛物线位置（飞向目标中心）
 * @param Progress 飞行进度（0-1）
 * @return 当前位置
 * @details 使用正弦函数计算高度偏移，形成平滑的抛物线
 */
FVector ASG_Projectile::CalculateParabolicPosition(float Progress) const
{
    // 线性插值基础位置
    FVector LinearPosition = FMath::Lerp(StartLocation, TargetLocation, Progress);
    // 计算抛物线高度偏移（使用正弦函数，在中点达到最高）
    float HeightOffset = FMath::Sin(Progress * PI) * ArcHeight;
    // 应用高度偏移
    return LinearPosition + FVector(0.0f, 0.0f, HeightOffset);
}

/**
 * @brief 计算到地面落点的抛物线位置
 * @param Progress 飞行进度（0-1）
 * @return 当前位置
 */
FVector ASG_Projectile::CalculateParabolicPositionToGround(float Progress) const
{
    // 线性插值基础位置（使用地面落点）
    FVector LinearPosition = FMath::Lerp(StartLocation, GroundImpactLocation, Progress);
    // 计算抛物线高度偏移
    float HeightOffset = FMath::Sin(Progress * PI) * ArcHeight;
    // 应用高度偏移
    return LinearPosition + FVector(0.0f, 0.0f, HeightOffset);
}

/**
 * @brief 更新归航飞行
 * @param DeltaTime 帧间隔时间
 * @details 持续追踪目标，按 HomingStrength 转向
 */
void ASG_Projectile::UpdateHomingFlight(float DeltaTime)
{
    // 更新目标位置
    if (CurrentTarget.IsValid() && TargetMode == ESGProjectileTargetMode::TargetActor)
    {
        TargetLocation = CalculateTargetLocation(CurrentTarget.Get());
    }

    // 计算当前方向
    FVector CurrentDirection = CurrentVelocity.GetSafeNormal();
    // 确定最终目标位置
    FVector FinalTarget = bFlyToGround ? GroundImpactLocation : TargetLocation;
    // 计算期望方向
    FVector DesiredDirection = (FinalTarget - GetActorLocation()).GetSafeNormal();

    // 插值转向（按 HomingStrength 每秒转向）
    FVector NewDirection = FMath::VInterpNormalRotationTo(
        CurrentDirection,
        DesiredDirection,
        DeltaTime,
        HomingStrength
    );

    // 更新速度向量
    CurrentVelocity = NewDirection * FlightSpeed;
    // 更新位置
    SetActorLocation(GetActorLocation() + CurrentVelocity * DeltaTime);
}

/**
 * @brief 更新旋转（朝向速度方向）
 * @details 使投射物始终朝向飞行方向
 */
void ASG_Projectile::UpdateRotation()
{
    // 确保速度向量不为零
    if (!CurrentVelocity.IsNearlyZero())
    {
        // 计算速度方向的旋转
        FRotator NewRotation = CurrentVelocity.Rotation();
        // 应用旋转
        SetActorRotation(NewRotation);
    }
}

// ==================== 目标位置计算函数 ====================

/**
 * @brief 计算目标位置（应用偏移）
 * @param InTarget 目标 Actor
 * @return 计算后的目标位置
 * @details 
 * 功能说明：
 * - 获取目标基础位置
 * - 对于单位，瞄准胶囊体中心
 * - 对于主城，瞄准检测盒中心
 * - 应用位置偏移
 */
FVector ASG_Projectile::CalculateTargetLocation(AActor* InTarget) const
{
    // 如果目标无效，向前飞行
    if (!InTarget)
    {
        return GetActorLocation() + GetActorForwardVector() * 5000.0f;
    }

    // 获取目标基础位置
    FVector BaseLocation = InTarget->GetActorLocation();

    // 对于单位，瞄准胶囊体中心（偏上一些）
    if (ASG_UnitsBase* TargetUnit = Cast<ASG_UnitsBase>(InTarget))
    {
        if (UCapsuleComponent* Capsule = TargetUnit->GetCapsuleComponent())
        {
            // 瞄准胶囊体中心偏上位置
            BaseLocation.Z += Capsule->GetScaledCapsuleHalfHeight() * 0.5f;
        }
    }
    // 对于主城，瞄准检测盒中心
    else if (ASG_MainCityBase* MainCity = Cast<ASG_MainCityBase>(InTarget))
    {
        if (UBoxComponent* DetectionBox = MainCity->GetAttackDetectionBox())
        {
            BaseLocation = DetectionBox->GetComponentLocation();
        }
    }

    // 应用位置偏移
    FVector FinalLocation = BaseLocation;
    if (!TargetLocationOffset.IsNearlyZero())
    {
        if (bUseWorldSpaceOffset)
        {
            // 世界空间偏移
            FinalLocation += TargetLocationOffset;
        }
        else
        {
            // 相对于目标朝向的偏移
            FRotator TargetRotation = InTarget->GetActorRotation();
            FinalLocation += TargetRotation.RotateVector(TargetLocationOffset);
        }
    }

    return FinalLocation;
}

/**
 * @brief 计算地面落点位置
 * @param InTargetLocation 目标位置
 * @return 地面落点位置
 * @details 从目标位置向下进行射线检测，找到地面位置
 */
FVector ASG_Projectile::CalculateGroundImpactLocation(const FVector& InTargetLocation) const
{
    // 射线检测起点（目标位置上方 100 厘米）
    FVector TraceStart = InTargetLocation + FVector(0.0f, 0.0f, 100.0f);
    // 射线检测终点（向下检测）
    FVector TraceEnd = InTargetLocation - FVector(0.0f, 0.0f, GroundTraceDistance);

    // 设置查询参数
    FCollisionQueryParams QueryParams;
    // 忽略自己
    QueryParams.AddIgnoredActor(this);
    // 忽略目标
    if (CurrentTarget.IsValid())
    {
        QueryParams.AddIgnoredActor(CurrentTarget.Get());
    }

    // 执行射线检测
    FHitResult HitResult;
    bool bHit = GetWorld()->LineTraceSingleByChannel(
        HitResult,
        TraceStart,
        TraceEnd,
        GroundTraceChannel,
        QueryParams
    );

    if (bHit)
    {
        // 检测到地面，返回击中点
        return HitResult.ImpactPoint;
    }
    else
    {
        // 未检测到地面，使用目标位置的 XY 坐标，Z 轴使用起点 Z 坐标
        return FVector(InTargetLocation.X, InTargetLocation.Y, StartLocation.Z);
    }
}

/**
 * @brief 检查目标是否仍然有效
 * @return 目标是否有效
 * @details 检查目标是否存活（未死亡）
 */
bool ASG_Projectile::IsTargetValid() const
{
    // 检查弱指针是否有效
    if (!CurrentTarget.IsValid())
    {
        return false;
    }

    AActor* Target = CurrentTarget.Get();

    // 检查是否是单位
    if (ASG_UnitsBase* TargetUnit = Cast<ASG_UnitsBase>(Target))
    {
        // 单位未死亡则有效
        return !TargetUnit->bIsDead;
    }

    // 检查是否是主城
    if (ASG_MainCityBase* TargetMainCity = Cast<ASG_MainCityBase>(Target))
    {
        // 主城存活则有效
        return TargetMainCity->IsAlive();
    }

    // 其他类型默认有效
    return true;
}

/**
 * @brief 处理目标丢失（切换到地面落点模式）
 * @details 当目标死亡或消失时，切换为飞向地面落点
 */
void ASG_Projectile::HandleTargetLost()
{
    // 标记目标丢失
    bTargetLost = true;
    // 切换到飞向地面模式
    bFlyToGround = true;

    // 重新计算地面落点
    GroundImpactLocation = CalculateGroundImpactLocation(TargetLocation);
    TotalFlightDistanceToGround = FVector::Dist(StartLocation, GroundImpactLocation);

    UE_LOG(LogSGGameplay, Log, TEXT("投射物目标丢失，切换到地面落点模式"));
    UE_LOG(LogSGGameplay, Log, TEXT("  当前位置：%s"), *GetActorLocation().ToString());
    UE_LOG(LogSGGameplay, Log, TEXT("  地面落点：%s"), *GroundImpactLocation.ToString());
}

/**
 * @brief 处理投射物落地
 * @details 执行落地特效、广播事件、销毁投射物
 */
void ASG_Projectile::HandleGroundImpact()
{
    // 防止重复处理
    if (bHasLanded)
    {
        return;
    }

    // 标记已落地
    bHasLanded = true;
    // 防止单位走上去被卡住，或投射物被二次检测
    if (CollisionCapsule)
    {
        CollisionCapsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
    UE_LOG(LogSGGameplay, Log, TEXT("投射物落地：%s"), *GroundImpactLocation.ToString());

    // 执行落地 GameplayCue
    ExecuteGroundImpactGameplayCue(GroundImpactLocation);

    // 构建落地信息
    FSGProjectileHitInfo GroundHitInfo;
    GroundHitInfo.HitLocation = GroundImpactLocation;
    GroundHitInfo.HitNormal = FVector::UpVector;
    GroundHitInfo.ProjectileDirection = CurrentVelocity.GetSafeNormal();
    GroundHitInfo.ProjectileSpeed = CurrentVelocity.Size();

    // 调用蓝图事件
    K2_OnGroundImpact(GroundImpactLocation);
    // 广播落地事件
    OnProjectileGroundImpact.Broadcast(GroundHitInfo);

    // 延迟3秒销毁投射物
    SetLifeSpan(3.0f);
}

// ==================== 区域随机点计算函数 ====================

/**
 * @brief 在区域内生成随机点
 * @param InCenter 区域中心
 * @param InRotation 区域朝向
 * @return 随机点位置（世界坐标）
 * @details 根据区域形状调用对应的生成函数
 */
FVector ASG_Projectile::GenerateRandomPointInArea(const FVector& InCenter, const FRotator& InRotation) const
{
    switch (AreaShape)
    {
    case ESGProjectileAreaShape::Circle:
        return GenerateRandomPointInCircle(InCenter);

    case ESGProjectileAreaShape::Rectangle:
        return GenerateRandomPointInRectangle(InCenter, InRotation);

    case ESGProjectileAreaShape::Sector:
        return GenerateRandomPointInSector(InCenter, InRotation);

    default:
        return InCenter;
    }
}

/**
 * @brief 在圆形区域内生成随机点
 * @param InCenter 区域中心
 * @return 随机点位置
 * @details 
 * 功能说明：
 * - 支持内半径（生成环形区域）
 * - 使用均匀分布确保点分布均匀
 */
FVector ASG_Projectile::GenerateRandomPointInCircle(const FVector& InCenter) const
{
    // 计算有效半径范围
    float MinRadius = AreaInnerRadius;
    float MaxRadius = AreaRadius;

    // 确保 MinRadius < MaxRadius
    if (MinRadius >= MaxRadius)
    {
        MinRadius = 0.0f;
    }

    // 使用均匀分布生成随机半径
    // 为了确保点在圆内均匀分布，需要对半径进行平方根变换
    float RandomValue = FMath::FRand();
    float MinRadiusSq = MinRadius * MinRadius;
    float MaxRadiusSq = MaxRadius * MaxRadius;
    float RandomRadiusSq = FMath::Lerp(MinRadiusSq, MaxRadiusSq, RandomValue);
    float RandomRadius = FMath::Sqrt(RandomRadiusSq);

    // 生成随机角度（0-360度）
    float RandomAngle = FMath::FRandRange(0.0f, 360.0f);

    // 计算偏移向量
    FVector Offset;
    Offset.X = RandomRadius * FMath::Cos(FMath::DegreesToRadians(RandomAngle));
    Offset.Y = RandomRadius * FMath::Sin(FMath::DegreesToRadians(RandomAngle));
    Offset.Z = 0.0f;

    return InCenter + Offset;
}

/**
 * @brief 在矩形区域内生成随机点
 * @param InCenter 区域中心
 * @param InRotation 区域朝向
 * @return 随机点位置
 */
FVector ASG_Projectile::GenerateRandomPointInRectangle(const FVector& InCenter, const FRotator& InRotation) const
{
    // 生成局部坐标的随机点
    float RandomX = FMath::FRandRange(-AreaSize.X * 0.5f, AreaSize.X * 0.5f);
    float RandomY = FMath::FRandRange(-AreaSize.Y * 0.5f, AreaSize.Y * 0.5f);

    // 创建局部偏移
    FVector LocalOffset(RandomX, RandomY, 0.0f);
    // 旋转到世界坐标
    FVector WorldOffset = InRotation.RotateVector(LocalOffset);

    return InCenter + WorldOffset;
}

/**
 * @brief 在扇形区域内生成随机点
 * @param InCenter 区域中心
 * @param InRotation 区域朝向
 * @return 随机点位置
 * @details 
 * 功能说明：
 * - 支持内半径（生成扇形环区域）
 * - 支持扇形朝向偏移
 */
FVector ASG_Projectile::GenerateRandomPointInSector(const FVector& InCenter, const FRotator& InRotation) const
{
    // 计算有效半径范围
    float MinRadius = AreaInnerRadius;
    float MaxRadius = AreaRadius;

    if (MinRadius >= MaxRadius)
    {
        MinRadius = 0.0f;
    }

    // 使用均匀分布生成随机半径
    float RandomValue = FMath::FRand();
    float MinRadiusSq = MinRadius * MinRadius;
    float MaxRadiusSq = MaxRadius * MaxRadius;
    float RandomRadiusSq = FMath::Lerp(MinRadiusSq, MaxRadiusSq, RandomValue);
    float RandomRadius = FMath::Sqrt(RandomRadiusSq);

    // 生成扇形范围内的随机角度
    float HalfAngle = SectorAngle * 0.5f;
    float RandomAngle = FMath::FRandRange(-HalfAngle, HalfAngle);
    // 应用扇形朝向偏移
    RandomAngle += SectorDirectionOffset;

    // 获取区域朝向的前向量
    FVector Forward = InRotation.RotateVector(FVector::ForwardVector);
    // 计算最终方向
    FVector Direction = Forward.RotateAngleAxis(RandomAngle, FVector::UpVector);

    // 计算偏移
    FVector Offset = Direction * RandomRadius;
    Offset.Z = 0.0f;

    return InCenter + Offset;
}

// ==================== 碰撞处理函数 ====================

/**
 * @brief 胶囊体 Overlap 事件回调
 * @details 当其他 Actor 与胶囊体重叠时触发
 */
void ASG_Projectile::OnCapsuleOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult
)
{
    // 调用统一的碰撞处理函数
    HandleProjectileImpact(OtherActor, SweepResult);
}

/**
 * @brief 胶囊体 Hit 事件回调
 * @details 当胶囊体与其他物体碰撞时触发
 */
void ASG_Projectile::OnCapsuleHit(
    UPrimitiveComponent* HitComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    FVector NormalImpulse,
    const FHitResult& Hit
)
{
    // 调用统一的碰撞处理函数
    HandleProjectileImpact(OtherActor, Hit);
}

/**
 * @brief 处理投射物碰撞
 * @param OtherActor 碰撞的 Actor
 * @param Hit 碰撞结果
 * @details 
 * 功能说明：
 * - 过滤友方单位和建筑
 * - 对敌方目标应用伤害
 * - 处理穿透逻辑
 * - 处理地面碰撞
 */
void ASG_Projectile::HandleProjectileImpact(AActor* OtherActor, const FHitResult& Hit)
{
    // 如果未初始化，忽略所有碰撞
    if (!bIsInitialized)
    {
        UE_LOG(LogSGGameplay, Verbose, TEXT("投射物未初始化，忽略碰撞：%s"), 
            OtherActor ? *OtherActor->GetName() : TEXT("None"));
        return;
    }
    
    UE_LOG(LogSGGameplay, Verbose, TEXT("投射物碰撞检测：%s"), OtherActor ? *OtherActor->GetName() : TEXT("None"));

    // ========== 基础检查 ==========
    
    // 忽略空 Actor
    if (!OtherActor)
    {
        return;
    }
    
    // 忽略自己
    if (OtherActor == this)
    {
        return;
    }
    
    // 忽略所有者和施放者
    if (OtherActor == GetOwner() || OtherActor == GetInstigator())
    {
        UE_LOG(LogSGGameplay, Verbose, TEXT("  忽略所有者/施放者"));
        return;
    }

    // ========== 友方过滤 ==========

    // 检查是否是单位
    ASG_UnitsBase* OtherUnit = Cast<ASG_UnitsBase>(OtherActor);
    if (OtherUnit)
    {
        // 忽略友方单位
        if (OtherUnit->FactionTag == InstigatorFactionTag)
        {
            UE_LOG(LogSGGameplay, Verbose, TEXT("  忽略友方单位：%s"), *OtherActor->GetName());
            return;
        }
    }

    // 检查是否是主城
    ASG_MainCityBase* OtherMainCity = Cast<ASG_MainCityBase>(OtherActor);
    if (OtherMainCity)
    {
        // 忽略友方主城
        if (OtherMainCity->FactionTag == InstigatorFactionTag)
        {
            UE_LOG(LogSGGameplay, Verbose, TEXT("  忽略友方主城：%s"), *OtherActor->GetName());
            return;
        }
    }
    
    // 检查碰撞组件是否属于友方主城
    UPrimitiveComponent* HitComponent = Hit.GetComponent();
    if (HitComponent)
    {
        AActor* ComponentOwner = HitComponent->GetOwner();
        if (ComponentOwner)
        {
            ASG_MainCityBase* OwnerCity = Cast<ASG_MainCityBase>(ComponentOwner);
            if (OwnerCity && OwnerCity->FactionTag == InstigatorFactionTag)
            {
                UE_LOG(LogSGGameplay, Verbose, TEXT("  忽略友方主城组件：%s"), *ComponentOwner->GetName());
                return;
            }
        }
    }

    // ========== 处理敌方主城 ==========
    
    if (OtherMainCity)
    {
        // 检查是否已击中过
        if (HitActors.Contains(OtherActor))
        {
            return;
        }

        // 检查主城是否存活
        if (!OtherMainCity->IsAlive())
        {
            Destroy();
            return;
        }

        UE_LOG(LogSGGameplay, Log, TEXT("  🏰 击中敌方主城：%s"), *OtherMainCity->GetName());

        // 构建击中信息
        FSGProjectileHitInfo HitInfo;
        HitInfo.HitActor = OtherActor;
        HitInfo.HitLocation = Hit.ImpactPoint.IsNearlyZero() ? OtherActor->GetActorLocation() : FVector(Hit.ImpactPoint);
        HitInfo.HitNormal = Hit.ImpactNormal.IsNearlyZero() ? -GetActorForwardVector() : FVector(Hit.ImpactNormal);
        HitInfo.ProjectileDirection = CurrentVelocity.GetSafeNormal();
        HitInfo.ProjectileSpeed = CurrentVelocity.Size();

        // 应用伤害
        ApplyDamageToTarget(OtherActor);
        // 记录已击中的 Actor
        HitActors.Add(OtherActor);

        // 执行击中特效和事件
        ExecuteHitGameplayCue(HitInfo);
        K2_OnHitTarget(HitInfo);
        OnProjectileHitTarget.Broadcast(HitInfo);

        // 检查是否应该销毁（非穿透或达到穿透上限）
        if (!bPenetrate || (MaxPenetrateCount > 0 && HitActors.Num() >= MaxPenetrateCount))
        {
            Destroy();
        }
        return;
    }

    // ========== 处理敌方单位 ==========
    
    if (OtherUnit)
    {
        // 检查是否已击中过
        if (HitActors.Contains(OtherActor))
        {
            return;
        }

        // 检查单位是否死亡
        if (OtherUnit->bIsDead)
        {
            return;
        }

        UE_LOG(LogSGGameplay, Log, TEXT("  🎯 击中敌方单位：%s"), *OtherUnit->GetName());

        // 构建击中信息
        FSGProjectileHitInfo HitInfo;
        HitInfo.HitActor = OtherActor;
        HitInfo.HitLocation = Hit.ImpactPoint.IsNearlyZero() ? OtherActor->GetActorLocation() : FVector(Hit.ImpactPoint);
        HitInfo.HitNormal = Hit.ImpactNormal.IsNearlyZero() ? -GetActorForwardVector() : FVector(Hit.ImpactNormal);
        HitInfo.HitBoneName = Hit.BoneName;
        HitInfo.ProjectileDirection = CurrentVelocity.GetSafeNormal();
        HitInfo.ProjectileSpeed = CurrentVelocity.Size();

        // 应用伤害
        ApplyDamageToTarget(OtherActor);
        // 记录已击中的 Actor
        HitActors.Add(OtherActor);

        // 执行击中特效和事件
        ExecuteHitGameplayCue(HitInfo);
        K2_OnHitTarget(HitInfo);
        OnProjectileHitTarget.Broadcast(HitInfo);

        // 检查是否应该销毁
        if (!bPenetrate || (MaxPenetrateCount > 0 && HitActors.Num() >= MaxPenetrateCount))
        {
            Destroy();
        }
        return;
    }

    // ========== 处理地面碰撞 ==========
    
    // 检查是否是地面（法线朝上）
    if (Hit.ImpactNormal.Z > 0.7f)
    {
        UE_LOG(LogSGGameplay, Log, TEXT("  投射物撞击地面"));
        HandleGroundImpact();
        return;
    }
    
    // 其他静态物体，忽略让投射物继续飞行
    UE_LOG(LogSGGameplay, Verbose, TEXT("  忽略静态物体：%s"), *OtherActor->GetName());
}

/**
 * @brief 对目标应用伤害
 * @param Target 目标 Actor
 * @details 使用 GAS 的 GameplayEffect 应用伤害
 */
void ASG_Projectile::ApplyDamageToTarget(AActor* Target)
{
    // 检查目标有效性
    if (!Target)
    {
        UE_LOG(LogSGGameplay, Error, TEXT("ApplyDamageToTarget 失败：目标为空"));
        return;
    }

    // 获取目标的 ASC
    UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
    if (!TargetASC)
    {
        UE_LOG(LogSGGameplay, Error, TEXT("ApplyDamageToTarget 失败：目标没有 ASC"));
        return;
    }

    // 检查攻击者 ASC
    if (!InstigatorASC)
    {
        UE_LOG(LogSGGameplay, Error, TEXT("ApplyDamageToTarget 失败：攻击者 ASC 为空"));
        return;
    }

    // 检查伤害效果类
    if (!DamageEffectClass)
    {
        UE_LOG(LogSGGameplay, Error, TEXT("ApplyDamageToTarget 失败：伤害 GE 未设置"));
        return;
    }

    // 创建效果上下文
    FGameplayEffectContextHandle EffectContext = InstigatorASC->MakeEffectContext();
    EffectContext.AddInstigator(GetOwner(), this);

    // 创建效果规格
    FGameplayEffectSpecHandle SpecHandle = InstigatorASC->MakeOutgoingSpec(DamageEffectClass, 1.0f, EffectContext);

    if (!SpecHandle.IsValid())
    {
        UE_LOG(LogSGGameplay, Error, TEXT("ApplyDamageToTarget 失败：创建 EffectSpec 失败"));
        return;
    }

    // 设置伤害倍率（通过 SetByCaller）
    FGameplayTag DamageTag = FGameplayTag::RequestGameplayTag(FName("Data.Damage"));
    SpecHandle.Data->SetSetByCallerMagnitude(DamageTag, DamageMultiplier);

     // 应用效果到目标
    FActiveGameplayEffectHandle ActiveHandle = InstigatorASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);

    // 检查应用结果
    if (ActiveHandle.IsValid() || SpecHandle.IsValid())
    {
        UE_LOG(LogSGGameplay, Log, TEXT("    ✓ 投射物伤害应用成功（倍率：%.2f）"), DamageMultiplier);
    }
    else
    {
        UE_LOG(LogSGGameplay, Error, TEXT("    ❌ 投射物伤害应用失败"));
    }
}

/**
 * @brief 启用碰撞的回调函数
 * @details 延迟启用碰撞，防止在友方建筑内部生成时立即碰撞
 */
void ASG_Projectile::EnableCollision()
{
    // 检查碰撞组件有效性
    if (CollisionCapsule)
    {
        // 启用查询碰撞（仅检测，不物理模拟）
        CollisionCapsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        UE_LOG(LogSGGameplay, Verbose, TEXT("投射物 %s：碰撞已启用"), *GetName());
    }
}

// ==================== GameplayCue 函数 ====================

/**
 * @brief 执行击中 GameplayCue
 * @param HitInfo 击中信息
 * @details 在击中位置播放击中特效
 */
void ASG_Projectile::ExecuteHitGameplayCue(const FSGProjectileHitInfo& HitInfo)
{
    // 检查标签是否有效
    if (!HitGameplayCueTag.IsValid())
    {
        return;
    }

    // 构建 Cue 参数
    FGameplayCueParameters CueParams;
    CueParams.Location = HitInfo.HitLocation;
    CueParams.Normal = HitInfo.HitNormal;
    CueParams.Instigator = GetInstigator();
    CueParams.EffectCauser = this;
    CueParams.SourceObject = this;
    
    // 通过 ASC 执行 Cue
    if (InstigatorASC)
    {
        InstigatorASC->ExecuteGameplayCue(HitGameplayCueTag, CueParams);
    }
    else
    {
        // 如果没有 ASC，直接通过 CueManager 执行
        if (UGameplayCueManager* CueManager = UAbilitySystemGlobals::Get().GetGameplayCueManager())
        {
            CueManager->HandleGameplayCue(nullptr, HitGameplayCueTag, EGameplayCueEvent::Executed, CueParams);
        }
    }
}

/**
 * @brief 激活飞行 GameplayCue
 * @details 激活拖尾特效，在投射物生命周期内持续显示
 */
void ASG_Projectile::ActivateTrailGameplayCue()
{
    // 检查标签是否有效且未激活
    if (!TrailGameplayCueTag.IsValid() || bTrailCueActive)
    {
        return;
    }

    // 构建 Cue 参数
    FGameplayCueParameters CueParams;
    CueParams.Location = GetActorLocation();
    CueParams.Instigator = GetInstigator();
    CueParams.EffectCauser = this;
    CueParams.SourceObject = this;

    // 通过 ASC 添加持续 Cue
    if (InstigatorASC)
    {
        InstigatorASC->AddGameplayCue(TrailGameplayCueTag, CueParams);
        bTrailCueActive = true;
    }
}

/**
 * @brief 移除飞行 GameplayCue
 * @details 移除拖尾特效
 */
void ASG_Projectile::RemoveTrailGameplayCue()
{
    // 检查标签是否有效且已激活
    if (!TrailGameplayCueTag.IsValid() || !bTrailCueActive)
    {
        return;
    }

    // 通过 ASC 移除 Cue
    if (InstigatorASC)
    {
        InstigatorASC->RemoveGameplayCue(TrailGameplayCueTag);
        bTrailCueActive = false;
    }
}

/**
 * @brief 执行销毁 GameplayCue
 * @details 在投射物销毁位置播放销毁特效
 */
void ASG_Projectile::ExecuteDestroyGameplayCue()
{
    // 检查标签是否有效
    if (!DestroyGameplayCueTag.IsValid())
    {
        return;
    }

    // 构建 Cue 参数
    FGameplayCueParameters CueParams;
    CueParams.Location = GetActorLocation();
    CueParams.Normal = -GetActorForwardVector();
    CueParams.Instigator = GetInstigator();
    CueParams.EffectCauser = this;

    // 通过 ASC 执行 Cue
    if (InstigatorASC)
    {
        InstigatorASC->ExecuteGameplayCue(DestroyGameplayCueTag, CueParams);
    }
}

/**
 * @brief 执行落地 GameplayCue
 * @param ImpactLocation 落地位置
 * @details 在落地位置播放落地特效（如爆炸、尘土等）
 */
void ASG_Projectile::ExecuteGroundImpactGameplayCue(const FVector& ImpactLocation)
{
    // 检查标签是否有效
    if (!GroundImpactGameplayCueTag.IsValid())
    {
        return;
    }

    // 构建 Cue 参数
    FGameplayCueParameters CueParams;
    CueParams.Location = ImpactLocation;
    CueParams.Normal = FVector::UpVector;  // 落地法线朝上
    CueParams.Instigator = GetInstigator();
    CueParams.EffectCauser = this;
    CueParams.SourceObject = this;

    // 通过 ASC 执行 Cue
    if (InstigatorASC)
    {
        InstigatorASC->ExecuteGameplayCue(GroundImpactGameplayCueTag, CueParams);
    }
    else
    {
        // 如果没有 ASC，直接通过 CueManager 执行
        if (UGameplayCueManager* CueManager = UAbilitySystemGlobals::Get().GetGameplayCueManager())
        {
            CueManager->HandleGameplayCue(nullptr, GroundImpactGameplayCueTag, EGameplayCueEvent::Executed, CueParams);
        }
    }
}