// Source/Sguo/Public/Actors/SG_PlacementPreview.h
// 🔧 修改 - 优化地面检测性能，移除昂贵的遍历逻辑

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SG_PlacementPreview.generated.h"

class UStaticMeshComponent;
class UDecalComponent;
class USG_CardDataBase;
class ASG_FrontLineManager;
enum class ESGPlacementType : uint8;

/**
 * @brief 放置预览 Actor
 * @details
 * 功能说明：
 * - 显示卡牌放置的预览效果
 * - 跟随鼠标移动并紧贴地面
 * - 性能优化版本：仅通过碰撞通道检测地面，忽略单位
 */
UCLASS()
class SGUO_API ASG_PlacementPreview : public AActor
{
    GENERATED_BODY()
    
public:    
    ASG_PlacementPreview();

protected:
    virtual void BeginPlay() override;

public:
    virtual void Tick(float DeltaTime) override;

    // ========== 公开接口 ==========
    
    UFUNCTION(BlueprintCallable, Category = "Placement")
    void InitializePreview(USG_CardDataBase* InCardData, APlayerController* InPlayerController);

    UFUNCTION(BlueprintCallable, Category = "Placement")
    bool CanPlaceAtCurrentLocation() const;

    UFUNCTION(BlueprintCallable, Category = "Placement")
    FVector GetPreviewLocation() const { return PreviewLocation; }

    UFUNCTION(BlueprintCallable, Category = "Placement")
    FRotator GetPreviewRotation() const { return PreviewRotation; }

    // 是否可以放置
    bool bCanPlace;

protected:
    // ========== 组件 ==========
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USceneComponent* RootComp;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* PreviewMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UDecalComponent* AreaIndicator;

    // ========== ✨ 新增/修改 - 地面检测配置（性能优化版） ==========
    
    /**
     * @brief 是否仅检测静态物体（推荐开启）
     * @details 如果开启，将强制使用 ObjectType 检测，且只检测 WorldStatic。这能最有效地忽略 Pawn 和其他动态物体。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground Detection|Optimization", 
        meta = (DisplayName = "仅检测静态地面(WorldStatic)"))
    bool bOnlyTraceWorldStatic = true;

    /**
     * @brief 地面检测通道
     * @details 当 bOnlyTraceWorldStatic 为 false 时使用此通道。
     * 建议设置为 ECC_WorldStatic 或 ECC_Visibility (如果你确定 Visibility 不会被 Pawn 阻挡)。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground Detection", 
        meta = (DisplayName = "地面检测通道", EditCondition = "!bOnlyTraceWorldStatic"))
    TEnumAsByte<ECollisionChannel> GroundTraceChannel = ECC_WorldStatic;

    /**
     * @brief 额外的地面对象类型
     * @details 除了 WorldStatic 外，你还想检测哪些类型的物体作为“地面”（例如 Landscape 即使是 WorldStatic 也可以显式添加）
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground Detection", 
        meta = (DisplayName = "包含的对象类型", EditCondition = "!bOnlyTraceWorldStatic"))
    TArray<TEnumAsByte<EObjectTypeQuery>> GroundObjectTypes;

    /**
     * @brief 射线检测距离
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground Detection", 
        meta = (DisplayName = "射线检测距离", ClampMin = "1000.0"))
    float RaycastDistance = 10000.0f;

    /**
     * @brief 地面偏移高度
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground Detection", 
        meta = (DisplayName = "地面偏移", ClampMin = "0.0", UIMax = "50.0"))
    float GroundOffset = 2.0f;



    // ========== 碰撞检测配置（判断是否可放置）==========
    
    /**
     * @brief 碰撞检测通道
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision Detection", 
        meta = (DisplayName = "碰撞检测通道"))
    TEnumAsByte<ECollisionChannel> CollisionCheckChannel = ECC_Pawn;

    /**
     * @brief 碰撞检测对象类型（可选）
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision Detection", 
        meta = (DisplayName = "碰撞对象类型（可选）"))
    TArray<TEnumAsByte<EObjectTypeQuery>> CollisionObjectTypes;

    /**
     * @brief 检测半径
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision Detection", 
        meta = (DisplayName = "检测半径", ClampMin = "10.0", UIMax = "500.0"))
    float CollisionCheckRadius = 100.0f;

    // ❌ 删除 - 同样的性能问题，建议使用 ObjectType 过滤，或者仅在 Start 时构建一次列表（如果非要用）
    // TArray<TSubclassOf<AActor>> CollisionIgnoredClasses;
    
    // ✨ 新增 - 替代方案：运行时忽略列表（仅存储特定实例）
    UPROPERTY(Transient)
    TArray<AActor*> IgnoredActorsForCollision;

    /**
     * @brief 是否忽略死亡单位
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision Detection", 
        meta = (DisplayName = "忽略死亡单位"))
    bool bIgnoreDeadUnits = true;

    // ========== 预览显示配置 ==========
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview Display", 
        meta = (DisplayName = "可放置颜色"))
    FLinearColor ValidPlacementColor = FLinearColor::Green;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview Display", 
        meta = (DisplayName = "不可放置颜色"))
    FLinearColor InvalidPlacementColor = FLinearColor::Red;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview Display", 
        meta = (DisplayName = "透明度", ClampMin = "0.0", ClampMax = "1.0"))
    float PreviewOpacity = 0.5f;

    // ========== 调试配置 ==========
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", 
        meta = (DisplayName = "调试：地面检测"))
    bool bDebugGroundTrace = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", 
        meta = (DisplayName = "调试：碰撞检测"))
    bool bDebugCollision = false;

    // ========== 运行时数据 ==========
    
    UPROPERTY(Transient)
    USG_CardDataBase* CardData;

    UPROPERTY(Transient)
    APlayerController* PlayerController;

    UPROPERTY(Transient)
    ASG_FrontLineManager* CachedFrontLineManager;

    UPROPERTY(Transient)
    UMaterialInstanceDynamic* PreviewMaterialInstance;

    FVector PreviewLocation;
    FRotator PreviewRotation;

private:
    // ========== 内部函数 ==========
    
    void UpdatePreviewLocation();
    void UpdatePreviewColor();
    bool CheckCollision() const;
    bool CheckFrontLineViolation() const;
    void CreateSinglePointPreview();
    void CreateAreaPreview();

};