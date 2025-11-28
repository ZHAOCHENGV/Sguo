// 📄 文件：Source/Sguo/Public/Units/SG_UnitsBase.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagContainer.h"
#include "SG_UnitsBase.generated.h"

// 前置声明
class USG_AbilitySystemComponent;
class USG_AttributeSet;
class UGameplayAbility;
class UAnimMontage;
struct FOnAttributeChangeData;
struct FSGUnitDataRow;
struct FSGUnitAttackDefinition;
class USG_CharacterCardData;

// 寻敌范围形状枚举
UENUM(BlueprintType)
enum class ESGTargetSearchShape : uint8
{
    Circle UMETA(DisplayName = "圆形"),
    Square UMETA(DisplayName = "正方形")
};

// 单位死亡委托声明
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSGUnitDeathSignature, ASG_UnitsBase*, DeadUnit);

/**
 * @brief 角色基类
 */
UCLASS()
class SGUO_API ASG_UnitsBase : public ACharacter, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    ASG_UnitsBase();
    
    // 单位死亡事件
    UPROPERTY(BlueprintAssignable, Category = "Unit Events")
    FSGUnitDeathSignature OnUnitDeathEvent;

    // ========== GAS 组件 ==========
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
    USG_AbilitySystemComponent* AbilitySystemComponent;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
    USG_AttributeSet* AttributeSet;

    // ========== 角色信息 ==========
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Info", meta = (Categories = "Unit.Faction", DisplayName = "阵营标签"))
    FGameplayTag FactionTag;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Info", meta = (Categories = "Unit.Type", DisplayName = "单位类型标签"))
    FGameplayTag UnitTypeTag;
    
    UPROPERTY(BlueprintReadWrite, Category = "Combat", meta = (DisplayName = "当前目标"))
    AActor* CurrentTarget;

    // ========== 基础属性配置 ==========
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Attributes", meta = (DisplayName = "基础生命值"))
    float BaseHealth = 500.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Attributes", meta = (DisplayName = "基础攻击力"))
    float BaseAttackDamage = 50.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Attributes", meta = (DisplayName = "基础移动速度"))
    float BaseMoveSpeed = 400.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Attributes", meta = (DisplayName = "基础攻击速度"))
    float BaseAttackSpeed = 1.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Attributes", meta = (DisplayName = "基础攻击范围"))
    float BaseAttackRange = 150.0f;

    // ========== 卡牌数据引用 ==========
    
    UPROPERTY(BlueprintReadOnly, Category = "Unit Config", meta = (DisplayName = "源卡牌数据"))
    TObjectPtr<USG_CharacterCardData> SourceCardData;

    UFUNCTION(BlueprintCallable, Category = "Unit Config")
    void SetSourceCardData(USG_CharacterCardData* CardData);

    // ========== DataTable 配置 ==========
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unit Config", meta = (DisplayName = "单位数据表"))
    TObjectPtr<UDataTable> UnitDataTable;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unit Config", meta = (DisplayName = "数据表行名称"))
    FName UnitDataRowName;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unit Config", meta = (DisplayName = "使用数据表配置"))
    bool bUseDataTable = false;

    // ========== 攻击技能配置 ==========
    
    UPROPERTY(BlueprintReadOnly, Category = "Attack Config", meta = (DisplayName = "攻击技能列表"))
    TArray<FSGUnitAttackDefinition> CachedAttackAbilities;

    UPROPERTY(BlueprintReadOnly, Category = "Attack Config", meta = (DisplayName = "当前攻击索引"))
    int32 CurrentAttackIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack Config", meta = (DisplayName = "通用攻击能力类"))
    TSubclassOf<UGameplayAbility> CommonAttackAbilityClass;

    UPROPERTY(BlueprintReadOnly, Category = "Attack Config", meta = (DisplayName = "通用攻击能力句柄"))
    FGameplayAbilitySpecHandle GrantedCommonAttackHandle;

    UPROPERTY()
    TMap<TSubclassOf<UGameplayAbility>, FGameplayAbilitySpecHandle> GrantedSpecificAbilities;

    // ========== 攻击冷却系统 ==========
    
    UPROPERTY(BlueprintReadOnly, Category = "Attack Config", meta = (DisplayName = "攻击冷却中"))
    bool bIsAttackOnCooldown = false;

    UPROPERTY(BlueprintReadOnly, Category = "Attack Config", meta = (DisplayName = "冷却剩余时间"))
    float CooldownRemainingTime = 0.0f;

    FTimerHandle AttackCooldownTimerHandle;

    UFUNCTION(BlueprintPure, Category = "Attack")
    bool IsAttackOnCooldown() const { return bIsAttackOnCooldown; }

    UFUNCTION(BlueprintPure, Category = "Attack")
    float GetCooldownRemainingTime() const { return CooldownRemainingTime; }

    UFUNCTION(BlueprintCallable, Category = "Attack")
    void StartAttackCooldown(float Duration);

    UFUNCTION()
    void OnAttackCooldownEnd();

    // ========== GAS 接口实现 ==========
    
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

    // ========== 初始化函数 ==========
    
    UFUNCTION(BlueprintCallable, Category = "Character")
    void InitializeCharacter(
        FGameplayTag InFactionTag,
        float HealthMultiplier = 1.0f,
        float DamageMultiplier = 1.0f,
        float SpeedMultiplier = 1.0f
    );

    // ========== 攻击系统函数 ==========
    
    UFUNCTION(BlueprintCallable, Category = "Attack")
    void LoadAttackAbilitiesFromDataTable();

    UFUNCTION(BlueprintCallable, Category = "Attack")
    void GrantCommonAttackAbility();

    UFUNCTION(BlueprintCallable, Category = "Attack")
    bool PerformAttack();

    UFUNCTION(BlueprintPure, Category = "Attack")
    FSGUnitAttackDefinition GetCurrentAttackDefinition() const;

    // ========== 战斗相关函数 ==========
    
    UFUNCTION(BlueprintCallable, Category = "Combat")
    AActor* FindNearestTarget();
    
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void SetTarget(AActor* NewTarget);
    
    UFUNCTION(BlueprintCallable, Category = "Combat")
    bool IsTargetValid() const;

protected:
    virtual void BeginPlay() override;
    virtual void PossessedBy(AController* NewController) override;

    void InitializeAttributes(float HealthMult, float DamageMult, float SpeedMult);
    void BindAttributeDelegates();

    void OnHealthChanged(const FOnAttributeChangeData& Data);
    
    UFUNCTION(BlueprintNativeEvent, Category = "Character")
    void OnDeath();
    virtual void OnDeath_Implementation();

public:
    UPROPERTY(BlueprintReadOnly, Category = "Character", meta = (DisplayName = "是否已死亡"))
    bool bIsDead = false;

    // ========== 调试可视化 ==========
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Visualization", meta = (DisplayName = "显示攻击范围"))
    bool bShowAttackRange = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Visualization", meta = (DisplayName = "显示视野范围"))
    bool bShowVisionRange = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Visualization", meta = (DisplayName = "视野范围", EditCondition = "!bUseDataTable", EditConditionHides))
    float VisionRange = 1500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Visualization", meta = (DisplayName = "攻击范围颜色"))
    FLinearColor AttackRangeColor = FLinearColor::Red;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Visualization", meta = (DisplayName = "视野范围颜色"))
    FLinearColor VisionRangeColor = FLinearColor::Yellow;

    UFUNCTION(BlueprintCallable, Category = "Debug Visualization")
    void ToggleAttackRangeVisualization();

    UFUNCTION(BlueprintCallable, Category = "Debug Visualization")
    void ToggleVisionRangeVisualization();

    virtual void Tick(float DeltaTime) override;

    // ========== AI 配置接口 ==========
    
    UFUNCTION(BlueprintPure, Category = "AI")
    float GetDetectionRange() const;
    
    UFUNCTION(BlueprintPure, Category = "AI")
    float GetChaseRange() const;
    
    UFUNCTION(BlueprintPure, Category = "AI")
    float GetAttackRangeForAI() const;

    UFUNCTION(BlueprintCallable, Category = "Character")
    bool IsLoadUnitDataFromTable();

protected:
    float CachedDetectionRange = 1500.0f;
    float CachedChaseRange = 2000.0f;
    
    FGameplayTag DetermineFactionTag() const;
    void InitializeWithDefaults();

public:
    // ========== 攻击状态控制 ==========

    UPROPERTY(BlueprintReadOnly, Category = "Attack State")
    bool bIsAttacking = false;

    UFUNCTION(BlueprintCallable, Category = "Attack")
    void StartAttackCycle(float AnimDuration);
    
    UFUNCTION(BlueprintCallable, Category = "Attack")
    void OnAttackAbilityFinished();

    // ========== 战斗表现配置 ==========
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Visuals", meta = (DisplayName = "死亡动画"))
    TObjectPtr<UAnimMontage> DeathMontage;

    // ========== 寻敌逻辑配置 ==========

    /**
     * @brief 寻敌范围形状
     * @details 选择圆形（半径）或正方形（使用 DetectionRange 作为半边长）进行索敌
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Search", meta = (DisplayName = "寻敌形状"))
    ESGTargetSearchShape TargetSearchShape = ESGTargetSearchShape::Circle;

    // 🔧 修改 - 移除 SearchBoxExtent，改用 DetectionRange
    // ❌ 删除 - 以下属性不再需要，正方形寻敌范围直接使用 DetectionRange
    // UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Search", 
    //     meta = (DisplayName = "正方形寻敌范围(半长宽)", EditCondition = "TargetSearchShape == ESGTargetSearchShape::Square", EditConditionHides))
    // FVector2D SearchBoxExtent = FVector2D(800.0f, 800.0f);

    /**
     * @brief 是否优先攻击最前排的敌人
     * @details 
     * - True: 忽略Y轴距离，优先选择X轴最靠近己方的敌人（防守逻辑）
     * - False: 选择直线距离最近的敌人（标准逻辑）
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Search", meta = (DisplayName = "优先攻击最前排"))
    bool bPrioritizeFrontmost = true;

    /**
     * @brief 调试：显示寻敌范围
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Visualization", meta = (DisplayName = "显示寻敌范围"))
    bool bShowSearchRange = false;

    // ✨ 新增 - 强制停止所有行为（用于死亡时调用）
    /**
     * @brief 强制停止所有行为
     * @details
     * 功能说明：
     * - 停止移动
     * - 取消所有正在执行的能力
     * - 停止攻击动画
     * - 清除目标
     * 使用场景：
     * - 单位死亡时调用
     * - 游戏暂停时调用
     */
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void ForceStopAllActions();

    // ✨ 新增 - 检查单位是否可被选为目标（AI寻敌接口）
    /**
     * @brief 检查单位是否可被选为目标
     * @return 是否可被选为目标
     * @details
     * 功能说明：
     * - 虚函数，子类可以重写以自定义逻辑
     * - 默认返回 true（普通单位可被选中）
     * - 站桩单位可以重写此函数返回 false
     * 使用场景：
     * - AI 寻找攻击目标时过滤单位
     * - 技能选择目标时判断有效性
     * 注意事项：
     * - 此函数不影响 AOE 伤害判定
     * - 只影响主动目标选择
     * - 死亡单位会在其他地方过滤，此函数不需要检查
     */
    UFUNCTION(BlueprintPure, Category = "Combat", meta = (DisplayName = "是否可被选为目标"))
    virtual bool CanBeTargeted() const;
};
