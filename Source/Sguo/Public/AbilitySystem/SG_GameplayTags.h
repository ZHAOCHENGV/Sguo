// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"


/**
 * @brief GameplayTags管理单例类
 * 
 * 详细说明：
 * - 使用单例模式确保全局只有一个实例
 * - 在InitializeNativeTags中初始化所有Tags
 * - 提供Get()静态方法获取实例
 * 
 * 使用示例：
 * const FGameplayTag& HeroTag = FSG_GameplayTags::Get().Card_Type_Hero;
 */
struct FSG_GameplayTags
{
public:
	/**
	 * @brief 获取单例实例
	 * @return FSG_GameplayTags& 单例引用
	 */
	static const FSG_GameplayTags& Get()
	{
		return GameplayTags;
	}

	/**
	 * @brief 初始化所有GameplayTags
	 * 
	 * 流程：
	 * 1. 从GameplayTagManager请求Tags
	 * 2. 验证Tags是否有效
	 * 3. 存储到成员变量中
	 * 
	 * 注意：
	 * - 此函数应在游戏模块启动时调用
	 * - 如果Tag不存在会输出警告
	 */
	static void InitializeNativeTags();

	// ========== 卡牌类型标签 ==========
	
	// 英雄卡牌标签
	FGameplayTag Card_Type_Hero;
	
	// 兵团卡牌标签
	FGameplayTag Card_Type_Troop;
	
	// 计谋卡牌标签
	FGameplayTag Card_Type_Strategy;

	// ========== 卡牌稀有度标签 ==========
	
	// 普通卡牌
	FGameplayTag Card_Rarity_Common;
	
	// 稀有卡牌
	FGameplayTag Card_Rarity_Rare;
	
	// 史诗卡牌
	FGameplayTag Card_Rarity_Epic;

	// ========== 单位类型标签 ==========
	
	// 步兵
	FGameplayTag Unit_Type_Infantry;
	
	// 骑兵
	FGameplayTag Unit_Type_Cavalry;
	
	// 弓兵
	FGameplayTag Unit_Type_Archer;
	
	// 弩兵
	FGameplayTag Unit_Type_Crossbowman;
	
	// 投石车
	FGameplayTag Unit_Type_SiegeTower;
	
	// 连弩战车
	FGameplayTag Unit_Type_BallistaTower;
	
	// 机关兽
	FGameplayTag Unit_Type_MechanicalBeast;

	// ========== 阵营标签 ==========
	
	// 玩家阵营
	FGameplayTag Unit_Faction_Player;
	
	// 敌方阵营
	FGameplayTag Unit_Faction_Enemy;

	// ========== 计谋效果标签 ==========
	
	// 业火计
	FGameplayTag Strategy_Effect_Fire;
	
	// 滚石计
	FGameplayTag Strategy_Effect_RollingStone;
	
	// 流木计
	FGameplayTag Strategy_Effect_RollingLog;
	
	// 狂风计
	FGameplayTag Strategy_Effect_Wind;
	
	// 火矢计
	FGameplayTag Strategy_Effect_FireArrow;
	
	// 强攻计
	FGameplayTag Strategy_Effect_DamageBoost;
	
	// 神速计
	FGameplayTag Strategy_Effect_SpeedBoost;
	
	// 康复计
	FGameplayTag Strategy_Effect_Heal;

	// ========== 计谋目标标签 ==========
	
	// 作用于友方
	FGameplayTag Strategy_Target_Friendly;
	
	// 作用于敌方
	FGameplayTag Strategy_Target_Enemy;
	
	// 区域效果
	FGameplayTag Strategy_Target_Area;
	
	// 全局效果
	FGameplayTag Strategy_Target_Global;

	// ========== 状态标签 ==========
	
	// 眩晕状态
	FGameplayTag State_Stunned;
	
	// 减速状态
	FGameplayTag State_Slowed;
	
	// 燃烧状态
	FGameplayTag State_Burning;
	
	// 麻痹状态
	FGameplayTag State_Paralyzed;

	// ========== Buff标签 ==========
	
	// 速度增益
	FGameplayTag Buff_Speed;
	
	// 伤害增益
	FGameplayTag Buff_Damage;
	
	// 狂暴状态
	FGameplayTag Buff_Rage;

	// ========== 能力标签 ==========
	
	// 英雄技能
	FGameplayTag Ability_Hero;
	
	// 需要非眩晕状态
	FGameplayTag Ability_Require_NotStunned;
	
	// 技能冷却中
	FGameplayTag Ability_Cooldown_Active;

	// ========== 战斗标签 ==========
	
	// 近战攻击
	FGameplayTag Combat_Attack_Melee;
	
	// 远程攻击
	FGameplayTag Combat_Attack_Ranged;
	
	// 范围攻击
	FGameplayTag Combat_Attack_AOE;
	
	// 死亡状态
	FGameplayTag Combat_Dead;

private:
	// 单例实例
	static FSG_GameplayTags GameplayTags;
};