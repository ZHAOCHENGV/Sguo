// ✨ 新增 - 伤害计算执行类实现
// Copyright notice placeholder

#include "AbilitySystem/GameplayEffects/SG_DamageExecutionCalc.h"
#include "AbilitySystem/SG_AttributeSet.h"
#include "AbilitySystem/SG_AbilitySystemComponent.h"
#include "Debug/SG_LogCategories.h"

// ========== 属性捕获结构体 ==========
// 用于声明需要捕获哪些属性

// 定义攻击者（Source）的属性捕获
struct FSGDamageStatics
{
	// 声明捕获 Source 的 AttackDamage 属性
	DECLARE_ATTRIBUTE_CAPTUREDEF(AttackDamage);
	
	// 构造函数：定义如何捕获属性
	FSGDamageStatics()
	{
		// 定义捕获 AttackDamage 属性
		// 参数说明：
		// - USG_AttributeSet::StaticClass()：属性所在的类
		// - USG_AttributeSet::GetAttackDamageAttribute()：具体的属性
		// - EGameplayEffectAttributeCaptureSource::Source：从攻击者捕获
		// - false：不捕获快照（使用实时值）
		DEFINE_ATTRIBUTE_CAPTUREDEF(USG_AttributeSet, AttackDamage, Source, false);
	}
};

// 获取静态属性捕获定义（单例模式）
static const FSGDamageStatics& DamageStatics()
{
	static FSGDamageStatics DStatics;
	return DStatics;
}

// ========== 构造函数 ==========
USG_DamageExecutionCalc::USG_DamageExecutionCalc()
{
	// 添加需要捕获的属性到执行计算中
	// 这样在 Execute_Implementation 中就可以读取这些属性
	RelevantAttributesToCapture.Add(DamageStatics().AttackDamageDef);
}

// ========== 执行伤害计算 ==========
void USG_DamageExecutionCalc::Execute_Implementation(
	const FGameplayEffectCustomExecutionParameters& ExecutionParams,
	OUT FGameplayEffectCustomExecutionOutput& OutExecutionOutput
) const
{
	// 输出日志：开始伤害计算
	UE_LOG(LogSGGameplay, Verbose, TEXT("========== 伤害计算开始 =========="));

	// 获取 Source（攻击者）和 Target（被攻击者）的 ASC
	UAbilitySystemComponent* SourceASC = ExecutionParams.GetSourceAbilitySystemComponent();
	UAbilitySystemComponent* TargetASC = ExecutionParams.GetTargetAbilitySystemComponent();

	// 获取 Source 和 Target 的 Actor
	AActor* SourceActor = SourceASC ? SourceASC->GetAvatarActor() : nullptr;
	AActor* TargetActor = TargetASC ? TargetASC->GetAvatarActor() : nullptr;

	// 获取 EffectSpec（包含伤害倍率等数据）
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();

	// ========== 步骤1：读取攻击者的攻击力 ==========
	
	// 创建评估参数（用于读取捕获的属性）
	FAggregatorEvaluateParameters EvaluationParameters;
	EvaluationParameters.SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	EvaluationParameters.TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

	// 从捕获的属性中读取 AttackDamage
	float AttackDamage = 0.0f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(
		DamageStatics().AttackDamageDef,
		EvaluationParameters,
		AttackDamage
	);

	// 输出日志：攻击者信息
	UE_LOG(LogSGGameplay, Verbose, TEXT("  攻击者：%s"), SourceActor ? *SourceActor->GetName() : TEXT("None"));
	UE_LOG(LogSGGameplay, Verbose, TEXT("  攻击力：%.1f"), AttackDamage);

	// ========== 步骤2：读取伤害倍率 ==========
	
	// 从 SetByCaller 读取伤害倍率
	// GameplayTag "Data.Damage" 用于标识伤害倍率
	// 如果未设置，默认为 1.0（100%伤害）
	const FGameplayTag DamageTag = FGameplayTag::RequestGameplayTag(FName("Data.Damage"));
	float DamageMultiplier = Spec.GetSetByCallerMagnitude(DamageTag, false, 1.0f);

	// 输出日志：伤害倍率
	UE_LOG(LogSGGameplay, Verbose, TEXT("  伤害倍率：%.2f"), DamageMultiplier);

	// ========== 步骤3：计算最终伤害 ==========
	
	// 计算公式：最终伤害 = 攻击力 * 伤害倍率
	float FinalDamage = AttackDamage * DamageMultiplier;

	// 输出日志：被攻击者和最终伤害
	UE_LOG(LogSGGameplay, Verbose, TEXT("  被攻击者：%s"), TargetActor ? *TargetActor->GetName() : TEXT("None"));
	UE_LOG(LogSGGameplay, Verbose, TEXT("  最终伤害：%.1f"), FinalDamage);

	// ========== 步骤4：应用伤害到 Target ==========
	
	// 如果最终伤害 > 0，则应用到 Target 的 IncomingDamage 属性
	if (FinalDamage > 0.0f)
	{
		// 创建输出修改器
		// EGameplayModOp::Additive：加法操作（累加伤害）
		OutExecutionOutput.AddOutputModifier(
			FGameplayModifierEvaluatedData(
				USG_AttributeSet::GetIncomingDamageAttribute(),
				EGameplayModOp::Additive,
				FinalDamage
			)
		);

		// 输出日志：伤害已应用
		UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 伤害已应用到 IncomingDamage"));
	}
	else
	{
		// 输出日志：无伤害
		UE_LOG(LogSGGameplay, Warning, TEXT("  ⚠️ 最终伤害为0，未应用"));
	}

	// 输出日志：伤害计算结束
	UE_LOG(LogSGGameplay, Verbose, TEXT("========================================"));
}
