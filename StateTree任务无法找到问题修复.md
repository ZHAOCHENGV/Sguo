# StateTree任务无法找到问题修复

## 🐛 问题描述

在UE 5.6的StateTree编辑器中，无法找到自定义的StateTree Tasks：
- `SG_StateTreeTask_FindTarget`
- `SG_StateTreeTask_MoveToTarget`
- `SG_StateTreeTask_PerformAttack`

**症状：**
- StateTree编辑器中搜索任务名称无结果
- 任务列表中没有显示自定义Tasks
- 只能看到引擎内置的Tasks

## 🔍 问题原因

StateTree Tasks 需要在 `USTRUCT()` 宏中添加 **meta 标签**才能在编辑器中正确显示：

1. **DisplayName** - 编辑器中显示的友好名称
2. **Category** - 任务分类（用于在编辑器中分组）

**错误的写法：**
```cpp
USTRUCT()  // ❌ 缺少meta标签
struct SGUO_API FSG_StateTreeTask_FindTarget : public FStateTreeTaskBase
{
    // ...
};
```

**正确的写法：**
```cpp
USTRUCT(meta = (DisplayName = "Find Target", Category = "AI|Combat"))  // ✅ 正确
struct SGUO_API FSG_StateTreeTask_FindTarget : public FStateTreeTaskBase
{
    // ...
};
```

## ✅ 解决方案

### 步骤1：修改Task头文件

我已经修复了所有3个Task的头文件：

#### 1. FindTarget Task
```cpp
// 文件：SG_StateTreeTask_FindTarget.h

USTRUCT(meta = (DisplayName = "Find Target", Category = "AI|Combat"))
struct SGUO_API FSG_StateTreeTask_FindTarget : public FStateTreeTaskBase
{
    // ...
};
```

#### 2. MoveToTarget Task
```cpp
// 文件：SG_StateTreeTask_MoveToTarget.h

USTRUCT(meta = (DisplayName = "Move To Target", Category = "AI|Movement"))
struct SGUO_API FSG_StateTreeTask_MoveToTarget : public FStateTreeTaskBase
{
    // ...
};
```

#### 3. PerformAttack Task
```cpp
// 文件：SG_StateTreeTask_PerformAttack.h

USTRUCT(meta = (DisplayName = "Perform Attack", Category = "AI|Combat"))
struct SGUO_API FSG_StateTreeTask_PerformAttack : public FStateTreeTaskBase
{
    // ...
};
```

### 步骤2：重新编译项目

**在Visual Studio中：**
1. 关闭UE编辑器
2. 在Visual Studio中点击 `Build` → `Build Solution` (Ctrl+Shift+B)
3. 等待编译完成（应该显示 `Build succeeded`）

**或者在Rider中：**
1. 关闭UE编辑器
2. 点击 `Build` → `Build Solution`
3. 等待编译完成

### 步骤3：重新打开UE编辑器

1. 打开 `Sguo.uproject`
2. UE会提示模块已更改，点击 `Yes` 重新编译
3. 等待编辑器启动完成

### 步骤4：验证修复

1. 打开 `ST_UnitAI` StateTree资产
2. 在任意State中点击 `Add Task`
3. 在搜索框中输入：
   - `Find Target` - 应该能找到
   - `Move To Target` - 应该能找到
   - `Perform Attack` - 应该能找到

## 📋 完整的Task列表

修复后，您应该能在StateTree编辑器中看到以下Tasks：

### AI|Combat 分类
- ✅ **Find Target** - 查找最近的敌人或主城
- ✅ **Perform Attack** - 执行GAS攻击能力

### AI|Movement 分类
- ✅ **Move To Target** - 导航移动到目标

## 🔧 如何在StateTree中使用

### 示例：构建完整的AI状态树

#### State 1: Idle（空闲）

1. **添加Task：**
   - 点击 `Add Task`
   - 搜索：`Find Target`
   - 添加到状态

2. **配置参数：**
   ```
   Find Target:
   ├─ Search Radius: 2000.0
   └─ Prioritize Main City: false
   ```

3. **添加转换：**
   - 条件：`Found Target != None`
   - 目标状态：`Chase`

#### State 2: Chase（追击）

1. **添加Task：**
   - 点击 `Add Task`
   - 搜索：`Move To Target`
   - 添加到状态

2. **配置参数：**
   ```
   Move To Target:
   ├─ Target Actor: (从Context获取)
   ├─ Acceptance Radius: 150.0
   └─ Use Attack Range As Acceptance: true
   ```

3. **添加转换：**
   - 条件：`In Attack Range`
   - 目标状态：`Attack`

#### State 3: Attack（攻击）

1. **添加Task：**
   - 点击 `Add Task`
   - 搜索：`Perform Attack`
   - 添加到状态

2. **配置参数：**
   ```
   Perform Attack:
   ├─ Face Target Before Attack: true
   └─ Attack Interval: 1.0
   ```

3. **添加转换：**
   - 条件：`Target Invalid`
   - 目标状态：`Idle`

## 🐛 常见问题

### 问题1：编译后仍然找不到Tasks

**解决方案：**
1. 完全关闭UE编辑器
2. 删除以下文件夹：
   - `Binaries/`
   - `Intermediate/`
   - `Saved/`
3. 右键 `.uproject` → `Generate Visual Studio project files`
4. 在Visual Studio中重新编译
5. 启动UE编辑器

### 问题2：编译错误

**可能的错误：**
```
error C2039: 'StaticStruct': is not a member of '...'
```

**解决方案：**
- 确保在 `Sguo.Build.cs` 中包含了 `StateTreeModule`：
```csharp
PublicDependencyModuleNames.AddRange(new string[] {
    "StateTreeModule",
    "GameplayStateTreeModule",
    // ...
});
```

### 问题3：Tasks显示但无法使用

**可能原因：**
- InstanceData 类型没有正确定义
- `GetInstanceDataType()` 返回值不正确

**解决方案：**
- 检查 `using FInstanceDataType = ...` 是否正确
- 确保 `GetInstanceDataType()` 返回 `FInstanceDataType::StaticStruct()`

## 📝 技术说明

### meta 标签的作用

`USTRUCT` 的 `meta` 标签用于提供编辑器元数据：

| 标签 | 作用 | 示例 |
|------|------|------|
| DisplayName | 编辑器显示名称 | `"Find Target"` |
| Category | 任务分类 | `"AI\|Combat"` |
| ToolTip | 工具提示 | `"Finds nearest enemy"` |

### Category 命名规范

推荐使用 `|` 分隔符创建层级结构：

```cpp
"AI|Combat"     // AI → Combat
"AI|Movement"   // AI → Movement
"AI|Utility"    // AI → Utility
"Game|Player"   // Game → Player
```

### 为什么需要这些标签？

1. **DisplayName**：
   - UE编辑器使用反射系统查找可用的Tasks
   - 没有DisplayName，编辑器不知道如何显示这个Task

2. **Category**：
   - 在大型项目中，可能有数百个Tasks
   - Category帮助组织和查找Tasks
   - 提高开发效率

## ✅ 验证清单

修复完成后，请检查以下项：

- [ ] Visual Studio编译成功，无错误
- [ ] UE编辑器启动无错误
- [ ] StateTree编辑器中可以找到 `Find Target`
- [ ] StateTree编辑器中可以找到 `Move To Target`
- [ ] StateTree编辑器中可以找到 `Perform Attack`
- [ ] Tasks分类正确（Combat / Movement）
- [ ] 可以添加Tasks到State
- [ ] Tasks参数可以正常编辑

## 🎯 下一步

完成修复后，您可以：

1. **继续创建StateTree资产**
   - 按照 `AI系统蓝图创建快速指南.md` 操作
   - 构建完整的AI状态树

2. **测试AI功能**
   - 创建测试关卡
   - 放置单位并验证AI行为

3. **继续开发其他功能**
   - 英雄技能系统
   - 策略卡系统

---

## 📊 修复总结

| 项目 | 修改前 | 修改后 |
|------|--------|--------|
| FindTarget | `USTRUCT()` | `USTRUCT(meta = (DisplayName = "Find Target", Category = "AI\|Combat"))` |
| MoveToTarget | `USTRUCT()` | `USTRUCT(meta = (DisplayName = "Move To Target", Category = "AI\|Movement"))` |
| PerformAttack | `USTRUCT()` | `USTRUCT(meta = (DisplayName = "Perform Attack", Category = "AI\|Combat"))` |

**Git提交：** `bc79e89`  
**状态：** ✅ 已推送到GitHub

---

**问题已修复！现在重新编译后，您应该能在StateTree编辑器中找到所有自定义Tasks了。** 🎉
