// ✨ 新增文件 - 单位属性调试 Widget 实现
// Copyright notice placeholder
/**
 * @file SG_UnitDebugWidget.cpp
 * @brief 单位属性调试显示 Widget 实现
 */

#include "Debug/SG_UnitDebugWidget.h"
#include "Units/SG_UnitsBase.h"
#include "AbilitySystem/SG_AttributeSet.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Debug/SG_LogCategories.h"

/**
 * @brief Widget 构建时调用
 * @details
 * 功能说明：
 * - 初始化 Widget 组件
 * - 设置默认显示状态
 */
void USG_UnitDebugWidget::NativeConstruct()
{
	// 调用父类实现
	Super::NativeConstruct();
	
	// 输出日志
	UE_LOG(LogSGUI, Verbose, TEXT("UnitDebugWidget 构建完成"));
}

/**
 * @brief 每帧更新
 * @param MyGeometry Widget 几何信息
 * @param InDeltaTime 帧间隔时间
 * @details
 * 功能说明：
 * - 控制更新频率
 * - 实时更新显示内容
 */
void USG_UnitDebugWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	// 调用父类实现
	Super::NativeTick(MyGeometry, InDeltaTime);
	
	// 检查单位是否有效
	if (!BoundUnit || !IsValid(BoundUnit))
	{
		// 单位无效，隐藏 Widget
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	
	// 检查单位是否死亡
	if (BoundUnit->bIsDead)
	{
		// 单位已死亡，隐藏 Widget
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	
	// 显示 Widget
	SetVisibility(ESlateVisibility::Visible);
	
	// 累加时间
	LastUpdateTime += InDeltaTime;
	
	// 检查是否到达更新间隔
	if (LastUpdateTime >= UpdateInterval)
	{
		// 重置计时器
		LastUpdateTime = 0.0f;
		// 更新显示内容
		UpdateDisplay();
	}
}

/**
 * @brief 绑定到单位
 * @param InUnit 要显示属性的单位
 * @details
 * 功能说明：
 * - 保存单位引用
 * - 立即更新显示内容
 */
void USG_UnitDebugWidget::BindToUnit(ASG_UnitsBase* InUnit)
{
	// 检查单位是否有效
	if (!InUnit)
	{
		// 输出警告
		UE_LOG(LogSGUI, Warning, TEXT("BindToUnit 失败：单位为空"));
		return;
	}
	
	// 保存单位引用
	BoundUnit = InUnit;
	
	// 输出日志
	UE_LOG(LogSGUI, Log, TEXT("UnitDebugWidget 绑定到单位：%s"), *InUnit->GetName());
	
	// 立即更新显示
	UpdateDisplay();
}

/**
 * @brief 更新显示内容
 * @details
 * 功能说明：
 * - 读取单位当前属性
 * - 更新血条和文本显示
 * - 根据生命值百分比改变颜色
 * 详细流程：
 * 1. 检查单位和 Widget 组件有效性
 * 2. 获取单位属性集
 * 3. 读取当前属性值
 * 4. 更新血条进度和颜色
 * 5. 更新文本显示
 */
void USG_UnitDebugWidget::UpdateDisplay()
{
	// 检查单位是否有效
	if (!BoundUnit || !IsValid(BoundUnit))
	{
		return;
	}
	
	// 获取属性集
	USG_AttributeSet* AttributeSet = BoundUnit->AttributeSet;
	if (!AttributeSet)
	{
		// 输出警告
		UE_LOG(LogSGUI, Warning, TEXT("单位 %s 的 AttributeSet 为空"), *BoundUnit->GetName());
		return;
	}
	
	// ========== 更新血条 ==========
	
	if (HealthBar)
	{
		// 获取当前生命值
		float CurrentHealth = AttributeSet->GetHealth();
		// 获取最大生命值
		float MaxHealth = AttributeSet->GetMaxHealth();
		
		// 计算生命值百分比
		float HealthPercent = (MaxHealth > 0.0f) ? (CurrentHealth / MaxHealth) : 0.0f;
		
		// 更新进度条
		HealthBar->SetPercent(HealthPercent);
		
		// 根据生命值百分比设置颜色
		FLinearColor BarColor = GetHealthBarColor(HealthPercent);
		HealthBar->SetFillColorAndOpacity(BarColor);
	}
	
	// ========== 更新单位名称 ==========
	
	if (UnitNameText && bShowUnitName)
	{
		// 构建显示文本
		FString DisplayName = BoundUnit->GetName();
	
		
		// 如果显示阵营标签
		if (bShowFactionTag)
		{
			// 获取阵营标签
			FString FactionStr = BoundUnit->FactionTag.ToString();
			// 简化阵营名称显示
			if (FactionStr.Contains(TEXT("Player")))
			{
				FactionStr = TEXT("[玩家]");
			}
			else if (FactionStr.Contains(TEXT("Enemy")))
			{
				FactionStr = TEXT("[敌人]");
			}
			
			// 组合名称和阵营
			DisplayName = FString::Printf(TEXT("%s %s"), *FactionStr, *DisplayName);
			//DisplayName.RemoveFromEnd(TEXT("_c"));
		}
		
		// 设置文本内容
		UnitNameText->SetText(FText::FromString(DisplayName));
		
		// 根据阵营设置文本颜色
		FGameplayTag PlayerTag = FGameplayTag::RequestGameplayTag(TEXT("Unit.Faction.Player"));
		FLinearColor TextColor = BoundUnit->FactionTag.MatchesTag(PlayerTag) ? PlayerColor : EnemyColor;
		UnitNameText->SetColorAndOpacity(FSlateColor(TextColor));
	}
	
	// ========== 更新生命值文本 ==========
	
	if (HealthText)
	{
		// 获取当前生命值
		float CurrentHealth = AttributeSet->GetHealth();
		// 获取最大生命值
		float MaxHealth = AttributeSet->GetMaxHealth();
		
		// 格式化文本：当前/最大
		FString HealthString = FString::Printf(TEXT("%.0f / %.0f"), CurrentHealth, MaxHealth);
		HealthText->SetText(FText::FromString(HealthString));
	}
	
	// ========== 更新详细属性文本 ==========
	
	if (DetailedStatsText && bShowDetailedStats)
	{
		// 格式化属性文本
		FString StatsString = FormatStatsText();
		// 设置文本内容
		DetailedStatsText->SetText(FText::FromString(StatsString));
		// 显示文本
		DetailedStatsText->SetVisibility(ESlateVisibility::Visible);
	}
	else if (DetailedStatsText)
	{
		// 隐藏详细属性
		DetailedStatsText->SetVisibility(ESlateVisibility::Collapsed);
	}
}

/**
 * @brief 获取血条颜色（根据生命值百分比）
 * @param HealthPercent 生命值百分比（0.0 ~ 1.0）
 * @return 血条颜色
 * @details
 * 功能说明：
 * - 根据生命值百分比返回不同颜色
 * - >70%：绿色
 * - 30%-70%：黄色
 * - <30%：红色
 */
FLinearColor USG_UnitDebugWidget::GetHealthBarColor(float HealthPercent) const
{
	// 高血量（>70%）
	if (HealthPercent > 0.7f)
	{
		return HighHealthColor;
	}
	// 中血量（30%-70%）
	else if (HealthPercent > 0.3f)
	{
		return MidHealthColor;
	}
	// 低血量（<30%）
	else
	{
		return LowHealthColor;
	}
}

/**
 * @brief 格式化属性文本
 * @return 格式化的属性字符串
 * @details
 * 功能说明：
 * - 将单位属性格式化为易读的文本
 * - 包含攻击、速度、范围等信息
 */
FString USG_UnitDebugWidget::FormatStatsText() const
{
	// 检查单位是否有效
	if (!BoundUnit || !BoundUnit->AttributeSet)
	{
		return TEXT("无数据");
	}
	
	// 获取属性集
	USG_AttributeSet* AttributeSet = BoundUnit->AttributeSet;
	
	// 构建属性字符串
	FString Result;
	
	// 添加攻击力
	Result += FString::Printf(TEXT("攻击: %.0f\n"), AttributeSet->GetAttackDamage());
	
	// 添加移动速度
	Result += FString::Printf(TEXT("移速: %.0f\n"), AttributeSet->GetMoveSpeed());
	
	// 添加攻击速度
	Result += FString::Printf(TEXT("攻速: %.2f/s\n"), AttributeSet->GetAttackSpeed());
	
	// 添加攻击范围
	Result += FString::Printf(TEXT("攻围: %.0f"), AttributeSet->GetAttackRange());
	
	// 返回格式化文本
	return Result;
}
