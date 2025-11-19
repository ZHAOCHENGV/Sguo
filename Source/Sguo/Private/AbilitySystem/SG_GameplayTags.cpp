// Fill out your copyright notice in the Description page of Project Settings.
/**
 * @file SG_GameplayTags.cpp
 * @brief GameplayTags管理类实现
 */


#include "../../Public/AbilitySystem/SG_GameplayTags.h"
#include "GameplayTagsManager.h"


// 定义单例实例
FSG_GameplayTags FSG_GameplayTags::GameplayTags;

void FSG_GameplayTags::InitializeNativeTags()
{
	// 获取GameplayTagsManager实例
	// GameplayTagsManager负责管理所有的GameplayTags
	UGameplayTagsManager& Manager = UGameplayTagsManager::Get();

	// 初始化所有Tags
	// RequestGameplayTag会查找对应的Tag，如果不存在会创建
	// 第二个参数为false表示如果Tag不存在不会报错
	
	// ========== 初始化卡牌类型标签 ==========
	GameplayTags.Card_Type_Hero = Manager.RequestGameplayTag(FName("Card.Type.Hero"), false);
	GameplayTags.Card_Type_Troop = Manager.RequestGameplayTag(FName("Card.Type.Troop"), false);
	GameplayTags.Card_Type_Strategy = Manager.RequestGameplayTag(FName("Card.Type.Strategy"), false);

	// ========== 初始化卡牌稀有度标签 ==========
	GameplayTags.Card_Rarity_Common = Manager.RequestGameplayTag(FName("Card.Rarity.Common"), false);
	GameplayTags.Card_Rarity_Rare = Manager.RequestGameplayTag(FName("Card.Rarity.Rare"), false);
	GameplayTags.Card_Rarity_Epic = Manager.RequestGameplayTag(FName("Card.Rarity.Epic"), false);

	// ========== 初始化单位类型标签 ==========
	GameplayTags.Unit_Type_Infantry = Manager.RequestGameplayTag(FName("Unit.Type.Infantry"), false);
	GameplayTags.Unit_Type_Cavalry = Manager.RequestGameplayTag(FName("Unit.Type.Cavalry"), false);
	GameplayTags.Unit_Type_Archer = Manager.RequestGameplayTag(FName("Unit.Type.Archer"), false);
	GameplayTags.Unit_Type_Crossbowman = Manager.RequestGameplayTag(FName("Unit.Type.Crossbowman"), false);
	GameplayTags.Unit_Type_SiegeTower = Manager.RequestGameplayTag(FName("Unit.Type.SiegeTower"), false);
	GameplayTags.Unit_Type_BallistaTower = Manager.RequestGameplayTag(FName("Unit.Type.BallistaTower"), false);
	GameplayTags.Unit_Type_MechanicalBeast = Manager.RequestGameplayTag(FName("Unit.Type.MechanicalBeast"), false);

	// ========== 初始化阵营标签 ==========
	GameplayTags.Unit_Faction_Player = Manager.RequestGameplayTag(FName("Unit.Faction.Player"), false);
	GameplayTags.Unit_Faction_Enemy = Manager.RequestGameplayTag(FName("Unit.Faction.Enemy"), false);

	// ========== 初始化计谋效果标签 ==========
	GameplayTags.Strategy_Effect_Fire = Manager.RequestGameplayTag(FName("Strategy.Effect.Fire"), false);
	GameplayTags.Strategy_Effect_RollingStone = Manager.RequestGameplayTag(FName("Strategy.Effect.RollingStone"), false);
	GameplayTags.Strategy_Effect_RollingLog = Manager.RequestGameplayTag(FName("Strategy.Effect.RollingLog"), false);
	GameplayTags.Strategy_Effect_Wind = Manager.RequestGameplayTag(FName("Strategy.Effect.Wind"), false);
	GameplayTags.Strategy_Effect_FireArrow = Manager.RequestGameplayTag(FName("Strategy.Effect.FireArrow"), false);
	GameplayTags.Strategy_Effect_DamageBoost = Manager.RequestGameplayTag(FName("Strategy.Effect.DamageBoost"), false);
	GameplayTags.Strategy_Effect_SpeedBoost = Manager.RequestGameplayTag(FName("Strategy.Effect.SpeedBoost"), false);
	GameplayTags.Strategy_Effect_Heal = Manager.RequestGameplayTag(FName("Strategy.Effect.Heal"), false);

	// ========== 初始化计谋目标标签 ==========
	GameplayTags.Strategy_Target_Friendly = Manager.RequestGameplayTag(FName("Strategy.Target.Friendly"), false);
	GameplayTags.Strategy_Target_Enemy = Manager.RequestGameplayTag(FName("Strategy.Target.Enemy"), false);
	GameplayTags.Strategy_Target_Area = Manager.RequestGameplayTag(FName("Strategy.Target.Area"), false);
	GameplayTags.Strategy_Target_Global = Manager.RequestGameplayTag(FName("Strategy.Target.Global"), false);

	// ========== 初始化状态标签 ==========
	GameplayTags.State_Stunned = Manager.RequestGameplayTag(FName("State.Stunned"), false);
	GameplayTags.State_Slowed = Manager.RequestGameplayTag(FName("State.Slowed"), false);
	GameplayTags.State_Burning = Manager.RequestGameplayTag(FName("State.Burning"), false);
	GameplayTags.State_Paralyzed = Manager.RequestGameplayTag(FName("State.Paralyzed"), false);

	// ========== 初始化Buff标签 ==========
	GameplayTags.Buff_Speed = Manager.RequestGameplayTag(FName("Buff.Speed"), false);
	GameplayTags.Buff_Damage = Manager.RequestGameplayTag(FName("Buff.Damage"), false);
	GameplayTags.Buff_Rage = Manager.RequestGameplayTag(FName("Buff.Rage"), false);

	// ========== 初始化能力标签 ==========
	GameplayTags.Ability_Hero = Manager.RequestGameplayTag(FName("Ability.Hero"), false);
	GameplayTags.Ability_Require_NotStunned = Manager.RequestGameplayTag(FName("Ability.Require.NotStunned"), false);
	GameplayTags.Ability_Cooldown_Active = Manager.RequestGameplayTag(FName("Ability.Cooldown.Active"), false);

	// ========== 初始化战斗标签 ==========
	GameplayTags.Combat_Attack_Melee = Manager.RequestGameplayTag(FName("Combat.Attack.Melee"), false);
	GameplayTags.Combat_Attack_Ranged = Manager.RequestGameplayTag(FName("Combat.Attack.Ranged"), false);
	GameplayTags.Combat_Attack_AOE = Manager.RequestGameplayTag(FName("Combat.Attack.AOE"), false);
	GameplayTags.Combat_Dead = Manager.RequestGameplayTag(FName("Combat.Dead"), false);
}