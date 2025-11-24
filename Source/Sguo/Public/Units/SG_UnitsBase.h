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
struct FSGUnitAttackDefinition; // ✨ 新增 - 前向声明
class USG_CharacterCardData;

// ✨ 新增 - 单位死亡委托声明
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSGUnitDeathSignature, ASG_UnitsBase*, DeadUnit);

/**
 * @brief 角色基类
 */
UCLASS()
class SGUO_API ASG_UnitsBase : public ACharacter, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    // 构造函数
    ASG_UnitsBase();
    
    // ✨ 新增 - 单位死亡事件
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

    // ========== ✨ 新增 - 卡牌数据引用 ==========
    
    UPROPERTY(BlueprintReadOnly, Category = "Unit Config", meta = (DisplayName = "源卡牌数据"))
    TObjectPtr<USG_CharacterCardData> SourceCardData;

    /**
     * @brief 设置源卡牌数据
     * @param CardData 卡牌数据
     * @details
     * 功能说明：
     * - 在生成单位后立即调用
     * - 缓存卡牌数据引用
     * - 供后续读取倍率使用
     */
    UFUNCTION(BlueprintCallable, Category = "Unit Config")
    void SetSourceCardData(USG_CharacterCardData* CardData);

    // ========== DataTable 配置 ==========
    
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unit Config", meta = (DisplayName = "单位数据表"))
    TObjectPtr<UDataTable> UnitDataTable;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unit Config", meta = (DisplayName = "数据表行名称"))
    FName UnitDataRowName;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unit Config", meta = (DisplayName = "使用数据表配置"))
    bool bUseDataTable = false;

    // ========== ✨ 新增 - 攻击技能配置（从 DataTable 加载）==========
    
    /**
     * @brief 攻击技能列表（从 DataTable 加载）
     */
    UPROPERTY(BlueprintReadOnly, Category = "Attack Config", meta = (DisplayName = "攻击技能列表"))
    TArray<FSGUnitAttackDefinition> CachedAttackAbilities;

    /**
     * @brief 当前攻击技能索引
     */
    UPROPERTY(BlueprintReadOnly, Category = "Attack Config", meta = (DisplayName = "当前攻击索引"))
    int32 CurrentAttackIndex = 0;

    /**
     * @brief 通用攻击能力类
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack Config", meta = (DisplayName = "通用攻击能力类"))
    TSubclassOf<UGameplayAbility> CommonAttackAbilityClass;

    /**
     * @brief 已授予的通用攻击能力
     */
    UPROPERTY(BlueprintReadOnly, Category = "Attack Config", meta = (DisplayName = "通用攻击能力句柄"))
    FGameplayAbilitySpecHandle GrantedCommonAttackHandle;

    /**
    * @brief 已授予的特定攻击能力映射
    * @details
    * 功能说明：
    * - 缓存已授予的特定 GA，避免重复授予
    * - Key: SpecificAbilityClass（GA 类型）
    * - Value: GrantedAbilityHandle（已授予的能力句柄）
    * 使用场景：
    * - 当 DataTable 中配置了 SpecificAbilityClass 时使用
    * - 第一次使用时授予并缓存
    * - 后续直接使用缓存的 Handle
    * 注意事项：
    * - 不需要网络复制（服务器权威）
    * - 在单位销毁时自动清理
    */
    UPROPERTY()
    TMap<TSubclassOf<UGameplayAbility>, FGameplayAbilitySpecHandle> GrantedSpecificAbilities;

    // ========== ✨ 新增 - 攻击冷却系统 ==========
    
    /**
     * @brief 攻击是否在冷却中
     * @details 用于快速检查是否可以攻击
     */
    UPROPERTY(BlueprintReadOnly, Category = "Attack Config", meta = (DisplayName = "攻击冷却中"))
    bool bIsAttackOnCooldown = false;

    /**
     * @brief 冷却剩余时间
     * @details 当前冷却还剩多少秒
     */
    UPROPERTY(BlueprintReadOnly, Category = "Attack Config", meta = (DisplayName = "冷却剩余时间"))
    float CooldownRemainingTime = 0.0f;

    /**
     * @brief 冷却计时器句柄
     * @details 用于管理冷却定时器
     */
    FTimerHandle AttackCooldownTimerHandle;

    
    // ========== ✨ 新增 - 冷却系统函数 ==========
    
    /**
     * @brief 检查攻击是否在冷却中
     * @return 是否在冷却中
     */
    UFUNCTION(BlueprintPure, Category = "Attack")
    bool IsAttackOnCooldown() const { return bIsAttackOnCooldown; }

    /**
     * @brief 获取冷却剩余时间
     * @return 剩余秒数
     */
    UFUNCTION(BlueprintPure, Category = "Attack")
    float GetCooldownRemainingTime() const { return CooldownRemainingTime; }

    /**
     * @brief 开始攻击冷却
     * @param Duration 冷却时间（秒）
     */
    UFUNCTION(BlueprintCallable, Category = "Attack")
    void StartAttackCooldown(float Duration);

    /**
     * @brief 冷却结束回调
     */
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

    // ========== ✨ 新增 - 攻击系统函数 ==========
    
    /**
     * @brief 从 DataTable 加载攻击技能配置
     */
    UFUNCTION(BlueprintCallable, Category = "Attack")
    void LoadAttackAbilitiesFromDataTable();

    /**
     * @brief 授予通用攻击能力
     */
    UFUNCTION(BlueprintCallable, Category = "Attack")
    void GrantCommonAttackAbility();

    /**
     * @brief 执行攻击（随机选择技能）
     */
    UFUNCTION(BlueprintCallable, Category = "Attack")
    bool PerformAttack();

    /**
     * @brief 获取当前攻击配置
     */
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
    // ========== 生命周期函数 ==========
    
    virtual void BeginPlay() override;
    virtual void PossessedBy(AController* NewController) override;

    // ========== GAS 初始化 ==========
    
    void InitializeAttributes(float HealthMult, float DamageMult, float SpeedMult);
    void BindAttributeDelegates();

    // ========== 属性变化回调 ==========
    
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

    // ========== ❌ 删除 - 旧的 DataTable 加载函数 ==========
    // UFUNCTION(BlueprintCallable, Category = "Character")
    // void LoadUnitDataFromTable();
    
    // ========== ✨ 新增 - 重命名的 DataTable 加载函数 ==========
    /**
     * @brief 从 DataTable 加载单位配置
     * @return 是否加载成功
     */
    UFUNCTION(BlueprintCallable, Category = "Character")
    bool IsLoadUnitDataFromTable();

protected:
    float CachedDetectionRange = 1500.0f;
    float CachedChaseRange = 2000.0f;
    
    FGameplayTag DetermineFactionTag() const;
    void InitializeWithDefaults();
};
