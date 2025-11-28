# 站桩单位系统 (Stationary Unit System)

## 📋 概述

站桩单位系统是基于 `ASG_UnitsBase` 的扩展，用于创建不可移动的特殊单位，如召唤物、陷阱、固定防御塔等。

## ✨ 新增功能

### 1. **站桩单位类** (`ASG_StationaryUnit`)

#### 文件位置
- 头文件：`Source/Sguo/Public/Units/SG_StationaryUnit.h`
- 实现文件：`Source/Sguo/Private/Units/SG_StationaryUnit.cpp`

#### 核心特性

| 特性 | 配置属性 | 说明 |
|------|----------|------|
| **浮空模式** | `bEnableHover` | 单位可以悬浮在空中（如雷电网、旋转飞刀） |
| **浮空高度** | `HoverHeight` | 相对于生成位置的垂直偏移（厘米） |
| **禁用重力** | `bDisableGravity` | 防止单位受重力影响掉落 |
| **禁用移动** | `bDisableMovement` | 完全禁止单位移动 |
| **可被选中** | `bCanBeTargeted` | 控制AI是否能选择此单位为攻击目标 |

### 2. **AI 寻敌过滤**

#### 修改的文件
- `Source/Sguo/Public/Units/SG_UnitsBase.h` - 添加 `CanBeTargeted()` 虚函数
- `Source/Sguo/Private/Units/SG_UnitsBase.cpp` - 实现默认行为（返回 true）
- `Source/Sguo/Private/AI/SG_AIControllerBase.cpp` - 在 `FindNearestTarget()` 中过滤

#### 工作原理
```cpp
// AI 寻敌时会检查
if (!Unit->CanBeTargeted())
{
    continue; // 跳过不可选中的单位
}
```

## 🎯 使用场景

### 场景 1：诸葛亮雷电网
```cpp
// 蓝图或 C++ 配置
bEnableHover = true;           // 启用浮空
HoverHeight = 50.0f;           // 贴近地面
bDisableGravity = true;        // 禁用重力
bDisableMovement = true;       // 禁用移动
bCanBeTargeted = false;        // 不可被选中（敌人会穿过）
```

### 场景 2：吕布旋转飞刀
```cpp
bEnableHover = true;           // 启用浮空
HoverHeight = 120.0f;          // 角色腰部高度
bDisableGravity = true;        // 禁用重力
bDisableMovement = true;       // 禁用移动
bCanBeTargeted = true;         // 可被攻击（敌人会停下攻击它）
```

### 场景 3：固定防御塔
```cpp
bEnableHover = false;          // 站立地面
HoverHeight = 0.0f;            // 不需要浮空
bDisableGravity = false;       // 保留物理效果
bDisableMovement = true;       // 禁用移动
bCanBeTargeted = true;         // 可被攻击
```

## 🔧 在蓝图中使用

### 步骤 1：创建蓝图类
1. 在内容浏览器中右键
2. 选择 `Blueprint Class`
3. 父类选择 `SG_StationaryUnit`
4. 命名为 `BP_StationaryUnit_XXX`

### 步骤 2：配置属性
打开蓝图，在 `Stationary Unit` 类别下配置：
- ✅ **启用浮空模式**：勾选 `bEnableHover`
- 🎚️ **浮空高度(厘米)**：设置 `HoverHeight`（如：100.0）
- ⚡ **禁用重力**：勾选 `bDisableGravity`
- 🚫 **禁用移动**：勾选 `bDisableMovement`
- 🎯 **可被选为目标**：根据需求勾选 `bCanBeTargeted`

### 步骤 3：使用相同的卡牌和数据
站桩单位可以使用与普通单位相同的：
- ✅ **DataTable 配置**
- ✅ **攻击技能**
- ✅ **GAS 属性**
- ✅ **角色卡数据**

## 📊 属性说明

### 浮空高度参考值

| 场景 | 推荐高度 | 说明 |
|------|----------|------|
| 贴地效果 | 50-100 cm | 雷电网、地面陷阱 |
| 腰部高度 | 100-150 cm | 旋转飞刀、护盾 |
| 头顶高度 | 180-220 cm | 光环、祝福效果 |
| 明显悬浮 | 200-500 cm | 漂浮物体、飞行单位 |

### 可被选中 (bCanBeTargeted) 的战术意义

| 设置 | 敌人行为 | 适用场景 |
|------|----------|----------|
| **True** | 停下攻击 | 拖延敌军、保护后排 |
| **False** | 直接穿过 | 区域控制、纯伤害效果 |

## 🔍 技术细节

### 继承关系
```
ACharacter (UE引擎基类)
  └─ ASG_UnitsBase (项目单位基类)
      └─ ASG_StationaryUnit (站桩单位)
```

### 核心函数

#### `CanBeTargeted() const`
- **功能**：检查单位是否可被AI选为目标
- **默认**：`ASG_UnitsBase` 返回 `true`
- **重写**：`ASG_StationaryUnit` 返回 `bCanBeTargeted` 配置值
- **虚函数**：子类可以进一步重写（如：受伤后才可被选中）

#### `ApplyStationarySettings()`
- **调用时机**：`BeginPlay` 时自动调用
- **功能**：
  1. 禁用移动能力
  2. 应用浮空效果
  3. 调整重力设置

#### `DisableMovementCapability()`
- **功能**：完全禁用 `CharacterMovement` 组件
- **实现**：
  ```cpp
  MovementComp->MaxWalkSpeed = 0.0f;
  MovementComp->SetComponentTickEnabled(false);
  MovementComp->SetMovementMode(MOVE_None);
  ```

#### `ApplyHoverEffect()`
- **功能**：提升单位到指定高度
- **实现**：
  ```cpp
  FVector NewLocation = CurrentLocation;
  NewLocation.Z += HoverHeight;
  SetActorLocation(NewLocation, false, nullptr, ETeleportType::TeleportPhysics);
  ```

## 🐛 调试建议

### 检查站桩单位是否正常工作
1. **在编辑器中查看**：站桩单位应该显示在正确的高度
2. **检查日志**：启用 `LogSGUnit` 日志类别
3. **测试移动**：站桩单位不应该响应移动指令
4. **测试AI**：观察敌人是否正确选择/忽略目标

### 常见问题

#### Q: 站桩单位还在移动？
**A**: 检查 `bDisableMovement` 是否设置为 `true`

#### Q: 浮空单位掉落到地面？
**A**: 确保 `bDisableGravity` 设置为 `true` 且 `bEnableHover` 为 `true`

#### Q: AI仍然攻击不可选中的单位？
**A**: 确认 `bCanBeTargeted` 设置为 `false`，并且 AI 使用的是 `FindNearestTarget` 或 `ASG_AIControllerBase::FindNearestTarget`

## 📝 代码示例

### C++ 中动态生成站桩单位
```cpp
ASG_StationaryUnit* StationaryUnit = GetWorld()->SpawnActor<ASG_StationaryUnit>(
    BP_StationaryUnit_Class,
    SpawnLocation,
    SpawnRotation
);

if (StationaryUnit)
{
    // 配置站桩属性
    StationaryUnit->bEnableHover = true;
    StationaryUnit->HoverHeight = 100.0f;
    StationaryUnit->bCanBeTargeted = false;
    
    // 初始化单位（使用与普通单位相同的流程）
    StationaryUnit->InitializeCharacter(
        FGameplayTag::RequestGameplayTag("Unit.Faction.Player"),
        1.0f, // HealthMultiplier
        1.0f, // DamageMultiplier
        1.0f  // SpeedMultiplier
    );
}
```

## 🚀 下一步

站桩单位系统为以下功能奠定了基础：
- ✅ 诸葛亮雷电网
- ✅ 吕布旋转飞刀
- ⏳ 陷阱系统
- ⏳ 固定防御塔
- ⏳ 召唤物系统

---

## 📞 技术支持

如有问题或建议，请参考：
- 主项目文档：`README.md`
- 策划案：`卡牌即时战略.docx`
- 日志类别：`LogSGUnit` (Units/SG_UnitsBase.cpp)
