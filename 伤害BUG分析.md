# 伤害系统 BUG 分析

## 🐛 问题现象

### 日志分析
```
找到目标数量：2
攻击目标：BP_步兵_C_UAID_88AEDD401BCA55A302_1913927583
✓ 伤害已应用到 IncomingDamage
❌ 伤害 GE 应用失败
攻击目标：BP_步兵_C_UAID_88AEDD401BCA55A302_1913927583  (同一个目标)
✓ 伤害已应用到 IncomingDamage
❌ 伤害 GE 应用失败
```

### 问题点
1. **同一个目标被检测到2次** - `BP_步兵_C_UAID_88AEDD401BCA55A302_1913927583` 出现了2次
2. **场景只有2个单位（1友1敌），却检测到2个目标** - 应该只检测到1个敌人
3. **GE应用失败但伤害生效** - "❌ 伤害 GE 应用失败" 但 IncomingDamage 被应用
4. **生命值减少2点** - 因为同一个目标被攻击了2次

---

## 🔍 根本原因

### 原因1：球形检测重复检测了同一个Actor的多个Component

**代码位置：** `SG_GameplayAbility_Attack.cpp` 第 215 行
```cpp
bool bHit = GetWorld()->OverlapMultiByChannel(
    OverlapResults,
    SourceLocation,
    FQuat::Identity,
    ECC_Pawn,
    CollisionShape,
    QueryParams
);
```

**问题：**
- `OverlapMultiByChannel` 会返回所有碰撞的组件
- 一个 Character 可能有多个碰撞组件（Capsule、Mesh等）
- 同一个 Actor 的不同组件会产生多个 `FOverlapResult`

**证据：**
- 日志显示同一个 Actor 名称出现了2次
- 这是因为同一个 Actor 的2个组件被检测到

### 原因2：没有去重逻辑

**代码位置：** `SG_GameplayAbility_Attack.cpp` 第 228-244 行
```cpp
for (const FOverlapResult& Result : OverlapResults)
{
    AActor* HitActor = Result.GetActor();
    if (!HitActor)
    {
        continue;
    }

    ASG_UnitsBase* TargetUnit = Cast<ASG_UnitsBase>(HitActor);
    if (TargetUnit && TargetUnit->FactionTag != SourceUnit->FactionTag)
    {
        OutTargets.Add(HitActor);  // ❌ 没有检查是否已添加
    }
}
```

**问题：**
- 没有检查 `HitActor` 是否已经在 `OutTargets` 中
- 同一个 Actor 会被添加多次

---

## ✅ 解决方案

### 方案1：使用 AddUnique（推荐）

在添加目标时使用 `AddUnique` 替代 `Add`：

```cpp
// ❌ 错误：会重复添加
OutTargets.Add(HitActor);

// ✅ 正确：自动去重
OutTargets.AddUnique(HitActor);
```

**优点：**
- 简单高效
- TArray 自带的方法
- 自动处理去重

### 方案2：使用 TSet（适合大量目标）

如果目标数量很多，可以使用 `TSet` 去重：

```cpp
TSet<AActor*> UniqueTargets;

for (const FOverlapResult& Result : OverlapResults)
{
    AActor* HitActor = Result.GetActor();
    if (!HitActor) continue;

    ASG_UnitsBase* TargetUnit = Cast<ASG_UnitsBase>(HitActor);
    if (TargetUnit && TargetUnit->FactionTag != SourceUnit->FactionTag)
    {
        UniqueTargets.Add(HitActor);  // TSet 自动去重
    }
}

// 转换回 TArray
OutTargets = UniqueTargets.Array();
```

### 方案3：只检测 RootComponent（最优）

更改碰撞查询参数，只检测 RootComponent：

```cpp
FCollisionQueryParams QueryParams;
QueryParams.AddIgnoredActor(AvatarActor);
// ✨ 新增：设置碰撞查询复杂度为 Simple（只检测简单碰撞体）
QueryParams.bTraceComplex = false;
```

**或者使用更精确的碰撞通道：**
```cpp
// 使用自定义的碰撞通道（需要在项目设置中配置）
// 或者使用 ObjectType 查询
FCollisionObjectQueryParams ObjectParams(ECC_Pawn);
```

---

## 🛠️ 修复代码

### 修复方式1：使用 AddUnique（最简单）

```cpp
// 在 SG_GameplayAbility_Attack.cpp 的 FindTargetsInRange 函数中

// ❌ 旧代码（第 242 行）
OutTargets.Add(HitActor);

// ✅ 新代码
OutTargets.AddUnique(HitActor);
```

### 修复方式2：完整的去重逻辑

```cpp
case ESGAttackAbilityType::Melee:
{
    // ... 前面的代码不变 ...
    
    // 🔧 修改 - 使用 TSet 自动去重
    TSet<AActor*> UniqueTargets;
    
    if (bHit)
    {
        for (const FOverlapResult& Result : OverlapResults)
        {
            AActor* HitActor = Result.GetActor();
            if (!HitActor) continue;

            ASG_UnitsBase* TargetUnit = Cast<ASG_UnitsBase>(HitActor);
            if (TargetUnit && TargetUnit->FactionTag != SourceUnit->FactionTag)
            {
                UniqueTargets.Add(HitActor);  // TSet 自动去重
                
                // ✨ 新增 - 输出日志确认唯一性
                UE_LOG(LogSGGameplay, Verbose, TEXT("    检测到敌方单位：%s"), *HitActor->GetName());
            }
        }
    }
    
    // 🔧 修改 - 转换为 TArray
    OutTargets = UniqueTargets.Array();
}
break;
```

---

## 📝 关于 GE 应用失败的问题

### 问题分析
日志显示：
```
✓ 伤害已应用到 IncomingDamage
❌ 伤害 GE 应用失败
```

这看起来矛盾，但实际上：

1. **"✓ 伤害已应用到 IncomingDamage"** - 来自 `SG_DamageExecutionCalc.cpp`
   - 这表示伤害计算完成
   - IncomingDamage 被修改

2. **"❌ 伤害 GE 应用失败"** - 来自 `SG_GameplayAbility_Attack.cpp`
   - 这是检查 `ActiveHandle.IsValid()` 的结果
   - 可能是 GE 应用返回了无效的 Handle

### 可能的原因

#### 原因1：Instant GE 不返回有效的 Handle
**解释：**
- `Instant` 类型的 GE 会立即执行并销毁
- 它可能不会返回一个持久的 `ActiveGameplayEffectHandle`
- 这不代表应用失败，只是 Handle 无效

**验证方法：**
检查 `ApplyGameplayEffectSpecToTarget` 的返回值：
```cpp
FActiveGameplayEffectHandle ActiveHandle = SourceASC->ApplyGameplayEffectSpecToTarget(
    *SpecHandle.Data.Get(),
    TargetASC
);

// ✨ 新增 - 详细日志
UE_LOG(LogSGGameplay, Warning, TEXT("    ActiveHandle.IsValid() = %d"), ActiveHandle.IsValid());
UE_LOG(LogSGGameplay, Warning, TEXT("    ActiveHandle.WasSuccessfullyApplied() = %d"), ActiveHandle.WasSuccessfullyApplied());
```

#### 原因2：GE 配置问题
参考之前的 `GE_Damage_Base配置诊断.md`

---

## 🎯 推荐的修复方案

### 第一步：修复重复检测问题（高优先级）

在 `SG_GameplayAbility_Attack.cpp` 中：

**位置1：近战攻击（第 242 行）**
```cpp
// 🔧 修改
OutTargets.AddUnique(HitActor);
```

**位置2：远程攻击（第 284 行）**
```cpp
// 🔧 修改
OutTargets.AddUnique(HitActor);
```

### 第二步：改进 GE 应用失败的判断（中优先级）

在 `ApplyDamageToTarget` 函数中：

```cpp
// 应用 GameplayEffect 到目标
FActiveGameplayEffectHandle ActiveHandle = SourceASC->ApplyGameplayEffectSpecToTarget(
    *SpecHandle.Data.Get(),
    TargetASC
);

// 🔧 修改 - 改进判断逻辑
// 对于 Instant GE，Handle 可能无效但仍然成功应用
// 应该检查 WasSuccessfullyApplied() 或者不检查 Handle
if (ActiveHandle.WasSuccessfullyApplied())
{
    UE_LOG(LogSGGameplay, Log, TEXT("    ✓ 伤害 GE 应用成功"));
}
else
{
    UE_LOG(LogSGGameplay, Error, TEXT("    ❌ 伤害 GE 应用失败"));
}
```

### 第三步：添加调试日志（低优先级）

在 `FindTargetsInRange` 函数中添加更详细的日志：

```cpp
UE_LOG(LogSGGameplay, Log, TEXT("  球形检测原始结果数量：%d"), OverlapResults.Num());
UE_LOG(LogSGGameplay, Log, TEXT("  去重后目标数量：%d"), OutTargets.Num());
```

---

## 📊 修复后的预期效果

### 修复前
```
找到目标数量：2
攻击目标：BP_步兵_C_UAID_88AEDD401BCA55A302_1913927583
✓ 伤害已应用到 IncomingDamage
❌ 伤害 GE 应用失败
攻击目标：BP_步兵_C_UAID_88AEDD401BCA55A302_1913927583
✓ 伤害已应用到 IncomingDamage
❌ 伤害 GE 应用失败
```

### 修复后
```
找到目标数量：1
攻击目标：BP_步兵_C_UAID_88AEDD401BCA55A302_1913927583
✓ 伤害已应用到 IncomingDamage
✓ 伤害 GE 应用成功
```

### 效果对比
| 指标 | 修复前 | 修复后 |
|-----|--------|--------|
| 检测到的目标数量 | 2（错误） | 1（正确） |
| 伤害应用次数 | 2 次 | 1 次 |
| 生命值减少 | 2 点 | 1 点（50） |
| GE 应用状态 | 失败 | 成功 |

---

## 🚀 立即执行的修复步骤

### 步骤1：修改代码（5 分钟）
1. 打开 `SG_GameplayAbility_Attack.cpp`
2. 找到第 242 行和第 284 行
3. 将 `OutTargets.Add(HitActor);` 改为 `OutTargets.AddUnique(HitActor);`
4. 保存文件

### 步骤2：编译项目（2 分钟）
在 UE 编辑器中点击 `Compile` 按钮

### 步骤3：测试验证（3 分钟）
1. 放置 1 个友方步兵和 1 个敌方步兵
2. 触发攻击
3. 查看日志：应该只有 1 次攻击记录
4. 查看生命值：应该减少 50 而不是 2

---

## 💡 经验总结

### 教训1：碰撞检测要注意去重
**问题：**
- `OverlapMultiByChannel` 会返回所有碰撞组件
- 同一个 Actor 可能有多个组件

**最佳实践：**
- 使用 `AddUnique` 而不是 `Add`
- 或者使用 `TSet` 自动去重
- 或者配置更精确的碰撞查询参数

### 教训2：Instant GE 的 Handle 检查
**问题：**
- Instant GE 立即执行并销毁
- 可能不返回有效的 Handle

**最佳实践：**
- 对 Instant GE 不检查 Handle
- 或者使用 `WasSuccessfullyApplied()`
- 或者只检查 `SpecHandle` 而不是 `ActiveHandle`

### 教训3：详细的调试日志很重要
**最佳实践：**
- 输出原始检测数量和去重后数量
- 输出每个检测到的 Actor 名称
- 输出 GE 应用的详细信息

---

## 📞 需要进一步帮助？

如果修复后仍有问题，请提供：
1. 修复后的完整日志
2. 生命值变化的详细数值
3. GE_Damage_Base 的配置截图

我会继续帮您分析和解决！
