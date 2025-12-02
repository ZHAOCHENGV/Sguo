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

/**
 * @brief 投射物默认配置命名空间
 * @details 包含构造函数中使用的默认值常量
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
 * 
 * @details 
 * **功能说明：**
 * - 创建并配置所有组件
 * - 设置碰撞响应
 * - 绑定碰撞事件
 * 
 * **详细流程：**
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
    
    // 使用常量设置默认胶囊体尺寸（用户可在蓝图或实例中修改组件属性）
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
    MeshComponent->SetupAttachment(CollisionCapsule);
    // 网格体不参与碰撞
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // 启用网络复制
    bReplicates = true;
}

/**
 * @brief BeginPlay 生命周期函数
 * 
 * @details 
 * **功能说明：**
 * - 设置生存时间
 * - 应用碰撞体旋转偏移
 * - 设置延迟启用碰撞
 * - 激活飞行特效
 * 
 * **详细流程：**
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

    // 只应用旋转偏移，碰撞尺寸使用组件自身设置
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
        FlightMode == ESGProjectileFlightMode::Parabolic ? TEXT("抛物线（物理正确）") : TEXT("归航"));
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
 * 
 * @details 
 * **功能说明：**
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

    // 清理命中销毁定时器
    if (GetWorldTimerManager().IsTimerActive(HitDestroyTimerHandle))
    {
        GetWorldTimerManager().ClearTimer(HitDestroyTimerHandle);
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
 * 
 * @details 
 * **功能说明：**
 * - 根据飞行模式更新位置
 * - 更新投射物旋转
 * - 检查目标有效性（抛物线模式）
 * - 绘制调试信息
 * 
 * **注意事项：**
 * - 已命中目标或已落地时不再更新位置
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

    // 已命中目标则不再更新位置（等待销毁）
    if (bHasHitTarget)
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
            // 目标丢失，进入惯性落地模式
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
            // 计算最大进度（估算落地点，假设继续到 Z = GroundZ）
            float MaxProgress = 2.0f; // 默认绘制到 t = 2.0
            
            // 按 5% 的步进绘制轨迹线段
            for (float t = 0.0f; t < MaxProgress; t += 0.05f)
            {
                // 计算两个相邻点的位置
                FVector P1 = CalculateParabolicPosition(t);
                FVector P2 = CalculateParabolicPosition(t + 0.05f);
                
                // 如果已经低于地面，停止绘制
                if (P2.Z < GroundZ)
                {
                    // 绘制最后一段到地面的线
                    DrawDebugLine(GetWorld(), P1, FVector(P2.X, P2.Y, GroundZ), FColor::Orange, false, 0.1f, 0, 1.0f);
                    break;
                }
                
                // 绿色 = t <= 1.0（正常弹道），橙色 = t > 1.0（延展弹道）
                FColor LineColor = (t < 1.0f) ? FColor::Green : FColor::Orange;
                DrawDebugLine(GetWorld(), P1, P2, LineColor, false, 0.1f, 0, 1.0f);
            }
        }
    }

    if (bDrawDebugTargetPoint)
    {
        // 绘制目标位置（黄色球）
        DrawDebugSphere(GetWorld(), TargetLocation, 20.0f, 8, FColor::Yellow, false, -1.0f, 0, 2.0f);
        // 绘制地面高度参考线（青色）
        DrawDebugLine(GetWorld(), 
            FVector(GetActorLocation().X, GetActorLocation().Y, GroundZ),
            FVector(TargetLocation.X, TargetLocation.Y, GroundZ),
            FColor::Cyan, false, -1.0f, 0, 1.0f);
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

// ==================== 胶囊体尺寸获取函数 ====================

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
 * 
 * @details 
 * **功能说明：**
 * - 根据 TargetMode 决定目标位置
 * - TargetActor: 飞向目标中心
 * - TargetAreaRandom: 飞向目标周围随机点
 * 
 * **详细流程：**
 * 1. 保存攻击者信息
 * 2. 设置忽略友方碰撞
 * 3. 重置状态标记
 * 4. 记录起始位置
 * 5. 根据目标模式计算目标位置
 * 6. 计算地面高度
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

    // 🔧 修复：清空已击中目标列表，确保新发射的投射物从零开始
    HitActors.Empty();

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
    bHasHitTarget = false;
    
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
            break;

        case ESGProjectileTargetMode::TargetAreaRandom:
            // 飞向目标周围随机点
            AreaCenterLocation = InTarget->GetActorLocation();
            AreaRotation = InTarget->GetActorRotation();
            // 生成随机点
            TargetLocation = GenerateRandomPointInArea(AreaCenterLocation, AreaRotation);
            break;

        default:
            // 其他模式使用目标位置
            TargetLocation = CalculateTargetLocation(InTarget);
            AreaCenterLocation = InTarget->GetActorLocation();
            AreaRotation = InTarget->GetActorRotation();
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

    // 计算地面高度（用于弹道延展时的落地判断）
    GroundZ = CalculateGroundZ(TargetLocation);

    // 计算飞行距离（起点到目标的直线距离）
    TotalFlightDistance = FVector::Dist(StartLocation, TargetLocation);

    // 初始化速度向量
    FVector Direction = (TargetLocation - StartLocation).GetSafeNormal();
    CurrentVelocity = Direction * FlightSpeed;
    
    // 重置飞行进度
    FlightProgress = 0.0f;
    
    // 标记为已初始化
    bIsInitialized = true;

    // 输出日志
    UE_LOG(LogSGGameplay, Log, TEXT("========== 初始化投射物（Actor目标）=========="));
    UE_LOG(LogSGGameplay, Log, TEXT("  目标：%s"), InTarget ? *InTarget->GetName() : TEXT("无"));
    UE_LOG(LogSGGameplay, Log, TEXT("  起点：%s"), *StartLocation.ToString());
    UE_LOG(LogSGGameplay, Log, TEXT("  目标位置：%s"), *TargetLocation.ToString());
    UE_LOG(LogSGGameplay, Log, TEXT("  地面高度：%.1f"), GroundZ);
    UE_LOG(LogSGGameplay, Log, TEXT("  飞行距离：%.1f"), TotalFlightDistance);
    UE_LOG(LogSGGameplay, Log, TEXT("  弧度高度：%.1f"), ArcHeight);
    UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
}

/**
 * @brief 初始化投射物（目标为位置）
 * @param InInstigatorASC 攻击者 ASC
 * @param InFactionTag 攻击者阵营
 * @param InTargetLocation 目标位置
 * @param InArcHeight 弧度高度（覆盖默认值，-1 表示使用默认）
 * 
 * @details 
 * **功能说明：**
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
    bHasHitTarget = false;  // 🔧 新增：重置命中状态

    // 🔧 修复：清空已击中目标列表
    HitActors.Empty();

    
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
        break;

    case ESGProjectileTargetMode::AreaCenter:
        // 飞向区域中心地面
        TargetLocation = InTargetLocation;
        break;

    case ESGProjectileTargetMode::AreaRandom:
        // 飞向区域内随机地面点
        TargetLocation = GenerateRandomPointInArea(InTargetLocation, AreaRotation);
        break;

    default:
        // 默认飞向指定位置
        TargetLocation = InTargetLocation;
        break;
    }

    // 计算地面高度
    GroundZ = CalculateGroundZ(TargetLocation);

    // 计算飞行距离
    TotalFlightDistance = FVector::Dist(StartLocation, TargetLocation);

    // 初始化速度向量
    FVector Direction = (TargetLocation - StartLocation).GetSafeNormal();
    CurrentVelocity = Direction * FlightSpeed;
    
    // 重置飞行进度
    FlightProgress = 0.0f;
    
    // 标记为已初始化
    bIsInitialized = true;

    // 输出日志
    UE_LOG(LogSGGameplay, Log, TEXT("========== 初始化投射物（位置目标）=========="));
    UE_LOG(LogSGGameplay, Log, TEXT("  起点：%s"), *StartLocation.ToString());
    UE_LOG(LogSGGameplay, Log, TEXT("  目标位置：%s"), *TargetLocation.ToString());
    UE_LOG(LogSGGameplay, Log, TEXT("  地面高度：%.1f"), GroundZ);
    UE_LOG(LogSGGameplay, Log, TEXT("  飞行距离：%.1f"), TotalFlightDistance);
    UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
}

/**
 * @brief 初始化投射物（目标为区域）
 * @param InInstigatorASC 攻击者 ASC
 * @param InFactionTag 攻击者阵营
 * @param InAreaCenter 区域中心位置
 * @param InAreaRotation 区域朝向
 * @param InArcHeight 弧度高度（覆盖默认值，-1 表示使用默认）
 * 
 * @details 
 * **功能说明：**
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
    bHasHitTarget = false;  // 🔧 新增：重置命中状态

    // 🔧 修复：清空已击中目标列表
    HitActors.Empty();
    
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

    // 计算地面高度
    GroundZ = CalculateGroundZ(TargetLocation);
    
    // 计算飞行距离
    TotalFlightDistance = FVector::Dist(StartLocation, TargetLocation);

    // 初始化速度向量
    FVector Direction = (TargetLocation - StartLocation).GetSafeNormal();
    CurrentVelocity = Direction * FlightSpeed;
    
    // 重置飞行进度
    FlightProgress = 0.0f;
    
    // 标记为已初始化
    bIsInitialized = true;

    // 输出日志
    UE_LOG(LogSGGameplay, Log, TEXT("========== 初始化投射物（区域目标）=========="));
    UE_LOG(LogSGGameplay, Log, TEXT("  区域形状：%s"),
        AreaShape == ESGProjectileAreaShape::Circle ? TEXT("圆形") :
        AreaShape == ESGProjectileAreaShape::Rectangle ? TEXT("矩形") : TEXT("扇形"));
    UE_LOG(LogSGGameplay, Log, TEXT("  区域中心：%s"), *InAreaCenter.ToString());
    UE_LOG(LogSGGameplay, Log, TEXT("  目标位置：%s"), *TargetLocation.ToString());
    UE_LOG(LogSGGameplay, Log, TEXT("  地面高度：%.1f"), GroundZ);
    UE_LOG(LogSGGameplay, Log, TEXT("  飞行距离：%.1f"), TotalFlightDistance);
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
 * 
 * @details 
 * **功能说明：**
 * - 直接飞向目标位置
 * - 持续追踪目标（如果有且未丢失）
 */
void ASG_Projectile::UpdateLinearFlight(float DeltaTime)
{
    // 如果有目标 Actor 且未丢失，动态更新目标位置
    if (!bTargetLost && CurrentTarget.IsValid() && TargetMode == ESGProjectileTargetMode::TargetActor)
    {
        TargetLocation = CalculateTargetLocation(CurrentTarget.Get());
    }

    // 计算本帧移动距离
    float MoveDistance = FlightSpeed * DeltaTime;
    
    // 获取当前位置
    FVector CurrentLocation = GetActorLocation();
    // 计算到目标的向量
    FVector ToTarget = TargetLocation - CurrentLocation;
    
    // 检查是否已到达目标
    if (ToTarget.Size() <= MoveDistance)
    {
        // 已到达目标位置
        SetActorLocation(TargetLocation);
        CurrentVelocity = ToTarget.GetSafeNormal() * FlightSpeed;
        
        // 如果到达目标且高度低于地面，触发落地
        if (TargetLocation.Z <= GroundZ)
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
 * @brief 更新抛物线飞行（物理正确版）
 * @param DeltaTime 帧间隔时间
 * 
 * @details 
 * **物理模型：**
 * - 使用二次曲线公式：h(t) = 4 * ArcHeight * t * (1-t)
 * - 当 t > 1.0 时自然延展，高度偏移变为负数
 * - 投射物会继续沿弹道飞行直到撞击地面
 * 
 * **目标跟踪：**
 * - 目标存活时：动态更新目标位置
 * - 目标丢失后：锁定最后位置，进入惯性落地模式
 * 
 * **详细流程：**
 * 1. 防止除零错误
 * 2. 计算本帧飞行距离和进度增量
 * 3. 更新飞行进度（不钳位）
 * 4. 计算新位置
 * 5. 计算速度向量
 * 6. 检查是否低于地面高度
 * 7. 如果目标有效且未丢失，动态更新目标位置
 */
void ASG_Projectile::UpdateParabolicFlight(float DeltaTime)
{
    // 防止除零
    if (TotalFlightDistance < KINDA_SMALL_NUMBER)
    {
        // 距离太短，直接落地
        HandleGroundImpact();
        return;
    }

    // 计算本帧飞行距离
    float DistanceThisFrame = FlightSpeed * DeltaTime;
    
    // 更新飞行进度（不再钳位，允许超过 1.0）
    FlightProgress += DistanceThisFrame / TotalFlightDistance;

    // 计算当前位置（使用物理正确的二次曲线公式）
    FVector NewLocation = CalculateParabolicPosition(FlightProgress);
    
    // 计算速度向量（用于旋转）
    FVector PreviousLocation = GetActorLocation();
    if (DeltaTime > KINDA_SMALL_NUMBER)
    {
        CurrentVelocity = (NewLocation - PreviousLocation) / DeltaTime;
    }
    
    // 如果速度过小，使用方向估算
    if (CurrentVelocity.Size() < 1.0f)
    {
        // 向前看一小段计算方向
        float NextProgress = FlightProgress + 0.01f;
        FVector NextLocation = CalculateParabolicPosition(NextProgress);
        CurrentVelocity = (NextLocation - NewLocation).GetSafeNormal() * FlightSpeed;
    }

    // 检查是否低于地面高度
    if (NewLocation.Z <= GroundZ)
    {
        // 已达到或低于地面，触发落地
        // 将 Z 坐标修正为地面高度
        NewLocation.Z = GroundZ;
        SetActorLocation(NewLocation);
        HandleGroundImpact();
        return;
    }

    // 更新位置
    SetActorLocation(NewLocation);

    // 如果目标还活着且未丢失，动态更新目标位置
    if (!bTargetLost && CurrentTarget.IsValid() && TargetMode == ESGProjectileTargetMode::TargetActor)
    {
        AActor* Target = CurrentTarget.Get();
        // 计算新的目标位置
        FVector NewTargetLocation = CalculateTargetLocation(Target);
        // 平滑更新目标位置（避免抖动）
        TargetLocation = FMath::VInterpTo(TargetLocation, NewTargetLocation, DeltaTime, 5.0f);
        // 更新总飞行距离
        TotalFlightDistance = FVector::Dist(StartLocation, TargetLocation);
        // 同时更新地面高度
        GroundZ = CalculateGroundZ(TargetLocation);
    }
}

/**
 * @brief 计算抛物线位置（物理正确的二次曲线）
 * @param Progress 飞行进度（可以超过 1.0）
 * @return 当前应处于的世界位置
 * 
 * @details 
 * **物理公式：**
 * $$Position = Lerp(Start, Target, t) + (0, 0, 4 \cdot ArcHeight \cdot t \cdot (1-t))$$
 * 
 * **特性：**
 * - t = 0: 起点，高度偏移 = 0
 * - t = 0.5: 最高点，高度偏移 = ArcHeight
 * - t = 1.0: 目标点，高度偏移 = 0
 * - t > 1.0: 延展阶段，t(1-t) < 0，高度偏移为负数，自然下落
 * 
 * 例如：
 * - t = 1.5 时：t(1-t) = 1.5 * (-0.5) = -0.75，高度偏移 = -3 * ArcHeight
 * - t = 2.0 时：t(1-t) = 2.0 * (-1.0) = -2.0，高度偏移 = -8 * ArcHeight
 */
FVector ASG_Projectile::CalculateParabolicPosition(float Progress) const
{
    // 线性插值基础位置（XY 平面的位置）
    // 当 t > 1.0 时，会外推到目标点之后
    FVector LinearPosition = FMath::Lerp(StartLocation, TargetLocation, Progress);
    
    // 计算抛物线高度偏移（物理正确的二次曲线）
    // 公式：h(t) = 4 * ArcHeight * t * (1-t)
    // 当 t = 0.5 时达到最大值 ArcHeight
    // 当 t > 1.0 时，t(1-t) 变为负数，高度自然下降
    float HeightOffset = 4.0f * ArcHeight * Progress * (1.0f - Progress);
    
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
    // 更新目标位置（如果目标有效且未丢失）
    if (!bTargetLost && CurrentTarget.IsValid() && TargetMode == ESGProjectileTargetMode::TargetActor)
    {
        TargetLocation = CalculateTargetLocation(CurrentTarget.Get());
    }

    // 计算当前方向
    FVector CurrentDirection = CurrentVelocity.GetSafeNormal();
    // 计算期望方向
    FVector DesiredDirection = (TargetLocation - GetActorLocation()).GetSafeNormal();

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
    FVector NewLocation = GetActorLocation() + CurrentVelocity * DeltaTime;
    
    // 检查是否低于地面
    if (NewLocation.Z <= GroundZ)
    {
        NewLocation.Z = GroundZ;
        SetActorLocation(NewLocation);
        HandleGroundImpact();
        return;
    }
    
    SetActorLocation(NewLocation);
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
 * 
 * @details 
 * **功能说明：**
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
 * @brief 计算地面高度
 * @param InLocation 参考位置
 * @return 地面 Z 坐标
 * 
 * @details 从参考位置向下进行射线检测，找到地面高度
 */
float ASG_Projectile::CalculateGroundZ(const FVector& InLocation) const
{
    // 射线检测起点（参考位置上方 100 厘米）
    FVector TraceStart = InLocation + FVector(0.0f, 0.0f, 100.0f);
    // 射线检测终点（向下检测）
    FVector TraceEnd = InLocation - FVector(0.0f, 0.0f, GroundTraceDistance);

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
        // 检测到地面，返回击中点的 Z 坐标
        return HitResult.ImpactPoint.Z;
    }
    else
    {
        // 未检测到地面，使用参考位置的 Z 坐标减去一定值
        // 或者使用起点的 Z 坐标作为参考
        return FMath::Min(InLocation.Z, StartLocation.Z) - 100.0f;
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
 * @brief 处理目标丢失
 * 
 * @details 
 * **弹道延展策略：**
 * - 不重新计算路径，保持当前弹道
 * - 锁定最后的目标位置
 * - 让投射物自然惯性落地
 * 
 * **与旧版的区别：**
 * - 旧版：bFlyToGround = true，强制切换到新路径
 * - 新版：仅标记 bTargetLost，继续使用当前弹道公式
 */
void ASG_Projectile::HandleTargetLost()
{
    // 标记目标丢失
    bTargetLost = true;
    
    // 锁定当前目标位置（不再更新）
    // TargetLocation 保持不变
    
    // 确保地面高度已计算
    if (GroundZ > TargetLocation.Z)
    {
        // 如果地面比目标还高（不应该发生），重新计算
        GroundZ = CalculateGroundZ(GetActorLocation());
    }

    UE_LOG(LogSGGameplay, Log, TEXT("投射物目标丢失，进入惯性落地模式"));
    UE_LOG(LogSGGameplay, Log, TEXT("  当前位置：%s"), *GetActorLocation().ToString());
    UE_LOG(LogSGGameplay, Log, TEXT("  锁定目标位置：%s"), *TargetLocation.ToString());
    UE_LOG(LogSGGameplay, Log, TEXT("  地面高度：%.1f"), GroundZ);
    UE_LOG(LogSGGameplay, Log, TEXT("  当前飞行进度：%.2f"), FlightProgress);
}

/**
 * @brief 处理投射物落地
 * 
 * @details 
 * **功能说明：**
 * - 🔧 修复：先禁用碰撞，再设置标记，防止竞态条件
 * - 标记已落地
 * - 禁用碰撞（防止后续物理检测消耗）
 * - 禁用 Tick（防止后续逻辑消耗，重大性能优化）
 * - 执行落地特效
 * - 广播落地事件
 * - 设置延迟销毁
 */
void ASG_Projectile::HandleGroundImpact()
{
    UE_LOG(LogSGGameplay, Warning, TEXT("🟤 HandleGroundImpact 被调用"));
    UE_LOG(LogSGGameplay, Warning, TEXT("  投射物：%s"), *GetName());
    UE_LOG(LogSGGameplay, Warning, TEXT("  当前 bHasLanded：%s"), bHasLanded ? TEXT("true") : TEXT("false"));

    // 防止重复处理
    if (bHasLanded)
    {
        UE_LOG(LogSGGameplay, Warning, TEXT("  ⛔ 已经落地过，跳过"));
        return;
    }

    // 🔧 修复 - 立即禁用碰撞（在设置 bHasLanded 之前）
    if (CollisionCapsule)
    {
        CollisionCapsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        CollisionCapsule->SetGenerateOverlapEvents(false);
        UE_LOG(LogSGGameplay, Warning, TEXT("  ✓ 已禁用碰撞和重叠事件"));
    }

    // 标记已落地
    bHasLanded = true;
    UE_LOG(LogSGGameplay, Warning, TEXT("  ✓ 设置 bHasLanded = true"));

    SetActorTickEnabled(false);
    
    CurrentVelocity = FVector::ZeroVector;
    
    FVector ImpactLocation = GetActorLocation();
    ImpactLocation.Z = GroundZ;
    
    UE_LOG(LogSGGameplay, Warning, TEXT("  落地位置：%s"), *ImpactLocation.ToString());

    RemoveTrailGameplayCue();
    ExecuteGroundImpactGameplayCue(ImpactLocation);

    FSGProjectileHitInfo GroundHitInfo;
    GroundHitInfo.HitLocation = ImpactLocation;
    GroundHitInfo.HitNormal = FVector::UpVector;
    GroundHitInfo.ProjectileDirection = CurrentVelocity.IsNearlyZero() ? GetActorForwardVector() : CurrentVelocity.GetSafeNormal();
    GroundHitInfo.ProjectileSpeed = 0.0f;

    K2_OnGroundImpact(ImpactLocation);
    OnProjectileGroundImpact.Broadcast(GroundHitInfo);

    SetLifeSpan(GroundImpactDestroyDelay);
    
    UE_LOG(LogSGGameplay, Warning, TEXT("  ✓ 落地处理完成，%.1f秒后销毁"), GroundImpactDestroyDelay);
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
 * 
 * @details 
 * **功能说明：**
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
 * 
 * @details 
 * **功能说明：**
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
 * 
 * @details 
 * **功能说明：**
 * - 过滤友方单位和建筑
 * - 确保每个目标只受一次伤害
 * - 对敌方目标应用伤害
 * - 处理穿透逻辑
 * - 非穿透模式下停止并隐藏
 * - 处理地面碰撞
 * 
 * **伤害逻辑：**
 * - 使用 HitActors 数组追踪已击中的目标
 * - 每个目标只会被添加一次，因此只受一次伤害
 * - 穿透模式：可以击中多个不同目标，每个一次
 * - 非穿透模式：击中第一个目标后停止
 * 
 * **注意事项：**
 * - 🔧 修复：落地后不再对单位造成伤害
 */
void ASG_Projectile::HandleProjectileImpact(AActor* OtherActor, const FHitResult& Hit)
{
      // ✨ 新增 - 所有碰撞事件的入口日志
    UE_LOG(LogSGGameplay, Warning, TEXT("🔶 HandleProjectileImpact 被调用"));
    UE_LOG(LogSGGameplay, Warning, TEXT("  投射物：%s"), *GetName());
    UE_LOG(LogSGGameplay, Warning, TEXT("  碰撞对象：%s"), OtherActor ? *OtherActor->GetName() : TEXT("空"));
    UE_LOG(LogSGGameplay, Warning, TEXT("  当前状态：bHasLanded=%s, bHasHitTarget=%s, bIsInitialized=%s"),
        bHasLanded ? TEXT("true") : TEXT("false"),
        bHasHitTarget ? TEXT("true") : TEXT("false"),
        bIsInitialized ? TEXT("true") : TEXT("false"));

    // ========== 前置状态检查（最高优先级） ==========
    
    // 🔧 修复 - 将 bHasLanded 检查移到最前面
    if (bHasLanded)
    {
        UE_LOG(LogSGGameplay, Warning, TEXT("  ⛔ 已落地，忽略此碰撞"));
        return;
    }
    
    if (!bIsInitialized)
    {
        UE_LOG(LogSGGameplay, Warning, TEXT("  ⛔ 未初始化，忽略此碰撞"));
        return;
    }
  
    if (bHasHitTarget)
    {
        UE_LOG(LogSGGameplay, Warning, TEXT("  ⛔ 已命中目标，忽略此碰撞"));
        return;
    }

    // ========== 基础有效性检查 ==========
    
    if (!OtherActor)
    {
        UE_LOG(LogSGGameplay, Warning, TEXT("  ⛔ 碰撞对象为空，忽略"));
        return;
    }
    
    if (OtherActor == this)
    {
        UE_LOG(LogSGGameplay, Warning, TEXT("  ⛔ 碰撞对象是自己，忽略"));
        return;
    }
    
    if (OtherActor == GetOwner() || OtherActor == GetInstigator())
    {
        UE_LOG(LogSGGameplay, Warning, TEXT("  ⛔ 碰撞对象是所有者/施放者，忽略"));
        return;
    }

    // ========== 重复击中检查 ==========
    if (HitActors.Contains(OtherActor))
    {
        UE_LOG(LogSGGameplay, Warning, TEXT("  ⛔ 已击中过此目标，忽略"));
        return;
    }

    // ========== 友方过滤 ==========

    ASG_UnitsBase* OtherUnit = Cast<ASG_UnitsBase>(OtherActor);
    if (OtherUnit)
    {
        UE_LOG(LogSGGameplay, Warning, TEXT("  碰撞对象是单位：%s"), *OtherUnit->GetName());
        UE_LOG(LogSGGameplay, Warning, TEXT("    单位阵营：%s"), *OtherUnit->FactionTag.ToString());
        UE_LOG(LogSGGameplay, Warning, TEXT("    投射物阵营：%s"), *InstigatorFactionTag.ToString());
        UE_LOG(LogSGGameplay, Warning, TEXT("    单位是否死亡：%s"), OtherUnit->bIsDead ? TEXT("是") : TEXT("否"));
        
        if (OtherUnit->FactionTag == InstigatorFactionTag)
        {
            UE_LOG(LogSGGameplay, Warning, TEXT("  ⛔ 友方单位，忽略"));
            return;
        }
        
        if (OtherUnit->bIsDead)
        {
            UE_LOG(LogSGGameplay, Warning, TEXT("  ⛔ 已死亡单位，忽略"));
            return;
        }
    }

    ASG_MainCityBase* OtherMainCity = Cast<ASG_MainCityBase>(OtherActor);
    if (OtherMainCity)
    {
        UE_LOG(LogSGGameplay, Warning, TEXT("  碰撞对象是主城：%s"), *OtherMainCity->GetName());
        
        if (OtherMainCity->FactionTag == InstigatorFactionTag)
        {
            UE_LOG(LogSGGameplay, Warning, TEXT("  ⛔ 友方主城，忽略"));
            return;
        }
        
        if (!OtherMainCity->IsAlive())
        {
            UE_LOG(LogSGGameplay, Warning, TEXT("  ⛔ 已摧毁主城，忽略"));
            return;
        }
    }
    
    UPrimitiveComponent* HitComponent = Hit.GetComponent();
    if (HitComponent)
    {
        AActor* ComponentOwner = HitComponent->GetOwner();
        if (ComponentOwner && ComponentOwner != OtherActor)
        {
            ASG_MainCityBase* OwnerCity = Cast<ASG_MainCityBase>(ComponentOwner);
            if (OwnerCity && OwnerCity->FactionTag == InstigatorFactionTag)
            {
                UE_LOG(LogSGGameplay, Warning, TEXT("  ⛔ 组件属于友方主城，忽略"));
                return;
            }
        }
    }

    // ========== 处理有效目标（敌方单位或主城） ==========
    
    if (OtherUnit || OtherMainCity)
    {
        UE_LOG(LogSGGameplay, Warning, TEXT("  ✅ 有效敌方目标，准备应用伤害"));
        UE_LOG(LogSGGameplay, Warning, TEXT("  击中位置：%s"), *Hit.ImpactPoint.ToString());

        FSGProjectileHitInfo HitInfo;
        HitInfo.HitActor = OtherActor;
        HitInfo.HitLocation = Hit.ImpactPoint.IsNearlyZero() ? OtherActor->GetActorLocation() : FVector(Hit.ImpactPoint);
        HitInfo.HitNormal = Hit.ImpactNormal.IsNearlyZero() ? -GetActorForwardVector() : FVector(Hit.ImpactNormal);
        HitInfo.HitBoneName = Hit.BoneName;
        HitInfo.ProjectileDirection = CurrentVelocity.GetSafeNormal();
        HitInfo.ProjectileSpeed = CurrentVelocity.Size();

        HitActors.Add(OtherActor);

        // 应用伤害
        ApplyDamageToTarget(OtherActor);

        ExecuteHitGameplayCue(HitInfo);
        K2_OnHitTarget(HitInfo);
        OnProjectileHitTarget.Broadcast(HitInfo);

        bool bShouldStop = false;
        
        if (!bPenetrate)
        {
            bShouldStop = true;
        }
        else if (MaxPenetrateCount > 0 && HitActors.Num() >= MaxPenetrateCount)
        {
            bShouldStop = true;
            UE_LOG(LogSGGameplay, Warning, TEXT("  达到最大穿透数量：%d"), MaxPenetrateCount);
        }

        if (bShouldStop)
        {
            UE_LOG(LogSGGameplay, Warning, TEXT("  投射物停止（非穿透或达到上限）"));
            HandleHitTarget(OtherActor, HitInfo);
        }
        
        return;
    }

    // ========== 处理地面碰撞 ==========
    
    if (Hit.ImpactNormal.Z > 0.7f)
    {
        UE_LOG(LogSGGameplay, Warning, TEXT("  🟤 检测到地面碰撞，法线Z：%.2f"), Hit.ImpactNormal.Z);
        GroundZ = Hit.ImpactPoint.Z;
        HandleGroundImpact();
        return;
    }
    
    UE_LOG(LogSGGameplay, Warning, TEXT("  忽略其他静态物体"));
   
}

/**
 * @brief 对目标应用伤害
 * @param Target 目标 Actor
 * @details 使用 GAS 的 GameplayEffect 应用伤害
 */
void ASG_Projectile::ApplyDamageToTarget(AActor* Target)
{
     // ✨ 新增 - 调试日志：输出伤害来源信息
    UE_LOG(LogSGGameplay, Warning, TEXT("========== 投射物伤害调试 =========="));
    UE_LOG(LogSGGameplay, Warning, TEXT("  投射物：%s"), *GetName());
    UE_LOG(LogSGGameplay, Warning, TEXT("  目标：%s"), Target ? *Target->GetName() : TEXT("空"));
    UE_LOG(LogSGGameplay, Warning, TEXT("  投射物状态："));
    UE_LOG(LogSGGameplay, Warning, TEXT("    bIsInitialized: %s"), bIsInitialized ? TEXT("true") : TEXT("false"));
    UE_LOG(LogSGGameplay, Warning, TEXT("    bHasHitTarget: %s"), bHasHitTarget ? TEXT("true") : TEXT("false"));
    UE_LOG(LogSGGameplay, Warning, TEXT("    bHasLanded: %s"), bHasLanded ? TEXT("true") : TEXT("false"));
    UE_LOG(LogSGGameplay, Warning, TEXT("    bTargetLost: %s"), bTargetLost ? TEXT("true") : TEXT("false"));
    UE_LOG(LogSGGameplay, Warning, TEXT("    FlightProgress: %.2f"), FlightProgress);
    UE_LOG(LogSGGameplay, Warning, TEXT("    当前位置：%s"), *GetActorLocation().ToString());
    UE_LOG(LogSGGameplay, Warning, TEXT("    地面高度：%.1f"), GroundZ);
    UE_LOG(LogSGGameplay, Warning, TEXT("    已击中目标数：%d"), HitActors.Num());
    
    // ✨ 新增 - 输出调用堆栈信息
    UE_LOG(LogSGGameplay, Warning, TEXT("  调用来源（检查是否从落地状态调用）"));
    
    // 检查目标有效性
    if (!Target)
    {
        UE_LOG(LogSGGameplay, Error, TEXT("  ❌ ApplyDamageToTarget 失败：目标为空"));
        UE_LOG(LogSGGameplay, Warning, TEXT("========================================"));
        return;
    }

    // 获取目标的 ASC
    UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
    if (!TargetASC)
    {
        UE_LOG(LogSGGameplay, Error, TEXT("  ❌ ApplyDamageToTarget 失败：目标没有 ASC"));
        UE_LOG(LogSGGameplay, Warning, TEXT("========================================"));
        return;
    }

    // 检查攻击者 ASC
    if (!InstigatorASC)
    {
        UE_LOG(LogSGGameplay, Error, TEXT("  ❌ ApplyDamageToTarget 失败：攻击者 ASC 为空"));
        UE_LOG(LogSGGameplay, Warning, TEXT("========================================"));
        return;
    }

    // 检查伤害效果类
    if (!DamageEffectClass)
    {
        UE_LOG(LogSGGameplay, Error, TEXT("  ❌ ApplyDamageToTarget 失败：伤害 GE 未设置"));
        UE_LOG(LogSGGameplay, Warning, TEXT("========================================"));
        return;
    }

    // 创建效果上下文
    FGameplayEffectContextHandle EffectContext = InstigatorASC->MakeEffectContext();
    
    // 使用 GetInstigator() 而不是 GetOwner()
    EffectContext.AddInstigator(GetInstigator(), this);

    // 创建效果规格
    FGameplayEffectSpecHandle SpecHandle = InstigatorASC->MakeOutgoingSpec(DamageEffectClass, 1.0f, EffectContext);

    if (!SpecHandle.IsValid())
    {
        UE_LOG(LogSGGameplay, Error, TEXT("  ❌ ApplyDamageToTarget 失败：创建 EffectSpec 失败"));
        UE_LOG(LogSGGameplay, Warning, TEXT("========================================"));
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
        UE_LOG(LogSGGameplay, Warning, TEXT("  ✓ 投射物伤害应用成功"));
        UE_LOG(LogSGGameplay, Warning, TEXT("    伤害倍率：%.2f"), DamageMultiplier);
        UE_LOG(LogSGGameplay, Warning, TEXT("    攻击者：%s"), GetInstigator() ? *GetInstigator()->GetName() : TEXT("空"));
    }
    else
    {
        UE_LOG(LogSGGameplay, Error, TEXT("  ❌ 投射物伤害应用失败"));
    }
    
    UE_LOG(LogSGGameplay, Warning, TEXT("========================================"));
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

/**
 * @brief 处理命中目标后的逻辑
 * @param HitActor 被击中的 Actor
 * @param HitInfo 击中信息
 * 
 * @details 
 * **功能说明：**
 * - 停止投射物移动（设置 bHasHitTarget 标记）
 * - 禁用碰撞防止重复检测
 * - 隐藏网格体
 * - 移除飞行拖尾特效
 * - 处理附着逻辑（可选）
 * - 设置延迟销毁定时器
 * - 触发蓝图事件
 * 
 * **详细流程：**
 * 1. 标记已命中，停止移动
 * 2. 禁用碰撞
 * 3. 隐藏网格体
 * 4. 移除拖尾特效
 * 5. 如果需要附着，将投射物附着到目标
 * 6. 调用蓝图事件
 * 7. 设置延迟销毁定时器
 */
void ASG_Projectile::HandleHitTarget(AActor* HitActor, const FSGProjectileHitInfo& HitInfo)
{
    // 标记已命中目标，停止移动
    bHasHitTarget = true;

    // 禁用碰撞，防止重复检测
    if (CollisionCapsule)
    {
        CollisionCapsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    // 隐藏网格体
    HideProjectileMesh();

    // 移除飞行拖尾特效
    RemoveTrailGameplayCue();

    // 清零速度
    CurrentVelocity = FVector::ZeroVector;

    UE_LOG(LogSGGameplay, Log, TEXT("投射物命中目标，停止移动：%s -> %s"), 
        *GetName(), 
        HitActor ? *HitActor->GetName() : TEXT("None"));

    // 处理附着逻辑
    if (bAttachToTargetOnHit && HitActor)
    {
        // 确定附着的骨骼名称
        FName BoneToAttach = AttachBoneName;
        
        // 如果没有指定骨骼，使用击中的骨骼
        if (BoneToAttach.IsNone() && !HitInfo.HitBoneName.IsNone())
        {
            BoneToAttach = HitInfo.HitBoneName;
        }

        // 附着到目标
        FAttachmentTransformRules AttachRules(
            EAttachmentRule::KeepWorld,  // 位置保持世界坐标
            EAttachmentRule::KeepWorld,  // 旋转保持世界坐标
            EAttachmentRule::KeepWorld,  // 缩放保持世界坐标
            true                          // 焊接模拟体
        );

        // 尝试获取骨骼网格体组件
        USkeletalMeshComponent* TargetSkelMesh = HitActor->FindComponentByClass<USkeletalMeshComponent>();
        
        if (TargetSkelMesh && !BoneToAttach.IsNone())
        {
            // 附着到骨骼
            AttachToComponent(TargetSkelMesh, AttachRules, BoneToAttach);
            UE_LOG(LogSGGameplay, Verbose, TEXT("  附着到骨骼：%s"), *BoneToAttach.ToString());
        }
        else
        {
            // 附着到根组件
            AttachToActor(HitActor, AttachRules);
            UE_LOG(LogSGGameplay, Verbose, TEXT("  附着到 Actor 根组件"));
        }
    }

    // 调用蓝图事件（命中后处理）
    K2_OnAfterHitTarget(HitInfo);

    // 设置延迟销毁定时器
    if (HitDestroyDelay > 0.0f)
    {
        GetWorldTimerManager().SetTimer(
            HitDestroyTimerHandle,
            this,
            &ASG_Projectile::OnHitDestroyTimerExpired,
            HitDestroyDelay,
            false  // 不循环
        );
        
        UE_LOG(LogSGGameplay, Verbose, TEXT("  设置销毁定时器：%.2f 秒"), HitDestroyDelay);
    }
    else
    {
        // 立即销毁
        OnHitDestroyTimerExpired();
    } 
}
/**
 * @brief 命中后延迟销毁回调
 * 
 * @details 
 * **功能说明：**
 * - 调用蓝图事件通知即将销毁
 * - 销毁投射物
 */
void ASG_Projectile::OnHitDestroyTimerExpired()
{
    UE_LOG(LogSGGameplay, Verbose, TEXT("投射物命中后销毁：%s"), *GetName());
    
    // 调用蓝图事件（销毁前）
    K2_OnBeforeDestroyAfterHit();
    
    // 销毁投射物
    Destroy();
}

/**
 * @brief 手动隐藏投射物网格体
 * @details 蓝图可调用，用于自定义隐藏时机
 */
void ASG_Projectile::HideProjectileMesh()
{
    if (MeshComponent)
    {
        MeshComponent->SetVisibility(false, true);  // true = 传播到子组件
        UE_LOG(LogSGGameplay, Verbose, TEXT("投射物网格体已隐藏：%s"), *GetName());
    }
}

/**
 * @brief 手动显示投射物网格体
 * @details 蓝图可调用，用于自定义显示时机
 */
void ASG_Projectile::ShowProjectileMesh()
{
    if (MeshComponent)
    {
        MeshComponent->SetVisibility(true, true);  // true = 传播到子组件
        UE_LOG(LogSGGameplay, Verbose, TEXT("投射物网格体已显示：%s"), *GetName());
    }
}
