// 📄 文件：Source/Sguo/Public/Data/SG_RollingLogCardData.h
// ✨ 新增 - 完整文件

#pragma once

#include "CoreMinimal.h"
#include "Data/SG_StrategyCardData.h"
#include "SG_RollingLogCardData.generated.h"

/**
 * @brief 流木计专属数据资产
 * 
 * @details
 * **功能说明：**
 * - 继承自 USG_StrategyCardData
 * - 包含流木计特有的配置参数
 * - 在编辑器中创建数据资产进行配置
 * 
 * **使用方式：**
 * 1. 在 Content Browser 中右键 -> Miscellaneous -> Data Asset
 * 2. 选择 SG_RollingLogCardData
 * 3. 配置所有参数
 * 4. 将此数据资产添加到卡组中
 */
UCLASS(BlueprintType)
class SGUO_API USG_RollingLogCardData : public USG_StrategyCardData
{
    GENERATED_BODY()

public:
    USG_RollingLogCardData();

    // ==================== 生成配置 ====================

    /**
     * @brief 生成间隔（秒）
     * @details 每隔多少秒生成一波滚木
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rolling Log|Spawn", 
        meta = (DisplayName = "生成间隔（秒）", ClampMin = "0.1", UIMin = "0.1", UIMax = "5.0"))
    float SpawnInterval = 0.5f;

    /**
     * @brief 每次生成数量
     * @details 每个生成器每次生成多少个滚木
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rolling Log|Spawn", 
        meta = (DisplayName = "每次生成数量", ClampMin = "1", UIMin = "1", UIMax = "10"))
    int32 SpawnCountPerInterval = 1;

    /**
     * @brief 生成持续时间（秒）
     * @details 效果持续多长时间，覆盖基类的 Duration
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rolling Log|Spawn", 
        meta = (DisplayName = "生成持续时间（秒）", ClampMin = "1.0", UIMin = "1.0", UIMax = "30.0"))
    float SpawnDuration = 6.0f;

    /**
     * @brief 生成位置随机偏移范围（厘米）
     * @details 滚木在生成器周围随机偏移的范围
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rolling Log|Spawn", 
        meta = (DisplayName = "位置随机偏移", ClampMin = "0.0", UIMin = "0.0", UIMax = "500.0"))
    float SpawnRandomOffset = 100.0f;

    // ==================== 滚木属性配置 ====================

    /**
     * @brief 滚木伤害值
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rolling Log|Damage", 
        meta = (DisplayName = "伤害值", ClampMin = "0.0", UIMin = "0.0"))
    float DamageAmount = 100.0f;

    /**
     * @brief 伤害效果类
     * @details 用于应用伤害的 GameplayEffect
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rolling Log|Damage", 
        meta = (DisplayName = "伤害效果类"))
    TSubclassOf<UGameplayEffect> LogDamageEffectClass;

    /**
     * @brief 击退距离（厘米）
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rolling Log|Knockback", 
        meta = (DisplayName = "击退距离", ClampMin = "0.0", UIMin = "0.0", UIMax = "1000.0"))
    float KnockbackDistance = 300.0f;

    /**
     * @brief 击退持续时间（秒）
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rolling Log|Knockback", 
        meta = (DisplayName = "击退持续时间", ClampMin = "0.1", UIMin = "0.1", UIMax = "2.0"))
    float KnockbackDuration = 0.3f;

    // ==================== 滚木运动配置 ====================

    /**
     * @brief 滚动速度（厘米/秒）
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rolling Log|Movement", 
        meta = (DisplayName = "滚动速度", ClampMin = "100.0", UIMin = "100.0", UIMax = "3000.0"))
    float RollSpeed = 800.0f;

    /**
     * @brief 最大滚动距离（厘米）
     * @details 滚木滚动超过此距离后自动销毁
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rolling Log|Movement", 
        meta = (DisplayName = "最大滚动距离", ClampMin = "100.0", UIMin = "100.0", UIMax = "10000.0"))
    float MaxRollDistance = 3000.0f;

    /**
     * @brief 滚木生存时间（秒）
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rolling Log|Movement", 
        meta = (DisplayName = "滚木生存时间", ClampMin = "1.0", UIMin = "1.0", UIMax = "30.0"))
    float LogLifeSpan = 10.0f;

    /**
     * @brief 视觉旋转速度（度/秒）
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rolling Log|Movement", 
        meta = (DisplayName = "旋转速度", ClampMin = "0.0", UIMin = "0.0", UIMax = "1080.0"))
    float RotationSpeed = 360.0f;

    // ==================== 滚木类配置 ====================

    /**
     * @brief 滚木 Actor 类
     * @details 要生成的滚木蓝图类
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rolling Log|Class", 
        meta = (DisplayName = "滚木类"))
    TSubclassOf<AActor> RollingLogClass;
};
