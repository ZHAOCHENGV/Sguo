# AI系统蓝图创建快速指南

## 🎯 概述

本指南将帮助您在UE 5.6编辑器中快速创建AI系统所需的蓝图资产。

**预计时间：** 15-20分钟  
**前置条件：** C++代码已编译成功

---

## 📋 任务检查清单

- [ ] 创建AIController蓝图 (BP_AIController)
- [ ] 创建StateTree资产 (ST_UnitAI)
- [ ] 配置单位蓝图AI属性
- [ ] 设置导航网格
- [ ] 测试AI功能

---

## 1️⃣ 创建AIController蓝图

### 步骤

1. **打开Content Browser**
   - 导航到：`Content/Blueprints/AI/`（如果没有此文件夹，请创建）

2. **创建蓝图类**
   - 右键 → `Blueprint Class`
   - 在搜索框输入：`SG_AIControllerBase`
   - 选择 `SG_AIControllerBase` 类
   - 命名：`BP_AIController`

3. **配置蓝图**
   - 双击打开 `BP_AIController`
   - 在 **Details** 面板中配置：

   ```
   AI Config:
   ├─ Target Search Radius: 2000.0
   ├─ Auto Find Target: ✓ (勾选)
   └─ Prioritize Main City: ✗ (不勾选)
   ```

4. **保存**
   - 点击 `Compile`
   - 点击 `Save`

### 预期结果

✅ 创建了基于 `SG_AIControllerBase` 的蓝图  
✅ 配置了搜索半径和目标优先级

---

## 2️⃣ 创建StateTree资产

### 步骤

1. **打开Content Browser**
   - 导航到：`Content/Blueprints/AI/`

2. **创建StateTree**
   - 右键 → `AI` → `State Tree`
   - 命名：`ST_UnitAI`

3. **打开StateTree编辑器**
   - 双击 `ST_UnitAI`

4. **构建状态树**

   #### State 1: 空闲 (Idle)
   
   **添加状态：**
   - 点击 `+` 添加新状态
   - 命名：`Idle`
   
   **添加Task：**
   - 点击 `Add Task`
   - 搜索：`SG_StateTreeTask_FindTarget`
   - 添加到状态
   
   **配置Task参数：**
   ```
   FindTarget:
   ├─ Search Radius: 2000.0
   └─ Prioritize Main City: false
   ```
   
   **添加转换（Transition）：**
   - 点击 `Add Transition`
   - 条件：`Found Target != None`
   - 目标状态：`Chase`（稍后创建）

   #### State 2: 追击 (Chase)
   
   **添加状态：**
   - 点击 `+` 添加新状态
   - 命名：`Chase`
   
   **添加Task：**
   - 点击 `Add Task`
   - 搜索：`SG_StateTreeTask_MoveToTarget`
   - 添加到状态
   
   **配置Task参数：**
   ```
   MoveToTarget:
   ├─ Target Actor: (从Context获取)
   ├─ Acceptance Radius: 150.0
   └─ Use Attack Range As Acceptance: true
   ```
   
   **添加转换：**
   - 点击 `Add Transition`
   - 条件：`In Attack Range`
   - 目标状态：`Attack`（稍后创建）

   #### State 3: 攻击 (Attack)
   
   **添加状态：**
   - 点击 `+` 添加新状态
   - 命名：`Attack`
   
   **添加Task：**
   - 点击 `Add Task`
   - 搜索：`SG_StateTreeTask_PerformAttack`
   - 添加到状态
   
   **配置Task参数：**
   ```
   PerformAttack:
   ├─ Face Target Before Attack: true
   └─ Attack Interval: 1.0
   ```
   
   **添加转换：**
   - 点击 `Add Transition`
   - 条件：`Target Invalid`
   - 目标状态：`Idle`

5. **保存StateTree**
   - 点击 `Save`

### 预期结果

✅ 创建了完整的StateTree状态机  
✅ 包含：空闲 → 追击 → 攻击 循环

---

## 3️⃣ 配置单位蓝图

### 步骤

1. **打开单位蓝图**
   - 导航到：`Content/Blueprints/Units/`
   - 打开：`BP_Unit_Infantry`（或其他单位蓝图）

2. **配置AI属性**
   - 在 **Details** 面板中找到 `AI` 分类
   - 配置：

   ```
   AI:
   ├─ Use AI Controller: ✓ (勾选)
   └─ AI Controller Class: BP_AIController
   ```

3. **配置阵营**
   - 在 **Details** 面板中找到 `Character Info` 分类
   - 配置：

   ```
   Character Info:
   └─ Faction Tag: 
      - 玩家单位：Unit.Faction.Player
      - 敌方单位：Unit.Faction.Enemy
   ```

4. **保存**
   - 点击 `Compile`
   - 点击 `Save`

### 预期结果

✅ 单位自动使用AI控制器  
✅ 正确设置了阵营标签

---

## 4️⃣ 配置导航网格

### 步骤

1. **在关卡中添加Nav Mesh Bounds Volume**
   - 打开测试关卡（或创建新关卡）
   - 在 `Place Actors` 面板中搜索：`Nav Mesh Bounds Volume`
   - 拖拽到场景中

2. **调整大小**
   - 选中 Nav Mesh Bounds Volume
   - 使用缩放工具（R键）调整大小
   - 确保覆盖整个战场区域

3. **查看导航网格**
   - 按 `P` 键显示导航网格
   - 绿色区域：可通行
   - 红色区域：不可通行
   - 如果没有显示，检查体积是否够大

4. **配置项目设置（可选）**
   - `Edit` → `Project Settings` → `Navigation Mesh`
   - 配置：

   ```
   Navigation Mesh:
   ├─ Runtime Generation: Dynamic
   ├─ Cell Size: 19.0
   ├─ Cell Height: 10.0
   ├─ Agent Radius: 34.0
   └─ Agent Height: 144.0
   ```

### 预期结果

✅ 导航网格覆盖战场  
✅ 按P键可以看到绿色网格

---

## 5️⃣ 测试AI功能

### 创建测试关卡

1. **新建关卡**
   - `File` → `New Level`
   - 选择：`Empty Level`
   - 保存为：`TestMap_AI`

2. **添加基础元素**
   - 添加：`Directional Light`（光源）
   - 添加：`Sky Atmosphere`（天空）
   - 添加：`Floor`（地板 - 使用Plane或Cube）

3. **添加Nav Mesh Bounds Volume**
   - 按照上面的步骤添加并调整大小

4. **放置测试单位**
   
   **玩家方：**
   - 拖入 `BP_Unit_Infantry` × 3
   - 设置 `Faction Tag`: `Unit.Faction.Player`
   - 位置：(0, 0, 100), (200, 0, 100), (400, 0, 100)
   
   **敌方：**
   - 拖入 `BP_Unit_Infantry` × 3
   - 设置 `Faction Tag`: `Unit.Faction.Enemy`
   - 位置：(1000, 0, 100), (1200, 0, 100), (1400, 0, 100)

5. **运行测试**
   - 点击 `Play` (Alt+P)
   - 观察AI行为

### 预期行为

✅ **阶段1：查找目标**
- 单位启动后自动查找最近的敌人
- 日志输出：`🎯 找到最近的敌人`

✅ **阶段2：移动到目标**
- 单位沿导航网格移动到敌人
- 日志输出：`✅ 开始移动到目标`
- 到达后：`✅ 已到达目标`

✅ **阶段3：攻击**
- 单位面向敌人并开始攻击
- 日志输出：`⚔️ AI触发攻击`
- 敌人生命值减少

✅ **阶段4：循环**
- 敌人死亡后，查找下一个目标
- 重复上述过程

### 查看日志

1. **打开Output Log**
   - `Window` → `Developer Tools` → `Output Log`

2. **过滤日志**
   - 在搜索框输入：`LogSGGameplay`
   - 只显示AI相关日志

3. **重要日志标记**
   ```
   🤖 - AI控制器事件
   🎯 - 目标查找
   🗺️ - 移动相关
   ⚔️ - 攻击相关
   ✅ - 成功
   ❌ - 失败/错误
   ```

---

## 🐛 常见问题

### 问题1：单位不移动

**可能原因：**
- ❌ 没有添加Nav Mesh Bounds Volume
- ❌ Nav Mesh Bounds Volume太小
- ❌ 单位不在导航网格上

**解决方案：**
1. 按 `P` 键检查导航网格
2. 调整Nav Mesh Bounds Volume大小
3. 确保单位生成在绿色区域内

### 问题2：单位不攻击

**可能原因：**
- ❌ 没有正确配置FactionTag
- ❌ 攻击能力没有授予
- ❌ StateTree没有正确配置

**解决方案：**
1. 检查单位的FactionTag是否不同
2. 查看日志确认是否授予了攻击能力
3. 重新检查StateTree配置

### 问题3：单位不查找目标

**可能原因：**
- ❌ AI Controller没有正确分配
- ❌ StateTree资产没有设置
- ❌ 搜索半径太小

**解决方案：**
1. 确认单位的 `AI Controller Class` 设置为 `BP_AIController`
2. 确认 `bUseAIController` 已勾选
3. 增大 `Target Search Radius`

### 问题4：编译错误

**可能原因：**
- ❌ C++代码没有成功编译
- ❌ 头文件包含缺失

**解决方案：**
1. 在Visual Studio中重新编译项目
2. 检查是否有编译错误
3. 确保所有头文件正确包含

---

## 📊 性能优化建议

### 大规模单位场景（100+单位）

1. **减少查找频率**
   ```cpp
   // 在FindTarget Task中添加延迟
   float FindTargetInterval = 0.5f; // 每0.5秒查找一次
   ```

2. **使用空间分割**
   - 实现Octree或Grid-based查询
   - 减少O(n)的暴力查找

3. **LOD系统**
   - 远距离单位使用简化AI逻辑
   - 近距离单位使用完整AI

4. **批量更新**
   - 不是每帧更新所有单位
   - 分批更新，每批10-20个单位

---

## ✅ 完成检查

完成以下检查后，AI系统即可投入使用：

- [ ] BP_AIController 创建并配置
- [ ] ST_UnitAI 创建并配置状态树
- [ ] 单位蓝图配置AI属性
- [ ] 导航网格正确显示
- [ ] 测试关卡可以正常运行
- [ ] 单位可以自动查找目标
- [ ] 单位可以自动移动到目标
- [ ] 单位可以自动攻击
- [ ] 日志输出正常

---

## 🚀 下一步

AI系统完成后，可以继续以下工作：

1. **添加更多AI行为**
   - 巡逻系统
   - 撤退逻辑
   - 支援队友

2. **优化性能**
   - 实现LOD系统
   - 优化目标查找算法

3. **继续其他系统**
   - 英雄技能系统
   - 策略卡系统
   - 主城弓箭手系统

---

**祝您配置顺利！如有问题请随时咨询。** 🎮
