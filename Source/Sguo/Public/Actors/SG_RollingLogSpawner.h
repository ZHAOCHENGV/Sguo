// 📄 文件：Source/Sguo/Public/Actors/SG_RollingLogSpawner.h
// 🔧 修改 - 添加滚木旋转预览可视化

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "SG_RollingLogSpawner.generated.h"

// 前置声明
class ASG_RollingLog;
class USG_RollingLogCardData;
class UAbilitySystemComponent;
class UArrowComponent;
class UBoxComponent;
class UBillboardComponent;
class UStaticMeshComponent;  // ✨ 新增 - 预览网格体

/**
 * @brief 流木计生成器状态枚举
 */
UENUM(BlueprintType)
enum class ESGSpawnerState : uint8
{
    Idle        UMETA(DisplayName = "待机"),
    Active      UMETA(DisplayName = "激活"),
    Cooldown    UMETA(DisplayName = "冷却")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSGSpawnerActivatedSignature, ASG_RollingLogSpawner*, Spawner);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSGSpawnerDeactivatedSignature, ASG_RollingLogSpawner*, Spawner);

/**
 * @brief 流木计生成器
 * 
 * @details
 * **功能说明：**
 * - 预先放置在场景中
 * - 方向由 Actor 的朝向决定（箭头指向滚动方向）
 * - ✨ 新增 - 可视化预览生成滚木的旋转
 * - 当玩家使用流木计卡牌时被激活
 * 
 * **可视化说明：**
 * - 红色箭头：滚动方向
 * - 预览网格体：显示滚木生成时的朝向
 * - 可在编辑器中调整 SpawnRotationOffset 来修改生成旋转
 */
UCLASS(BlueprintType, Blueprintable)
class SGUO_API ASG_RollingLogSpawner : public AActor
{
    GENERATED_BODY()

public:
    ASG_RollingLogSpawner();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

#if WITH_EDITOR
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

public:
    // ==================== 事件委托 ====================

    UPROPERTY(BlueprintAssignable, Category = "Spawner Events", meta = (DisplayName = "激活事件"))
    FSGSpawnerActivatedSignature OnSpawnerActivated;

    UPROPERTY(BlueprintAssignable, Category = "Spawner Events", meta = (DisplayName = "停止事件"))
    FSGSpawnerDeactivatedSignature OnSpawnerDeactivated;

    // ==================== 组件 ====================

    /**
     * @brief 场景根组件
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "场景根"))
    TObjectPtr<USceneComponent> SceneRoot;

    /**
     * @brief 方向指示箭头（编辑器可见）
     * @details 箭头方向即滚木滚动方向
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "方向箭头"))
    TObjectPtr<UArrowComponent> DirectionArrow;

    /**
     * @brief 生成区域可视化（编辑器可见）
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "生成区域"))
    TObjectPtr<UBoxComponent> SpawnAreaBox;

    /**
     * @brief 广告牌组件（编辑器可见）
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "广告牌"))
    TObjectPtr<UBillboardComponent> BillboardComponent;

    // ✨ 新增 - 滚木预览组件
    /**
     * @brief 滚木旋转预览组件
     * @details 
     * - 在编辑器中显示滚木生成时的朝向
     * - 可以直观地看到滚木的旋转
     * - 运行时隐藏
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "滚木预览"))
    TObjectPtr<UStaticMeshComponent> LogPreviewMesh;

    // ==================== 生成旋转配置 ==================== // ✨ 新增区域

    /**
     * @brief 生成旋转偏移
     * @details 
     * - 相对于生成器朝向的额外旋转
     * - 用于调整滚木生成时的初始姿态
     * - 可在编辑器中实时预览
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Rotation", meta = (DisplayName = "生成旋转偏移"))
    FRotator SpawnRotationOffset = FRotator::ZeroRotator;

    /**
     * @brief 是否使用自定义生成旋转
     * @details 
     * - true: 使用 SpawnRotationOffset
     * - false: 使用滚动方向自动计算旋转
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Rotation", meta = (DisplayName = "使用自定义旋转"))
    bool bUseCustomSpawnRotation = false;

    /**
     * @brief 预览网格体
     * @details 用于在编辑器中显示的预览网格
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Rotation", meta = (DisplayName = "预览网格"))
    TObjectPtr<UStaticMesh> PreviewMesh;

    /**
     * @brief 预览网格体缩放
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Rotation", meta = (DisplayName = "预览缩放"))
    FVector PreviewMeshScale = FVector(1.0f, 1.0f, 1.0f);

    /**
     * @brief 显示预览网格体
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Rotation", meta = (DisplayName = "显示预览网格"))
    bool bShowPreviewMesh = true;

    /**
     * @brief 预览网格体透明度
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Rotation", meta = (DisplayName = "预览透明度", ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
    float PreviewMeshOpacity = 0.5f;

    // ==================== 配置 ====================

    /**
     * @brief 生成器所属阵营
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner Config", 
        meta = (DisplayName = "所属阵营", Categories = "Unit.Faction"))
    FGameplayTag FactionTag;

    /**
     * @brief 默认滚木类（备用）
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Spawner Config", 
        meta = (DisplayName = "默认滚木类"))
    TSubclassOf<ASG_RollingLog> DefaultRollingLogClass;

    /**
     * @brief 生成区域宽度（厘米）
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner Config", 
        meta = (DisplayName = "生成区域宽度", ClampMin = "0.0", UIMin = "0.0", UIMax = "2000.0"))
    float SpawnAreaWidth = 800.0f;

    /**
     * @brief 生成高度偏移（厘米）
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner Config", 
        meta = (DisplayName = "生成高度偏移", ClampMin = "0.0", UIMin = "0.0", UIMax = "500.0"))
    float SpawnHeightOffset = 50.0f;

    /**
     * @brief 激活后冷却时间（秒）
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner Config", 
        meta = (DisplayName = "冷却时间", ClampMin = "0.0", UIMin = "0.0", UIMax = "60.0"))
    float CooldownTime = 0.0f;

    // ==================== 广告牌配置 ====================

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Billboard Config", 
        meta = (DisplayName = "广告牌图标"))
    TObjectPtr<UTexture2D> BillboardSprite;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Billboard Config", 
        meta = (DisplayName = "广告牌缩放", ClampMin = "0.1", UIMin = "0.1", UIMax = "10.0"))
    float BillboardScale = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Billboard Config", 
        meta = (DisplayName = "广告牌高度偏移", UIMin = "0.0", UIMax = "500.0"))
    float BillboardHeightOffset = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Billboard Config", 
        meta = (DisplayName = "运行时显示广告牌"))
    bool bShowBillboardAtRuntime = false;

public:
    // ==================== 核心接口 ====================

    UFUNCTION(BlueprintCallable, Category = "Spawner", meta = (DisplayName = "激活生成器"))
    bool Activate(USG_RollingLogCardData* CardData, UAbilitySystemComponent* InSourceASC = nullptr);

    UFUNCTION(BlueprintCallable, Category = "Spawner", meta = (DisplayName = "停止生成器"))
    void Deactivate();

    UFUNCTION(BlueprintPure, Category = "Spawner", meta = (DisplayName = "获取状态"))
    ESGSpawnerState GetCurrentState() const { return CurrentState; }

    UFUNCTION(BlueprintPure, Category = "Spawner", meta = (DisplayName = "是否可激活"))
    bool CanActivate() const { return CurrentState == ESGSpawnerState::Idle; }

    UFUNCTION(BlueprintPure, Category = "Spawner", meta = (DisplayName = "获取滚动方向"))
    FVector GetRollDirection() const;

    // ✨ 新增 - 获取生成旋转
    /**
     * @brief 获取滚木生成时的旋转
     * @return 生成旋转（世界空间）
     */
    UFUNCTION(BlueprintPure, Category = "Spawner", meta = (DisplayName = "获取生成旋转"))
    FRotator GetSpawnRotation() const;

    UFUNCTION(BlueprintPure, Category = "Spawner", meta = (DisplayName = "获取剩余时间"))
    float GetRemainingTime() const { return RemainingDuration; }

    UFUNCTION(BlueprintPure, Category = "Spawner", meta = (DisplayName = "获取冷却剩余时间"))
    float GetCooldownRemainingTime() const { return CooldownRemainingTime; }

    // ✨ 新增 - 预览控制
    /**
     * @brief 更新预览网格体
     */
    UFUNCTION(BlueprintCallable, Category = "Spawner", meta = (DisplayName = "更新预览"))
    void UpdatePreviewMesh();

    /**
     * @brief 设置预览可见性
     */
    UFUNCTION(BlueprintCallable, Category = "Spawner", meta = (DisplayName = "设置预览可见性"))
    void SetPreviewVisibility(bool bVisible);

    // 广告牌控制
    UFUNCTION(BlueprintCallable, Category = "Billboard", meta = (DisplayName = "设置广告牌可见性"))
    void SetBillboardVisibility(bool bVisible);

    UFUNCTION(BlueprintCallable, Category = "Billboard", meta = (DisplayName = "更新广告牌图标"))
    void UpdateBillboardSprite(UTexture2D* NewSprite);

protected:
    // ==================== 内部状态 ====================

    UPROPERTY(BlueprintReadOnly, Category = "Spawner State", meta = (DisplayName = "当前状态"))
    ESGSpawnerState CurrentState = ESGSpawnerState::Idle;

    UPROPERTY(Transient)
    TObjectPtr<USG_RollingLogCardData> ActiveCardData;

    UPROPERTY(Transient)
    TObjectPtr<UAbilitySystemComponent> SourceASC;

    float SpawnTimer = 0.0f;
    float RemainingDuration = 0.0f;
    float CooldownRemainingTime = 0.0f;

    UPROPERTY(Transient)
    TArray<TWeakObjectPtr<ASG_RollingLog>> SpawnedLogs;

    // ✨ 新增 - 预览材质实例
    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> PreviewMaterialInstance;

protected:
    // ==================== 内部函数 ====================

    void SpawnRollingLogs();
    FVector CalculateRandomSpawnLocation() const;

    UFUNCTION()
    void OnRollingLogDestroyed(ASG_RollingLog* DestroyedLog);

    void EnterCooldown();
    void UpdateSpawnAreaVisualization();
    void SetupBillboard();

    // ✨ 新增 - 设置预览网格体
    void SetupPreviewMesh();

    // ✨ 新增 - 创建预览材质
    void CreatePreviewMaterial();

public:
    // ==================== 蓝图事件 ====================

    UFUNCTION(BlueprintImplementableEvent, Category = "Spawner", meta = (DisplayName = "On Log Spawned (BP)"))
    void K2_OnLogSpawned(ASG_RollingLog* SpawnedLog);

    UFUNCTION(BlueprintImplementableEvent, Category = "Spawner", meta = (DisplayName = "On Activated (BP)"))
    void K2_OnActivated();

    UFUNCTION(BlueprintImplementableEvent, Category = "Spawner", meta = (DisplayName = "On Deactivated (BP)"))
    void K2_OnDeactivated();

    UFUNCTION(BlueprintImplementableEvent, Category = "Spawner", meta = (DisplayName = "On Cooldown Finished (BP)"))
    void K2_OnCooldownFinished();

    // ==================== 调试配置 ====================

#if WITH_EDITORONLY_DATA
    UPROPERTY(EditAnywhere, Category = "Debug", meta = (DisplayName = "显示生成区域"))
    bool bShowSpawnArea = true;

    UPROPERTY(EditAnywhere, Category = "Debug", meta = (DisplayName = "显示滚动方向"))
    bool bShowRollDirection = true;

    UPROPERTY(EditAnywhere, Category = "Debug", meta = (DisplayName = "显示生成旋转坐标轴"))
    bool bShowSpawnRotationAxis = true;
#endif
};
