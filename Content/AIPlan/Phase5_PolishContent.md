# Phase 5: Polish & Content — 详细实施计划

## 概述

Phase 5 依赖 Phase 4，包含三个可并行子任务：5A 编辑器高级功能、5B 关卡设计、5C 视觉反馈+测试。

---

## 5A: 编辑器高级功能

### 新增 Widget 清单
- WBP_GroupManagerPanel — 分组管理面板（新建/编辑/删除分组，拾色器）
- WBP_GroupEntry — 分组列表单行条目（色块+名称+删除）
- WBP_ColorPickerPopup — HSV 颜色拾取器
- WBP_StyleSelector — 样式选择面板（缩略图网格）
- WBP_StatusBar — 底部状态栏

### 门/踏板关联放置状态机

```
Normal → 选门笔刷+点击 → PlacingPlatesForDoor
  自动创建新分组 + 放置门 + 切换到踏板放置模式
  点击 Floor → 放置同组踏板
  点击同组踏板 → 移除
  Esc → 退出回 Normal（无踏板时警告）

Normal → 点击已有门 → EditingDoorGroup
  进入该门的踏板编辑模式
  操作同上
  Esc → 退出

擦除门 → 确认后删除门+同组所有踏板+分组
```

### 安全检查
- 擦除含起点/箱子/门/目标格时弹确认
- 保存前校验：无起点/目标格 → Error 阻止；无踏板门/不连通 → Warn

### 验收标准
A1-A12: 分组 CRUD、关联放置、样式选择、安全检查、新建关卡、状态栏

---

## 5B: 关卡设计

### Level 1 — 教学关 (8x6)

```
W  W  W  W  W  W  W  W
W  .  .  .  .  .  .  W
W  .  P  .  .  D0 .  W
W  .  .  B  .  .  .  W
W  S  .  .  P0 .  G  W
W  W  W  W  W  W  W  W
```
- 1箱子 + 1踏板 + 1门 + 1目标格
- 教会推箱子压踏板开门，走到目标格通关
- 约 8-10 步

### Level 2 — 双组门 (10x8)

```
W  W  W  W  W  W  W  W  W  W
W  .  .  .  W  .  .  .  .  W
W  .  .  .  D0 .  .  G  .  W
W  .  B  .  W  .  B  .  .  W
W  .  .  S  W  W  D1 W  W  W
W  .  P0 .  .  .  .  .  .  W
W  .  .  .  W  P1 .  .  .  W
W  W  W  W  W  W  W  W  W  W
```
- 2箱子 + 2组各1踏板1门 + 目标格
- 颜色配对：红踏板开红门，蓝踏板开蓝门
- 约 18-22 步

### Level 3 — 多开关联动 (10x9)

```
W  W  W  W  W  W  W  W  W  W
W  .  .  .  .  .  .  .  .  W
W  .  B  .  W  W  D0 .  G  W
W  .  .  .  I  I  I  .  .  W
W  .  B  .  I  .  I  .  .  W
W  S  .  .  I  I  I  .  .  W
W  .  .  .  W  W  W  .  .  W
W  .  P0 .  B  .  P0 .  .  W
W  W  W  W  W  W  W  W  W  W
```
- 3箱子 + 1组2踏板 + 冰面区域 + 目标格
- 引入冰面滑行，一门多踏板
- 约 20-28 步

### Level 4 — 综合挑战 (12x11)

```
W  W  W  W  W  W  W  W  W  W  W  W
W  .  .  .  .  W  .  .  .  .  .  W
W  .  B  .  .  D0 .  .  B  .  .  W
W  .  .  .  .  W  .  .  .  P2 .  W
W  P0 .  .  .  W  W  W  D2 W  W  W
W  .  .  S  .  .  .  .  .  .  .  W
W  .  B  .  I  I  I  .  B  .  .  W
W  P1 .  .  I  E  I  .  .  P1 .  W
W  .  .  .  I  I  I  .  .  .  .  W
W  .  B  .  D1 .  .  .  .  G  .  W
W  W  W  W  W  W  W  W  W  W  W  W
```
- 5箱子 + 3组门 + 冰面 + 坑洞(E) + 目标格
- 综合所有机制，坑洞陷阱可能导致死局（需撤销）
- 约 40-55 步

每个关卡均有完整 JSON 数据（见总体架构文档格式）。

---

## 5C: 视觉反馈 + 集成测试

### 视觉效果清单
| 效果 | 触发条件 | 实现方式 |
|------|---------|---------|
| 箱子到位反馈 | 箱子在踏板上 | 动态材质 EmissiveColor 渐变为 GroupColor |
| 踏板激活发光 | 踏板被占据 | EmissiveIntensity 0.5→5.0，可选 PointLight |
| 门开关动画 | 踏板组激活/失活 | Timeline Z轴下沉/上升 0.5秒 |
| 冰面材质 | Ice 格子 | 半透明浅蓝高光材质 |
| 冰面滑行轨迹 | 冰面滑行中 | Decal 贴花 1.5秒淡出 或 加速移动+粒子 |
| 目标格脉冲 | 始终 | Emissive 金色 Sine 脉冲 2.0-6.0 |
| 坑洞填平 | 箱子入坑 | 箱子缩小下沉 + 地面升起 |
| 过关特效 | 通关 | 屏幕闪光 + 面板淡入 |

### UI 面板
- WBP_GameHUD: 关卡名+步数+撤销+重置
- WBP_LevelCompletePanel: 步数统计+下一关/重玩/返回
- WBP_MainMenu: 开始游戏+编辑器+关卡选择

### 测试用例清单
- 功能测试 F01-F31: 基础移动、推箱子、坑洞、踏板/门、冰面、通关、撤销、关卡流转
- 编辑器测试 E01-E17: 新建、笔刷绘制、擦除、门/踏板放置、保存/加载、测试
- 边界测试 X01-X18: 并发输入、移动锁、门上卡箱子、冰面+坑洞、大地图性能、空JSON

### 验收标准
C1-C14: 所有视觉效果可见、UI 功能正常、全流程通关无崩溃、所有测试通过、帧率>30FPS
