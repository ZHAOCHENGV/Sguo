// 📄 文件：Buildings/SG_MainCityBase.cpp

#include "Buildings/SG_MainCityBase.h"
#include "AbilitySystem/SG_AbilitySystemComponent.h"
#include "Buildings/SG_BuildingAttributeSet.h"
#include "Components/StaticMeshComponent.h"
#include "Debug/SG_LogCategories.h"

/**
 * @brief 构造函数
 */
ASG_MainCityBase::ASG_MainCityBase()
{
    // 禁用 Tick（主城不需要每帧更新）
    PrimaryActorTick.bCanEverTick = false;

    // 创建根组件
    RootComp = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
    // 设置为根组件
    RootComponent = RootComp;

    // 创建主城网格体
    CityMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CityMesh"));
    // 附加到根组件
    CityMesh->SetupAttachment(RootComp);
    // 启用碰撞
    CityMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

    // 创建 Ability System Component
    AbilitySystemComponent = CreateDefaultSubobject<USG_AbilitySystemComponent>(TEXT("AbilitySystemComponent"));
    // 设置复制模式
    AbilitySystemComponent->SetIsReplicated(true);
    // 设置复制模式为 Mixed
    AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

    // 创建建筑属性集
    AttributeSet = CreateDefaultSubobject<USG_BuildingAttributeSet>(TEXT("AttributeSet"));
    
    // 设置默认阵营为玩家
    FactionTag = FGameplayTag::RequestGameplayTag(TEXT("Unit.Faction.Player"));
}

/**
 * @brief 获取 AbilitySystemComponent（GAS 接口要求）
 */
UAbilitySystemComponent* ASG_MainCityBase::GetAbilitySystemComponent() const
{
    // 返回 ASC 组件
    return AbilitySystemComponent;
}

/**
 * @brief BeginPlay 生命周期
 */
void ASG_MainCityBase::BeginPlay()
{
    // 调用父类 BeginPlay
    Super::BeginPlay();
    
    // 输出日志
    UE_LOG(LogSGGameplay, Log, TEXT("主城 BeginPlay：%s（阵营：%s）"), 
        *GetName(), *FactionTag.ToString());
    
    // 初始化 ASC
    if (AbilitySystemComponent)
    {
        // 初始化 ASC
        AbilitySystemComponent->InitAbilityActorInfo(this, this);
        // 输出日志
        UE_LOG(LogSGGameplay, Log, TEXT("  ✓ ASC 初始化完成"));
    }
    
    // 初始化主城
    InitializeMainCity();
}

/**
 * @brief 初始化主城
 */
void ASG_MainCityBase::InitializeMainCity()
{
    // 输出日志
    UE_LOG(LogSGGameplay, Log, TEXT("========== 初始化主城：%s =========="), *GetName());
    
    // 检查属性集是否有效
    if (!AttributeSet)
    {
        // 输出错误
        UE_LOG(LogSGGameplay, Error, TEXT("❌ AttributeSet 为空"));
        // 返回
        return;
    }

    // 设置初始生命值
    AttributeSet->SetMaxHealth(InitialHealth);
    AttributeSet->SetHealth(InitialHealth);
    
    // 输出日志
    UE_LOG(LogSGGameplay, Log, TEXT("  初始生命值：%.0f"), InitialHealth);

    // 绑定属性变化委托
    BindAttributeDelegates();
    
    // 输出日志
    UE_LOG(LogSGGameplay, Log, TEXT("✓ 主城初始化完成"));
    UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
}

/**
 * @brief 绑定属性变化委托
 */
void ASG_MainCityBase::BindAttributeDelegates()
{
    // 检查 ASC 和 AttributeSet 是否有效
    if (!AbilitySystemComponent || !AttributeSet)
    {
        // 输出警告
        UE_LOG(LogSGGameplay, Warning, TEXT("⚠️ 无法绑定属性委托：ASC 或 AttributeSet 为空"));
        // 返回
        return;
    }

    // 监听生命值变化
    AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(AttributeSet->GetHealthAttribute())
        .AddUObject(this, &ASG_MainCityBase::OnHealthChanged);
    
    // 输出日志
    UE_LOG(LogSGGameplay, Verbose, TEXT("✓ 已绑定生命值变化委托"));
}

/**
 * @brief 生命值变化回调
 */
void ASG_MainCityBase::OnHealthChanged(const FOnAttributeChangeData& Data)
{
    // 如果已经被摧毁，直接返回
    if (bIsDestroyed)
    {
        return;
    }
    
    // 获取新的生命值
    float NewHealth = Data.NewValue;
    // 获取最大生命值
    float MaxHealth = AttributeSet->GetMaxHealth();
    
    // 输出生命值变化日志
    UE_LOG(LogSGGameplay, Log, TEXT("%s 生命值变化：%.0f / %.0f（%.1f%%）"), 
        *GetName(), NewHealth, MaxHealth, (NewHealth / MaxHealth) * 100.0f);

    // 检测主城被摧毁
    if (NewHealth <= 0.0f && Data.OldValue > 0.0f)
    {
        // 输出日志
        UE_LOG(LogSGGameplay, Warning, TEXT("✗ %s 被摧毁！"), *GetName());
        // 调用摧毁处理
        OnMainCityDestroyed();
    }
}

/**
 * @brief 主城被摧毁时调用
 */
void ASG_MainCityBase::OnMainCityDestroyed_Implementation()
{
    // 标记为已摧毁
    bIsDestroyed = true;
    
    // 输出日志
    UE_LOG(LogSGGameplay, Log, TEXT("========== %s 执行摧毁逻辑 =========="), *GetName());
    
    // 判断阵营
    if (FactionTag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Unit.Faction.Player"))))
    {
        // 玩家主城被摧毁 → 游戏失败
        UE_LOG(LogSGGameplay, Warning, TEXT("⚠️ 玩家主城被摧毁 → 游戏失败"));
        // TODO: 触发游戏失败逻辑
    }
    else if (FactionTag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Unit.Faction.Enemy"))))
    {
        // 敌方主城被摧毁 → 游戏胜利
        UE_LOG(LogSGGameplay, Warning, TEXT("✓ 敌方主城被摧毁 → 游戏胜利"));
        // TODO: 触发游戏胜利逻辑
    }

    // TODO: 播放摧毁动画
    // TODO: 播放摧毁音效
    // TODO: 生成碎片特效

    // 延迟销毁
    SetLifeSpan(5.0f);
    // 输出日志
    UE_LOG(LogSGGameplay, Log, TEXT("  将在 5 秒后销毁"));
}

/**
 * @brief 获取当前生命值
 */
float ASG_MainCityBase::GetCurrentHealth() const
{
    // 检查属性集是否有效
    if (AttributeSet)
    {
        // 返回当前生命值
        return AttributeSet->GetHealth();
    }
    // 返回 0
    return 0.0f;
}

/**
 * @brief 获取最大生命值
 */
float ASG_MainCityBase::GetMaxHealth() const
{
    // 检查属性集是否有效
    if (AttributeSet)
    {
        // 返回最大生命值
        return AttributeSet->GetMaxHealth();
    }
    // 返回 0
    return 0.0f;
}

/**
 * @brief 获取生命值百分比
 */
float ASG_MainCityBase::GetHealthPercentage() const
{
    // 检查属性集是否有效
    if (AttributeSet)
    {
        // 获取当前生命值
        float Current = AttributeSet->GetHealth();
        // 获取最大生命值
        float Max = AttributeSet->GetMaxHealth();
        
        // 避免除零
        if (Max > 0.0f)
        {
            // 返回百分比
            return (Current / Max);
        }
    }
    // 返回 0
    return 0.0f;
}
