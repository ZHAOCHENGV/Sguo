// 📄 文件：Source/Sguo/Public/Strategies/SG_StrategyEffect_RollingLog.h
// 🔧 修改 - 完整文件，添加方向计算和修复编译错误

#pragma once

#include "CoreMinimal.h"
#include "Strategies/SG_StrategyEffectBase.h"
#include "SG_StrategyEffect_RollingLog.generated.h"

// 前置声明
class ASG_RollingLog;
class UDecalComponent;
class UNiagaraComponent;
class ASG_MainCityBase;

/**
 * @brief 滚木滚动方向枚举
 * @details 定义滚木相对于主城连线的滚动方向
 */
UENUM(BlueprintType)
enum class ESGRollingLogDirection : uint8
{
    /** 向左滚动（相对于主城连线） */
    Left        UMETA(DisplayName = "向左"),
    
    /** 向右滚动（相对于主城连线） */
    Right       UMETA(DisplayName = "向右"),
    
    /** 向前滚动（朝向敌方主城） */
    Forward     UMETA(DisplayName = "向前"),
    
    /** 自定义方向 */
    Custom      UMETA(DisplayName = "自定义")
};

/**
 * @brief 滚木生成配置
 * @details 用于配置滚木的生成参数
 */
USTRUCT(BlueprintType)
struct FSGRollingLogSpawnConfig
{
    GENERATED_BODY()

    /** 滚木类 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Spawn Config", meta = (DisplayName = "滚木类"))
    TSubclassOf<ASG_RollingLog> RollingLogClass;

    /** 生成间隔（秒） */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Spawn Config", meta = (DisplayName = "生成间隔", ClampMin = "0.1", UIMin = "0.1", UIMax = "5.0"))
    float SpawnInterval = 0.5f;

    /** 每次生成数量 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Spawn Config", meta = (DisplayName = "每次生成数量", ClampMin = "1", UIMin = "1", UIMax = "10"))
    int32 SpawnCountPerInterval = 1;

    /** 生成区域宽度（厘米） - 垂直于滚动方向 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Spawn Config", meta = (DisplayName = "生成区域宽度", ClampMin = "100.0", UIMin = "100.0", UIMax = "2000.0"))
    float SpawnAreaWidth = 800.0f;

    /** 生成区域长度（厘米）- 沿滚动方向 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Spawn Config", meta = (DisplayName = "生成区域长度", ClampMin = "100.0", UIMin = "100.0", UIMax = "1000.0"))
    float SpawnAreaLength = 200.0f;

    /** 生成高度偏移（厘米） */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Spawn Config", meta = (DisplayName = "生成高度偏移", ClampMin = "0.0", UIMin = "0.0", UIMax = "500.0"))
    float SpawnHeightOffset = 50.0f;
};

/**
 * @brief 流木计效果类
 * 
 * @details
 * **功能说明：**
 * - 发动后持续生成滚动的木桩
 * - 木桩沿指定方向滚动（向左/向右/向前）
 * - 方向基于我方主城到敌方主城的连线计算
 * - 击中敌人造成伤害并击退
 * - 木桩击中一个目标后破碎
 * 
 * **方向说明：**
 * - Forward：沿主城连线方向滚动（朝敌方）
 * - Left：垂直于主城连线，向左滚动
 * - Right：垂直于主城连线，向右滚动
 * - Custom：玩家自定义方向
 * 
 * **详细流程：**
 * 1. 玩家选择目标位置
 * 2. 玩家选择滚动方向（左/右/前）
 * 3. 显示预览效果（箭头指示方向）
 * 4. 确认后开始持续生成滚木
 * 5. 滚木沿指定方向滚动
 * 6. 持续时间结束后停止生成
 * 
 * **注意事项：**
 * - 滚木位置在生成区域内随机
 * - 所有滚木方向相同
 * - 6秒持续时间内持续生成
 */
UCLASS(BlueprintType, Blueprintable)
class SGUO_API ASG_StrategyEffect_RollingLog : public ASG_StrategyEffectBase
{
    GENERATED_BODY()

public:
    ASG_StrategyEffect_RollingLog();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    // ==================== 接口重写 ====================

    /**
     * @brief 检查是否需要目标选择
     * @return 需要（玩家需要选择位置和方向）
     */
    virtual bool RequiresTargetSelection_Implementation() const override;

    /**
     * @brief 开始目标选择
     * @return 是否成功开始
     */
    virtual bool StartTargetSelection_Implementation() override;

    /**
     * @brief 更新目标位置
     * @param NewLocation 新的目标位置
     */
    virtual void UpdateTargetLocation_Implementation(const FVector& NewLocation) override;

    /**
     * @brief 确认目标
     * @return 是否成功确认
     */
    virtual bool ConfirmTarget_Implementation() override;

    /**
     * @brief 取消效果
     */
    virtual void CancelEffect_Implementation() override;

    /**
     * @brief 执行效果
     */
    virtual void ExecuteEffect_Implementation() override;

public:
    // ==================== 滚木生成配置 ====================

    /**
     * @brief 滚木生成配置
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Rolling Log Config", meta = (DisplayName = "滚木生成配置"))
    FSGRollingLogSpawnConfig SpawnConfig;

    /**
     * @brief 效果持续时间（秒）
     * @details 覆盖卡牌数据中的持续时间
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Rolling Log Config", meta = (DisplayName = "效果持续时间", ClampMin = "1.0", UIMin = "1.0", UIMax = "30.0"))
    float RollingLogDuration = 6.0f;

    // ==================== ✨ 新增 - 方向配置 ====================

    /**
     * @brief 默认滚动方向类型
     * @details 基于主城连线计算实际方向
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Direction Config", meta = (DisplayName = "默认滚动方向"))
    ESGRollingLogDirection DefaultRollDirectionType = ESGRollingLogDirection::Left;

    /**
     * @brief 当前滚动方向类型
     */
    UPROPERTY(BlueprintReadWrite, Category = "Direction Config", meta = (DisplayName = "当前滚动方向类型"))
    ESGRollingLogDirection CurrentRollDirectionType = ESGRollingLogDirection::Left;

    // ==================== 预览配置 ====================

    /**
     * @brief 方向指示箭头网格
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Preview Config", meta = (DisplayName = "方向箭头网格"))
    UStaticMesh* DirectionArrowMesh;  // 🔧 修改 - 使用原始指针

    /**
     * @brief 方向指示箭头材质
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Preview Config", meta = (DisplayName = "方向箭头材质"))
    UMaterialInterface* DirectionArrowMaterial;  // 🔧 修改 - 使用原始指针

    /**
     * @brief 区域预览贴花材质
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Preview Config", meta = (DisplayName = "区域预览贴花材质"))
    UMaterialInterface* AreaPreviewDecalMaterial;  // 🔧 修改 - 使用原始指针

    /**
     * @brief 预览颜色（有效位置）
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Preview Config", meta = (DisplayName = "有效位置颜色"))
    FLinearColor ValidPreviewColor = FLinearColor(0.0f, 1.0f, 0.0f, 0.5f);

    /**
     * @brief 预览颜色（无效位置）
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Preview Config", meta = (DisplayName = "无效位置颜色"))
    FLinearColor InvalidPreviewColor = FLinearColor(1.0f, 0.0f, 0.0f, 0.5f);

public:
    // ==================== 方向控制 ====================

    /**
     * @brief 设置滚动方向类型
     * @param NewDirectionType 新的方向类型
     * @details 会自动计算基于主城连线的实际方向
     */
    UFUNCTION(BlueprintCallable, Category = "Rolling Log", meta = (DisplayName = "设置滚动方向类型"))
    void SetRollDirectionType(ESGRollingLogDirection NewDirectionType);

    /**
     * @brief 设置自定义滚动方向
     * @param NewDirection 新的滚动方向（会归一化到水平面）
     */
    UFUNCTION(BlueprintCallable, Category = "Rolling Log", meta = (DisplayName = "设置自定义滚动方向"))
    void SetCustomRollDirection(FVector NewDirection);

    /**
     * @brief 获取当前滚动方向
     * @return 当前滚动方向（世界空间）
     */
    UFUNCTION(BlueprintPure, Category = "Rolling Log", meta = (DisplayName = "获取滚动方向"))
    FVector GetRollDirection() const { return RollDirection; }

    /**
     * @brief 获取主城连线方向
     * @return 从我方主城指向敌方主城的方向
     */
    UFUNCTION(BlueprintPure, Category = "Rolling Log", meta = (DisplayName = "获取主城连线方向"))
    FVector GetMainCityLineDirection() const { return MainCityLineDirection; }

    /**
     * @brief 切换到下一个方向（左->右->前->左）
     */
    UFUNCTION(BlueprintCallable, Category = "Rolling Log", meta = (DisplayName = "切换方向"))
    void CycleRollDirection();

    /**
     * @brief 旋转滚动方向（仅自定义模式）
     * @param DeltaYaw 旋转角度（度）
     */
    UFUNCTION(BlueprintCallable, Category = "Rolling Log", meta = (DisplayName = "旋转滚动方向"))
    void RotateRollDirection(float DeltaYaw);

protected:
    // ==================== 内部状态 ====================

    /** 滚动方向（归一化，水平面） */
    FVector RollDirection = FVector::ForwardVector;

    /** ✨ 新增 - 主城连线方向（从我方指向敌方） */
    FVector MainCityLineDirection = FVector::ForwardVector;

    /** ✨ 新增 - 主城连线的右方向 */
    FVector MainCityLineRight = FVector::RightVector;

    /** 生成计时器 */
    float SpawnTimer = 0.0f;

    /** 效果已持续时间 */
    float ElapsedTime = 0.0f;

    /** 是否正在执行 */
    bool bIsExecuting = false;

    /** ✨ 新增 - 是否已计算主城方向 */
    bool bMainCityDirectionCalculated = false;

    /** 已生成的滚木列表 */
    UPROPERTY()
    TArray<TWeakObjectPtr<ASG_RollingLog>> SpawnedLogs;

    // ==================== 预览组件 ====================

    /** 方向箭头网格组件 */
    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> ArrowMeshComponent;

    /** 区域预览贴花组件 */
    UPROPERTY()
    TObjectPtr<UDecalComponent> AreaDecalComponent;

    /** 预览特效组件 */
    UPROPERTY()
    TObjectPtr<UNiagaraComponent> PreviewEffectComponent;

protected:
    // ==================== 内部函数 ====================

    /**
     * @brief 创建预览组件
     */
    void CreatePreviewComponents();

    /**
     * @brief 更新预览显示
     */
    void UpdatePreviewDisplay();

    /**
     * @brief 隐藏预览
     */
    void HidePreview();

    /**
     * @brief 显示预览
     */
    void ShowPreview();

    /**
     * @brief 生成滚木
     */
    void SpawnRollingLogs();

    /**
     * @brief 计算随机生成位置
     * @return 生成位置（世界坐标）
     */
    FVector CalculateRandomSpawnLocation() const;

    /**
     * @brief 清理所有已生成的滚木
     */
    void CleanupSpawnedLogs();

    /**
     * @brief 滚木销毁回调
     * @param DestroyedLog 被销毁的滚木
     */
    UFUNCTION()
    void OnRollingLogDestroyed(ASG_RollingLog* DestroyedLog);

    // ==================== ✨ 新增 - 主城方向计算 ====================

    /**
     * @brief 计算主城连线方向
     * @details 查找我方和敌方主城，计算连线方向
     */
    void CalculateMainCityDirection();

    /**
     * @brief 根据方向类型计算实际滚动方向
     */
    void UpdateRollDirectionFromType();

    /**
     * @brief 查找指定阵营的主城
     * @param FactionTag 阵营标签
     * @return 主城 Actor，未找到返回 nullptr
     */
    ASG_MainCityBase* FindMainCityByFaction(const FGameplayTag& FactionTag) const;

public:
    // ==================== 蓝图事件 ====================

    /**
     * @brief 滚木生成蓝图事件
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "Rolling Log", meta = (DisplayName = "On Log Spawned (BP)"))
    void K2_OnLogSpawned(ASG_RollingLog* SpawnedLog);

    /**
     * @brief 效果开始蓝图事件
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "Rolling Log", meta = (DisplayName = "On Effect Started (BP)"))
    void K2_OnEffectStarted();

    /**
     * @brief 效果结束蓝图事件
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "Rolling Log", meta = (DisplayName = "On Effect Ended (BP)"))
    void K2_OnEffectEnded();

    /**
     * @brief ✨ 新增 - 方向改变蓝图事件
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "Rolling Log", meta = (DisplayName = "On Direction Changed (BP)"))
    void K2_OnDirectionChanged(ESGRollingLogDirection NewDirection, FVector NewDirectionVector);
};
