// 📄 文件：GameplayMechanics/SG_FrontLineManager.h

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameFramework/Actor.h"
#include "SG_FrontLineManager.generated.h"

// 前置声明
class USplineComponent;
class ASG_UnitsBase;
class UBillboardComponent;
class ASG_MainCityBase;
// ✨ 新增 - 静态网格体组件前置声明
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class ESGFrontLineZone : uint8
{
    // 玩家控制区域
    PlayerZone      UMETA(DisplayName = "Player Zone"),
    
    // 中立区域（双方前线之间）
    NeutralZone     UMETA(DisplayName = "Neutral Zone"),
    
    // 敌人控制区域
    EnemyZone       UMETA(DisplayName = "Enemy Zone")
};

/**
 * @brief 前线管理器（实时跟随版 + 可视化网格体）
 * @details
 * 功能说明：
 * - ⚡ 优化 - 每帧直接读取最前方单位位置
 * - ⚡ 优化 - 前线实时跟随，无任何延迟
 * - ⚡ 优化 - 使用缓存减少查询次数
 * - ✨ 新增 - 玩家前线可视化静态网格体（运行时可见）
 */
UCLASS(BlueprintType, Blueprintable)
class SGUO_API ASG_FrontLineManager : public AActor
{
    GENERATED_BODY()

public:
    /**
     * @brief 构造函数
     * @details 初始化组件和默认值
     */
    ASG_FrontLineManager();

    // ========== 组件 ==========
    
    /**
     * @brief 根组件
     * @details 作为所有子组件的父级
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "根组件"))
    USceneComponent* RootComp;
    
    /**
     * @brief 玩家前线样条线组件
     * @details 用于可视化玩家前线位置，在编辑器中显示为蓝色线条
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "玩家前线样条线"))
    USplineComponent* PlayerFrontLineSpline;



    /**
     * @brief Actor 广告牌组件
     * @details 在编辑器中显示的图标，方便在场景中定位该 Actor
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "Actor图标"))
    UBillboardComponent* ActorBillboard;

    // ✨ 新增 - 玩家前线可视化网格体组件
    /**
     * @brief 玩家前线可视化网格体
     * @details 
     * 功能说明：
     * - 在游戏运行时显示玩家前线的3D模型
     * - 跟随前线位置实时移动
     * - 可在编辑器中设置网格体和材质
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "玩家前线网格体"))
    UStaticMeshComponent* PlayerFrontLineMesh;

    // ========== 前线配置 ==========
    
    /**
     * @brief 初始前线 X 坐标
     * @details 游戏开始时双方前线的初始位置，通常设置在地图中央
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Front Line", meta = (DisplayName = "初始前线X坐标"))
    float InitialFrontLineX = 0.0f;
    
    /**
     * @brief 前线宽度
     * @details 前线在 Y 轴方向的延伸范围，决定前线的长度
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Front Line", meta = (DisplayName = "前线宽度"))
    float FrontLineWidth = 5000.0f;
    
    /**
     * @brief 前线高度
     * @details 前线在 Z 轴方向的位置，用于可视化显示
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Front Line", meta = (DisplayName = "前线高度"))
    float FrontLineHeight = 10.0f;
    
    /**
     * @brief 玩家前线颜色
     * @details 用于调试绘制时显示玩家前线的颜色（默认蓝色）
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Front Line", meta = (DisplayName = "玩家前线颜色"))
    FLinearColor PlayerFrontLineColor = FLinearColor::Blue;

    /**
     * @brief 敌人前线颜色
     * @details 用于调试绘制时显示敌人前线的颜色（默认红色）
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Front Line", meta = (DisplayName = "敌人前线颜色"))
    FLinearColor EnemyFrontLineColor = FLinearColor::Red;
    
    /**
     * @brief 前线粗细
     * @details 调试绘制时前线的线条粗细
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Front Line", meta = (DisplayName = "前线线条粗细"))
    float FrontLineThickness = 10.0f;
    
    /**
     * @brief 是否启用调试绘制
     * @details 开启后会在游戏运行时绘制前线、区域和单位标记
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Front Line", meta = (DisplayName = "启用调试绘制"))
    bool bEnableDebugDraw = true;

    /**
     * @brief 最小前线间距
     * @details 双方前线之间的最小距离，防止前线重叠
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Front Line", meta = (DisplayName = "最小前线间距"))
    float MinFrontLineDistance = 500.0f;

    /**
     * @brief 前线偏移量
     * @details 前线相对于最前方单位的偏移距离，前线会在单位前方这个距离处
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Front Line", meta = (DisplayName = "前线偏移量"))
    float FrontLineOffset = 150.0f;

    /**
     * @brief 是否只追踪越过初始线的单位
     * @details 
     * - true：只有越过初始前线的单位才会被追踪（推荐）
     * - false：追踪所有单位，即使在己方区域内
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Front Line", meta = (DisplayName = "只追踪越线单位"))
    bool bOnlyTrackCrossedUnits = true;

    /**
     * @brief 重新扫描间隔（秒）
     * @details 
     * 功能说明：
     * - 定期重新扫描所有单位，找到新的最前方单位
     * - 在两次扫描之间，直接读取缓存单位的位置（实时跟随）
     * - 当最前方单位死亡时，会立即触发重新扫描
     * 
     * 建议值：
     * - 0.5 ~ 2.0 秒（平衡性能和准确性）
     * - 值越小，切换最前方单位越及时，但性能开销越大
     * - 值越大，性能越好，但可能延迟发现新的最前方单位
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Front Line", meta = (DisplayName = "重新扫描间隔(秒)", ClampMin = "0", ClampMax = "10.0"))
    float RescanInterval = 1.0f;

    // ✨ 新增 - 前线可视化配置
    // ========== 前线可视化配置 ==========
    
    /**
     * @brief 是否显示玩家前线网格体
     * @details 开启后在游戏运行时显示玩家前线的3D模型
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Front Line", meta = (DisplayName = "显示玩家前线网格体"))
    bool bShowPlayerFrontLineMesh = true;

    /**
     * @brief 前线网格体缩放
     * @details 
     * 功能说明：
     * - 控制前线网格体的缩放大小
     * - X：前线方向（通常不需要缩放）
     * - Y：前线宽度方向（根据 FrontLineWidth 自动计算）
     * - Z：前线高度方向
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Front Line", meta = (DisplayName = "前线网格体缩放"))
    FVector FrontLineMeshScale = FVector(1.0f, 1.0f, 1.0f);

    // ========== 查询接口 ==========
    
    /**
     * @brief 获取玩家前线 X 坐标
     * @return 当前玩家前线的 X 坐标
     */
    UFUNCTION(BlueprintPure, Category = "Front Line", meta = (DisplayName = "获取玩家前线X坐标"))
    float GetPlayerFrontLineX() const { return CurrentPlayerFrontLineX; }

    /**
     * @brief 获取敌人前线 X 坐标
     * @return 当前敌人前线的 X 坐标
     */
    UFUNCTION(BlueprintPure, Category = "Front Line", meta = (DisplayName = "获取敌人前线X坐标"))
    float GetEnemyFrontLineX() const { return CurrentEnemyFrontLineX; }

    /**
     * @brief 获取玩家主城 X 坐标
     * @return 玩家主城的 X 坐标
     */
    UFUNCTION(BlueprintPure, Category = "Front Line", meta = (DisplayName = "获取玩家主城X坐标"))
    float GetPlayerMainCityX() const { return PlayerMainCityX; }

    /**
     * @brief 获取敌人主城 X 坐标
     * @return 敌人主城的 X 坐标
     */
    UFUNCTION(BlueprintPure, Category = "Front Line", meta = (DisplayName = "获取敌人主城X坐标"))
    float GetEnemyMainCityX() const { return EnemyMainCityX; }

    /**
     * @brief 玩家是否在左侧
     * @return true：玩家在左侧，敌人在右侧；false：玩家在右侧，敌人在左侧
     */
    UFUNCTION(BlueprintPure, Category = "Front Line", meta = (DisplayName = "玩家是否在左侧"))
    bool IsPlayerOnLeftSide() const { return bPlayerOnLeftSide; }

    /**
     * @brief 获取指定位置所属的区域
     * @param Location 要查询的世界坐标位置
     * @return 该位置所属的区域类型（玩家区域/中立区域/敌人区域）
     */
    UFUNCTION(BlueprintPure, Category = "Front Line", meta = (DisplayName = "获取位置所属区域"))
    ESGFrontLineZone GetZoneAtLocation(const FVector& Location) const;
    
    /**
     * @brief 判断位置是否在玩家区域
     * @param Location 要查询的世界坐标位置
     * @return true：在玩家区域；false：不在玩家区域
     */
    UFUNCTION(BlueprintPure, Category = "Front Line", meta = (DisplayName = "是否在玩家区域"))
    bool IsInPlayerZone(const FVector& Location) const;
    
    /**
     * @brief 判断位置是否在敌人区域
     * @param Location 要查询的世界坐标位置
     * @return true：在敌人区域；false：不在敌人区域
     */
    UFUNCTION(BlueprintPure, Category = "Front Line", meta = (DisplayName = "是否在敌人区域"))
    bool IsInEnemyZone(const FVector& Location) const;

    /**
     * @brief 判断位置是否在中立区域
     * @param Location 要查询的世界坐标位置
     * @return true：在中立区域；false：不在中立区域
     */
    UFUNCTION(BlueprintPure, Category = "Front Line", meta = (DisplayName = "是否在中立区域"))
    bool IsInNeutralZone(const FVector& Location) const;

    /**
     * @brief 获取玩家最前方单位
     * @return 当前玩家最前方的作战单位指针，如果没有则返回 nullptr
     */
    UFUNCTION(BlueprintPure, Category = "Front Line", meta = (DisplayName = "获取玩家最前方单位"))
    ASG_UnitsBase* GetPlayerFrontmostUnit() const { return CachedPlayerFrontmostUnit; }

    /**
     * @brief 获取敌人最前方单位
     * @return 当前敌人最前方的作战单位指针，如果没有则返回 nullptr
     */
    UFUNCTION(BlueprintPure, Category = "Front Line", meta = (DisplayName = "获取敌人最前方单位"))
    ASG_UnitsBase* GetEnemyFrontmostUnit() const { return CachedEnemyFrontmostUnit; }
    
    /**
     * @brief 获取前线管理器单例
     * @param WorldContextObject 世界上下文对象
     * @return 前线管理器实例指针，如果不存在则返回 nullptr
     * @details 使用静态缓存优化查询性能
     */
    UFUNCTION(BlueprintPure, Category = "Front Line", meta = (WorldContext = "WorldContextObject", DisplayName = "获取前线管理器"))
    static ASG_FrontLineManager* GetFrontLineManager(UObject* WorldContextObject);


    /**
     * @brief 单位死亡回调
     * @param DeadUnit 死亡的单位
     * @details
     * 功能说明：
     * - 当最前方单位死亡时触发
     * - 立即清除缓存并重新扫描
     * - 确保前线始终跟踪有效单位
     */
    UFUNCTION()
    void OnUnitDeath(ASG_UnitsBase* DeadUnit);
    
    
protected:
    /**
     * @brief 游戏开始时调用
     * @details 
     * 执行流程：
     * 1. 查找并缓存双方主城位置
     * 2. 确定玩家和敌人的方向（左/右）
     * 3. 打印初始化日志
     * 4. 设置前线初始位置
     * 5. 立即执行一次单位扫描
     * 6. 更新可视化
     * 7. 启动定时重新扫描
     */
    virtual void BeginPlay() override;
    
    /**
     * @brief 每帧更新
     * @param DeltaTime 距离上一帧的时间间隔（秒）
     * @details 
     * 执行流程：
     * 1. 从缓存的最前方单位读取实时位置
     * 2. 更新前线位置（无插值，零延迟）
     * 3. 调整前线间距，防止重叠
     * 4. 更新可视化（样条线 + 网格体）
     * 5. 绘制调试信息
     */
    virtual void Tick(float DeltaTime) override;

    /**
     * @brief 绑定单位死亡事件
     * @param Unit 要绑定的单位
     * @details 将 OnUnitDeath 函数绑定到单位的死亡委托
     */
    UFUNCTION()
    void BindUnitDeathEvent(ASG_UnitsBase* Unit);
    
    /**
     * @brief 解绑单位死亡事件
     * @param Unit 要解绑的单位
     * @details 从单位的死亡委托中移除 OnUnitDeath 函数
     */
    UFUNCTION()
    void UnbindUnitDeathEvent(ASG_UnitsBase* Unit);


private:
    /**
     * @brief 重新扫描最前方单位
     * @details 
     * 执行流程：
     * 1. 获取场景中所有单位
     * 2. 根据阵营和位置筛选最前方单位
     * 3. 更新缓存的最前方单位
     * 4. 解绑旧单位的死亡事件
     * 5. 绑定新单位的死亡事件
     * 
     * 调用时机：
     * - BeginPlay 时立即调用一次
     * - 之后每隔 RescanInterval 秒调用一次
     * - 最前方单位死亡时立即调用
     */
    void RescanFrontmostUnits();
    
    /**
     * @brief 更新前线位置（每帧调用）
     * @details 
     * 执行流程：
     * 1. 从缓存单位读取实时位置（无需遍历所有单位）
     * 2. 计算新的前线位置（单位位置 + 偏移量）
     * 3. 直接设置前线位置（无插值）
     * 4. 调整前线间距
     * 5. 更新可视化
     * 
     * 性能优化：
     * - 只读取2个单位的位置，O(1) 复杂度
     * - 无插值计算，零延迟跟随
     * - 只在位置改变时更新可视化
     */
    void UpdateFrontLinePositionRealtime();
    
    /**
     * @brief 更新前线可视化
     * @details 
     * 更新内容：
     * - 更新样条线位置
     * - 更新玩家前线网格体位置和缩放
     */
    void UpdateFrontLineVisualization();
    
    /**
     * @brief 绘制调试信息
     * @details 
     * 绘制内容：
     * - 玩家前线（蓝色实线）
     * - 敌人前线（红色实线）
     * - 中立区中线（黄色虚线）
     * - 前线位置文字
     * - 最前方单位标记（球体 + 文字）
     */
    void DrawDebugInfo();
    
    /**
     * @brief 调整前线间距
     * @details 
     * 功能说明：
     * - 确保双方前线不会过于接近
     * - 当前线间距小于 MinFrontLineDistance 时，将双方前线向外推开
     * - 保持前线间距至少为 MinFrontLineDistance
     */
    void AdjustFrontLineDistance();
    
    /**
     * @brief 查找并缓存主城位置
     * @details 
     * 执行流程：
     * 1. 获取场景中所有主城
     * 2. 根据 Faction 标签区分玩家和敌人主城
     * 3. 缓存主城引用和位置
     * 4. 根据主城位置确定玩家方向（左/右）
     */
    void FindAndCacheMainCities();
    
 


private:
    // ========== 运行时数据 ==========
    
    // 当前玩家前线的 X 坐标
    float CurrentPlayerFrontLineX = 0.0f;
    
    // 当前敌人前线的 X 坐标
    float CurrentEnemyFrontLineX = 0.0f;
    
    // 玩家主城的 X 坐标
    float PlayerMainCityX = 0.0f;
    
    // 敌人主城的 X 坐标
    float EnemyMainCityX = 0.0f;
    
    // 玩家是否在左侧（true：玩家在左，敌人在右；false：玩家在右，敌人在左）
    bool bPlayerOnLeftSide = true;
    
    // 重新扫描定时器句柄
    FTimerHandle RescanTimerHandle;
    
    // ========== 缓存数据 ==========
    
    // 缓存的玩家最前方单位
    UPROPERTY(Transient)
    ASG_UnitsBase* CachedPlayerFrontmostUnit = nullptr;

    // 缓存的敌人最前方单位
    UPROPERTY(Transient)
    ASG_UnitsBase* CachedEnemyFrontmostUnit = nullptr;

    // 缓存的玩家主城
    UPROPERTY(Transient)
    ASG_MainCityBase* CachedPlayerMainCity = nullptr;

    // 缓存的敌人主城
    UPROPERTY(Transient)
    ASG_MainCityBase* CachedEnemyMainCity = nullptr;


public:
    // ========== 阵营配置 ==========

    /**
     * @brief 可推进前线的阵营标签
     * @details 
     * 功能说明：
     * - 只有匹配此标签的单位才能推进前线
     * - 支持部分匹配（例如：Unit.Faction 可以匹配 Unit.Faction.Player 和 Unit.Faction.Enemy）
     * - 可以设置为具体阵营（Unit.Faction.Player）或通用标签（Unit.Faction）
     * 
     * 使用示例：
     * - "Unit.Faction.Player" - 只有玩家单位可以推进
     * - "Unit.Faction.Enemy" - 只有敌人单位可以推进
     * - "Unit.Faction" - 所有阵营单位都可以推进
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Front Line", meta = (DisplayName = "可推进前线的阵营标签", Categories = "Unit.Faction"))
    FGameplayTag ActiveFactionTag;

    /**
     * @brief 对立阵营标签
     * @details 
     * 功能说明：
     * - 用于识别对立阵营的单位
     * - 对立阵营的单位会被追踪但不会推进前线
     * - 如果留空，则只追踪 ActiveFactionTag 的单位
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Front Line", meta = (DisplayName = "对立阵营标签", Categories = "Unit.Faction"))
    FGameplayTag OpposingFactionTag;
};
