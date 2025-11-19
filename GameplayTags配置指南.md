# GameplayTags 配置指南

## ⚠️ **错误说明**

**错误信息：**
```
"XXX" was not found, tags must be loaded from config or registered as a native tag
```

**原因：**
代码中使用了 GameplayTag（如 `Data.Damage`、`Unit.Type.Infantry` 等），但这些标签没有在项目中注册。

---

## 🔧 **解决方案：两种方法**

### **方法1：使用配置文件（推荐）** ⭐

#### **步骤1：我已经为您创建了配置文件**

**文件路径：** `/Config/Tags/GameplayTags.ini`

这个文件包含了所有需要的 GameplayTag 定义（约 40+ 个标签）。

#### **步骤2：在 Unreal Engine 编辑器中验证**

```
1. 启动编辑器
2. 打开：Project Settings → GameplayTags
3. 检查是否显示了所有标签
```

#### **步骤3：如果标签没有自动加载**

在 `DefaultEngine.ini` 中添加：

**文件路径：** `/Config/DefaultEngine.ini`

```ini
[/Script/GameplayTags.GameplayTagsSettings]
ImportTagsFromConfig=True
```

---

### **方法2：在编辑器中手动添加（备用方案）**

如果配置文件方法不工作，可以在编辑器中手动添加：

#### **步骤：**

```
1. 打开编辑器
2. Project Settings → GameplayTags
3. 点击 "Add New Gameplay Tag"
4. 逐个添加以下标签
```

#### **必须添加的核心标签（最小集合）：**

```
数据标签：
✅ Data.Damage

能力标签：
✅ Ability.Attack
✅ Ability.Attack.Melee
✅ Ability.Attack.Ranged

单位类型：
✅ Unit.Type.Infantry
✅ Unit.Type.Cavalry
✅ Unit.Type.Archer
✅ Unit.Type.Crossbow

阵营标签：
✅ Unit.Faction.Player
✅ Unit.Faction.Enemy
```

---

## 📋 **完整标签列表**

### **1. 数据系统标签**
```
Data.Damage                        # 伤害倍率标签
```

### **2. 能力系统标签**
```
Ability.Attack                     # 攻击能力父标签
├─ Ability.Attack.Melee           # 近战攻击
└─ Ability.Attack.Ranged          # 远程攻击
```

### **3. 单位类型标签**
```
Unit.Type                          # 单位类型父标签
├─ Unit.Type.Infantry             # 步兵
├─ Unit.Type.Cavalry              # 骑兵
├─ Unit.Type.Archer               # 弓兵
├─ Unit.Type.Crossbow             # 弩兵
└─ Unit.Type.Hero                 # 英雄
```

### **4. 阵营标签**
```
Unit.Faction                       # 阵营父标签
├─ Unit.Faction.Player            # 玩家阵营
└─ Unit.Faction.Enemy             # 敌方阵营
```

### **5. 状态标签**
```
State.Dead                         # 死亡状态
State.Stunned                      # 眩晕状态
State.Immune                       # 免疫状态
```

### **6. 技能标签（武将技能）**
```
Ability.Skill                      # 技能父标签
├─ Ability.Skill.CaoCao           # 曹操技能
│  └─ Ability.Skill.CaoCao.SwordRain
└─ Ability.Skill.LiuBei           # 刘备技能
   └─ Ability.Skill.LiuBei.SummonTroops
```

### **7. 计谋卡标签**
```
Ability.Strategy                   # 计谋卡父标签
├─ Ability.Strategy.FlowWood      # 流木计
├─ Ability.Strategy.FireArrow     # 火矢计
└─ Ability.Strategy.SpeedBoost    # 神速计
```

### **8. 效果标签**
```
Effect.Buff                        # 增益效果
├─ Effect.Buff.SpeedUp
├─ Effect.Buff.AttackSpeedUp
└─ Effect.Buff.DamageUp

Effect.Debuff                      # 减益效果
├─ Effect.Debuff.Slow
├─ Effect.Debuff.Stun
└─ Effect.Debuff.Knockback
```

### **9. 事件标签**
```
Event.Death                        # 死亡事件
Event.Attack.Hit                   # 攻击命中事件
Event.Attack.Miss                  # 攻击未命中事件
Event.Ability.Activate             # 技能激活事件
Event.Ability.End                  # 技能结束事件
```

### **10. 冷却标签**
```
Cooldown.Attack                    # 攻击冷却
Cooldown.Skill                     # 技能冷却
Cooldown.Strategy                  # 计谋冷却
```

---

## 🔍 **验证配置**

### **步骤1：检查标签是否加载**

```
编辑器 → Project Settings → GameplayTags
```

应该看到：
- ✅ 左侧树形结构显示所有标签
- ✅ 标签有正确的层级关系
- ✅ 可以展开/折叠标签树

### **步骤2：测试标签使用**

在蓝图或 C++ 中测试：
```cpp
FGameplayTag TestTag = FGameplayTag::RequestGameplayTag(FName("Data.Damage"));
if (TestTag.IsValid())
{
    UE_LOG(LogTemp, Log, TEXT("✓ GameplayTag 加载成功！"));
}
else
{
    UE_LOG(LogTemp, Error, TEXT("✗ GameplayTag 加载失败！"));
}
```

---

## 🚨 **常见问题**

### **问题1：配置文件不生效**

**解决方案：**
```
1. 确认文件路径正确：/Config/Tags/GameplayTags.ini
2. 重启编辑器
3. 检查 DefaultEngine.ini 是否包含 ImportTagsFromConfig=True
```

### **问题2：标签仍然找不到**

**解决方案：**
```
1. 使用方法2：在编辑器中手动添加
2. Project Settings → GameplayTags → Add New Gameplay Tag
3. 逐个添加核心标签（至少 10 个）
```

### **问题3：标签名称拼写错误**

**解决方案：**
```
检查 C++ 代码中的标签名称是否与配置文件一致：

✅ 正确：FGameplayTag::RequestGameplayTag(FName("Data.Damage"))
❌ 错误：FGameplayTag::RequestGameplayTag(FName("Data.Damge"))  // 拼写错误

✅ 正确：Unit.Type.Infantry
❌ 错误：Unit.Type.infantry  // 大小写错误
```

---

## 📝 **配置检查清单**

完成配置后，请检查：

- [ ] `/Config/Tags/GameplayTags.ini` 文件存在
- [ ] 编辑器中可以看到所有标签（Project Settings → GameplayTags）
- [ ] 至少添加了 10 个核心标签
- [ ] 重新编译项目无错误
- [ ] 运行编辑器无 GameplayTag 警告

---

## 🎯 **推荐配置流程**

```
1. 使用我创建的 GameplayTags.ini 文件（已包含所有标签）
   ↓
2. 重启 Unreal Engine 编辑器
   ↓
3. 打开 Project Settings → GameplayTags 验证
   ↓
4. 如果看不到标签，检查 DefaultEngine.ini
   ↓
5. 如果仍然不行，使用方法2手动添加核心标签
   ↓
6. 重新编译项目
   ↓
7. 测试验证
```

---

## 📞 **需要帮助？**

如果配置后仍有问题，请提供：
1. 编辑器中 GameplayTags 面板的截图
2. 完整的错误日志
3. `/Config/Tags/GameplayTags.ini` 文件内容

---

**配置完成后，请重新编译项目并告诉我结果！** ✨
