---
name: ue-asset-skill
description: UE 编辑器资产操作 Skill。通过 Python Remote Execution 协议与本机 UE 编辑器通信，支持资产增删改查、蓝图变量/组件操作、属性读写、蓝图编译等。
---

# UE Asset Skill — 编辑器资产操作

## Purpose

本 Skill 提供对 UE 编辑器中资产的全面操作能力，通过 Python Remote Execution 协议与本机运行中的 UE 编辑器通信。支持：

- 资产增删改查（CRUD）
- 蓝图变量操作（添加/重命名/删除）
- 组件操作（添加/删除/列出）
- 属性读写
- 蓝图编译
- 任意 Python 代码执行（高级）

---

## When to Use

### Trigger Conditions

- 用户需要在 UE 编辑器中创建、修改、删除资产
- 用户需要操作蓝图（添加变量、组件、编译等）
- 用户需要查询资产信息（列出资产、检查存在性、查找引用）
- 用户需要读写资产属性
- 用户需要执行自定义 UE Python 代码
- 用户需要分析 UE 资产或关卡（Level）

### Keywords

"创建蓝图", "添加变量", "添加组件", "删除资产", "重命名", "编译蓝图",
"UE 资产", "Blueprint", "Component", "Widget", "DataTable",
"资产操作", "编辑器操作", "UE Python", "分析资产", "分析关卡", "分析场景"

---

## Prerequisites

1. **UE 编辑器已启动**
2. **已启用 Python Remote Execution**：
   - Edit → Project Settings → Plugins → Python → Remote Execution → ✅ Enable Remote Execution
3. **Python 环境**：Python 3.8+（无需额外第三方依赖，仅使用标准库）

---

## Scripts Location

所有脚本位于本 Skill 的 `scripts/` 目录下：

```
ue-asset-skill/
├── SKILL.md                    # 本文件
└── scripts/
    ├── ue_remote.py            # UE 通信层（Remote Execution 协议）
    └── ue_asset_ops.py         # 资产操作命令行工具（所有操作入口）
```

**脚本绝对路径**（根据当前工作区）：
- 通信层：`e:\TMR_AI\AIKnowledge_Client\spec\system\skills\ue-asset-skill\scripts\ue_remote.py`
- 操作入口：`e:\TMR_AI\AIKnowledge_Client\spec\system\skills\ue-asset-skill\scripts\ue_asset_ops.py`

---

## Available Operations

### 连接管理

| 命令 | 功能 | 示例 |
|------|------|------|
| `connect` | 连接到 UE 编辑器 | `python ue_asset_ops.py connect --timeout 6` |
| `status` | 获取编辑器状态 | `python ue_asset_ops.py status` |

### 资产查询

| 命令 | 功能 | 示例 |
|------|------|------|
| `list` | 列出目录下资产 | `python ue_asset_ops.py list --dir "/Game/UI" --filter "WidgetBlueprint"` |
| `search` | 按名称搜索资产（C++ AssetRegistry） | `python ue_asset_ops.py search --name "UWColor"` |
| `info` | 获取资产详情 | `python ue_asset_ops.py info --path "/Game/Blueprints/BP_Test"` |
| `mat-info` | 获取材质详细信息(包含参数和默认值) | `python ue_asset_ops.py mat-info --path "/Game/Test/quanli/farm/f001"` |
| `exists` | 检查资产存在性 | `python ue_asset_ops.py exists --path "/Game/UI/WBP_Main"` |
| `references` | 查找资产引用 | `python ue_asset_ops.py references --path "/Game/UI/WBP_Main"` |

### 资产 CRUD

| 命令 | 功能 | 示例 |
|------|------|------|
| `create-bp` | 创建蓝图 | `python ue_asset_ops.py create-bp --path "/Game/BP/BP_New" --parent "Actor"` |
| `duplicate` | 复制资产 | `python ue_asset_ops.py duplicate --source "/Game/BP/BP_A" --dest "/Game/BP/BP_B"` |
| `delete` | 删除资产 | `python ue_asset_ops.py delete --path "/Game/BP/BP_Old" --force` |
| `rename` | 重命名资产 | `python ue_asset_ops.py rename --source "/Game/BP/BP_Old" --dest "/Game/BP/BP_New"` |

### 蓝图变量操作

| 命令 | 功能 | 示例 |
|------|------|------|
| `add-var` | 添加成员变量 | `python ue_asset_ops.py add-var --path "/Game/BP/BP_Test" --name "Health" --type "float"` |
| `rename-var` | 重命名变量 | `python ue_asset_ops.py rename-var --path "/Game/BP/BP_Test" --old "Health" --new "MaxHealth"` |

### 组件操作

| 命令 | 功能 | 示例 |
|------|------|------|
| `list-comp` | 列出组件 | `python ue_asset_ops.py list-comp --path "/Game/BP/BP_Test"` |
| `add-comp` | 添加组件 | `python ue_asset_ops.py add-comp --path "/Game/BP/BP_Test" --class "PointLightComponent"` |
| `remove-comp` | 删除组件 | `python ue_asset_ops.py remove-comp --path "/Game/BP/BP_Test" --name "PointLight"` |

### UMG 控件操作

| 命令 | 功能 | 示例 |
|------|------|------|
| `list-widgets` | 列出 UMG 控件树 | `python ue_asset_ops.py list-widgets --path "/Game/UI/WBP_Main"` |
| `list-widgets` | 按名称过滤控件 | `python ue_asset_ops.py list-widgets --path "/Game/UI/WBP_Main" --filter "btn"` |
| `list-widgets` | 扁平化输出 | `python ue_asset_ops.py list-widgets --path "/Game/UI/WBP_Main" --flat` |
| `add-widget` | 添加 UMG 控件 | `python ue_asset_ops.py add-widget --path "/Game/UI/WBP_Main" --class "TextBlock" --name "txt_Title" --parent "CanvasPanel_0"` |
| `remove-widget` | 删除 UMG 控件 | `python ue_asset_ops.py remove-widget --path "/Game/UI/WBP_Main" --name "txt_Title"` |
| `set-widget-prop` | 设置控件属性 | `python ue_asset_ops.py set-widget-prop --path "/Game/UI/WBP_Main" --name "txt_Title" --prop "text" --value "标题"` |
| `get-widget-prop` | 读取控件属性 | `python ue_asset_ops.py get-widget-prop --path "/Game/UI/WBP_Main" --name "txt_Title" --prop "text"` |
| `reparent-widget` | 移动控件(换父) | `python ue_asset_ops.py reparent-widget --path "/Game/UI/WBP_Main" --name "txt_Title" --parent "Overlay_0"` |
| `widget-classes` | 可用控件类列表 | `python ue_asset_ops.py widget-classes --keyword "text"` |

### DataTable 操作

| 命令 | 功能 | 示例 |
|------|------|------|
| `dt-info` | 获取 DataTable 结构信息 | `python ue_asset_ops.py dt-info --path "/Game/Data/DT_Config"` |
| `dt-list-rows` | 列出所有行名 | `python ue_asset_ops.py dt-list-rows --path "/Game/Data/DT_Config" --offset 0 --limit 50` |
| `dt-get-row` | 获取单行数据 | `python ue_asset_ops.py dt-get-row --path "/Game/Data/DT_Config" --row "Row1"` |
| `dt-add-row` | 添加一行 | `python ue_asset_ops.py dt-add-row --path "/Game/Data/DT_Config" --row "NewRow" --values '{"Color":"(R=1,G=0,B=0,A=1)"}'` |
| `dt-update-row` | 修改一行 | `python ue_asset_ops.py dt-update-row --path "/Game/Data/DT_Config" --row "Row1" --values '{"Color":"(R=0,G=1,B=0,A=1)"}'` |
| `dt-remove-row` | 删除一行 | `python ue_asset_ops.py dt-remove-row --path "/Game/Data/DT_Config" --row "Row1"` |

### 场景操作（当前关卡）

| 命令 | 功能 | 示例 |
|------|------|------|
| `open-level` | 打开指定关卡地图 | `python ue_asset_ops.py open-level --path "/Game/Maps/MyMap"` |
| `spawn-actor` | 在当前关卡生成 Actor | `python ue_asset_ops.py spawn-actor --class_path "/Game/Blueprints/BP_Test.BP_Test"` |
| `delete-actor` | 从当前关卡删除 Actor | `python ue_asset_ops.py delete-actor --name "BP_Test_1"` |
| `list-actors` | 列出当前关卡中的 Actor | `python ue_asset_ops.py list-actors --filter "BP_Test"` |
| `rename-actor` | 重命名当前关卡中的 Actor | `python ue_asset_ops.py rename-actor --old "BP_Test_1" --new "BP_Test_2"` |
| `set-actor-transform` | 设置 Actor 变换 | `python ue_asset_ops.py set-actor-transform --name "BP_Test_1" --location '{"x":100,"y":0,"z":0}' --rotation '{"pitch":0,"yaw":90,"roll":0}'` |
| `set-actor-prop` | 设置 Actor 属性 | `python ue_asset_ops.py set-actor-prop --name "BP_Test_1" --prop "LightComponent.Intensity" --value "5000"` |
| `save-level` | 保存当前关卡 | `python ue_asset_ops.py save-level` |

> **注意**：所有修改场景的操作（如 `spawn-actor`、`delete-actor`、`rename-actor`、`set-actor-transform`、`set-actor-prop`）都会自动保存当前关卡，无需额外调用 `save-level` 命令。
>
> **重要**：关卡中的 Actor 操作（如 `list-actors`、`set-actor-transform` 等）只能操作**当前编辑器中打开的关卡**。如果需要操作其他关卡中的 Actor，请先使用 `open-level` 命令打开目标关卡。

### 属性操作

| 命令 | 功能 | 示例 |
|------|------|------|
| `get-prop` | 读取属性 | `python ue_asset_ops.py get-prop --path "/Game/BP/BP_Test" --prop "RootComponent.RelativeLocation"` |
| `set-prop` | 设置属性 | `python ue_asset_ops.py set-prop --path "/Game/BP/BP_Test" --prop "bHidden" --value "true"` |
| `set-mat-param` | 设置材质参数 | `python ue_asset_ops.py set-mat-param --path "/Game/Mat/M_Test" --param "Color" --type "vector" --value '{"R":1,"G":0,"B":0,"A":1}'` |

### 编译与保存

| 命令 | 功能 | 示例 |
|------|------|------|
| `compile` | 编译蓝图 | `python ue_asset_ops.py compile --path "/Game/BP/BP_Test"` |
| `save` | 保存资产 | `python ue_asset_ops.py save --path "/Game/BP/BP_Test"` |
| `save-all` | 保存所有脏资产 | `python ue_asset_ops.py save-all` |

### 高级操作

| 命令 | 功能 | 示例 |
|------|------|------|
| `exec` | 执行任意 Python 代码 | `python ue_asset_ops.py exec --code "import unreal; print(unreal.SystemLibrary.get_engine_version())"` |
| `classes` | 获取可用类列表 | `python ue_asset_ops.py classes --base "ActorComponent" --keyword "Light"` |

---

## Algorithm

### Step 0: 查找资产路径（如果不知道完整路径）

**重要**：当用户提供的是资产名称而非完整路径时，**必须先使用 `search` 命令查找**：

```bash
python ue_asset_ops.py search --name "资产名称" --path "/Game/LetsGo/Data"
```

### Step 1: 确定脚本路径

脚本位于 `e:\TMR_AI\AIKnowledge_Client\spec\system\skills\ue-asset-skill\scripts\` 目录下。

### Step 2: 执行操作

使用 `terminal` 工具执行命令：

```bash
python e:\TMR_AI\AIKnowledge_Client\spec\system\skills\ue-asset-skill\scripts\ue_asset_ops.py <command> [options]
```

### Step 3: 解析输出

所有命令输出 JSON 格式结果，包含 `success` 字段：
- `{"success": true, ...}` — 操作成功
- `{"success": false, "error": "..."}` — 操作失败

### Step 4: 蓝图修改后自动编译

**重要**：任何修改蓝图的操作（添加变量、添加组件、设置属性等）脚本内部会自动编译蓝图，无需额外调用 `compile` 命令。

---

## Detailed Command Reference

### connect — 连接编辑器

```bash
python ue_asset_ops.py connect [--timeout SECONDS]
```

**参数**：
- `--timeout`：扫描超时（秒），默认 6

**输出示例**：
```json
{"success": true, "connection": {"connected": true, "machine": "DESKTOP-XXX", "engine_version": "4.26.2"}}
```

### search — 按名称搜索资产（推荐）

```bash
python ue_asset_ops.py search --name NAME [--path PATH] [--filter CLASS] [--limit N] [--verify]
```

**参数**：
- `--name`：资产名称关键词（模糊匹配，不区分大小写）
- `--path`：限定搜索路径（如 `/Game/LetsGo/Data`），指定后搜索更快
- `--filter`：按资产类型过滤（如 `DataTable`、`Blueprint`、`WidgetBlueprint`）
- `--limit`：最大返回数量，默认 20
- `--verify`：通过 UE 编辑器验证资产类型（较慢但更准确）

**输出示例**：
```json
{"success": true, "results": [{"name": "UWColor", "path": "/Game/LetsGo/Data/AssetData/UWColor"}], "count": 1, "search_name": "UWColor", "method": "ue_editor_search"}
```

**性能**：
- **C++ 插件模式**（推荐）：利用 AssetRegistry 内存索引，毫秒级完成，可搜索所有目录（包括 `/Game/Feature`）。需要 BlueprintTool 插件已编译加载。
- **文件系统回退模式**：当 C++ 插件不可用时自动回退，在已知目录中搜索通常 < 0.2 秒。`/Game/Feature` 等超大目录可能超时。

### create-bp — 创建蓝图

```bash
python ue_asset_ops.py create-bp --path PATH [--parent CLASS] [--type TYPE]
```

**参数**：
- `--path`：蓝图路径（如 `/Game/Blueprints/BP_MyActor`）
- `--parent`：父类名称，默认 `Actor`（可选：`Pawn`、`Character`、`UserWidget`）
- `--type`：蓝图类型，默认 `Blueprint`（可选：`WidgetBlueprint`）

### add-var — 添加成员变量

```bash
python ue_asset_ops.py add-var --path PATH --name NAME [--type TYPE] [--default VALUE]
```

**参数**：
- `--path`：蓝图资产路径
- `--name`：变量名称
- `--type`：变量类型，默认 `bool`。支持：
  - `bool` — 布尔值
  - `int` / `int32` — 32位整数
  - `int64` — 64位整数
  - `float` / `real` — 浮点数
  - `string` / `str` — 字符串
  - `name` — FName
  - `text` — FText
  - `vector` — FVector
  - `rotator` — FRotator
  - `transform` — FTransform
- `--default`：默认值（JSON 格式字符串）

### rename-var — 重命名变量

```bash
python ue_asset_ops.py rename-var --path PATH --old OLD_NAME --new NEW_NAME
```

使用引擎内部重命名机制，保持变量 GUID 不变，自动更新所有引用节点。

### add-comp — 添加组件

```bash
python ue_asset_ops.py add-comp --path PATH --class CLASS [--name NAME] [--parent PARENT]
```

**参数**：
- `--path`：蓝图资产路径
- `--class`：组件类名（如 `StaticMeshComponent`、`PointLightComponent`）
- `--name`：组件名称（可选，留空自动生成）
- `--parent`：父组件名称（可选，留空挂载到根组件）

### list-widgets — 列出 UMG 控件树

```bash
python ue_asset_ops.py list-widgets --path PATH [--filter KEYWORD] [--flat]
```

**参数**：
- `--path`：WidgetBlueprint 资产路径
- `--filter`：按名称/类型/文本内容过滤（不区分大小写）
- `--flat`：扁平化输出（默认无过滤时返回树形结构）

**输出示例**（扁平化 + 过滤 TextBlock）：
```json
{
  "success": true,
  "asset": "/Game/UI/WBP_Main",
  "widgets": [
    {"name": "w_txt_Title", "class": "LGTextBlock", "depth": 3, "is_variable": true, "parent": "CanvasPanel_0", "text": "标题"},
    {"name": "LGTextBlock_0", "class": "LGTextBlock", "depth": 5, "is_variable": false, "parent": "Overlay", "text": "开始游戏"}
  ],
  "count": 2
}
```

**输出示例**（树形结构，无过滤）：
```json
{
  "success": true,
  "asset": "/Game/UI/WBP_Main",
  "tree": {
    "Name": "CanvasPanel",
    "Class": "CanvasPanel",
    "IsVariable": false,
    "Children": [
      {"Name": "w_btn_Start", "Class": "Button", "IsVariable": true, "Children": [...]},
      {"Name": "LGTextBlock_0", "Class": "LGTextBlock", "IsVariable": false, "Text": "开始游戏", "Children": []}
    ]
  }
}
```

**说明**：
- 通过 C++ 插件 `BlueprintToolLibrary.list_widgets` 获取完整控件树（毫秒级）
- 自动识别 TextBlock 类型控件并通过 `get_widget_property` 读取其文本内容
- 支持按名称、类型、文本内容进行模糊过滤
- 无过滤时默认返回树形结构；有过滤或 `--flat` 时返回扁平化列表

### add-widget — 添加 UMG 控件

```bash
python ue_asset_ops.py add-widget --path PATH --class CLASS --name NAME [--parent PARENT] [--slot SLOT]
```

**参数**：
- `--path`：WidgetBlueprint 资产路径
- `--class`：控件类名（如 `TextBlock`、`Button`、`Image`、`CanvasPanel`、`Overlay`）
- `--name`：控件名称
- `--parent`：父控件名称（留空则添加到根控件下）
- `--slot`：Slot 类型（如 `CanvasPanelSlot`，通常留空自动推断）

**示例**：
```bash
# 在 CanvasPanel_0 下添加一个 TextBlock
python ue_asset_ops.py add-widget --path "/Game/UI/WBP_Main" --class "TextBlock" --name "txt_Hello" --parent "CanvasPanel_0"

# 添加一个 Button
python ue_asset_ops.py add-widget --path "/Game/UI/WBP_Main" --class "Button" --name "btn_Submit" --parent "CanvasPanel_0"
```

### remove-widget — 删除 UMG 控件

```bash
python ue_asset_ops.py remove-widget --path PATH --name NAME
```

**参数**：
- `--path`：WidgetBlueprint 资产路径
- `--name`：要删除的控件名称

### set-widget-prop — 设置 UMG 控件属性

```bash
python ue_asset_ops.py set-widget-prop --path PATH --name NAME --prop PROPERTY --value VALUE
```

**参数**：
- `--path`：WidgetBlueprint 资产路径
- `--name`：控件名称
- `--prop`：属性名称（如 `text`、`visibility`、`color_and_opacity`、`is_enabled`）
- `--value`：属性值（字符串格式）

**常用属性**：
- `text` — TextBlock 的文本内容
- `visibility` — 可见性（`Visible`、`Hidden`、`Collapsed`）
- `is_enabled` — 是否启用
- `tool_tip_text` — 提示文本

**示例**：
```bash
# 修改文本
python ue_asset_ops.py set-widget-prop --path "/Game/UI/WBP_Main" --name "txt_Title" --prop "text" --value "新标题"

# 设置可见性
python ue_asset_ops.py set-widget-prop --path "/Game/UI/WBP_Main" --name "btn_Submit" --prop "visibility" --value "Collapsed"
```

### get-widget-prop — 读取 UMG 控件属性

```bash
python ue_asset_ops.py get-widget-prop --path PATH --name NAME --prop PROPERTY
```

**参数**：
- `--path`：WidgetBlueprint 资产路径
- `--name`：控件名称
- `--prop`：属性名称

**输出示例**：
```json
{
  "success": true,
  "path": "/Game/UI/WBP_Main",
  "widget": "txt_Title",
  "property": "text",
  "value": "INVTEXT(\"标题\")",
  "display_value": "标题"
}
```

### reparent-widget — 移动 UMG 控件

```bash
python ue_asset_ops.py reparent-widget --path PATH --name NAME --parent NEW_PARENT
```

**参数**：
- `--path`：WidgetBlueprint 资产路径
- `--name`：要移动的控件名称
- `--parent`：新的父控件名称

### widget-classes — 获取可用 UMG 控件类列表

```bash
python ue_asset_ops.py widget-classes [--keyword KEYWORD]
```

**参数**：
- `--keyword`：关键词过滤（不区分大小写）

**输出示例**：
```json
{
  "classes": ["Button", "CanvasPanel", "Image", "Overlay", "TextBlock", "..."],
  "count": 42
}
```

### dt-info — 获取 DataTable 结构信息

```bash
python ue_asset_ops.py dt-info --path PATH
```

**参数**：
- `--path`：DataTable 资产路径

**输出示例**：
```json
{
  "success": true,
  "path": "/Game/LetsGo/Data/AssetData/UWColor2",
  "row_struct": "UserWidgetColor",
  "row_count": 66,
  "columns": ["Color"],
  "row_names_preview": ["Red", "Green", "Blue", "Gray", "Yellow"]
}
```

**说明**：
- `row_struct`：行结构体名称
- `columns`：通过探测发现的列名列表（自动尝试常见列名）
- `row_names_preview`：前 20 个行名预览

### dt-list-rows — 列出 DataTable 行名

```bash
python ue_asset_ops.py dt-list-rows --path PATH [--offset N] [--limit N]
```

**参数**：
- `--path`：DataTable 资产路径
- `--offset`：起始偏移，默认 0
- `--limit`：返回数量，默认 50

### dt-get-row — 获取单行数据

```bash
python ue_asset_ops.py dt-get-row --path PATH --row ROW_NAME
```

**参数**：
- `--path`：DataTable 资产路径
- `--row`：行名

**输出示例**：
```json
{
  "success": true,
  "path": "/Game/LetsGo/Data/AssetData/UWColor2",
  "row_name": "Red",
  "data": {
    "Name": "Red",
    "Color": "(R=0.783538,G=0.147027,B=0.135633,A=1.000000)"
  }
}
```

### dt-add-row — 添加一行

```bash
python ue_asset_ops.py dt-add-row --path PATH --row ROW_NAME --values JSON
```

**参数**：
- `--path`：DataTable 资产路径
- `--row`：新行名称
- `--values`：列值 JSON 对象（如 `{"Color":"(R=0.1,G=0.2,B=0.3,A=1.0)"}`)

**安全机制**：
- 使用 C++ 原生接口 `BlueprintToolLibrary.add_data_table_row` 直接添加行
- 不会影响现有数据，操作原子性
- 如果行已存在或数据格式不匹配会报错

### dt-update-row — 修改一行

```bash
python ue_asset_ops.py dt-update-row --path PATH --row ROW_NAME --values JSON
```

**参数**：
- `--path`：DataTable 资产路径
- `--row`：要修改的行名
- `--values`：要更新的列值 JSON 对象（只需包含要修改的列）

**安全机制**：
- 使用 C++ 原生接口 `BlueprintToolLibrary.modify_data_table_row` 直接修改行
- 只修改指定行，不影响其他数据

### dt-remove-row — 删除一行

```bash
python ue_asset_ops.py dt-remove-row --path PATH --row ROW_NAME
```

**参数**：
- `--path`：DataTable 资产路径
- `--row`：要删除的行名

**安全机制**：
- 使用 C++ 原生接口 `BlueprintToolLibrary.remove_data_table_row` 直接删除行
- 只删除指定行，不影响其他数据

### open-level — 打开指定关卡地图

```bash
python ue_asset_ops.py open-level --path MAP_PATH
```

**参数**：
- `--path`：关卡资产路径（如 `/Game/Maps/MyMap`）

**输出示例**：
```json
{"success": true, "map_path": "/Game/Maps/MyMap", "level_name": "MyMap", "previous_level": "OldMap", "message": "关卡打开成功: MyMap"}
```

**说明**：
- 打开关卡前会验证资产是否存在，防止加载不存在的地图
- 打开新关卡会替换当前关卡（未保存的修改会丢失）
- 超时时间为 30 秒（大型关卡加载可能较慢）

### spawn-actor — 在当前关卡生成 Actor

```bash
python ue_asset_ops.py spawn-actor --class_path CLASS_PATH [--location JSON] [--rotation JSON]
```

**参数**：
- `--class_path`：Actor 类路径（如 `/Game/Blueprints/BP_Test.BP_Test`）
- `--location`：位置 JSON（如 `{"x":0, "y":0, "z":0}`）
- `--rotation`：旋转 JSON（如 `{"pitch":0, "yaw":0, "roll":0}`）

**说明**：生成成功后会自动保存当前关卡。

### delete-actor — 从当前关卡删除 Actor

```bash
python ue_asset_ops.py delete-actor --name ACTOR_NAME
```

**参数**：
- `--name`：Actor 在大纲中的名称（Label）

**说明**：删除成功后会自动保存当前关卡。

### list-actors — 列出当前关卡中的 Actor

```bash
python ue_asset_ops.py list-actors [--filter CLASS_NAME]
```

**参数**：
- `--filter`：按类名过滤（不区分大小写）

### rename-actor — 重命名当前关卡中的 Actor

```bash
python ue_asset_ops.py rename-actor --old OLD_NAME --new NEW_NAME
```

**参数**：
- `--old`：Actor 在大纲中的旧名称（Label）
- `--new`：Actor 在大纲中的新名称（Label）

**说明**：重命名成功后会自动保存当前关卡。

### set-actor-transform — 设置当前关卡中 Actor 的变换

```bash
python ue_asset_ops.py set-actor-transform --name ACTOR_NAME [--location JSON] [--rotation JSON] [--scale JSON]
```

**参数**：
- `--name`：Actor 在大纲中的名称（Label）
- `--location`：位置 JSON（如 `{"x":0, "y":0, "z":0}`）
- `--rotation`：旋转 JSON（如 `{"pitch":0, "yaw":0, "roll":0}`）
- `--scale`：缩放 JSON（如 `{"x":1, "y":1, "z":1}`）

**说明**：设置成功后会自动保存当前关卡。

### set-actor-prop — 设置当前关卡中 Actor 的属性

```bash
python ue_asset_ops.py set-actor-prop --name ACTOR_NAME --prop PROPERTY --value JSON
```

**参数**：
- `--name`：Actor 在大纲中的名称（Label）
- `--prop`：属性名称（支持点分隔，如 `LightComponent.Intensity`）
- `--value`：属性值（JSON 格式）

**说明**：设置成功后会自动保存当前关卡。支持的特殊类型与 `set-prop` 相同（Vector, Rotator, LinearColor, Enum）。

### save-level — 保存当前关卡

```bash
python ue_asset_ops.py save-level
```

**说明**：显式保存当前打开的关卡。

### set-prop — 设置属性

```bash
python ue_asset_ops.py set-prop --path PATH --prop PROPERTY --value JSON
```

**参数**：
- `--path`：资产路径
- `--prop`：属性名称
- `--value`：属性值（JSON 格式）

**支持的特殊类型**：
- **Vector**: `{"x": 1.0, "y": 2.0, "z": 3.0}`
- **Rotator**: `{"pitch": 90.0, "yaw": 0.0, "roll": 0.0}`
- **LinearColor**: `{"r": 1.0, "g": 0.0, "b": 0.0, "a": 1.0}`
- **Enum**: `{"enum": "BlendMode.BLEND_TRANSLUCENT"}`

### set-mat-param — 设置材质/材质实例参数

```bash
python ue_asset_ops.py set-mat-param --path PATH --param PARAM_NAME --type TYPE --value VALUE
```

**参数**：
- `--path`：材质或材质实例资产路径
- `--param`：参数名称
- `--type`：参数类型（`scalar`、`vector`、`texture`）
- `--value`：参数值
  - `scalar`：传数字（如 `1.5`）
  - `vector`：传 JSON 字符串（如 `'{"R":1,"G":0,"B":0,"A":1}'`）
  - `texture`：传纹理资产路径（如 `"/Game/Textures/T_MyTex"`）

**示例**：
```bash
# 修改材质实例的向量参数
python ue_asset_ops.py set-mat-param --path "/Game/Mat/MI_Test" --param "BaseColor" --type "vector" --value '{"R":1,"G":0,"B":0,"A":1}'

# 修改主材质的标量参数默认值
python ue_asset_ops.py set-mat-param --path "/Game/Mat/M_Test" --param "Roughness" --type "scalar" --value "0.5"
```

### exec — 执行任意代码

```bash
python ue_asset_ops.py exec --code "PYTHON_CODE" [--timeout SECONDS]
```

代码中使用 `import unreal` 访问 UE Python API。使用 `print()` 输出结果。

如需返回结构化数据：
```python
print(f"@@RESULT@@:{json.dumps(your_data)}")
```

---

## Common Workflows

### 创建一个带变量和组件的 Actor 蓝图

```bash
# 1. 创建蓝图
python ue_asset_ops.py create-bp --path "/Game/Blueprints/BP_MyActor" --parent "Actor"

# 2. 添加变量
python ue_asset_ops.py add-var --path "/Game/Blueprints/BP_MyActor" --name "Health" --type "float"
python ue_asset_ops.py add-var --path "/Game/Blueprints/BP_MyActor" --name "IsAlive" --type "bool"

# 3. 添加组件
python ue_asset_ops.py add-comp --path "/Game/Blueprints/BP_MyActor" --class "StaticMeshComponent" --name "Mesh"
python ue_asset_ops.py add-comp --path "/Game/Blueprints/BP_MyActor" --class "BoxComponent" --name "Collision"
```

### 查询并修改资产

```bash
# 查看资产信息
python ue_asset_ops.py info --path "/Game/UI/WBP_MainMenu"

# 列出组件
python ue_asset_ops.py list-comp --path "/Game/Blueprints/BP_Player"

# 修改属性
python ue_asset_ops.py set-prop --path "/Game/Blueprints/BP_Player" --prop "bHidden" --value "true"
```

### DataTable 增删改查

```bash
# 1. 搜索 DataTable 资产
python ue_asset_ops.py search --name "UWColor" --filter "DataTable"

# 2. 查看结构信息
python ue_asset_ops.py dt-info --path "/Game/LetsGo/Data/AssetData/UWColor2"

# 3. 获取某行数据
python ue_asset_ops.py dt-get-row --path "/Game/LetsGo/Data/AssetData/UWColor2" --row "Red"

# 4. 添加新行
python ue_asset_ops.py dt-add-row --path "/Game/LetsGo/Data/AssetData/UWColor2" --row "MyColor" --values "{\"Color\":\"(R=0.5,G=0.5,B=0.5,A=1.0)\"}"

# 5. 修改行
python ue_asset_ops.py dt-update-row --path "/Game/LetsGo/Data/AssetData/UWColor2" --row "MyColor" --values "{\"Color\":\"(R=1.0,G=0.0,B=0.0,A=1.0)\"}"

# 6. 删除行
python ue_asset_ops.py dt-remove-row --path "/Game/LetsGo/Data/AssetData/UWColor2" --row "MyColor"
```

---

## Error Handling

| 错误信息 | 原因 | 解决方案 |
|----------|------|----------|
| `无法连接到 UE 编辑器` | 编辑器未启动或未启用 Remote Execution | 启动编辑器并启用 Python Remote Execution |
| `资产不存在` | 路径错误 | 使用 `list` 命令确认正确路径 |
| `资产已存在` | 创建时目标路径已有资产 | 使用不同路径或先删除 |
| `变量不存在` | 重命名时旧名称不匹配 | 使用 `exec` 命令列出当前变量 |
| `UE 连接已断开` | 编辑器关闭或网络问题 | 重新执行命令（会自动重连） |

---

## Communication Protocol

本 Skill 使用 UE Python Remote Execution 协议：

1. **UDP 组播扫描**（`239.0.0.1:6766`）：发送 ping 发现本机 UE 编辑器
2. **TCP 反连**：UE 编辑器主动连接到脚本监听的 TCP 端口
3. **命令执行**：通过 TCP 发送 Python 代码，UE 执行后返回结果
4. **连接复用**：脚本使用 `--use-pool` 模式时可复用连接（默认每次独立连接）

---

## Notes

- 所有修改蓝图的操作会自动触发编译（`kismet recompile`）
- 变量类型支持基本类型和常用结构体（Vector、Rotator、Transform）
- `exec` 命令是万能后备，任何其他命令无法覆盖的操作都可以通过它实现
- 脚本仅使用 Python 标准库，无需安装额外依赖
