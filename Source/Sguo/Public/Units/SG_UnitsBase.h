// 📄 文件：Source/Sguo/Public/Units/SG_UnitsBase.h
// 🔧 修改 - 完整文件

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
class UBehaviorTree;

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

    // ========== 攻击状态 ==========
    
    /**
     * @brief 是否正在播放攻击动画（动画僵直）
     * @details
     * - true：正在播放攻击动画，不能开始新的攻击
     * - false：可以开始新的攻击
     */
    UPROPERTY(BlueprintReadOnly, Category = "Attack State", meta = (DisplayName = "正在攻击中"))
    bool bIsAttacking = false;

    /**
     * @brief 动画僵直剩余时间
     */
    UPROPERTY(BlueprintReadOnly, Category = "Attack State", meta = (DisplayName = "动画僵直剩余时间"))
    float AttackAnimationRemainingTime = 0.0f;

    // ========== 技能独立冷却系统 ==========
    
    /**
     * @brief 运行时技能冷却池
     * @details
     * - 索引对应 CachedAttackAbilities 的索引
     * - 值为该技能的剩余冷却时间（秒）
     * - 0 表示技能可用
     */
    UPROPERTY(BlueprintReadOnly, Category = "Attack Cooldown", meta = (DisplayName = "技能冷却池"))
    TArray<float> AbilityCooldowns;

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

    // ========== 技能冷却系统函数 ==========
    
    /**
     * @brief 初始化技能冷却池
     */
    UFUNCTION(BlueprintCallable, Category = "Attack")
    void InitializeAbilityCooldowns();

    /**
     * @brief 获取当前优先级最高且未冷却的技能索引
     * @return 技能索引，-1 表示没有可用技能
     */
    UFUNCTION(BlueprintCallable, Category = "Attack")
    int32 GetBestAvailableAbilityIndex() const;

    /**
     * @brief 检查指定索引的技能是否在冷却中
     */
    UFUNCTION(BlueprintPure, Category = "Attack")
    bool IsAbilityOnCooldown(int32 AbilityIndex) const;

    /**
     * @brief 启动指定技能的独立冷却
     */
    UFUNCTION(BlueprintCallable, Category = "Attack")
    void StartAbilityCooldown(int32 AbilityIndex, float CooldownDuration);

    /**
     * @brief 检查是否有至少一个技能可用
     */
    UFUNCTION(BlueprintPure, Category = "Attack")
    bool HasAvailableAbility() const;

    /**
     * @brief 开始攻击动画僵直（由 GA 调用）
     * @param AnimDuration 动画时长
     */
    UFUNCTION(BlueprintCallable, Category = "Attack")
    void StartAttackAnimation(float AnimDuration);

    /**
     * @brief 攻击动画结束回调（由 GA 调用）
     */
    UFUNCTION(BlueprintCallable, Category = "Attack")
    void OnAttackAnimationFinished();

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

    // ✨ 新增 - 内部更新函数
    void UpdateAbilityCooldowns(float DeltaTime);
    void UpdateAttackAnimationState(float DeltaTime);

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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Visualization", meta = (DisplayName = "显示技能冷却信息"))
    bool bShowAbilityCooldowns = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Visualization", meta = (DisplayName = "显示寻敌范围"))
    bool bShowSearchRange = false;

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
    // ========== 战斗表现配置 ==========
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Visuals", meta = (DisplayName = "死亡动画"))
    TObjectPtr<UAnimMontage> DeathMontage;

    // ========== 寻敌逻辑配置 ==========

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Search", meta = (DisplayName = "寻敌形状"))
    ESGTargetSearchShape TargetSearchShape = ESGTargetSearchShape::Circle;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Search", meta = (DisplayName = "优先攻击最前排"))
    bool bPrioritizeFrontmost = true;

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void ForceStopAllActions();

    UFUNCTION(BlueprintPure, Category = "Combat", meta = (DisplayName = "是否可被选为目标"))
    virtual bool CanBeTargeted() const;

public:
    // ========== AI 行为树配置 ==========

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Config", meta = (DisplayName = "单位行为树"))
    TObjectPtr<UBehaviorTree> UnitBehaviorTree;

    UFUNCTION(BlueprintPure, Category = "AI Config", meta = (DisplayName = "获取单位行为树"))
    UBehaviorTree* GetUnitBehaviorTree() const { return UnitBehaviorTree; }

    UFUNCTION(BlueprintPure, Category = "AI Config", meta = (DisplayName = "是否有自定义行为树"))
    bool HasCustomBehaviorTree() const { return UnitBehaviorTree != nullptr; }
};
