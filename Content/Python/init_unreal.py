"""Editor startup script: registers the Blueprint tool menu entries.

Unreal runs every init_unreal.py found on the Python path at editor startup,
so this file only needs the PythonScriptPlugin to be enabled.
"""

import unreal

SECTION_NAME = "BPExploder"
SECTION_LABEL = "Blueprint Tools"

MENU_PATHS = (
    "LevelEditor.ActorContextMenu",
    "LevelEditor.MainMenu.Tools",
)

ENTRIES = (
    (
        "BPExploder_Preview",
        "Preview: list meshes and lights inside selection",
        "Logs what would be created, without changing the level.",
        "import importlib, bp_exploder; importlib.reload(bp_exploder); bp_exploder.preview_selected()",
    ),
    (
        "BPExploder_Explode",
        "Explode to separate actors",
        "Spawns one actor per static mesh and per light, keeps nested Blueprints whole, "
        "and leaves the original actor in place.",
        "import importlib, bp_exploder; importlib.reload(bp_exploder); bp_exploder.explode_selected()",
    ),
    (
        "BPExploder_ExplodeMeshes",
        "Explode to separate actors (meshes only)",
        "Same, but skips the light components.",
        "import importlib, bp_exploder; importlib.reload(bp_exploder); bp_exploder.explode_selected_meshes_only()",
    ),
    (
        "BPExploder_ExplodeFlat",
        "Explode to separate actors (flatten nested Blueprints)",
        "Takes nested Blueprints apart into meshes and lights instead of keeping them whole.",
        "import importlib, bp_exploder; importlib.reload(bp_exploder); bp_exploder.explode_selected_flattened()",
    ),
    (
        "BPExploder_ExplodeDelete",
        "Explode to separate actors (delete original)",
        "Spawns the separate actors and deletes the source actor.",
        "import importlib, bp_exploder; importlib.reload(bp_exploder); bp_exploder.explode_selected_and_delete()",
    ),
    (
        "BPExploder_MeshToBlueprintPreview",
        "Preview: meshes to same-name Blueprints",
        "Logs which selected meshes have a matching Blueprint, without changing the level.",
        "import importlib, mesh_to_blueprint; importlib.reload(mesh_to_blueprint); mesh_to_blueprint.preview_selected()",
    ),
    (
        "BPExploder_MeshToBlueprint",
        "Replace meshes with same-name Blueprints",
        "Replaces selected meshes with their Blueprint, for example SM_TableLamp with BP_HK_TableLamp.",
        "import importlib, mesh_to_blueprint; importlib.reload(mesh_to_blueprint); mesh_to_blueprint.replace_selected()",
    ),
)


def _make_entry(name, label, tooltip, command):
    entry = unreal.ToolMenuEntry(name=name, type=unreal.MultiBlockType.MENU_ENTRY)
    entry.set_label(label)
    try:
        entry.set_tool_tip(tooltip)
    except Exception:
        pass
    entry.set_string_command(unreal.ToolMenuStringCommandType.PYTHON, "", command)
    return entry


def register_menus():
    tool_menus = unreal.ToolMenus.get()
    registered = 0

    for menu_path in MENU_PATHS:
        # extend_menu (not find_menu): these menus are registered lazily, so at
        # editor startup find_menu would still return None for most of them.
        menu = tool_menus.extend_menu(menu_path)
        if menu is None:
            unreal.log_warning("[BPExploder] Menu not found: %s" % menu_path)
            continue

        menu.add_section(SECTION_NAME, SECTION_LABEL)
        for name, label, tooltip, command in ENTRIES:
            menu.add_menu_entry(SECTION_NAME, _make_entry(name, label, tooltip, command))
        registered += 1

    try:
        tool_menus.refresh_all_widgets()
    except Exception:
        pass
    unreal.log("[BPExploder] Menu entries registered in %d menu(s)." % registered)


try:
    register_menus()
except Exception as error:  # never block editor startup on a menu failure
    unreal.log_warning("[BPExploder] Menu registration failed: %s" % error)
