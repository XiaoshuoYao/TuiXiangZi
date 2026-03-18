# Module E: 分组系统（Group System）

## 概述
右侧分组管理面板及其子组件，包含 GroupManagerPanel、GroupEntryWidget、ColorPickerPopup 三个紧密耦合的 Widget。管理门/压力板的分组 CRUD 和颜色编辑。

## 前置依赖
- 无编译依赖（可与 A/B/C/D 并行开发）
- 运行时依赖 Module A（ConfirmDialog 用于删除确认），由 MainWidget 串联

## 产出文件
```
Source/TuiXiangZi/UI/Editor/GroupManagerPanel.h
Source/TuiXiangZi/UI/Editor/GroupManagerPanel.cpp
Source/TuiXiangZi/UI/Editor/GroupEntryWidget.h
Source/TuiXiangZi/UI/Editor/GroupEntryWidget.cpp
Source/TuiXiangZi/UI/Editor/ColorPickerPopup.h
Source/TuiXiangZi/UI/Editor/ColorPickerPopup.cpp
Content/Blueprints/UI/Editor/WBP_GroupManagerPanel.uasset
Content/Blueprints/UI/Editor/WBP_GroupEntryWidget.uasset
Content/Blueprints/UI/Editor/WBP_ColorPickerPopup.uasset
```

---

## Widget 1: UGroupEntryWidget（分组条目）

### 类定义
```cpp
UCLASS()
class UGroupEntryWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void Setup(int32 InGroupId, const FText& InDisplayName, FLinearColor InBaseColor);

    void SetSelected(bool bSelected);
    void SetBaseColor(FLinearColor NewColor);
    void SetInteractionEnabled(bool bEnabled);  // PlacingPlatesForDoor 模式限制

    int32 GetGroupId() const { return GroupId; }

    // --- Delegates ---
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGroupEntryAction, int32, GroupId);

    UPROPERTY(BlueprintAssignable)
    FOnGroupEntryAction OnRowClicked;      // 整行点击 → 选中

    UPROPERTY(BlueprintAssignable)
    FOnGroupEntryAction OnColorEditClicked; // 🎨 按钮

    UPROPERTY(BlueprintAssignable)
    FOnGroupEntryAction OnDeleteClicked;    // 🗑 按钮

protected:
    UPROPERTY(meta = (BindWidget))
    UButton* RowButton;        // 整行可点击

    UPROPERTY(meta = (BindWidget))
    UTextBlock* SelectionMark;  // "▶" 选中标记

    UPROPERTY(meta = (BindWidget))
    UTextBlock* NameText;       // 分组名

    UPROPERTY(meta = (BindWidget))
    UImage* ColorPreview;       // 颜色预览方块

    UPROPERTY(meta = (BindWidget))
    UButton* ColorEditButton;   // 🎨 调色板

    UPROPERTY(meta = (BindWidget))
    UButton* DeleteButton;      // 🗑 删除

    int32 GroupId = 0;
    bool bIsSelected = false;

    virtual void NativeConstruct() override;

    UFUNCTION() void HandleRowClicked();
    UFUNCTION() void HandleColorEditClicked();
    UFUNCTION() void HandleDeleteClicked();
};
```

### 布局
```
[RowButton 整行按钮, 高度 36px]
└─ HorizontalBox
   ├─ [TextBlock 16px] SelectionMark: "▶"(选中) / " "(未选中)
   ├─ [Spacer 4px]
   ├─ [TextBlock Fill] NameText: "Group 1"
   ├─ [Spacer 8px]
   ├─ [Image 20x20] ColorPreview: 纯色方块，颜色 = BaseColor
   ├─ [Spacer 4px]
   ├─ [Button 28x28] ColorEditButton: "🎨" 文字或调色板图标
   ├─ [Spacer 4px]
   └─ [Button 28x28] DeleteButton: "🗑" 文字或删除图标
```

### 交互逻辑
```
Setup(GroupId, DisplayName, BaseColor):
  ├─ this->GroupId = GroupId
  ├─ NameText->SetText(DisplayName)
  └─ ColorPreview->SetColorAndOpacity(BaseColor)

SetSelected(bSelected):
  ├─ bIsSelected = bSelected
  ├─ SelectionMark->SetText(bSelected ? "▶" : " ")
  └─ RowButton 背景:
      ├─ 选中: #2A4466 (蓝色调)
      └─ 未选中: 透明

SetBaseColor(NewColor):
  └─ ColorPreview->SetColorAndOpacity(NewColor)

SetInteractionEnabled(bEnabled):
  ├─ ColorEditButton->SetIsEnabled(bEnabled)
  ├─ DeleteButton->SetIsEnabled(bEnabled)
  └─ RowButton->SetIsEnabled(bEnabled)
      // PlacingPlatesForDoor 模式下:
      //   当前分组: ColorEditButton 可用, 其他禁用
      //   其他分组: 全部禁用

悬浮效果:
  ├─ RowButton OnHovered → 背景 #333333
  └─ RowButton OnUnhovered → 恢复原色
```

---

## Widget 2: UGroupManagerPanel（分组管理面板）

### 类定义
```cpp
UCLASS()
class UGroupManagerPanel : public UUserWidget
{
    GENERATED_BODY()
public:
    // --- 供 MainWidget 调用 ---
    UFUNCTION(BlueprintCallable)
    void AddGroupEntry(int32 GroupId, const FMechanismGroupStyleData& Style);

    UFUNCTION(BlueprintCallable)
    void RemoveGroupEntry(int32 GroupId);

    UFUNCTION(BlueprintCallable)
    void RefreshActiveGroup(int32 ActiveGroupId);

    UFUNCTION(BlueprintCallable)
    void UpdateGroupColor(int32 GroupId, FLinearColor NewBaseColor);

    UFUNCTION(BlueprintCallable)
    void ClearAll();

    UFUNCTION(BlueprintCallable)
    void RebuildFromGameMode(ALevelEditorGameMode* GameMode);

    // PlacingPlatesForDoor 模式下的限制
    UFUNCTION(BlueprintCallable)
    void SetPlacementMode(bool bIsPlacing, int32 PlacingGroupId = 0);

    // --- Delegates（供 MainWidget 绑定）---
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGroupManagerAction, int32, GroupId);

    UPROPERTY(BlueprintAssignable)
    FOnGroupManagerAction OnSelectGroup;

    UPROPERTY(BlueprintAssignable)
    FOnGroupManagerAction OnEditGroupColor;

    UPROPERTY(BlueprintAssignable)
    FOnGroupManagerAction OnDeleteGroup;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRequestNewGroup);
    UPROPERTY(BlueprintAssignable)
    FOnRequestNewGroup OnRequestNewGroup;

protected:
    UPROPERTY(meta = (BindWidget))
    UButton* NewGroupButton;

    UPROPERTY(meta = (BindWidget))
    UScrollBox* GroupList;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* EmptyHintText;   // "放置门以创建新分组"

    // GroupEntryWidget 的 Blueprint 类引用（在 Blueprint 中设置）
    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<UGroupEntryWidget> GroupEntryWidgetClass;

    UPROPERTY()
    TMap<int32, UGroupEntryWidget*> EntryMap;

    int32 CurrentActiveGroupId = 0;

    virtual void NativeConstruct() override;

    UFUNCTION() void HandleNewGroupClicked();

    // GroupEntry delegate 中转
    UFUNCTION() void HandleEntryRowClicked(int32 GroupId);
    UFUNCTION() void HandleEntryColorEditClicked(int32 GroupId);
    UFUNCTION() void HandleEntryDeleteClicked(int32 GroupId);

    void UpdateEmptyHint();
};
```

### 布局
```
┌──────── 分组管理 ──────────┐   宽度: 250px 固定
│                            │
│  [+ 新建分组]              │   ← Button, 宽度 Fill
│                            │
├────────────────────────────┤   ← Separator 1px
│                            │
│  ScrollBox (GroupList):    │   ← 填充剩余高度
│  ┌────────────────────────┐│
│  │ ▶ Group 1  [■][🎨][🗑]││   ← GroupEntryWidget
│  ├────────────────────────┤│
│  │   Group 2  [■][🎨][🗑]││
│  ├────────────────────────┤│
│  │   Group 3  [■][🎨][🗑]││
│  └────────────────────────┘│
│                            │
│  (空列表时显示):            │
│  "放置门以创建新分组"       │   ← EmptyHintText, 灰色, 居中
│                            │
└────────────────────────────┘
背景: #252525
```

### 交互逻辑

**AddGroupEntry(GroupId, Style):**
```
AddGroupEntry(GroupId, Style):
  ├─ UGroupEntryWidget* Entry = CreateWidget<UGroupEntryWidget>(this, GroupEntryWidgetClass)
  ├─ Entry->Setup(GroupId, FText::FromString(Style.DisplayName), Style.BaseColor)
  │
  ├─ 绑定 Entry 的 Delegates:
  │   ├─ Entry->OnRowClicked      → HandleEntryRowClicked
  │   ├─ Entry->OnColorEditClicked → HandleEntryColorEditClicked
  │   └─ Entry->OnDeleteClicked    → HandleEntryDeleteClicked
  │
  ├─ GroupList->AddChild(Entry)
  ├─ EntryMap.Add(GroupId, Entry)
  └─ UpdateEmptyHint()
```

**RemoveGroupEntry(GroupId):**
```
RemoveGroupEntry(GroupId):
  ├─ if UGroupEntryWidget* Entry = EntryMap.FindRef(GroupId):
  │   ├─ GroupList->RemoveChild(Entry)
  │   ├─ Entry->RemoveFromParent()
  │   └─ EntryMap.Remove(GroupId)
  ├─ if GroupId == CurrentActiveGroupId:
  │   └─ CurrentActiveGroupId = 0  // 清除选中
  └─ UpdateEmptyHint()
```

**RefreshActiveGroup(ActiveGroupId):**
```
RefreshActiveGroup(ActiveGroupId):
  ├─ CurrentActiveGroupId = ActiveGroupId
  └─ for (auto& [Id, Entry] : EntryMap):
      └─ Entry->SetSelected(Id == ActiveGroupId)
```

**UpdateGroupColor(GroupId, NewBaseColor):**
```
UpdateGroupColor(GroupId, NewBaseColor):
  └─ if UGroupEntryWidget* Entry = EntryMap.FindRef(GroupId):
      └─ Entry->SetBaseColor(NewBaseColor)
```

**ClearAll():**
```
ClearAll():
  ├─ GroupList->ClearChildren()
  ├─ EntryMap.Empty()
  ├─ CurrentActiveGroupId = 0
  └─ UpdateEmptyHint()
```

**RebuildFromGameMode(GameMode):**
```
RebuildFromGameMode(GM):
  ├─ ClearAll()
  ├─ TArray<int32> Ids = GM->GetAllGroupIds()
  ├─ for (int32 Id : Ids):
  │   └─ AddGroupEntry(Id, GM->GetGroupStyle(Id))
  └─ RefreshActiveGroup(GM->GetCurrentGroupId())
```

**SetPlacementMode(bIsPlacing, PlacingGroupId):**
```
SetPlacementMode(bIsPlacing, PlacingGroupId):
  ├─ NewGroupButton->SetIsEnabled(!bIsPlacing)
  │
  └─ for (auto& [Id, Entry] : EntryMap):
      ├─ if bIsPlacing:
      │   ├─ if Id == PlacingGroupId:
      │   │   └─ Entry->SetInteractionEnabled(true)
      │   │       // 仅允许 ColorEdit（在放置过程中调颜色）
      │   │       // Row 和 Delete 仍禁用（不能切换/删除正在编辑的分组）
      │   └─ else:
      │       └─ Entry->SetInteractionEnabled(false)  // 完全禁用
      └─ else:
          └─ Entry->SetInteractionEnabled(true)  // 全部恢复
```

**Delegate 中转:**
```
HandleEntryRowClicked(GroupId):
  └─ OnSelectGroup.Broadcast(GroupId)

HandleEntryColorEditClicked(GroupId):
  └─ OnEditGroupColor.Broadcast(GroupId)

HandleEntryDeleteClicked(GroupId):
  └─ OnDeleteGroup.Broadcast(GroupId)
      // MainWidget 收到后弹出 ConfirmDialog，确认后调 GameMode->DeleteGroup()

HandleNewGroupClicked():
  └─ OnRequestNewGroup.Broadcast()

UpdateEmptyHint():
  └─ EmptyHintText->SetVisibility(EntryMap.Num() == 0 ? Visible : Collapsed)
```

---

## Widget 3: UColorPickerPopup（颜色选择器弹窗）

### 类定义
```cpp
UCLASS()
class UColorPickerPopup : public UUserWidget
{
    GENERATED_BODY()
public:
    void Setup(int32 InGroupId, FLinearColor CurrentBaseColor);

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnColorConfirmed,
        int32, GroupId, FLinearColor, BaseColor, FLinearColor, ActiveColor);
    UPROPERTY(BlueprintAssignable)
    FOnColorConfirmed OnColorConfirmed;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnColorCancelled);
    UPROPERTY(BlueprintAssignable)
    FOnColorCancelled OnCancelled;

protected:
    // --- BindWidget ---
    UPROPERTY(meta = (BindWidget))
    UImage* SVPlane;           // Saturation-Value 平面 (256x256)

    UPROPERTY(meta = (BindWidget))
    UImage* SVCursor;          // SV 平面上的十字光标

    UPROPERTY(meta = (BindWidget))
    UImage* HueBar;            // Hue 滑条 (30x256)

    UPROPERTY(meta = (BindWidget))
    UImage* HueCursor;         // Hue 滑条上的横线光标

    UPROPERTY(meta = (BindWidget))
    UImage* OldColorPreview;   // 旧颜色预览

    UPROPERTY(meta = (BindWidget))
    UImage* NewColorPreview;   // 新颜色预览

    UPROPERTY(meta = (BindWidget))
    UEditableTextBox* HexInput; // HEX 输入框

    UPROPERTY(meta = (BindWidget))
    UButton* ApplyButton;

    UPROPERTY(meta = (BindWidget))
    UButton* CancelButton;

    UPROPERTY(meta = (BindWidget))
    UButton* CloseButton;

    virtual void NativeConstruct() override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry,
        const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseMove(const FGeometry& InGeometry,
        const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry,
        const FPointerEvent& InMouseEvent) override;

    int32 GroupId = 0;
    FLinearColor OriginalColor;

    // HSV 状态
    float CurrentH = 0.0f;    // 0~360
    float CurrentS = 1.0f;    // 0~1
    float CurrentV = 1.0f;    // 0~1

    bool bDraggingSV = false;
    bool bDraggingHue = false;

    void UpdateFromHSV();
    void UpdateCursorPositions();
    void RegenerateSVTexture();  // 根据当前 H 重新生成 SV 平面纹理
    FLinearColor HSVToLinear(float H, float S, float V) const;
    void LinearToHSV(FLinearColor Color, float& OutH, float& OutS, float& OutV) const;
    FLinearColor CalculateActiveColor(FLinearColor Base) const;

    UFUNCTION() void HandleHexCommitted(const FText& Text, ETextCommit::Type CommitType);
    UFUNCTION() void HandleApplyClicked();
    UFUNCTION() void HandleCancelClicked();
};
```

### 布局
```
┌─────────── 选择颜色 ────────────┐   宽度: 360px
│                            [X]  │
├─────────────────────────────────┤
│                                 │
│  ┌──────────────────┐  ┌────┐  │
│  │                  │  │    │  │
│  │   SV 平面        │  │ H  │  │   SV: 220x220
│  │   220 x 220      │  │ Bar│  │   H Bar: 24x220
│  │                  │  │    │  │
│  └──────────────────┘  └────┘  │
│                                 │
│  旧色 [■■■■] → 新色 [■■■■]    │   预览块: 40x24 each
│                                 │
│  HEX: [#FF6600        ]        │   EditableTextBox
│                                 │
│      [取消]       [应用]        │
└─────────────────────────────────┘
```

### 交互逻辑

**Setup(GroupId, CurrentBaseColor):**
```
Setup(GroupId, BaseColor):
  ├─ this->GroupId = GroupId
  ├─ OriginalColor = BaseColor
  ├─ OldColorPreview->SetColorAndOpacity(BaseColor)
  │
  ├─ LinearToHSV(BaseColor, CurrentH, CurrentS, CurrentV)
  ├─ UpdateFromHSV()        // 更新所有视觉元素
  ├─ UpdateCursorPositions() // 放置光标到正确位置
  └─ RegenerateSVTexture()   // 根据 H 生成 SV 平面
```

**SV 平面拖拽:**
```
NativeOnMouseButtonDown:
  ├─ 检测点击是否在 SVPlane 区域内:
  │   ├─ 是 → bDraggingSV = true, 更新 S/V
  │   └─ 否 → 检测是否在 HueBar 区域内:
  │       ├─ 是 → bDraggingHue = true, 更新 H
  │       └─ 否 → 忽略
  └─ return FReply::Handled().CaptureMouse(SharedThis(this))

NativeOnMouseMove (bDraggingSV == true):
  ├─ 获取鼠标在 SVPlane 内的相对坐标 (0~1)
  ├─ CurrentS = Clamp(RelativeX, 0, 1)
  ├─ CurrentV = Clamp(1.0 - RelativeY, 0, 1)  // Y 翻转
  ├─ UpdateFromHSV()
  └─ UpdateCursorPositions()

NativeOnMouseMove (bDraggingHue == true):
  ├─ 获取鼠标在 HueBar 内的相对 Y (0~1)
  ├─ CurrentH = Clamp(RelativeY * 360.0f, 0, 360)
  ├─ RegenerateSVTexture()  // H 变了，SV 平面背景色需要重新生成
  ├─ UpdateFromHSV()
  └─ UpdateCursorPositions()

NativeOnMouseButtonUp:
  ├─ bDraggingSV = false
  ├─ bDraggingHue = false
  └─ return FReply::Handled().ReleaseMouseCapture()
```

**UpdateFromHSV():**
```
UpdateFromHSV():
  ├─ FLinearColor NewColor = HSVToLinear(CurrentH, CurrentS, CurrentV)
  ├─ NewColorPreview->SetColorAndOpacity(NewColor)
  │
  ├─ // HEX 更新
  │   FColor NewFColor = NewColor.ToFColor(true)
  │   FString Hex = FString::Printf(TEXT("#%02X%02X%02X"), NewFColor.R, NewFColor.G, NewFColor.B)
  │   HexInput->SetText(FText::FromString(Hex))
  │
  └─ SVCursor 位置 = (CurrentS * PlaneWidth, (1-CurrentV) * PlaneHeight)
     HueCursor 位置 Y = (CurrentH / 360) * BarHeight
```

**RegenerateSVTexture():**
```
// 为 SV 平面生成动态纹理
// 简化方案: 使用 Material Instance Dynamic
//   - 创建 M_SVPlane 材质: 输入 Hue 参数, 输出 SV 渐变
//   - 每次 H 变化: MID->SetScalarParameterValue("Hue", CurrentH / 360.0f)
// 这样避免每帧生成纹理
```

**HEX 输入:**
```
HandleHexCommitted(Text, CommitType):
  ├─ FString Hex = Text.ToString()
  ├─ 移除 '#' 前缀
  ├─ if Hex.Len() == 6 && IsValidHex(Hex):
  │   ├─ FColor Parsed = FColor::FromHex(Hex)
  │   ├─ FLinearColor NewColor = FLinearColor(Parsed)
  │   ├─ LinearToHSV(NewColor, CurrentH, CurrentS, CurrentV)
  │   ├─ RegenerateSVTexture()
  │   ├─ UpdateFromHSV()
  │   └─ UpdateCursorPositions()
  └─ else:
      └─ 恢复上一个有效的 HEX 值（重新调用 UpdateFromHSV）
```

**应用/取消:**
```
HandleApplyClicked():
  ├─ FLinearColor Base = HSVToLinear(CurrentH, CurrentS, CurrentV)
  ├─ FLinearColor Active = CalculateActiveColor(Base)
  └─ OnColorConfirmed.Broadcast(GroupId, Base, Active)

CalculateActiveColor(Base):
  └─ return FLinearColor(
         FMath::Min(Base.R * 1.3f, 1.0f),
         FMath::Min(Base.G * 1.3f, 1.0f),
         FMath::Min(Base.B * 1.3f, 1.0f),
         1.0f)

HandleCancelClicked():
  └─ OnCancelled.Broadcast()
```

---

## 分组颜色完整流程（3 个 Widget 协作）

### 途径 1: 放置门时自动创建
```
GameMode.PaintAtGrid(Door) → CreateNewGroup() → OnGroupCreated(Id)
  └─ MainWidget → GroupMgr->AddGroupEntry(Id, Style)
      └─ 条目显示自动分配的预设颜色
```

### 途径 2: 在 GroupManager 中手动修改颜色
```
用户点击 Entry 的 [🎨] 按钮
  → Entry.OnColorEditClicked(GroupId)
  → GroupMgr.OnEditGroupColor(GroupId)
  → MainWidget.ShowColorPicker(GroupId):
      ├─ FMechanismGroupStyleData Style = GameMode->GetGroupStyle(GroupId)
      ├─ ColorPicker->Setup(GroupId, Style.BaseColor)
      └─ MainWidget.ShowDialog(ColorPicker)

用户在 ColorPicker 中选择颜色并点击 "应用":
  → ColorPicker.OnColorConfirmed(GroupId, NewBase, NewActive)
  → MainWidget:
      ├─ GameMode->SetGroupColor(GroupId, NewBase, NewActive)
      ├─ GroupMgr->UpdateGroupColor(GroupId, NewBase)  // 更新条目预览
      └─ CloseDialog()
```

### PlacingPlatesForDoor 模式下改颜色
```
GameMode.OnEditorModeChanged(PlacingPlatesForDoor):
  → MainWidget:
      └─ GroupMgr->SetPlacementMode(true, CurrentGroupId)
          ├─ 当前分组: ColorEdit 可用, Row/Delete 禁用
          └─ 其他分组: 全部禁用

用户点击当前分组的 [🎨]:
  → 正常 ColorPicker 流程
  → 弹窗显示时 Viewport 被遮罩阻挡
  → 关闭 ColorPicker 后回到 PlacingPlatesForDoor 模式继续放置

GameMode.OnEditorModeChanged(Normal):
  → MainWidget:
      └─ GroupMgr->SetPlacementMode(false)
          └─ 所有条目恢复正常交互
```

---

## 验收标准

### GroupEntryWidget
1. Setup 后正确显示名称和颜色预览
2. SetSelected 切换 ▶ 标记和背景色
3. 三个按钮各广播正确的 delegate + GroupId
4. 悬浮效果正常

### GroupManagerPanel
1. AddGroupEntry 正确创建条目并添加到列表
2. RemoveGroupEntry 正确移除条目
3. 空列表时显示提示文本
4. RefreshActiveGroup 正确高亮选中条目
5. SetPlacementMode 正确控制各条目的交互状态
6. RebuildFromGameMode 正确重建列表（加载关卡后）

### ColorPickerPopup
1. SV 平面拖拽实时更新颜色
2. Hue 滑条拖拽更新 SV 平面背景 + 颜色
3. HEX 输入框双向同步（HSV ↔ HEX）
4. 旧色/新色对比显示正确
5. 应用后广播正确的 BaseColor + ActiveColor（1.3x 亮度）
6. 取消时不修改任何颜色
