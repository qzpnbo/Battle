#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""UE 资产操作命令行工具.

Usage:
    python ue_asset_ops.py <command> [options]

"""

import argparse
import json
import os
import sys
from typing import Any, Dict

# 确保可以导入同目录下的 ue_remote
_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
if _SCRIPT_DIR not in sys.path:
    sys.path.insert(0, _SCRIPT_DIR)

from ue_remote import close_session, get_connection_info, send_command

# ====================================================================
# 辅助函数
# ====================================================================

_RESULT_PREFIX = "@@RESULT@@:"


def _extract_output(result: Dict[str, Any]) -> str:
    """从 UE command_result 中提取 stdout 文本."""
    if "error" in result:
        return f"ERROR: {result['error']}"
    output_lines = result.get("output", [])
    if not output_lines:
        return ""
    texts = []
    for line in output_lines:
        o = line.get("output", "")
        if o:
            texts.append(o)
    return "\n".join(texts)


def _run_ue_code(code: str, read_timeout: float = 30.0) -> Dict[str, Any]:
    """执行 UE Python 代码并返回结构化结果."""
    result = send_command(code, read_timeout=read_timeout)
    if "error" in result:
        return {"success": False, "error": result["error"]}

    output = _extract_output(result)

    # 检查是否有结构化返回
    for line in output.split("\n"):
        if line.startswith(_RESULT_PREFIX):
            json_str = line[len(_RESULT_PREFIX):]
            try:
                data = json.loads(json_str)
                return {"success": True, "data": data}
            except json.JSONDecodeError:
                return {"success": True, "data": json_str}

    # 检查是否有错误输出
    error_lines = []
    for line_data in result.get("output", []):
        if line_data.get("type", "").lower() == "error":
            error_lines.append(line_data.get("output", ""))

    if error_lines:
        return {"success": False, "error": "\n".join(error_lines), "output": output}

    return {"success": True, "data": output}


def _output(data: Any) -> None:
    """输出 JSON 结果到 stdout."""
    if isinstance(data, str):
        print(data)
    else:
        print(json.dumps(data, ensure_ascii=False, indent=2))


# ====================================================================
# 命令实现
# ====================================================================


def cmd_connect(args: argparse.Namespace) -> None:
    """连接到 UE 编辑器."""
    result = send_command(
        "import unreal; print('connected')", timeout=args.timeout
    )
    if "error" in result:
        _output({
            "success": False,
            "error": result["error"],
            "hint": "请确认:1. UE 编辑器已启动 2. 已启用 Python Remote Execution",
        })
    else:
        info = get_connection_info()
        _output({"success": True, "connection": info})


def cmd_status(args: argparse.Namespace) -> None:
    """获取编辑器状态."""
    info = get_connection_info()
    if not info.get("connected"):
        # 尝试连接
        result = send_command("import unreal; print('ok')", timeout=6)
        if "error" in result:
            _output({"connected": False, "message": "未连接到 UE 编辑器"})
            return
        info = get_connection_info()

    code = """
import json

import unreal

info = {
    "engine_version": unreal.SystemLibrary.get_engine_version(),
    "platform": unreal.SystemLibrary.get_platform_user_name(),
}
print(f"@@RESULT@@:{json.dumps(info)}")
"""
    result = _run_ue_code(code)
    if result["success"]:
        _output({"connected": True, "connection": info, "editor_info": result["data"]})
    else:
        _output({"connected": True, "connection": info})


def cmd_list(args: argparse.Namespace) -> None:
    """列出目录下资产."""
    code = f"""
import json

import unreal

eal = unreal.EditorAssetLibrary
directory = "{args.dir}"
recursive = {args.recursive}

assets = eal.list_assets(directory, recursive=recursive, include_folder=False)
result = []
class_filter = "{args.filter}"

for asset_path in assets:
    asset_data = unreal.EditorAssetLibrary.find_asset_data(asset_path)
    asset_class = str(asset_data.asset_class) if asset_data else "Unknown"

    if class_filter and class_filter.lower() not in asset_class.lower():
        continue

    result.append({{
        "path": str(asset_path),
        "class": asset_class,
        "name": str(asset_path).split("/")[-1].split(".")[0],
    }})

print(f"@@RESULT@@:{{json.dumps(result, ensure_ascii=False)}}")
"""
    result = _run_ue_code(code, read_timeout=60.0)
    _output(result["data"] if result["success"] else result)


def cmd_info(args: argparse.Namespace) -> None:
    """获取资产详情."""
    code = f"""
import json

import unreal

asset_path = "{args.path}"
eal = unreal.EditorAssetLibrary

if not eal.does_asset_exist(asset_path):
    print(f"@@RESULT@@:{{json.dumps({{'error': '资产不存在: {args.path}'}}, ensure_ascii=False)}}")
else:
    asset = eal.load_asset(asset_path)
    asset_data = eal.find_asset_data(asset_path)

    info = {{
        "path": asset_path,
        "exists": True,
        "class": str(asset_data.asset_class) if asset_data else "Unknown",
        "package_name": str(asset_data.package_name) if asset_data else "",
        "object_path": str(asset_data.object_path) if asset_data else "",
    }}

    if asset and hasattr(asset, 'generated_class'):
        info["is_blueprint"] = True
        info["parent_class"] = str(asset.parent_class.get_name()) if hasattr(asset, 'parent_class') and asset.parent_class else "Unknown"

    print(f"@@RESULT@@:{{json.dumps(info, ensure_ascii=False)}}")
"""
    result = _run_ue_code(code)
    _output(result["data"] if result["success"] else result)


def cmd_mat_info(args: argparse.Namespace) -> None:
    """获取材质详细信息."""
    code = f"""
import json

import unreal

asset_path = "{args.path}"
eal = unreal.EditorAssetLibrary

if not eal.does_asset_exist(asset_path):
    print(f"@@RESULT@@:{{json.dumps({{'error': '资产不存在: ' + asset_path}}, ensure_ascii=False)}}")
else:
    asset = eal.load_asset(asset_path)
    asset_class = asset.get_class().get_name()

    if asset_class not in ["Material", "MaterialInstanceConstant"]:
        print(f"@@RESULT@@:{{json.dumps({{'error': '资产不是材质或材质实例: ' + asset_class}}, ensure_ascii=False)}}")
    else:
        info = {{
            "name": asset.get_name(),
            "class": asset_class,
            "path": asset_path
        }}

        # 基础属性
        if asset_class == "Material":
            try:
                info['blend_mode'] = str(asset.get_editor_property('blend_mode'))
                info['shading_model'] = str(asset.get_editor_property('shading_model'))
                info['two_sided'] = asset.get_editor_property('two_sided')
            except Exception:
                pass
        elif asset_class == "MaterialInstanceConstant":
            try:
                parent = asset.get_editor_property('parent')
                info['parent'] = parent.get_path_name() if parent else None
            except Exception:
                pass

        # 参数获取
        mel = unreal.MaterialEditingLibrary
        try:
            scalar_names = [str(n) for n in mel.get_scalar_parameter_names(asset)]
            vector_names = [str(n) for n in mel.get_vector_parameter_names(asset)]
            texture_names = [str(n) for n in mel.get_texture_parameter_names(asset)]

            scalars = {{}}
            for n in scalar_names:
                if asset_class == "Material":
                    val = mel.get_material_default_scalar_parameter_value(asset, n)
                else:
                    val = mel.get_material_instance_scalar_parameter_value(asset, n)
                scalars[n] = val

            vectors = {{}}
            for n in vector_names:
                if asset_class == "Material":
                    val = mel.get_material_default_vector_parameter_value(asset, n)
                else:
                    val = mel.get_material_instance_vector_parameter_value(asset, n)
                if val:
                    vectors[n] = {{"R": val.r, "G": val.g, "B": val.b, "A": val.a}}

            textures = {{}}
            for n in texture_names:
                if asset_class == "Material":
                    val = mel.get_material_default_texture_parameter_value(asset, n)
                else:
                    val = mel.get_material_instance_texture_parameter_value(asset, n)
                textures[n] = val.get_path_name() if val else None

            info['parameters'] = {{
                "scalar": scalars,
                "vector": vectors,
                "texture": textures
            }}
            info['success'] = True
            print(f"@@RESULT@@:{{json.dumps(info, ensure_ascii=False)}}")
        except Exception as e:
            print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': str(e)}}, ensure_ascii=False)}}")
"""
    result = _run_ue_code(code, read_timeout=15.0)
    _output(result["data"] if result["success"] else result)


def cmd_set_mat_param(args: argparse.Namespace) -> None:
    """设置材质/材质实例参数."""
    code = f"""
import json

import unreal

asset_path = "{args.path}"
param_name = "{args.param}"
param_type = "{args.type}".lower()
value_str = '''{args.value}'''

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary

if not eal.does_asset_exist(asset_path):
    print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': '资产不存在: ' + asset_path}}, ensure_ascii=False)}}")
else:
    try:
        asset = eal.load_asset(asset_path)
        asset_class = asset.get_class().get_name()
        
        if asset_class not in ["Material", "MaterialInstanceConstant"]:
            print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': '资产不是材质或材质实例: ' + asset_class}}, ensure_ascii=False)}}")
        else:
            # 解析值
            val = None
            if param_type == "scalar":
                val = float(value_str)
            elif param_type == "vector":
                v_dict = json.loads(value_str)
                val = unreal.LinearColor(v_dict.get('R', 0.0), v_dict.get('G', 0.0), v_dict.get('B', 0.0), v_dict.get('A', 1.0))
            elif param_type == "texture":
                val = eal.load_asset(value_str)
                if not val:
                    raise Exception(f"找不到纹理资产: {{value_str}}")
            else:
                raise Exception(f"不支持的参数类型: {{param_type}}")

            if asset_class == "MaterialInstanceConstant":
                if param_type == "scalar":
                    mel.set_material_instance_scalar_parameter_value(asset, param_name, val)
                elif param_type == "vector":
                    mel.set_material_instance_vector_parameter_value(asset, param_name, val)
                elif param_type == "texture":
                    mel.set_material_instance_texture_parameter_value(asset, param_name, val)
                mel.update_material_instance(asset)
            else:
                # Base Material - hacky way to find expression
                asset_name = asset_path.rsplit('/', 1)[-1]
                expr_class = ""
                if param_type == "scalar":
                    expr_class = "MaterialExpressionScalarParameter"
                elif param_type == "vector":
                    expr_class = "MaterialExpressionVectorParameter"
                elif param_type == "texture":
                    expr_class = "MaterialExpressionTextureSampleParameter2D"
                
                found = False
                for i in range(100):
                    obj_name = f"{{asset_path}}.{{asset_name}}:{{expr_class}}_{{i}}"
                    obj = unreal.load_object(None, obj_name)
                    if obj:
                        try:
                            if str(obj.get_editor_property('parameter_name')) == param_name:
                                if param_type == "texture":
                                    obj.set_editor_property('texture', val)
                                else:
                                    obj.set_editor_property('default_value', val)
                                found = True
                                break
                        except:
                            pass
                if not found:
                    raise Exception(f"在主材质中找不到参数节点: {{param_name}}")
                
                mel.recompile_material(asset)
            
            eal.save_loaded_asset(asset)
            print(f"@@RESULT@@:{{json.dumps({{'success': True, 'path': asset_path, 'param': param_name, 'message': '材质参数设置成功'}}, ensure_ascii=False)}}")
    except Exception as e:
        print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': f'设置材质参数失败: {{e}}'}}, ensure_ascii=False)}}")
"""
    result = _run_ue_code(code, read_timeout=30.0)
    _output(result["data"] if result["success"] else result)


def cmd_exists(args: argparse.Namespace) -> None:
    """检查资产存在性."""
    code = f"""
import json

import unreal

exists = unreal.EditorAssetLibrary.does_asset_exist("{args.path}")
print(f"@@RESULT@@:{{json.dumps({{'path': '{args.path}', 'exists': exists}})}}")
"""
    result = _run_ue_code(code)
    _output(result["data"] if result["success"] else result)


def cmd_create_bp(args: argparse.Namespace) -> None:
    """创建蓝图."""
    code = f"""
import json

import unreal

asset_path = "{args.path}"
parent_class_name = "{args.parent}"
blueprint_type = "{args.type}"

eal = unreal.EditorAssetLibrary
if eal.does_asset_exist(asset_path):
    print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': '资产已存在: ' + asset_path}}, ensure_ascii=False)}}")
else:
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    parts = asset_path.rsplit("/", 1)
    package_path = parts[0]
    asset_name = parts[1] if len(parts) > 1 else "NewBlueprint"

    factory = None
    if blueprint_type == "WidgetBlueprint":
        factory = unreal.WidgetBlueprintFactory()
    else:
        factory = unreal.BlueprintFactory()
        parent_cls = getattr(unreal, parent_class_name, None)
        if parent_cls is not None:
            factory.set_editor_property("parent_class", parent_cls)
        else:
            _class_map = {{"Actor": unreal.Actor, "Pawn": unreal.Pawn, "Character": unreal.Character}}
            fallback = _class_map.get(parent_class_name)
            if fallback:
                factory.set_editor_property("parent_class", fallback)

    new_asset = asset_tools.create_asset(asset_name, package_path, None, factory)

    if new_asset:
        try:
            unreal.KismetSystemLibrary.execute_console_command(None, f'kismet recompile {{asset_path}}')
        except Exception:
            pass
        eal.save_asset(asset_path)
        print(f"@@RESULT@@:{{json.dumps({{'success': True, 'path': asset_path, 'name': asset_name, 'message': '蓝图创建并编译成功'}}, ensure_ascii=False)}}")
    else:
        print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': '创建失败,请检查路径和父类是否有效'}}, ensure_ascii=False)}}")
"""
    result = _run_ue_code(code, read_timeout=30.0)
    _output(result["data"] if result["success"] else result)


def cmd_duplicate(args: argparse.Namespace) -> None:
    """复制资产."""
    code = f"""
import json

import unreal

source = "{args.source}"
dest = "{args.dest}"
eal = unreal.EditorAssetLibrary

if not eal.does_asset_exist(source):
    print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': '源资产不存在: ' + source}}, ensure_ascii=False)}}")
elif eal.does_asset_exist(dest):
    print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': '目标路径已存在资产: ' + dest}}, ensure_ascii=False)}}")
else:
    result = eal.duplicate_asset(source, dest)
    if result:
        print(f"@@RESULT@@:{{json.dumps({{'success': True, 'source': source, 'dest': dest, 'message': '复制成功'}}, ensure_ascii=False)}}")
    else:
        print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': '复制失败'}}, ensure_ascii=False)}}")
"""
    result = _run_ue_code(code)
    _output(result["data"] if result["success"] else result)


def cmd_delete(args: argparse.Namespace) -> None:
    """删除资产."""
    code = f"""
import json

import unreal

asset_path = "{args.path}"
force = {args.force}
eal = unreal.EditorAssetLibrary

if not eal.does_asset_exist(asset_path):
    print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': '资产不存在: ' + asset_path}}, ensure_ascii=False)}}")
else:
    if not force:
        referencers = eal.find_package_referencers_for_asset(asset_path)
        if referencers:
            ref_list = [str(r) for r in referencers[:10]]
            print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': '资产被引用,无法删除', 'referencers': ref_list, 'hint': '使用 --force 强制删除'}}, ensure_ascii=False)}}")
        else:
            result = eal.delete_asset(asset_path)
            print(f"@@RESULT@@:{{json.dumps({{'success': result, 'path': asset_path, 'message': '删除成功' if result else '删除失败'}}, ensure_ascii=False)}}")
    else:
        result = eal.delete_asset(asset_path)
        print(f"@@RESULT@@:{{json.dumps({{'success': result, 'path': asset_path, 'message': '强制删除成功' if result else '删除失败'}}, ensure_ascii=False)}}")
"""
    result = _run_ue_code(code)
    _output(result["data"] if result["success"] else result)


def cmd_rename(args: argparse.Namespace) -> None:
    """重命名资产."""
    code = f"""
import json

import unreal

source = "{args.source}"
dest = "{args.dest}"
eal = unreal.EditorAssetLibrary

if not eal.does_asset_exist(source):
    print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': '源资产不存在: ' + source}}, ensure_ascii=False)}}")
elif eal.does_asset_exist(dest):
    print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': '目标路径已存在: ' + dest}}, ensure_ascii=False)}}")
else:
    result = eal.rename_asset(source, dest)
    if result:
        print(f"@@RESULT@@:{{json.dumps({{'success': True, 'old_path': source, 'new_path': dest, 'message': '重命名成功'}}, ensure_ascii=False)}}")
    else:
        print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': '重命名失败'}}, ensure_ascii=False)}}")
"""
    result = _run_ue_code(code)
    _output(result["data"] if result["success"] else result)


def cmd_add_var(args: argparse.Namespace) -> None:
    """添加成员变量."""
    # 类型映射
    type_mapping = {
        "bool": "bool", "int": "int", "int32": "int", "int64": "int64",
        "float": "real", "real": "real", "double": "real",
        "string": "string", "str": "string", "name": "name", "text": "text",
        "vector": "struct", "rotator": "struct", "transform": "struct",
        "object": "object",
    }
    struct_mapping = {
        "vector": "/Script/CoreUObject.Vector",
        "rotator": "/Script/CoreUObject.Rotator",
        "transform": "/Script/CoreUObject.Transform",
    }

    pin_category = type_mapping.get(args.type.lower(), args.type)
    struct_path = struct_mapping.get(args.type.lower(), "")

    code = f"""
import json

import unreal

asset_path = "{args.path}"
variable_name = "{args.name}"
pin_category = "{pin_category}"
struct_path = "{struct_path}"

eal = unreal.EditorAssetLibrary

if not eal.does_asset_exist(asset_path):
    print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': '蓝图不存在: ' + asset_path}}, ensure_ascii=False)}}")
else:
    try:
        bp_asset = eal.load_asset(asset_path)
        if bp_asset is None:
            print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': '无法加载蓝图: ' + asset_path}}, ensure_ascii=False)}}")
        else:
            added = False

            # 方案1: 使用 BlueprintToolLibrary
            try:
                result_ok = unreal.BlueprintToolLibrary.add_blueprint_variable(asset_path, variable_name)
                if result_ok:
                    added = True
            except Exception:
                pass

            # 方案2: 使用 PEBlueprintAsset
            if not added:
                try:
                    pe_bp = unreal.PEBlueprintAsset()
                    pe_bp.set_editor_property("blueprint", bp_asset)
                    pe_bp.set_editor_property("need_save", True)
                    pin_type = unreal.PEGraphPinType()
                    pin_type.pin_category = pin_category
                    if struct_path:
                        struct_obj = unreal.load_object(None, struct_path)
                        if struct_obj:
                            pin_type.pin_sub_category_object = struct_obj
                    pin_value_type = unreal.PEGraphTerminalType()
                    pe_bp.add_member_variable(variable_name, pin_type, pin_value_type, 0, 0, 0)
                    added = True
                except Exception as pe_err:
                    print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': f'添加变量失败: {{pe_err}}'}}, ensure_ascii=False)}}")

            if added:
                try:
                    unreal.KismetSystemLibrary.execute_console_command(None, f'kismet recompile {{asset_path}}')
                except Exception:
                    pass
                eal.save_asset(asset_path)
                print(f"@@RESULT@@:{{json.dumps({{'success': True, 'path': asset_path, 'variable': variable_name, 'type': pin_category, 'message': '变量添加并编译成功'}}, ensure_ascii=False)}}")
    except Exception as e:
        print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': f'添加变量失败: {{e}}'}}, ensure_ascii=False)}}")
"""
    result = _run_ue_code(code, read_timeout=30.0)
    _output(result["data"] if result["success"] else result)


def cmd_rename_var(args: argparse.Namespace) -> None:
    """重命名变量."""
    code = f"""
import json

import unreal

asset_path = "{args.path}"
old_name = "{args.old}"
new_name = "{args.new}"

eal = unreal.EditorAssetLibrary

if not eal.does_asset_exist(asset_path):
    print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': '蓝图不存在: ' + asset_path}}, ensure_ascii=False)}}")
else:
    bp_asset = eal.load_asset(asset_path)
    if bp_asset is None:
        print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': '无法加载蓝图: ' + asset_path}}, ensure_ascii=False)}}")
    else:
        try:
            vars_list = unreal.BlueprintToolLibrary.list_blueprint_variables(asset_path)
            if old_name not in list(vars_list):
                print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': f'变量不存在: {{old_name}},当前变量列表: {{list(vars_list)}}'}}, ensure_ascii=False)}}")
            elif new_name in list(vars_list):
                print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': f'目标变量名已存在: {{new_name}}'}}, ensure_ascii=False)}}")
            else:
                renamed = unreal.BlueprintToolLibrary.rename_blueprint_variable(asset_path, old_name, new_name)

                if renamed:
                    eal.save_asset(asset_path)
                    new_vars = unreal.BlueprintToolLibrary.list_blueprint_variables(asset_path)
                    print(f"@@RESULT@@:{{json.dumps({{'success': True, 'old_name': old_name, 'new_name': new_name, 'current_variables': list(new_vars), 'message': f'变量重命名成功: {{old_name}} → {{new_name}}'}}, ensure_ascii=False)}}")
                else:
                    print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': f'重命名失败,变量 {{old_name}} 可能不存在或 {{new_name}} 已被占用'}}, ensure_ascii=False)}}")
        except Exception as e:
            print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': f'重命名变量失败: {{e}}'}}, ensure_ascii=False)}}")
"""
    result = _run_ue_code(code, read_timeout=30.0)
    _output(result["data"] if result["success"] else result)


def cmd_list_comp(args: argparse.Namespace) -> None:
    """列出组件."""
    code = f"""
import json

import unreal

asset_path = "{args.path}"
eal = unreal.EditorAssetLibrary

asset = eal.load_asset(asset_path)
if asset is None:
    print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': '无法加载资产: ' + asset_path}}, ensure_ascii=False)}}")
else:
    try:
        scs = asset.get_editor_property("simple_construction_script") if hasattr(asset, 'get_editor_property') else None
        components = []
        if scs:
            all_nodes = scs.get_all_nodes()
            for node in all_nodes:
                comp_template = node.get_editor_property("component_template")
                if comp_template:
                    parent_node = node.get_editor_property("parent_component_or_variable_name")
                    components.append({{
                        "name": str(node.get_editor_property("internal_variable_name")),
                        "class": str(comp_template.get_class().get_name()),
                        "parent": str(parent_node) if parent_node else None,
                    }})
        print(f"@@RESULT@@:{{json.dumps({{'success': True, 'asset': asset_path, 'components': components}}, ensure_ascii=False)}}")
    except Exception as e:
        print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': f'获取组件列表失败: {{e}}'}}, ensure_ascii=False)}}")
"""
    result = _run_ue_code(code)
    _output(result["data"] if result["success"] else result)


def cmd_add_comp(args: argparse.Namespace) -> None:
    """添加组件."""
    comp_name = args.name or ""
    parent_name = args.parent or ""
    code = f"""
import json

import unreal

asset_path = "{args.path}"
component_class_name = "{getattr(args, 'class')}"
component_name = "{comp_name}"
parent_name = "{parent_name}"

eal = unreal.EditorAssetLibrary
asset = eal.load_asset(asset_path)

if asset is None:
    print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': '无法加载蓝图: ' + asset_path}}, ensure_ascii=False)}}")
else:
    try:
        scs = asset.get_editor_property("simple_construction_script")
        if scs is None:
            print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': '该资产不是蓝图或不支持组件'}}, ensure_ascii=False)}}")
        else:
            comp_class = unreal.EditorAssetLibrary.load_blueprint_class("/Script/Engine." + component_class_name)
            if comp_class is None:
                for module in ["CoreUObject", "UMG", "AIModule"]:
                    comp_class = unreal.EditorAssetLibrary.load_blueprint_class(f"/Script/{{module}}.{{component_class_name}}")
                    if comp_class:
                        break

            if comp_class is None:
                print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': f'找不到组件类: {{component_class_name}}'}}, ensure_ascii=False)}}")
            else:
                new_node = scs.create_node(comp_class, component_name if component_name else component_class_name)
                if new_node:
                    scs.add_node(new_node)
                    try:
                        unreal.KismetSystemLibrary.execute_console_command(None, f'kismet recompile {{asset_path}}')
                    except Exception:
                        pass
                    eal.save_loaded_asset(asset)
                    actual_name = str(new_node.get_editor_property("internal_variable_name"))
                    print(f"@@RESULT@@:{{json.dumps({{'success': True, 'component_name': actual_name, 'class': component_class_name, 'message': '组件添加并编译成功'}}, ensure_ascii=False)}}")
                else:
                    print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': '创建组件节点失败'}}, ensure_ascii=False)}}")
    except Exception as e:
        print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': f'添加组件失败: {{e}}'}}, ensure_ascii=False)}}")
"""
    result = _run_ue_code(code, read_timeout=30.0)
    _output(result["data"] if result["success"] else result)


def cmd_remove_comp(args: argparse.Namespace) -> None:
    """删除组件."""
    code = f"""
import json

import unreal

asset_path = "{args.path}"
component_name = "{args.name}"

eal = unreal.EditorAssetLibrary
asset = eal.load_asset(asset_path)

if asset is None:
    print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': '无法加载蓝图: ' + asset_path}}, ensure_ascii=False)}}")
else:
    try:
        scs = asset.get_editor_property("simple_construction_script")
        if scs is None:
            print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': '该资产不支持组件操作'}}, ensure_ascii=False)}}")
        else:
            all_nodes = scs.get_all_nodes()
            target_node = None
            for node in all_nodes:
                name = str(node.get_editor_property("internal_variable_name"))
                if name == component_name:
                    target_node = node
                    break

            if target_node is None:
                print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': f'未找到组件: {{component_name}}'}}, ensure_ascii=False)}}")
            else:
                scs.remove_node(target_node)
                try:
                    unreal.KismetSystemLibrary.execute_console_command(None, f'kismet recompile {{asset_path}}')
                except Exception:
                    pass
                eal.save_loaded_asset(asset)
                print(f"@@RESULT@@:{{json.dumps({{'success': True, 'component': component_name, 'message': '组件删除并编译成功'}}, ensure_ascii=False)}}")
    except Exception as e:
        print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': f'删除组件失败: {{e}}'}}, ensure_ascii=False)}}")
"""
    result = _run_ue_code(code, read_timeout=30.0)
    _output(result["data"] if result["success"] else result)


def cmd_get_prop(args: argparse.Namespace) -> None:
    """读取属性."""
    code = f"""
import json

import unreal

asset_path = "{args.path}"
property_name = "{args.prop}"
eal = unreal.EditorAssetLibrary

asset = eal.load_asset(asset_path)
if asset is None:
    print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': '无法加载资产: ' + asset_path}}, ensure_ascii=False)}}")
else:
    try:
        parts = property_name.split(".")
        obj = asset
        for part in parts[:-1]:
            obj = obj.get_editor_property(part)
        value = obj.get_editor_property(parts[-1])

        if isinstance(value, (bool, int, float, str)):
            serialized = value
        elif isinstance(value, unreal.Vector):
            serialized = {{"x": value.x, "y": value.y, "z": value.z}}
        elif isinstance(value, unreal.Rotator):
            serialized = {{"pitch": value.pitch, "yaw": value.yaw, "roll": value.roll}}
        elif isinstance(value, unreal.LinearColor):
            serialized = {{"r": value.r, "g": value.g, "b": value.b, "a": value.a}}
        else:
            serialized = str(value)

        print(f"@@RESULT@@:{{json.dumps({{'success': True, 'property': property_name, 'value': serialized}}, ensure_ascii=False)}}")
    except Exception as e:
        print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': f'读取属性失败: {{e}}'}}, ensure_ascii=False)}}")
"""
    result = _run_ue_code(code)
    _output(result["data"] if result["success"] else result)


def cmd_set_prop(args: argparse.Namespace) -> None:
    """设置属性."""
    code = f"""
import json

import unreal

asset_path = "{args.path}"
property_name = "{args.prop}"
value_json = '''{args.value}'''
eal = unreal.EditorAssetLibrary

asset = eal.load_asset(asset_path)
if asset is None:
    print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': '无法加载资产: ' + asset_path}}, ensure_ascii=False)}}")
else:
    try:
        value = json.loads(value_json)

        if isinstance(value, dict):
            if 'enum' in value:
                enum_parts = value['enum'].split('.')
                if len(enum_parts) == 2 and hasattr(unreal, enum_parts[0]):
                    enum_class = getattr(unreal, enum_parts[0])
                    target_enum_name = enum_parts[1].lower()
                    # 遍历枚举类的所有属性，进行忽略大小写的匹配
                    for attr in dir(enum_class):
                        if attr.lower() == target_enum_name:
                            value = getattr(enum_class, attr)
                            break
            elif 'x' in value and 'y' in value and 'z' in value:
                value = unreal.Vector(value['x'], value['y'], value['z'])
            elif 'pitch' in value and 'yaw' in value and 'roll' in value:
                value = unreal.Rotator(value['pitch'], value['yaw'], value['roll'])
            elif 'r' in value and 'g' in value and 'b' in value:
                a = value.get('a', 1.0)
                value = unreal.LinearColor(value['r'], value['g'], value['b'], a)

        parts = property_name.split(".")
        obj = asset
        for part in parts[:-1]:
            obj = obj.get_editor_property(part)
        obj.set_editor_property(parts[-1], value)

        if asset.get_class().get_name() in ['Blueprint', 'WidgetBlueprint', 'AnimBlueprint']:
            try:
                unreal.KismetSystemLibrary.execute_console_command(None, f'kismet recompile {{asset_path}}')
            except Exception:
                pass

        eal.save_loaded_asset(asset)
        print(f"@@RESULT@@:{{json.dumps({{'success': True, 'property': property_name, 'message': '属性设置成功'}}, ensure_ascii=False)}}")
    except Exception as e:
        print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': f'设置属性失败: {{e}}'}}, ensure_ascii=False)}}")
"""
    result = _run_ue_code(code)
    _output(result["data"] if result["success"] else result)


def cmd_compile(args: argparse.Namespace) -> None:
    """编译蓝图."""
    code = f"""
import json

import unreal

asset_path = "{args.path}"
eal = unreal.EditorAssetLibrary

if not eal.does_asset_exist(asset_path):
    print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': '蓝图不存在: ' + asset_path}}, ensure_ascii=False)}}")
else:
    try:
        bp_asset = eal.load_asset(asset_path)
        if bp_asset is None:
            print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': '无法加载蓝图: ' + asset_path}}, ensure_ascii=False)}}")
        else:
            unreal.KismetSystemLibrary.execute_console_command(None, f'kismet recompile {{asset_path}}')
            eal.save_asset(asset_path)
            print(f"@@RESULT@@:{{json.dumps({{'success': True, 'path': asset_path, 'message': '蓝图编译成功'}}, ensure_ascii=False)}}")
    except Exception as e:
        print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': f'编译失败: {{e}}'}}, ensure_ascii=False)}}")
"""
    result = _run_ue_code(code, read_timeout=30.0)
    _output(result["data"] if result["success"] else result)


def cmd_save(args: argparse.Namespace) -> None:
    """保存资产."""
    code = f"""
import json

import unreal

asset_path = "{args.path}"
eal = unreal.EditorAssetLibrary

if not eal.does_asset_exist(asset_path):
    print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': '资产不存在: ' + asset_path}}, ensure_ascii=False)}}")
else:
    result = eal.save_asset(asset_path)
    print(f"@@RESULT@@:{{json.dumps({{'success': result, 'path': asset_path, 'message': '保存成功' if result else '保存失败'}}, ensure_ascii=False)}}")
"""
    result = _run_ue_code(code)
    _output(result["data"] if result["success"] else result)


def cmd_save_all(args: argparse.Namespace) -> None:
    """保存所有脏资产."""
    code = """
import json

import unreal

eal = unreal.EditorAssetLibrary
result = eal.save_all_dirty_packages()
print(f"@@RESULT@@:{json.dumps({'success': result, 'message': '所有脏资产已保存' if result else '保存失败'}, ensure_ascii=False)}")
"""
    result = _run_ue_code(code)
    _output(result["data"] if result["success"] else result)


def cmd_references(args: argparse.Namespace) -> None:
    """查找资产引用."""
    code = f"""
import json

import unreal

asset_path = "{args.path}"
eal = unreal.EditorAssetLibrary

if not eal.does_asset_exist(asset_path):
    print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': '资产不存在: ' + asset_path}}, ensure_ascii=False)}}")
else:
    referencers = eal.find_package_referencers_for_asset(asset_path)
    ref_list = [str(r) for r in referencers] if referencers else []
    print(f"@@RESULT@@:{{json.dumps({{'success': True, 'asset': asset_path, 'referencers': ref_list, 'count': len(ref_list)}}, ensure_ascii=False)}}")
"""
    result = _run_ue_code(code, read_timeout=60.0)
    _output(result["data"] if result["success"] else result)


def _ue_search_assets(
    name: str,
    limit: int = 20,
    search_path: str = "",
) -> list:
    """搜索资产(按名称模糊匹配).

    使用 BlueprintTool C++ 插件的 SearchAssetsByName 方法,
    在 C++ 层面利用 AssetRegistry 内存索引搜索,毫秒级完成.

    Args:
        name: 资产名称关键词(模糊匹配,不区分大小写).
        limit: 最大返回数量.
        search_path: 可选的 UE 路径前缀(如 /Game/Feature).

    Returns:
        匹配的资产信息列表.
    """
    search_path_arg = f'"{search_path}"' if search_path else '""'
    code = f"""
import json

import unreal

try:
    results = unreal.BlueprintToolLibrary.search_assets_by_name(
        "{name}", {limit}, {search_path_arg}
    )
    items = []
    for path in results:
        path_str = str(path)
        # 过滤幽灵路径: AssetRegistry 缓存可能包含已删除/移动的资产
        if not unreal.EditorAssetLibrary.does_asset_exist(path_str):
            continue
        
        # 默认过滤掉重定向器(ObjectRedirector)
        reg = unreal.AssetRegistryHelpers.get_asset_registry()
        package_assets = reg.get_assets_by_package_name(path_str)
        if package_assets and all(str(a.asset_class) == "ObjectRedirector" for a in package_assets):
            continue
            
        asset_name = path_str.rsplit("/", 1)[-1]
        items.append({{"name": asset_name, "path": path_str}})
    print(f"@@RESULT@@:{{json.dumps({{'items': items}}, ensure_ascii=False)}}")
except AttributeError:
    print(f"@@RESULT@@:{{json.dumps({{'error': 'BlueprintTool 插件未加载'}}, ensure_ascii=False)}}")
except Exception as e:
    print(f"@@RESULT@@:{{json.dumps({{'error': str(e)}}, ensure_ascii=False)}}")
"""
    result = _run_ue_code(code, read_timeout=15.0)
    if not result["success"]:
        return []

    data = result["data"]
    if isinstance(data, dict):
        if "items" in data:
            return data["items"]
        # 有错误信息,返回空列表
        return []

    return []


def cmd_search(args: argparse.Namespace) -> None:
    """按名称搜索资产(模糊匹配).

    通过 UE 编辑器的 EditorAssetLibrary 搜索资产.
    指定 --path 可限定搜索范围(更快),不指定则搜索常见目录.
    """
    name = args.name
    class_filter = args.filter or ""
    limit = args.limit
    verify = args.verify
    search_path = args.path or ""

    # 通过 UE 编辑器搜索
    results = _ue_search_assets(name, limit=limit, search_path=search_path)

    if not results:
        _output({
            "success": True,
            "results": [],
            "count": 0,
            "search_name": name,
            "message": "未找到匹配的资产",
        })
        return

    # 如果需要按类型过滤或验证
    if verify or class_filter:
        paths_to_verify = [r["path"] for r in results]
        paths_json = json.dumps(paths_to_verify, ensure_ascii=False)

        code = f"""
import json

import unreal

paths = {paths_json}
class_filter = "{class_filter}".lower()
verified = []

for p in paths:
    if not unreal.EditorAssetLibrary.does_asset_exist(p):
        continue
    asset_data = unreal.EditorAssetLibrary.find_asset_data(p)
    if not asset_data:
        continue
    asset_class = str(asset_data.asset_class)
    if class_filter and class_filter not in asset_class.lower():
        continue
    verified.append({{
        "name": str(asset_data.asset_name),
        "path": p,
        "package": str(asset_data.package_name),
        "class": asset_class,
    }})

print(f"@@RESULT@@:{{json.dumps({{'results': verified, 'count': len(verified)}}, ensure_ascii=False)}}")
"""
        verify_result = _run_ue_code(code, read_timeout=30.0)
        if verify_result["success"] and isinstance(verify_result["data"], dict):
            data = verify_result["data"]
            data["search_name"] = name
            data["method"] = "ue_editor_search + verify"
            _output({"success": True, **data})
            return

    # 直接返回搜索结果
    _output({
        "success": True,
        "results": results,
        "count": len(results),
        "search_name": name,
        "method": "ue_editor_search",
    })


def cmd_exec(args: argparse.Namespace) -> None:
    """执行任意 Python 代码."""
    result = _run_ue_code(args.code, read_timeout=args.timeout)
    if result["success"]:
        data = result["data"]
        if isinstance(data, str):
            print(data)
        else:
            _output(data)
    else:
        _output(result)


def cmd_list_widgets(args: argparse.Namespace) -> None:
    """列出 UMG WidgetBlueprint 的控件树."""
    name_filter = args.filter or ""
    flat = args.flat
    code = f"""
import json

import unreal

asset_path = "{args.path}"
name_filter = "{name_filter}".lower()
flat_mode = {flat}

eal = unreal.EditorAssetLibrary

if not eal.does_asset_exist(asset_path):
    print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': '资产不存在: ' + asset_path}}, ensure_ascii=False)}}")
else:
    try:
        # 使用 BlueprintToolLibrary.list_widgets 获取完整控件树(JSON 字符串)
        raw = unreal.BlueprintToolLibrary.list_widgets(asset_path)
        tree = json.loads(raw)

        # 收集所有 TextBlock 类型控件的名称,用于批量获取文本
        text_widgets = []
        def collect_text_widgets(node):
            cls = node.get('Class', '')
            if 'TextBlock' in cls or 'RichText' in cls:
                text_widgets.append(node.get('Name', ''))
            for child in node.get('Children', []):
                collect_text_widgets(child)
        collect_text_widgets(tree)

        # 批量获取 TextBlock 的文本内容
        text_map = {{}}
        for widget_name in text_widgets:
            try:
                raw_text = unreal.BlueprintToolLibrary.get_widget_property(
                    asset_path, widget_name, 'text'
                )
                # 解析 INVTEXT("xxx") 格式
                if raw_text and raw_text.startswith('INVTEXT("') and raw_text.endswith('")'):
                    text_map[widget_name] = raw_text[9:-2]
                elif raw_text and raw_text.startswith('NSLOCTEXT('):
                    # NSLOCTEXT("ns", "key", "value") 格式
                    parts = raw_text.split('"')
                    if len(parts) >= 6:
                        text_map[widget_name] = parts[5]
                    else:
                        text_map[widget_name] = raw_text
                elif raw_text:
                    text_map[widget_name] = raw_text
            except Exception:
                pass

        def flatten(node, depth=0, parent=''):
            results = []
            name = node.get('Name', '')
            cls = node.get('Class', '')
            is_var = node.get('IsVariable', False)
            info = {{'name': name, 'class': cls, 'depth': depth, 'is_variable': is_var}}
            if parent:
                info['parent'] = parent
            # 添加文本内容
            if name in text_map:
                info['text'] = text_map[name]
            # 应用过滤
            text_val = info.get('text', '')
            if not name_filter or name_filter in name.lower() or name_filter in cls.lower() or name_filter in text_val.lower():
                results.append(info)
            # 递归子节点
            for child in node.get('Children', []):
                results.extend(flatten(child, depth + 1, name))
            return results

        # 将文本信息注入树结构
        def inject_text(node):
            name = node.get('Name', '')
            if name in text_map:
                node['Text'] = text_map[name]
            for child in node.get('Children', []):
                inject_text(child)

        if flat_mode:
            widgets = flatten(tree)
            print(f"@@RESULT@@:{{json.dumps({{'success': True, 'asset': asset_path, 'widgets': widgets, 'count': len(widgets)}}, ensure_ascii=False)}}")
        else:
            # 树形模式: 如果有过滤条件则扁平化输出,否则返回原始树
            if name_filter:
                widgets = flatten(tree)
                print(f"@@RESULT@@:{{json.dumps({{'success': True, 'asset': asset_path, 'widgets': widgets, 'count': len(widgets)}}, ensure_ascii=False)}}")
            else:
                inject_text(tree)
                print(f"@@RESULT@@:{{json.dumps({{'success': True, 'asset': asset_path, 'tree': tree}}, ensure_ascii=False)}}")
    except AttributeError:
        print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': 'BlueprintToolLibrary.list_widgets 方法不可用,请确认 C++ 插件已加载'}}, ensure_ascii=False)}}")
    except Exception as e:
        print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': f'获取控件树失败: {{e}}'}}, ensure_ascii=False)}}")
"""
    result = _run_ue_code(code, read_timeout=30.0)
    _output(result["data"] if result["success"] else result)


def cmd_add_widget(args: argparse.Namespace) -> None:
    """添加 UMG 控件."""
    parent = args.parent or ""
    slot_type = args.slot or ""
    code = f"""
import json

import unreal

asset_path = "{args.path}"
widget_class = "{getattr(args, 'class')}"
widget_name = "{args.name}"
parent_name = "{parent}"
slot_type = "{slot_type}"

eal = unreal.EditorAssetLibrary

if not eal.does_asset_exist(asset_path):
    print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': '资产不存在: ' + asset_path}}, ensure_ascii=False)}}")
else:
    try:
        result = unreal.BlueprintToolLibrary.add_widget(
            asset_path, widget_class, widget_name, parent_name, slot_type
        )
        if result:
            eal.save_asset(asset_path)
            print(f"@@RESULT@@:{{json.dumps({{'success': True, 'path': asset_path, 'widget_name': widget_name, 'widget_class': widget_class, 'parent': parent_name or '(root)', 'message': '控件添加成功'}}, ensure_ascii=False)}}")
        else:
            print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': f'添加控件失败,请检查类名和父控件名是否正确'}}, ensure_ascii=False)}}")
    except Exception as e:
        print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': f'添加控件失败: {{e}}'}}, ensure_ascii=False)}}")
"""
    result = _run_ue_code(code, read_timeout=30.0)
    _output(result["data"] if result["success"] else result)


def cmd_remove_widget(args: argparse.Namespace) -> None:
    """删除 UMG 控件."""
    code = f"""
import json

import unreal

asset_path = "{args.path}"
widget_name = "{args.name}"

eal = unreal.EditorAssetLibrary

if not eal.does_asset_exist(asset_path):
    print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': '资产不存在: ' + asset_path}}, ensure_ascii=False)}}")
else:
    try:
        result = unreal.BlueprintToolLibrary.remove_widget(asset_path, widget_name)
        if result:
            eal.save_asset(asset_path)
            print(f"@@RESULT@@:{{json.dumps({{'success': True, 'path': asset_path, 'widget_name': widget_name, 'message': '控件删除成功'}}, ensure_ascii=False)}}")
        else:
            print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': f'删除控件失败,控件 {{widget_name}} 可能不存在'}}, ensure_ascii=False)}}")
    except Exception as e:
        print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': f'删除控件失败: {{e}}'}}, ensure_ascii=False)}}")
"""
    result = _run_ue_code(code, read_timeout=30.0)
    _output(result["data"] if result["success"] else result)


def cmd_set_widget_prop(args: argparse.Namespace) -> None:
    """设置 UMG 控件属性."""
    code = f"""
import json

import unreal

asset_path = "{args.path}"
widget_name = "{args.name}"
property_name = "{args.prop}"
value = '''{args.value}'''

eal = unreal.EditorAssetLibrary

if not eal.does_asset_exist(asset_path):
    print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': '资产不存在: ' + asset_path}}, ensure_ascii=False)}}")
else:
    try:
        result = unreal.BlueprintToolLibrary.set_widget_property(
            asset_path, widget_name, property_name, value
        )
        if result:
            eal.save_asset(asset_path)
            print(f"@@RESULT@@:{{json.dumps({{'success': True, 'path': asset_path, 'widget': widget_name, 'property': property_name, 'value': value, 'message': '控件属性设置成功'}}, ensure_ascii=False)}}")
        else:
            print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': f'设置属性失败,请检查控件名和属性名是否正确'}}, ensure_ascii=False)}}")
    except Exception as e:
        print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': f'设置控件属性失败: {{e}}'}}, ensure_ascii=False)}}")
"""
    result = _run_ue_code(code, read_timeout=30.0)
    _output(result["data"] if result["success"] else result)


def cmd_get_widget_prop(args: argparse.Namespace) -> None:
    """读取 UMG 控件属性."""
    code = f"""
import json

import unreal

asset_path = "{args.path}"
widget_name = "{args.name}"
property_name = "{args.prop}"

eal = unreal.EditorAssetLibrary

if not eal.does_asset_exist(asset_path):
    print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': '资产不存在: ' + asset_path}}, ensure_ascii=False)}}")
else:
    try:
        value = unreal.BlueprintToolLibrary.get_widget_property(
            asset_path, widget_name, property_name
        )
        # 解析 INVTEXT/NSLOCTEXT 格式
        display_value = value
        if value and value.startswith('INVTEXT("') and value.endswith('")'):
            display_value = value[9:-2]
        elif value and value.startswith('NSLOCTEXT('):
            parts = value.split('"')
            if len(parts) >= 6:
                display_value = parts[5]
        print(f"@@RESULT@@:{{json.dumps({{'success': True, 'path': asset_path, 'widget': widget_name, 'property': property_name, 'value': value, 'display_value': display_value}}, ensure_ascii=False)}}")
    except Exception as e:
        print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': f'读取控件属性失败: {{e}}'}}, ensure_ascii=False)}}")
"""
    result = _run_ue_code(code, read_timeout=30.0)
    _output(result["data"] if result["success"] else result)


def cmd_reparent_widget(args: argparse.Namespace) -> None:
    """移动 UMG 控件(更换父控件)."""
    code = f"""
import json

import unreal

asset_path = "{args.path}"
widget_name = "{args.name}"
new_parent = "{args.parent}"

eal = unreal.EditorAssetLibrary

if not eal.does_asset_exist(asset_path):
    print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': '资产不存在: ' + asset_path}}, ensure_ascii=False)}}")
else:
    try:
        result = unreal.BlueprintToolLibrary.reparent_widget(
            asset_path, widget_name, new_parent
        )
        if result:
            eal.save_asset(asset_path)
            print(f"@@RESULT@@:{{json.dumps({{'success': True, 'path': asset_path, 'widget': widget_name, 'new_parent': new_parent, 'message': '控件移动成功'}}, ensure_ascii=False)}}")
        else:
            print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': f'移动控件失败,请检查控件名和目标父控件名是否正确'}}, ensure_ascii=False)}}")
    except Exception as e:
        print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': f'移动控件失败: {{e}}'}}, ensure_ascii=False)}}")
"""
    result = _run_ue_code(code, read_timeout=30.0)
    _output(result["data"] if result["success"] else result)


def cmd_widget_classes(args: argparse.Namespace) -> None:
    """获取可用 UMG 控件类列表."""
    keyword = args.keyword or ""
    code = f"""
import json

import unreal

keyword = "{keyword}".lower()

try:
    all_classes = unreal.BlueprintToolLibrary.get_available_widget_classes()
    classes = [str(c) for c in all_classes]
    if keyword:
        classes = [c for c in classes if keyword in c.lower()]
    classes.sort()
    result_data = json.dumps({{'classes': classes, 'count': len(classes)}}, ensure_ascii=False)
    print(f"@@RESULT@@:{{result_data}}")
except Exception as e:
    print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': f'获取控件类列表失败: {{e}}'}}, ensure_ascii=False)}}")
"""
    result = _run_ue_code(code, read_timeout=15.0)
    _output(result["data"] if result["success"] else result)


def cmd_classes(args: argparse.Namespace) -> None:
    """获取可用类列表."""
    code = f"""
import json

import unreal

base_class_name = "{args.base}"
keyword = "{args.keyword}".lower()

common_component_classes = [
    "StaticMeshComponent", "SkeletalMeshComponent",
    "BoxComponent", "SphereComponent", "CapsuleComponent",
    "PointLightComponent", "SpotLightComponent", "DirectionalLightComponent",
    "AudioComponent", "ParticleSystemComponent", "NiagaraComponent",
    "CameraComponent", "SpringArmComponent",
    "ArrowComponent", "BillboardComponent",
    "TextRenderComponent", "WidgetComponent",
    "SceneComponent", "ChildActorComponent",
    "SplineComponent", "SplineMeshComponent",
    "DecalComponent", "PostProcessComponent",
]

common_asset_classes = [
    "Blueprint", "WidgetBlueprint", "AnimBlueprint",
    "Material", "MaterialInstance", "MaterialFunction",
    "Texture2D", "TextureCube", "RenderTarget2D",
    "StaticMesh", "SkeletalMesh",
    "SoundWave", "SoundCue",
    "DataTable", "CurveTable",
    "ParticleSystem", "NiagaraSystem",
    "AnimSequence", "AnimMontage", "BlendSpace",
    "Level", "World",
]

if base_class_name.lower() in ["actorcomponent", "component", "scenecomponent"]:
    classes = common_component_classes
else:
    classes = common_component_classes + common_asset_classes

if keyword:
    classes = [c for c in classes if keyword in c.lower()]

result_str = json.dumps({{'classes': classes, 'count': len(classes)}}, ensure_ascii=False)
print(f"@@RESULT@@:{{result_str}}")
"""
    result = _run_ue_code(code)
    _output(result["data"] if result["success"] else result)


# ====================================================================
# DataTable 操作（使用 BlueprintToolLibrary C++ 原生接口）
# ====================================================================


def cmd_dt_info(args: argparse.Namespace) -> None:
    """获取 DataTable 结构信息."""
    code = f"""
import json

import unreal

asset_path = "{args.path}"
btl = unreal.BlueprintToolLibrary
result_str = btl.get_data_table_info(asset_path)
if result_str:
    info = json.loads(result_str)
    info['success'] = True
    print(f"@@RESULT@@:{{json.dumps(info, ensure_ascii=False)}}")
else:
    print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': '获取 DataTable 信息失败,资产可能不存在或不是 DataTable: ' + asset_path}}, ensure_ascii=False)}}")
"""
    result = _run_ue_code(code, read_timeout=15.0)
    _output(result["data"] if result["success"] else result)


def cmd_dt_list_rows(args: argparse.Namespace) -> None:
    """列出 DataTable 的所有行名."""
    offset = args.offset
    limit = args.limit
    code = f"""
import json

import unreal

asset_path = "{args.path}"
btl = unreal.BlueprintToolLibrary
result_str = btl.list_data_table_rows(asset_path)
if result_str:
    data = json.loads(result_str)
    all_names = data if isinstance(data, list) else data.get('Rows', [])
    total = len(all_names)
    sliced = all_names[{offset}:{offset}+{limit}]
    print(f"@@RESULT@@:{{json.dumps({{'success': True, 'path': asset_path, 'total': total, 'offset': {offset}, 'limit': {limit}, 'row_names': sliced}}, ensure_ascii=False)}}")
else:
    print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': '获取行名失败,资产可能不存在: ' + asset_path}}, ensure_ascii=False)}}")
"""
    result = _run_ue_code(code, read_timeout=15.0)
    _output(result["data"] if result["success"] else result)


def cmd_dt_get_row(args: argparse.Namespace) -> None:
    """获取 DataTable 某一行的数据."""
    code = f"""
import json

import unreal

asset_path = "{args.path}"
row_name = "{args.row}"
btl = unreal.BlueprintToolLibrary
result_str = btl.get_data_table_row(asset_path, row_name)
if result_str:
    row_data = json.loads(result_str)
    row_data['success'] = True
    row_data['path'] = asset_path
    print(f"@@RESULT@@:{{json.dumps(row_data, ensure_ascii=False)}}")
else:
    print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': f'获取行数据失败,行可能不存在: {{row_name}}'}}, ensure_ascii=False)}}")
"""
    result = _run_ue_code(code, read_timeout=15.0)
    _output(result["data"] if result["success"] else result)


def cmd_dt_add_row(args: argparse.Namespace) -> None:
    """向 DataTable 添加一行数据.

    使用 BlueprintToolLibrary.add_data_table_row 原生接口直接添加行。
    """
    row_name = args.row
    values_json = args.values
    code = f"""
import json

import unreal

asset_path = "{args.path}"
row_name = "{row_name}"
values_json = '''{values_json}'''
btl = unreal.BlueprintToolLibrary

try:
    row_data_json = values_json
    success = btl.add_data_table_row(asset_path, row_name, row_data_json)
    if success:
        # 验证并保存
        unreal.EditorAssetLibrary.save_loaded_asset(
            unreal.EditorAssetLibrary.load_asset(asset_path))
        row_names = unreal.DataTableFunctionLibrary.get_data_table_row_names(
            unreal.EditorAssetLibrary.load_asset(asset_path))
        print(f"@@RESULT@@:{{json.dumps({{'success': True, 'path': asset_path, 'row_name': row_name, 'total_rows': len(row_names), 'message': '行添加成功'}}, ensure_ascii=False)}}")
    else:
        print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': f'添加行失败,可能行已存在或数据格式不匹配: {{row_name}}'}}, ensure_ascii=False)}}")
except Exception as e:
    print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': f'添加行异常: {{e}}'}}, ensure_ascii=False)}}")
"""
    result = _run_ue_code(code, read_timeout=15.0)
    _output(result["data"] if result["success"] else result)


def cmd_dt_remove_row(args: argparse.Namespace) -> None:
    """从 DataTable 删除一行."""
    row_name = args.row
    code = f"""
import json

import unreal

asset_path = "{args.path}"
row_name = "{row_name}"
btl = unreal.BlueprintToolLibrary

try:
    success = btl.remove_data_table_row(asset_path, row_name)
    if success:
        # 保存
        unreal.EditorAssetLibrary.save_loaded_asset(
            unreal.EditorAssetLibrary.load_asset(asset_path))
        row_names = unreal.DataTableFunctionLibrary.get_data_table_row_names(
            unreal.EditorAssetLibrary.load_asset(asset_path))
        print(f"@@RESULT@@:{{json.dumps({{'success': True, 'path': asset_path, 'removed_row': row_name, 'total_rows': len(row_names), 'message': '行删除成功'}}, ensure_ascii=False)}}")
    else:
        print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': f'删除行失败,行可能不存在: {{row_name}}'}}, ensure_ascii=False)}}")
except Exception as e:
    print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': f'删除行异常: {{e}}'}}, ensure_ascii=False)}}")
"""
    result = _run_ue_code(code, read_timeout=15.0)
    _output(result["data"] if result["success"] else result)


def cmd_dt_update_row(args: argparse.Namespace) -> None:
    """修改 DataTable 某一行的字段值."""
    row_name = args.row
    values_json = args.values
    code = f"""
import json

import unreal

asset_path = "{args.path}"
row_name = "{row_name}"
values_json = '''{values_json}'''
btl = unreal.BlueprintToolLibrary

try:
    row_data_json = values_json
    success = btl.modify_data_table_row(asset_path, row_name, row_data_json)
    if success:
        # 保存
        unreal.EditorAssetLibrary.save_loaded_asset(
            unreal.EditorAssetLibrary.load_asset(asset_path))
        print(f"@@RESULT@@:{{json.dumps({{'success': True, 'path': asset_path, 'row_name': row_name, 'updated_fields': list(json.loads(values_json).keys()), 'message': '行更新成功'}}, ensure_ascii=False)}}")
    else:
        print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': f'修改行失败,行可能不存在或数据格式不匹配: {{row_name}}'}}, ensure_ascii=False)}}")
except Exception as e:
    print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': f'修改行异常: {{e}}'}}, ensure_ascii=False)}}")
"""
    result = _run_ue_code(code, read_timeout=15.0)
    _output(result["data"] if result["success"] else result)


# ====================================================================
# 场景操作（当前关卡）
# ====================================================================

def cmd_spawn_actor(args: argparse.Namespace) -> None:
    """在当前关卡生成 Actor."""
    code = f"""
import json
import unreal

actor_class_path = "{args.class_path}"
location_json = '''{args.location}'''
rotation_json = '''{args.rotation}'''

try:
    # 解析位置和旋转
    loc_dict = json.loads(location_json) if location_json else {{"x": 0, "y": 0, "z": 0}}
    rot_dict = json.loads(rotation_json) if rotation_json else {{"pitch": 0, "yaw": 0, "roll": 0}}
    
    location = unreal.Vector(loc_dict.get('x', 0), loc_dict.get('y', 0), loc_dict.get('z', 0))
    rotation = unreal.Rotator(rot_dict.get('pitch', 0), rot_dict.get('yaw', 0), rot_dict.get('roll', 0))
    
    # 加载类
    actor_class = unreal.EditorAssetLibrary.load_blueprint_class(actor_class_path)
    if not actor_class:
        actor_class = unreal.EditorAssetLibrary.load_class(actor_class_path)
        
    if not actor_class:
        print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': f'无法加载 Actor 类: {{actor_class_path}}'}}, ensure_ascii=False)}}")
    else:
        actor = unreal.EditorLevelLibrary.spawn_actor_from_class(actor_class, location, rotation)
        if actor:
            # 自动保存当前关卡
            unreal.EditorLevelLibrary.save_current_level()
            print(f"@@RESULT@@:{{json.dumps({{'success': True, 'actor_name': actor.get_actor_label(), 'message': 'Actor 生成成功并已保存关卡'}}, ensure_ascii=False)}}")
        else:
            print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': '生成 Actor 失败'}}, ensure_ascii=False)}}")
except Exception as e:
    print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': f'生成 Actor 异常: {{e}}'}}, ensure_ascii=False)}}")
"""
    result = _run_ue_code(code, read_timeout=15.0)
    _output(result["data"] if result["success"] else result)


def cmd_delete_actor(args: argparse.Namespace) -> None:
    """从当前关卡删除 Actor."""
    code = f"""
import json
import unreal

actor_name = "{args.name}"

try:
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    target_actor = None
    for a in actors:
        if a.get_actor_label() == actor_name:
            target_actor = a
            break
            
    if target_actor:
        success = unreal.EditorLevelLibrary.destroy_actor(target_actor)
        if success:
            # 自动保存当前关卡
            unreal.EditorLevelLibrary.save_current_level()
            print(f"@@RESULT@@:{{json.dumps({{'success': True, 'actor_name': actor_name, 'message': 'Actor 删除成功并已保存关卡'}}, ensure_ascii=False)}}")
        else:
            print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': '删除 Actor 失败'}}, ensure_ascii=False)}}")
    else:
        print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': f'在当前关卡中找不到名为 {{actor_name}} 的 Actor'}}, ensure_ascii=False)}}")
except Exception as e:
    print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': f'删除 Actor 异常: {{e}}'}}, ensure_ascii=False)}}")
"""
    result = _run_ue_code(code, read_timeout=15.0)
    _output(result["data"] if result["success"] else result)


def cmd_list_actors(args: argparse.Namespace) -> None:
    """列出当前关卡中的 Actor."""
    code = f"""
import json
import unreal

filter_class = "{args.filter}".lower()

try:
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    result_list = []
    for a in actors:
        cls_name = a.get_class().get_name()
        if not filter_class or filter_class in cls_name.lower():
            result_list.append({{
                "name": a.get_actor_label(),
                "class": cls_name,
                "location": {{"x": a.get_actor_location().x, "y": a.get_actor_location().y, "z": a.get_actor_location().z}}
            }})
            
    print(f"@@RESULT@@:{{json.dumps({{'success': True, 'count': len(result_list), 'actors': result_list}}, ensure_ascii=False)}}")
except Exception as e:
    print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': f'获取 Actor 列表异常: {{e}}'}}, ensure_ascii=False)}}")
"""
    result = _run_ue_code(code, read_timeout=15.0)
    _output(result["data"] if result["success"] else result)


def cmd_rename_actor(args: argparse.Namespace) -> None:
    """重命名当前关卡中的 Actor."""
    code = f"""
import json
import unreal

old_name = "{args.old}"
new_name = "{args.new}"

try:
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    target_actor = None
    for a in actors:
        if a.get_actor_label() == old_name:
            target_actor = a
            break
            
    if target_actor:
        target_actor.set_actor_label(new_name)
        unreal.EditorLevelLibrary.save_current_level()
        print(f"@@RESULT@@:{{json.dumps({{'success': True, 'old_name': old_name, 'new_name': new_name, 'message': 'Actor 重命名成功并已保存关卡'}}, ensure_ascii=False)}}")
    else:
        print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': f'在当前关卡中找不到名为 {{old_name}} 的 Actor'}}, ensure_ascii=False)}}")
except Exception as e:
    print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': f'重命名 Actor 异常: {{e}}'}}, ensure_ascii=False)}}")
"""
    result = _run_ue_code(code, read_timeout=15.0)
    _output(result["data"] if result["success"] else result)


def cmd_set_actor_transform(args: argparse.Namespace) -> None:
    """设置当前关卡中 Actor 的变换(位置/旋转/缩放)."""
    code = f"""
import json
import unreal

actor_name = "{args.name}"
location_json = '''{args.location}'''
rotation_json = '''{args.rotation}'''
scale_json = '''{args.scale}'''

try:
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    target_actor = None
    for a in actors:
        if a.get_actor_label() == actor_name:
            target_actor = a
            break
            
    if target_actor:
        if location_json:
            loc_dict = json.loads(location_json)
            target_actor.set_actor_location(unreal.Vector(loc_dict.get('x', 0), loc_dict.get('y', 0), loc_dict.get('z', 0)), False, False)
        if rotation_json:
            rot_dict = json.loads(rotation_json)
            target_actor.set_actor_rotation(unreal.Rotator(rot_dict.get('roll', 0), rot_dict.get('pitch', 0), rot_dict.get('yaw', 0)), False)
        if scale_json:
            scale_dict = json.loads(scale_json)
            target_actor.set_actor_scale3d(unreal.Vector(scale_dict.get('x', 1), scale_dict.get('y', 1), scale_dict.get('z', 1)))
            
        unreal.EditorLevelLibrary.save_current_level()
        print(f"@@RESULT@@:{{json.dumps({{'success': True, 'actor_name': actor_name, 'message': 'Actor 变换设置成功并已保存关卡'}}, ensure_ascii=False)}}")
    else:
        print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': f'在当前关卡中找不到名为 {{actor_name}} 的 Actor'}}, ensure_ascii=False)}}")
except Exception as e:
    print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': f'设置 Actor 变换异常: {{e}}'}}, ensure_ascii=False)}}")
"""
    result = _run_ue_code(code, read_timeout=15.0)
    _output(result["data"] if result["success"] else result)


def cmd_open_level(args: argparse.Namespace) -> None:
    """打开指定关卡地图."""
    code = f"""
import json
import unreal

map_path = "{args.path}"

try:
    # 先验证资产是否存在
    if not unreal.EditorAssetLibrary.does_asset_exist(map_path):
        print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': f'关卡资产不存在: {{map_path}}'}}, ensure_ascii=False)}}")
    else:
        # 获取当前关卡名称
        current_world = unreal.EditorLevelLibrary.get_editor_world()
        current_name = current_world.get_name() if current_world else ''
        
        # 打开关卡
        success = unreal.EditorLevelLibrary.load_level(map_path)
        if success:
            new_world = unreal.EditorLevelLibrary.get_editor_world()
            new_name = new_world.get_name() if new_world else ''
            print(f"@@RESULT@@:{{json.dumps({{'success': True, 'map_path': map_path, 'level_name': new_name, 'previous_level': current_name, 'message': f'关卡打开成功: {{new_name}}'}}, ensure_ascii=False)}}")
        else:
            print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': f'打开关卡失败: {{map_path}}'}}, ensure_ascii=False)}}")
except Exception as e:
    print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': f'打开关卡异常: {{e}}'}}, ensure_ascii=False)}}")
"""
    result = _run_ue_code(code, read_timeout=30.0)
    _output(result["data"] if result["success"] else result)


def cmd_save_level(args: argparse.Namespace) -> None:
    """保存当前关卡."""
    code = f"""
import json
import unreal

try:
    success = unreal.EditorLevelLibrary.save_current_level()
    if success:
        print(f"@@RESULT@@:{{json.dumps({{'success': True, 'message': '当前关卡保存成功'}}, ensure_ascii=False)}}")
    else:
        print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': '当前关卡保存失败'}}, ensure_ascii=False)}}")
except Exception as e:
    print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': f'保存关卡异常: {{e}}'}}, ensure_ascii=False)}}")
"""
    result = _run_ue_code(code, read_timeout=15.0)
    _output(result["data"] if result["success"] else result)


def cmd_set_actor_prop(args: argparse.Namespace) -> None:
    """设置当前关卡中 Actor 的属性."""
    code = f"""
import json
import unreal

actor_name = "{args.name}"
property_name = "{args.prop}"
value_json = '''{args.value}'''

try:
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    target_actor = None
    for a in actors:
        if a.get_actor_label() == actor_name:
            target_actor = a
            break
            
    if target_actor:
        value = json.loads(value_json)

        if isinstance(value, dict):
            if 'enum' in value:
                enum_parts = value['enum'].split('.')
                if len(enum_parts) == 2 and hasattr(unreal, enum_parts[0]):
                    enum_class = getattr(unreal, enum_parts[0])
                    target_enum_name = enum_parts[1].lower()
                    for attr in dir(enum_class):
                        if attr.lower() == target_enum_name:
                            value = getattr(enum_class, attr)
                            break
            elif 'x' in value and 'y' in value and 'z' in value:
                value = unreal.Vector(value['x'], value['y'], value['z'])
            elif 'pitch' in value and 'yaw' in value and 'roll' in value:
                value = unreal.Rotator(value['pitch'], value['yaw'], value['roll'])
            elif 'r' in value and 'g' in value and 'b' in value:
                a = value.get('a', 1.0)
                value = unreal.LinearColor(value['r'], value['g'], value['b'], a)

        parts = property_name.split(".")
        obj = target_actor
        for part in parts[:-1]:
            obj = obj.get_editor_property(part)
        obj.set_editor_property(parts[-1], value)
        
        unreal.EditorLevelLibrary.save_current_level()
        print(f"@@RESULT@@:{{json.dumps({{'success': True, 'actor_name': actor_name, 'property': property_name, 'message': 'Actor 属性设置成功并已保存关卡'}}, ensure_ascii=False)}}")
    else:
        print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': f'在当前关卡中找不到名为 {{actor_name}} 的 Actor'}}, ensure_ascii=False)}}")
except Exception as e:
    print(f"@@RESULT@@:{{json.dumps({{'success': False, 'error': f'设置 Actor 属性异常: {{e}}'}}, ensure_ascii=False)}}")
"""
    result = _run_ue_code(code, read_timeout=15.0)
    _output(result["data"] if result["success"] else result)


# ====================================================================
# 命令行解析
# ====================================================================


def main() -> None:
    """主入口."""
    parser = argparse.ArgumentParser(
        description="UE 资产操作命令行工具",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    subparsers = parser.add_subparsers(dest="command", help="可用命令")

    # connect
    p = subparsers.add_parser("connect", help="连接到 UE 编辑器")
    p.add_argument("--timeout", type=float, default=6.0, help="扫描超时(秒)")

    # status
    subparsers.add_parser("status", help="获取编辑器状态")

    # list
    p = subparsers.add_parser("list", help="列出目录下资产")
    p.add_argument("--dir", required=True, help="资产目录路径")
    p.add_argument("--filter", default="", help="按类型过滤")
    p.add_argument("--recursive", type=bool, default=True, help="是否递归")

    # info
    p = subparsers.add_parser("info", help="获取资产详情")
    p.add_argument("--path", required=True, help="资产路径")

    # mat-info
    p = subparsers.add_parser("mat-info", help="获取材质详细信息(包含参数和默认值)")
    p.add_argument("--path", required=True, help="材质资产路径")

    # set-mat-param
    p = subparsers.add_parser("set-mat-param", help="设置材质/材质实例参数")
    p.add_argument("--path", required=True, help="材质资产路径")
    p.add_argument("--param", required=True, help="参数名称")
    p.add_argument("--type", required=True, choices=["scalar", "vector", "texture"], help="参数类型")
    p.add_argument("--value", required=True, help="参数值(scalar传数字, vector传JSON如{\"R\":1,\"G\":0,\"B\":0,\"A\":1}, texture传资产路径)")

    # exists
    p = subparsers.add_parser("exists", help="检查资产存在性")
    p.add_argument("--path", required=True, help="资产路径")

    # create-bp
    p = subparsers.add_parser("create-bp", help="创建蓝图")
    p.add_argument("--path", required=True, help="蓝图路径")
    p.add_argument("--parent", default="Actor", help="父类名称")
    p.add_argument("--type", default="Blueprint", help="蓝图类型")

    # duplicate
    p = subparsers.add_parser("duplicate", help="复制资产")
    p.add_argument("--source", required=True, help="源资产路径")
    p.add_argument("--dest", required=True, help="目标路径")

    # delete
    p = subparsers.add_parser("delete", help="删除资产")
    p.add_argument("--path", required=True, help="资产路径")
    p.add_argument("--force", action="store_true", help="强制删除")

    # rename
    p = subparsers.add_parser("rename", help="重命名资产")
    p.add_argument("--source", required=True, help="当前路径")
    p.add_argument("--dest", required=True, help="新路径")

    # add-var
    p = subparsers.add_parser("add-var", help="添加成员变量")
    p.add_argument("--path", required=True, help="蓝图路径")
    p.add_argument("--name", required=True, help="变量名称")
    p.add_argument("--type", default="bool", help="变量类型")
    p.add_argument("--default", default="", help="默认值")

    # rename-var
    p = subparsers.add_parser("rename-var", help="重命名变量")
    p.add_argument("--path", required=True, help="蓝图路径")
    p.add_argument("--old", required=True, help="旧变量名")
    p.add_argument("--new", required=True, help="新变量名")

    # list-comp
    p = subparsers.add_parser("list-comp", help="列出组件")
    p.add_argument("--path", required=True, help="蓝图路径")

    # add-comp
    p = subparsers.add_parser("add-comp", help="添加组件")
    p.add_argument("--path", required=True, help="蓝图路径")
    p.add_argument("--class", required=True, help="组件类名")
    p.add_argument("--name", default="", help="组件名称")
    p.add_argument("--parent", default="", help="父组件名称")

    # remove-comp
    p = subparsers.add_parser("remove-comp", help="删除组件")
    p.add_argument("--path", required=True, help="蓝图路径")
    p.add_argument("--name", required=True, help="组件名称")

    # list-widgets
    p = subparsers.add_parser("list-widgets", help="列出 UMG 控件树")
    p.add_argument("--path", required=True, help="WidgetBlueprint 资产路径")
    p.add_argument("--filter", default="", help="按名称/类型/文本过滤(不区分大小写)")
    p.add_argument("--flat", action="store_true", help="扁平化输出(默认树形)")

    # add-widget
    p = subparsers.add_parser("add-widget", help="添加 UMG 控件")
    p.add_argument("--path", required=True, help="WidgetBlueprint 资产路径")
    p.add_argument("--class", required=True, help="控件类名(如 TextBlock, Button, Image, CanvasPanel)")
    p.add_argument("--name", required=True, help="控件名称")
    p.add_argument("--parent", default="", help="父控件名称(留空则添加到根)")
    p.add_argument("--slot", default="", help="Slot 类型(如 CanvasPanelSlot)")

    # remove-widget
    p = subparsers.add_parser("remove-widget", help="删除 UMG 控件")
    p.add_argument("--path", required=True, help="WidgetBlueprint 资产路径")
    p.add_argument("--name", required=True, help="控件名称")

    # set-widget-prop
    p = subparsers.add_parser("set-widget-prop", help="设置 UMG 控件属性")
    p.add_argument("--path", required=True, help="WidgetBlueprint 资产路径")
    p.add_argument("--name", required=True, help="控件名称")
    p.add_argument("--prop", required=True, help="属性名称(如 text, visibility, color_and_opacity)")
    p.add_argument("--value", required=True, help="属性值(字符串格式)")

    # get-widget-prop
    p = subparsers.add_parser("get-widget-prop", help="读取 UMG 控件属性")
    p.add_argument("--path", required=True, help="WidgetBlueprint 资产路径")
    p.add_argument("--name", required=True, help="控件名称")
    p.add_argument("--prop", required=True, help="属性名称")

    # reparent-widget
    p = subparsers.add_parser("reparent-widget", help="移动 UMG 控件(更换父控件)")
    p.add_argument("--path", required=True, help="WidgetBlueprint 资产路径")
    p.add_argument("--name", required=True, help="控件名称")
    p.add_argument("--parent", required=True, help="新的父控件名称")

    # widget-classes
    p = subparsers.add_parser("widget-classes", help="获取可用 UMG 控件类列表")
    p.add_argument("--keyword", default="", help="关键词过滤")

    # get-prop
    p = subparsers.add_parser("get-prop", help="读取属性")
    p.add_argument("--path", required=True, help="资产路径")
    p.add_argument("--prop", required=True, help="属性名称")

    # set-prop
    p = subparsers.add_parser("set-prop", help="设置属性")
    p.add_argument("--path", required=True, help="资产路径")
    p.add_argument("--prop", required=True, help="属性名称")
    p.add_argument("--value", required=True, help="属性值(JSON 格式)")

    # compile
    p = subparsers.add_parser("compile", help="编译蓝图")
    p.add_argument("--path", required=True, help="蓝图路径")

    # save
    p = subparsers.add_parser("save", help="保存资产")
    p.add_argument("--path", required=True, help="资产路径")

    # save-all
    subparsers.add_parser("save-all", help="保存所有脏资产")

    # references
    p = subparsers.add_parser("references", help="查找资产引用")
    p.add_argument("--path", required=True, help="资产路径")

    # search
    p = subparsers.add_parser("search", help="按名称搜索资产(模糊匹配)")
    p.add_argument("--name", required=True, help="资产名称关键词(模糊匹配)")
    p.add_argument("--filter", default="", help="按资产类型过滤(如 DataTable, Blueprint, WidgetBlueprint)")
    p.add_argument("--limit", type=int, default=20, help="最大返回数量(默认20)")
    p.add_argument("--path", default="", help="限定搜索路径(如 /Game/LetsGo/Data),不指定则搜索常见目录")
    p.add_argument("--verify", action="store_true", help="通过 UE 编辑器验证资产类型(较慢但更准确)")

    # exec
    p = subparsers.add_parser("exec", help="执行任意 Python 代码")
    p.add_argument("--code", required=True, help="Python 代码")
    p.add_argument("--timeout", type=float, default=60.0, help="执行超时(秒)")

    # classes
    p = subparsers.add_parser("classes", help="获取可用类列表")
    p.add_argument("--base", default="", help="基类过滤")
    p.add_argument("--keyword", default="", help="关键词搜索")

    # dt-info
    p = subparsers.add_parser("dt-info", help="获取 DataTable 结构信息")
    p.add_argument("--path", required=True, help="DataTable 资产路径")

    # dt-list-rows
    p = subparsers.add_parser("dt-list-rows", help="列出 DataTable 行名")
    p.add_argument("--path", required=True, help="DataTable 资产路径")
    p.add_argument("--offset", type=int, default=0, help="起始偏移(默认0)")
    p.add_argument("--limit", type=int, default=100, help="最大返回数量(默认100)")

    # dt-get-row
    p = subparsers.add_parser("dt-get-row", help="获取 DataTable 某行数据")
    p.add_argument("--path", required=True, help="DataTable 资产路径")
    p.add_argument("--row", required=True, help="行名(RowName)")

    # dt-add-row
    p = subparsers.add_parser("dt-add-row", help="向 DataTable 添加一行")
    p.add_argument("--path", required=True, help="DataTable 资产路径")
    p.add_argument("--row", required=True, help="新行名(RowName)")
    p.add_argument("--values", required=True, help="行数据(JSON 格式,如 {\"Color\":\"(R=0.1,G=0.2,B=0.3,A=1.0)\"})")

    # dt-remove-row
    p = subparsers.add_parser("dt-remove-row", help="从 DataTable 删除一行")
    p.add_argument("--path", required=True, help="DataTable 资产路径")
    p.add_argument("--row", required=True, help="要删除的行名(RowName)")

    # dt-update-row
    p = subparsers.add_parser("dt-update-row", help="修改 DataTable 某行的字段")
    p.add_argument("--path", required=True, help="DataTable 资产路径")
    p.add_argument("--row", required=True, help="行名(RowName)")
    p.add_argument("--values", required=True, help="要更新的字段(JSON 格式,如 {\"Color\":\"(R=1.0,G=0.0,B=0.0,A=1.0)\"})")

    # spawn-actor
    p = subparsers.add_parser("spawn-actor", help="在当前关卡生成 Actor")
    p.add_argument("--class_path", required=True, help="Actor 类路径(如 /Game/Blueprints/BP_Test.BP_Test)")
    p.add_argument("--location", default="", help="位置 JSON (如 {\"x\":0, \"y\":0, \"z\":0})")
    p.add_argument("--rotation", default="", help="旋转 JSON (如 {\"pitch\":0, \"yaw\":0, \"roll\":0})")

    # delete-actor
    p = subparsers.add_parser("delete-actor", help="从当前关卡删除 Actor")
    p.add_argument("--name", required=True, help="Actor 在大纲中的名称(Label)")

    # list-actors
    p = subparsers.add_parser("list-actors", help="列出当前关卡中的 Actor")
    p.add_argument("--filter", default="", help="按类名过滤(不区分大小写)")

    # rename-actor
    p = subparsers.add_parser("rename-actor", help="重命名当前关卡中的 Actor")
    p.add_argument("--old", required=True, help="旧名称(Label)")
    p.add_argument("--new", required=True, help="新名称(Label)")

    # set-actor-transform
    p = subparsers.add_parser("set-actor-transform", help="设置当前关卡中 Actor 的变换")
    p.add_argument("--name", required=True, help="Actor 名称(Label)")
    p.add_argument("--location", default="", help="位置 JSON (如 {\"x\":0, \"y\":0, \"z\":0})")
    p.add_argument("--rotation", default="", help="旋转 JSON (如 {\"pitch\":0, \"yaw\":0, \"roll\":0})")
    p.add_argument("--scale", default="", help="缩放 JSON (如 {\"x\":1, \"y\":1, \"z\":1})")

    # open-level
    p = subparsers.add_parser("open-level", help="打开指定关卡地图")
    p.add_argument("--path", required=True, help="关卡资产路径 (如 /Game/Maps/MyMap)")

    # save-level
    subparsers.add_parser("save-level", help="保存当前关卡")

    # set-actor-prop
    p = subparsers.add_parser("set-actor-prop", help="设置当前关卡中 Actor 的属性")
    p.add_argument("--name", required=True, help="Actor 名称(Label)")
    p.add_argument("--prop", required=True, help="属性名称(支持点分隔，如 LightComponent.Intensity)")
    p.add_argument("--value", required=True, help="属性值(JSON格式)")

    args = parser.parse_args()

    if not args.command:
        parser.print_help()
        sys.exit(1)

    # 命令分发
    cmd_map = {
        "connect": cmd_connect,
        "status": cmd_status,
        "list": cmd_list,
        "info": cmd_info,
        "mat-info": cmd_mat_info,
        "set-mat-param": cmd_set_mat_param,
        "exists": cmd_exists,
        "create-bp": cmd_create_bp,
        "duplicate": cmd_duplicate,
        "delete": cmd_delete,
        "rename": cmd_rename,
        "add-var": cmd_add_var,
        "rename-var": cmd_rename_var,
        "list-comp": cmd_list_comp,
        "add-comp": cmd_add_comp,
        "remove-comp": cmd_remove_comp,
        "list-widgets": cmd_list_widgets,
        "add-widget": cmd_add_widget,
        "remove-widget": cmd_remove_widget,
        "set-widget-prop": cmd_set_widget_prop,
        "get-widget-prop": cmd_get_widget_prop,
        "reparent-widget": cmd_reparent_widget,
        "widget-classes": cmd_widget_classes,
        "get-prop": cmd_get_prop,
        "set-prop": cmd_set_prop,
        "compile": cmd_compile,
        "save": cmd_save,
        "save-all": cmd_save_all,
        "references": cmd_references,
        "search": cmd_search,
        "exec": cmd_exec,
        "classes": cmd_classes,
        "dt-info": cmd_dt_info,
        "dt-list-rows": cmd_dt_list_rows,
        "dt-get-row": cmd_dt_get_row,
        "dt-add-row": cmd_dt_add_row,
        "dt-remove-row": cmd_dt_remove_row,
        "dt-update-row": cmd_dt_update_row,
        "spawn-actor": cmd_spawn_actor,
        "delete-actor": cmd_delete_actor,
        "list-actors": cmd_list_actors,
        "rename-actor": cmd_rename_actor,
        "set-actor-transform": cmd_set_actor_transform,
        "open-level": cmd_open_level,
        "save-level": cmd_save_level,
        "set-actor-prop": cmd_set_actor_prop,
    }

    handler = cmd_map.get(args.command)
    if handler:
        try:
            handler(args)
        finally:
            close_session()
    else:
        parser.print_help()
        sys.exit(1)


if __name__ == "__main__":
    main()
