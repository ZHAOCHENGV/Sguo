// SG_PlacementPreview.h
// Fill out your copyright notice in the Description page of Project Settings.

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
 * - 根据是否可放置显示不同颜色
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

    // ========== 地面检测配置 ==========
    
    /**
     * @brief 地面检测通道
     * @details 用于检测地面高度的碰撞通道
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground Detection", 
        meta = (DisplayName = "地面检测通道"))
    TEnumAsByte<ECollisionChannel> GroundTraceChannel = ECC_WorldStatic;

    /**
     * @brief 地面检测对象类型（可选）
     * @details 如果设置了，会使用对象类型查询代替通道查询
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground Detection", 
        meta = (DisplayName = "地面对象类型（可选）"))
    TArray<TEnumAsByte<EObjectTypeQuery>> GroundObjectTypes;

    /**
     * @brief 射线检测距离
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground Detection", 
        meta = (DisplayName = "射线检测距离", ClampMin = "1000.0"))
    float RaycastDistance = 10000.0f;

    /**
     * @brief 地面偏移高度
     * @details 预览 Actor 距离地面的高度，避免 Z-Fighting
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground Detection", 
        meta = (DisplayName = "地面偏移", ClampMin = "0.0", UIMax = "10.0"))
    float GroundOffset = 1.0f;

    /**
     * @brief 地面检测时忽略的 Actor 类
     * @details 射线检测地面时会忽略这些类的 Actor
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground Detection", 
        meta = (DisplayName = "地面检测忽略的类"))
    TArray<TSubclassOf<AActor>> GroundTraceIgnoredClasses;

    // ========== 碰撞检测配置（判断是否可放置）==========
    
    /**
     * @brief 碰撞检测通道
     * @details 用于检测是否与其他单位重叠的碰撞通道
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision Detection", 
        meta = (DisplayName = "碰撞检测通道"))
    TEnumAsByte<ECollisionChannel> CollisionCheckChannel = ECC_Pawn;

    /**
     * @brief 碰撞检测对象类型（可选）
     * @details 如果设置了，会使用对象类型查询代替通道查询
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision Detection", 
        meta = (DisplayName = "碰撞对象类型（可选）"))
    TArray<TEnumAsByte<EObjectTypeQuery>> CollisionObjectTypes;

    /**
     * @brief 碰撞检测半径
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision Detection", 
        meta = (DisplayName = "检测半径", ClampMin = "10.0", UIMax = "500.0"))
    float CollisionCheckRadius = 100.0f;

    /**
     * @brief 碰撞检测时忽略的 Actor 类
     * @details 碰撞检测时会忽略这些类的 Actor
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision Detection", 
        meta = (DisplayName = "碰撞检测忽略的类"))
    TArray<TSubclassOf<AActor>> CollisionIgnoredClasses;

    /**
     * @brief 是否忽略死亡单位
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision Detection", 
        meta = (DisplayName = "忽略死亡单位"))
    bool bIgnoreDeadUnits = true;

    // ========== 预览显示配置 ==========
    
    /**
     * @brief 可放置颜色
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview Display", 
        meta = (DisplayName = "可放置颜色"))
    FLinearColor ValidPlacementColor = FLinearColor::Green;

    /**
     * @brief 不可放置颜色
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview Display", 
        meta = (DisplayName = "不可放置颜色"))
    FLinearColor InvalidPlacementColor = FLinearColor::Red;

    /**
     * @brief 预览透明度
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview Display", 
        meta = (DisplayName = "透明度", ClampMin = "0.0", ClampMax = "1.0"))
    float PreviewOpacity = 0.5f;

    // ========== 调试配置 ==========
    
    /**
     * @brief 启用地面检测调试
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", 
        meta = (DisplayName = "调试：地面检测"))
    bool bDebugGroundTrace = false;

    /**
     * @brief 启用碰撞检测调试
     */
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
    void BuildGroundTraceIgnoreList(FCollisionQueryParams& OutParams) const;
    void BuildCollisionIgnoreList(FCollisionQueryParams& OutParams) const;
};
