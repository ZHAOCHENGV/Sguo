// 📄 文件：Source/Sguo/Private/Data/SG_RollingLogCardData.cpp
// ✨ 新增 - 完整文件

#include "Data/SG_RollingLogCardData.h"

USG_RollingLogCardData::USG_RollingLogCardData()
{
	// 设置默认卡牌类型标签
	// CardTypeTag = FGameplayTag::RequestGameplayTag(FName("Card.Type.Strategy.RollingLog"), false);
    
	// 设置默认计谋效果标签
	StrategyEffectTag = FGameplayTag::RequestGameplayTag(FName("Strategy.Effect.RollingLog"), false);
    
	// 设置默认目标类型为敌方
	TargetType = ESGStrategyTargetType::Enemy;
    
	// 设置默认放置类型为全局（不需要选择位置）
	PlacementType = ESGPlacementType::Global;
    
	// 同步持续时间
	Duration = SpawnDuration;
}
