# GE_Damage_Base 配置诊断指南

## 问题描述
根据您的日志：
```
Log: ✓ 伤害已应用到 IncomingDamage
Error: ❌ 伤害 GE 应用失败
```

伤害计算完成了，但 GE 应用失败。这通常是因为 **GE_Damage_Base 配置不正确**。

---

## 必须检查的配置项

### 1. **GameplayEffect Duration Policy（持续策略）**

**UE 5.6 正确配置：**
- **Duration Policy**: `Instant`（即时应用）
- **Captured Attribute**: `AttackDamage`（Source）
- **Captured Source**: `Source`（攻击者）
- **Override**: `Not Snapshotted`（不使用快照）

**为什么选择 Instant：**
- 伤害是即时效果，不需要持续时间
- `Instant` 类型的 GE 会立即执行并销毁

---

### 2. **Executions（执行计算）配置**

**步骤：**
1. 在 `Executions` 数组中添加一个元素
2. 选择 **Calculation Class**: `SG_DamageExecutionCalc`
3. 配置 `Calculation Modifiers`（计算修饰符）

**Calculation Modifiers 配置：**

| 属性 | 值 |
|------|-----|
| **Backing Attribute** | `AttackDamage` |
| **Attribute Source** | `Source`（攻击者） |
| **Snapshot** | `false`（不使用快照） |
| **Channel** | 留空 |

**关键点：**
- `AttackDamage` 必须从 `Source`（攻击者）捕获
- 必须设置为不使用快照（使用实时值）

---

### 3. **Modifiers（修饰符）配置**

**❌ 错误配置（会导致应用失败）：**
- 在 `Modifiers` 数组中添加了 `IncomingDamage` 的修改
- 原因：`IncomingDamage` 应该由 `Execution Calculation` 修改，而不是直接在 `Modifiers` 中修改

**✅ 正确配置：**
- **Modifiers 数组应该为空**
- 或者只用于其他效果（如护甲、抗性等）

---

### 4. **SetByCaller Magnitudes（按调用者设置的数值）**

**配置步骤：**
1. 在 `SetByCaller Magnitudes` 数组中添加一个元素
2. 设置 `Data Tag`: `Data.Damage`
3. 不设置默认值（由代码在运行时设置）

**GameplayTag 配置：**
- 确保在 **Project Settings → GameplayTags** 中配置了：
  ```
  Data.Damage
  ```

---

## 完整的 GE_Damage_Base 配置检查清单

### ✅ 步骤 1：Duration Policy
- [ ] Duration Policy = `Instant`

### ✅ 步骤 2：Executions
- [ ] Executions 数组有 1 个元素
- [ ] Calculation Class = `SG_DamageExecutionCalc`
- [ ] Calculation Modifiers 配置正确：
  - [ ] Backing Attribute = `AttackDamage`
  - [ ] Attribute Source = `Source`
  - [ ] Snapshot = `false`

### ✅ 步骤 3：Modifiers
- [ ] **Modifiers 数组为空**（不要在这里修改 IncomingDamage）

### ✅ 步骤 4：SetByCaller
- [ ] SetByCaller Magnitudes 数组有 1 个元素
- [ ] Data Tag = `Data.Damage`

### ✅ 步骤 5：GameplayTags
- [ ] 在 Project Settings 中配置了 `Data.Damage` tag

---

## 从您的截图看到的问题

### 🔴 问题 1：Execution 配置错误

**您的截图显示：**
- `Captured Attribute` 显示为 `AttackDamage`
- `Captured Source` 显示为 `Source`（正确）
- `Override` 显示为 `Not Snapshotted`（正确）

**但是，可能的问题：**
1. **Calculation Modifiers 数组可能为空**
   - 您需要在 `Calculation Modifiers` 数组中添加一个元素
   - 必须配置 `AttackDamage` 的捕获

2. **Modifiers 数组可能有错误配置**
   - 如果您在 `Modifiers` 中直接修改了 `IncomingDamage`，请删除
   - `IncomingDamage` 应该由 `SG_DamageExecutionCalc` 修改

---

## 正确的蓝图界面应该是这样

### Executions 区域
```
Executions
  [0]
    Calculation Class: SG_DamageExecutionCalc
    Calculation Modifiers
      [0]
        Backing Attribute: AttackDamage
        Attribute Source: Source
        Snapshot: false
```

### Modifiers 区域
```
Modifiers
  (空数组)
```

### SetByCaller Magnitudes 区域
```
SetByCaller Magnitudes
  [0]
    Data Tag: Data.Damage
```

---

## 如何检查配置

### 方法 1：在编辑器中检查
1. 打开 `GE_Damage_Base` 蓝图
2. 选择蓝图的根节点
3. 在 Details 面板中查看：
   - `Duration Policy`
   - `Executions`
   - `Modifiers`
   - `SetByCaller Magnitudes`

### 方法 2：查看日志
在您的代码中添加更详细的日志：
```cpp
// 在 SG_GameplayAbility_Attack.cpp 的 ApplyDamageToTarget 函数中
UE_LOG(LogSGGameplay, Warning, TEXT("GE 应用结果 - Active Handle: %s, Valid: %d"), 
    *ActiveHandle.ToString(), ActiveHandle.IsValid());

// 如果应用失败，输出 EffectSpec 的详细信息
if (!ActiveHandle.IsValid())
{
    UE_LOG(LogSGGameplay, Error, TEXT("GE 应用失败详情："));
    UE_LOG(LogSGGameplay, Error, TEXT("  - DamageGEClass 是否有效: %d"), DamageGEClass != nullptr);
    UE_LOG(LogSGGameplay, Error, TEXT("  - 目标 ASC 是否有效: %d"), TargetASC != nullptr);
    UE_LOG(LogSGGameplay, Error, TEXT("  - 攻击者 ASC 是否有效: %d"), AbilitySystemComponent != nullptr);
}
```

---

## 快速修复步骤

### 步骤 1：删除并重新创建 GE_Damage_Base
1. 右键 Content Browser → Gameplay → Gameplay Effect
2. 命名为 `GE_Damage_Base`
3. 设置 `Duration Policy` = `Instant`

### 步骤 2：配置 Executions
1. 在 `Executions` 数组中点击 `+` 添加元素
2. 设置 `Calculation Class` = `SG_DamageExecutionCalc`
3. **关键步骤**：在 `Calculation Modifiers` 数组中添加元素：
   - `Backing Attribute` = `AttackDamage`
   - `Attribute Source` = `Source`
   - `Snapshot` = `false`

### 步骤 3：确保 Modifiers 为空
1. 检查 `Modifiers` 数组
2. 如果有任何元素，全部删除

### 步骤 4：配置 SetByCaller
1. 在 `SetByCaller Magnitudes` 数组中点击 `+` 添加元素
2. 设置 `Data Tag` = `Data.Damage`

### 步骤 5：保存并测试
1. 保存蓝图
2. 重新编译项目
3. 进入游戏测试

---

## 额外的调试建议

### 1. 在攻击能力中添加日志
在 `SG_GameplayAbility_Attack.cpp` 的 `ApplyDamageToTarget` 函数中添加：
```cpp
UE_LOG(LogSGGameplay, Warning, TEXT("准备应用 GE："));
UE_LOG(LogSGGameplay, Warning, TEXT("  - DamageGEClass: %s"), DamageGEClass ? *DamageGEClass->GetName() : TEXT("NULL"));
UE_LOG(LogSGGameplay, Warning, TEXT("  - 伤害倍率: %.2f"), DamageMultiplier);
UE_LOG(LogSGGameplay, Warning, TEXT("  - 目标: %s"), Target ? *Target->GetName() : TEXT("NULL"));
```

### 2. 检查 GameplayTag 是否配置
确保在 **Project Settings → GameplayTags** 中配置了：
- `Data.Damage`

### 3. 验证 ASC 的有效性
确保目标和攻击者都有有效的 `AbilitySystemComponent`。

---

## 常见错误和解决方案

| 错误现象 | 可能原因 | 解决方案 |
|---------|---------|---------|
| ❌ GE 应用失败 | Modifiers 中错误配置了 IncomingDamage | 删除 Modifiers 中的所有元素 |
| ❌ GE 应用失败 | Duration Policy 设置为 Duration 或 Infinite | 改为 `Instant` |
| ❌ GE 应用失败 | Calculation Modifiers 未配置 | 添加 AttackDamage 的捕获配置 |
| ❌ 伤害为 0 | SetByCaller 的 Tag 不匹配 | 确保 Tag 为 `Data.Damage` |
| ❌ 伤害为 0 | 攻击者的 AttackDamage 为 0 | 确保单位已初始化属性 |

---

## 总结

**最可能的问题：**
1. `Modifiers` 数组中错误配置了 `IncomingDamage`
2. `Calculation Modifiers` 数组为空或配置错误
3. `Duration Policy` 不是 `Instant`

**修复优先级：**
1. **高优先级**：检查并删除 `Modifiers` 中的所有元素
2. **高优先级**：在 `Calculation Modifiers` 中添加 `AttackDamage` 捕获配置
3. **中优先级**：确保 `Duration Policy` = `Instant`
4. **低优先级**：配置 `SetByCaller Magnitudes`

请按照这个顺序检查您的 GE_Damage_Base 配置！
