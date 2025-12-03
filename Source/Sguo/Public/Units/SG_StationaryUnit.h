// 📄 文件：Source/Sguo/Public/Units/SG_StationaryUnit.h
// 🔧 修改 - 添加计谋技能执行支持
// ✅ 这是完整文件

#pragma once

#include "CoreMinimal.h"
#include "Units/SG_UnitsBase.h"
#include "SG_StationaryUnit.generated.h"

class UAnimMontage;
class UGameplayAbility;

// ✨ 新增 - 计谋技能执行状态
UENUM(BlueprintType)
enum class ESGStrategySkillState : uint8
{
    None        UMETA(DisplayName = "无"),
    Executing   UMETA(DisplayName = "执行中"),
    Cooldown    UMETA(DisplayName = "冷却中")
};

/**
 * @brief 站桩单位类
 */
UCLASS(BlueprintType, Blueprintable)
class SGUO_API ASG_StationaryUnit : public ASG_UnitsBase
{
    GENERATED_BODY()

public:
    ASG_StationaryUnit();

    // ========== ✨ 新增 - 计谋技能参数缓存 ==========

    UPROPERTY(BlueprintReadOnly, Category = "Stationary Unit|Strategy Skill")
    float StrategySkillDamageMultiplier = 1.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Stationary Unit|Strategy Skill")
    float StrategySkillArcHeight = 0.5f;

    UPROPERTY(BlueprintReadOnly, Category = "Stationary Unit|Strategy Skill")
    float StrategySkillFlightSpeed = 1500.0f;

    

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

public:
    // ========== 站桩配置 ==========
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stationary Unit", 
        meta = (DisplayName = "启用浮空模式"))
    bool bEnableHover = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stationary Unit", 
        meta = (DisplayName = "浮空高度(厘米)", EditCondition = "bEnableHover", EditConditionHides, ClampMin = "-500.0", ClampMax = "1000.0"))
    float HoverHeight = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stationary Unit", 
        meta = (DisplayName = "禁用重力"))
    bool bDisableGravity = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stationary Unit", 
        meta = (DisplayName = "可被选为目标"))
    bool bCanBeTargeted = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stationary Unit", 
        meta = (DisplayName = "禁用移动"))
    bool bDisableMovement = true;

    // ========== 火矢计配置（兼容旧代码） ==========
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stationary Unit|Fire Arrow", 
        meta = (DisplayName = "火矢攻击蒙太奇"))
    TObjectPtr<UAnimMontage> FireArrowMontage;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stationary Unit|Fire Arrow", 
        meta = (DisplayName = "火矢投射物类"))
    TSubclassOf<AActor> FireArrowProjectileClass;

    UPROPERTY(BlueprintReadWrite, Category = "Stationary Unit|Fire Arrow", 
        meta = (DisplayName = "正在执行火矢计"))
    bool bIsExecutingFireArrow = false;

    // ========== ✨ 新增 - 计谋技能执行系统 ==========

    /**
     * @brief 当前计谋技能状态
     */
    UPROPERTY(BlueprintReadOnly, Category = "Stationary Unit|Strategy Skill", 
        meta = (DisplayName = "计谋技能状态"))
    ESGStrategySkillState StrategySkillState = ESGStrategySkillState::None;

    /**
     * @brief 计谋技能持续时间剩余
     */
    UPROPERTY(BlueprintReadOnly, Category = "Stationary Unit|Strategy Skill", 
        meta = (DisplayName = "技能剩余时间"))
    float StrategySkillRemainingTime = 0.0f;

    /**
     * @brief 计谋技能射击间隔计时器
     */
    UPROPERTY(BlueprintReadOnly, Category = "Stationary Unit|Strategy Skill", 
        meta = (DisplayName = "射击间隔计时器"))
    float StrategySkillFireTimer = 0.0f;

    /**
     * @brief 当前计谋技能的射击间隔
     */
    UPROPERTY(BlueprintReadOnly, Category = "Stationary Unit|Strategy Skill", 
        meta = (DisplayName = "当前射击间隔"))
    float CurrentFireInterval = 0.0f;

    /**
     * @brief 当前计谋技能的目标位置
     */
    UPROPERTY(BlueprintReadOnly, Category = "Stationary Unit|Strategy Skill", 
        meta = (DisplayName = "技能目标位置"))
    FVector StrategySkillTargetLocation = FVector::ZeroVector;

    /**
     * @brief 当前计谋技能的区域半径
     */
    UPROPERTY(BlueprintReadOnly, Category = "Stationary Unit|Strategy Skill", 
        meta = (DisplayName = "技能区域半径"))
    float StrategySkillAreaRadius = 0.0f;

    /**
     * @brief 当前计谋技能每轮发射数量
     */
    UPROPERTY(BlueprintReadOnly, Category = "Stationary Unit|Strategy Skill", 
        meta = (DisplayName = "每轮发射数量"))
    int32 StrategySkillArrowsPerRound = 1;

    /**
     * @brief 当前使用的投射物类
     */
    UPROPERTY(BlueprintReadOnly, Category = "Stationary Unit|Strategy Skill")
    TSubclassOf<AActor> CurrentProjectileClass;

    /**
     * @brief 当前使用的攻击蒙太奇
     */
    UPROPERTY(BlueprintReadOnly, Category = "Stationary Unit|Strategy Skill")
    TObjectPtr<UAnimMontage> CurrentAttackMontage;

    // ========== 查询接口 ==========
    
    virtual bool CanBeTargeted() const override;

    UFUNCTION(BlueprintPure, Category = "Stationary Unit", meta = (DisplayName = "是否浮空"))
    bool IsHovering() const { return bEnableHover; }

    UFUNCTION(BlueprintPure, Category = "Stationary Unit", meta = (DisplayName = "获取浮空高度"))
    float GetHoverHeight() const { return HoverHeight; }

    // ========== ✨ 新增 - 计谋技能接口 ==========

    /**
     * @brief 开始执行计谋技能
     * @param TargetLocation 目标位置
     * @param AreaRadius 区域半径
     * @param Duration 持续时间
     * @param FireInterval 射击间隔
     * @param ArrowsPerRound 每轮发射数量
     * @param ProjectileClass 投射物类（可选）
     * @param AttackMontage 攻击蒙太奇（可选，为空则使用 DataTable 配置）
     * @details
     * 功能说明：
     * - 打断当前普通攻击
     * - 设置计谋技能参数
     * - 开始持续射击
     *  🔧 修改：增加了数值参数 (DamageMultiplier, ArcHeight, FlightSpeed)
     */
    UFUNCTION(BlueprintCallable, Category = "Stationary Unit|Strategy Skill", 
        meta = (DisplayName = "开始计谋技能"))
    void StartStrategySkill(
        const FVector& TargetLocation,
        float AreaRadius,
        float Duration,
        float FireInterval,
        int32 ArrowsPerRound,
        TSubclassOf<AActor> ProjectileClass = nullptr,
        UAnimMontage* AttackMontage = nullptr,
        float DamageMultiplier = 1.0f,      // ✨ 新增
        float ArcHeight = 0.5f,             // ✨ 新增
        float FlightSpeed = 1500.0f         // ✨ 新增
   
    );

    /**
     * @brief 停止计谋技能
     * @details
     * 功能说明：
     * - 清除计谋技能状态
     * - 恢复普通攻击
     */
    UFUNCTION(BlueprintCallable, Category = "Stationary Unit|Strategy Skill", 
        meta = (DisplayName = "停止计谋技能"))
    void StopStrategySkill();

    /**
     * @brief 检查是否正在执行计谋技能
     */
    UFUNCTION(BlueprintPure, Category = "Stationary Unit|Strategy Skill", 
        meta = (DisplayName = "是否正在执行计谋技能"))
    bool IsExecutingStrategySkill() const { return StrategySkillState == ESGStrategySkillState::Executing; }

    /**
     * @brief 执行一次计谋技能射击
     * @details
     * 功能说明：
     * - 播放攻击蒙太奇（根据射击间隔调整播放速度）
     * - 在区域内随机位置发射投射物
     */
    UFUNCTION(BlueprintCallable, Category = "Stationary Unit|Strategy Skill", 
        meta = (DisplayName = "执行计谋射击"))
    void ExecuteStrategyFire();

    // ========== 旧版火矢接口（保持兼容） ==========
    
    UFUNCTION(BlueprintCallable, Category = "Stationary Unit|Fire Arrow", 
        meta = (DisplayName = "开始火矢技能"))
    void StartFireArrowSkill();

    UFUNCTION(BlueprintCallable, Category = "Stationary Unit|Fire Arrow", 
        meta = (DisplayName = "结束火矢技能"))
    void EndFireArrowSkill();

    UFUNCTION(BlueprintCallable, Category = "Stationary Unit|Fire Arrow", 
        meta = (DisplayName = "发射火矢"))
    AActor* FireArrow(const FVector& TargetLocation, TSubclassOf<AActor> ProjectileClassOverride = nullptr);

    UFUNCTION(BlueprintPure, Category = "Stationary Unit|Fire Arrow", 
        meta = (DisplayName = "获取火矢投射物类"))
    TSubclassOf<AActor> GetFireArrowProjectileClass() const;

protected:
    void ApplyStationarySettings();
    void DisableMovementCapability();
    void ApplyHoverEffect();

    /**
     * @brief 更新计谋技能逻辑
     * @param DeltaTime 帧间隔
     */
    void UpdateStrategySkill(float DeltaTime);

    /**
     * @brief 获取 DataTable 中配置的攻击蒙太奇
     * @param AbilityIndex 技能索引（默认 0）
     * @return 攻击蒙太奇
     */
    UAnimMontage* GetDataTableAttackMontage(int32 AbilityIndex = 0) const;

    /**
     * @brief 获取 DataTable 中配置的投射物类
     * @param AbilityIndex 技能索引（默认 0）
     * @return 投射物类
     */
    TSubclassOf<AActor> GetDataTableProjectileClass(int32 AbilityIndex = 0) const;

    UPROPERTY(Transient)
    TSubclassOf<AActor> CachedOriginalProjectileClass;
};
