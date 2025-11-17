// 📄 文件：Buildings/SG_MainCityBase.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "SG_MainCityBase.generated.h"

// 前向声明
class USG_AbilitySystemComponent;
class USG_BuildingAttributeSet;
class UStaticMeshComponent;
struct FOnAttributeChangeData;

/**
 * @brief 主城基类
 * @details
 * 功能说明：
 * - 可以在场景中放置
 * - 具有血量属性（使用 GAS）
 * - 支持玩家/敌人两个阵营
 * - 被摧毁时触发游戏结束
 * 使用场景：
 * - 在编辑器中拖放到场景
 * - 设置阵营标签
 * - 配置初始生命值
 * 注意事项：
 * - 需要在蓝图中设置静态网格体
 * - 阵营标签必须正确设置
 */
UCLASS(BlueprintType, Blueprintable)
class SGUO_API ASG_MainCityBase : public AActor, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    /**
     * @brief 构造函数
     */
    ASG_MainCityBase();

    // ========== GAS 组件 ==========
    
    /**
     * @brief Ability System Component
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
    USG_AbilitySystemComponent* AbilitySystemComponent;
    
    /**
     * @brief 建筑属性集
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
    USG_BuildingAttributeSet* AttributeSet;

    // ========== 组件 ==========
    
    /**
     * @brief 根组件
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USceneComponent* RootComp;
    
    /**
     * @brief 主城网格体
     * @details 在蓝图中设置静态网格体
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* CityMesh;

    // ========== 主城配置 ==========
    
    /**
     * @brief 阵营标签
     * @details
     * 功能说明：
     * - 区分玩家主城和敌方主城
     * - Unit.Faction.Player：玩家主城
     * - Unit.Faction.Enemy：敌方主城
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Main City", 
        meta = (Categories = "Unit.Faction"))
    FGameplayTag FactionTag;
    
    /**
     * @brief 初始生命值
     * @details 主城的初始血量
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Main City")
    float InitialHealth = 10000.0f;

    // ========== GAS 接口实现 ==========
    
    /**
     * @brief 获取 AbilitySystemComponent（GAS 接口要求）
     */
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

    // ========== 初始化函数 ==========
    
    /**
     * @brief 初始化主城
     * @details
     * 功能说明：
     * - 设置初始生命值
     * - 绑定属性变化委托
     */
    UFUNCTION(BlueprintCallable, Category = "Main City")
    void InitializeMainCity();

    // ========== 查询函数 ==========
    
    /**
     * @brief 获取当前生命值
     */
    UFUNCTION(BlueprintPure, Category = "Main City")
    float GetCurrentHealth() const;
    
    /**
     * @brief 获取最大生命值
     */
    UFUNCTION(BlueprintPure, Category = "Main City")
    float GetMaxHealth() const;
    
    /**
     * @brief 获取生命值百分比
     */
    UFUNCTION(BlueprintPure, Category = "Main City")
    float GetHealthPercentage() const;

protected:
    // ========== 生命周期函数 ==========
    
    /**
     * @brief BeginPlay 生命周期
     */
    virtual void BeginPlay() override;

    // ========== 属性变化回调 ==========
    
    /**
     * @brief 生命值变化时调用
     */
    void OnHealthChanged(const FOnAttributeChangeData& Data);
    
    /**
     * @brief 主城被摧毁时调用
     * @details 蓝图可以重写此函数实现自定义逻辑
     */
    UFUNCTION(BlueprintNativeEvent, Category = "Main City")
    void OnMainCityDestroyed();
    virtual void OnMainCityDestroyed_Implementation();

private:
    /**
     * @brief 绑定属性变化委托
     */
    void BindAttributeDelegates();
    
    /**
     * @brief 是否已经被摧毁
     * @details 防止重复触发摧毁逻辑
     */
    bool bIsDestroyed = false;
};
