"""Replace selected StaticMeshActors with the Blueprint of the same name.

The Blueprint is found from the mesh asset name, not the actor label: SM_TableLamp
becomes BP_HK_TableLamp, and FramesSmall becomes BP_HK_FramesSmall. BP_<name> is
tried after BP_HK_<name>. The new actor keeps the transform, outliner folder, tags
and material overrides of the actor it replaces. Actors with no matching Blueprint
are left alone and reported.

Usage from the Output Log console (Cmd: Python):
    import mesh_to_blueprint; mesh_to_blueprint.preview_selected()
    import mesh_to_blueprint; mesh_to_blueprint.replace_selected()

Menu entries are registered by init_unreal.py.
"""

import unreal

LOG_PREFIX = "[MeshToBlueprint] "

MESH_PREFIX = "SM_"
BLUEPRINT_PREFIXES = ("BP_HK_", "BP_")
BLUEPRINT_FOLDERS = ("/Game/HouseKeeping/Blueprints", "/Game/Design/Blueprints")


def _log(message):
    unreal.log(LOG_PREFIX + message)


def _warn(message):
    unreal.log_warning(LOG_PREFIX + message)


def _actor_subsystem():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def blueprint_names(mesh_name):
    """Candidate Blueprint names for a mesh asset name, best match first."""
    base = mesh_name[len(MESH_PREFIX):] if mesh_name.startswith(MESH_PREFIX) else mesh_name
    return ["%s%s" % (prefix, base) for prefix in BLUEPRINT_PREFIXES]


def find_blueprint_class(mesh_name):
    for name in blueprint_names(mesh_name):
        for folder in BLUEPRINT_FOLDERS:
            try:
                blueprint_class = unreal.load_class(None, "%s/%s.%s_C" % (folder, name, name))
            except Exception:
                continue
            if blueprint_class is not None:
                return blueprint_class
    return None


def _mesh_component(actor):
    return actor.get_component_by_class(unreal.StaticMeshComponent)


def plan_replacements(actors):
    """Return (replacements, skipped): [(actor, mesh, class)] and reasons as text."""
    replacements = []
    skipped = []

    for actor in actors:
        label = actor.get_actor_label()
        if not isinstance(actor, unreal.StaticMeshActor):
            skipped.append("%s (not a StaticMeshActor)" % label)
            continue

        mesh = _mesh_component(actor).get_editor_property("static_mesh")
        if mesh is None:
            skipped.append("%s (no static mesh assigned)" % label)
            continue

        blueprint_class = find_blueprint_class(mesh.get_name())
        if blueprint_class is None:
            skipped.append("%s (%s: no Blueprint named %s)"
                           % (label, mesh.get_name(), " or ".join(blueprint_names(mesh.get_name()))))
            continue

        replacements.append((actor, mesh, blueprint_class))

    return replacements, skipped


def _replace(actor, mesh, blueprint_class):
    subsystem = _actor_subsystem()
    transform = actor.get_actor_transform()

    new_actor = subsystem.spawn_actor_from_class(
        blueprint_class, transform.translation, transform.rotation.rotator())
    if new_actor is None:
        return None

    new_actor.set_actor_transform(transform, False, False)
    new_actor.set_folder_path(actor.get_folder_path())
    new_actor.set_editor_property("tags", actor.get_editor_property("tags"))

    source_component = _mesh_component(actor)
    target_component = _mesh_component(new_actor)
    if target_component is not None:
        for index, material in enumerate(source_component.get_editor_property("override_materials")):
            if material is not None:
                target_component.set_material(index, material)

        target_mesh = target_component.get_editor_property("static_mesh")
        if target_mesh is None or target_mesh.get_path_name() != mesh.get_path_name():
            _warn("%s uses a different mesh than %s."
                  % (blueprint_class.get_name(), mesh.get_path_name()))

    if min(transform.scale3d.x, transform.scale3d.y, transform.scale3d.z) < 0:
        _warn("%s has negative scale, check its physics and collision."
              % new_actor.get_actor_label())

    subsystem.destroy_actor(actor)
    return new_actor


def replace_selected():
    """Replace every selected StaticMeshActor that has a matching Blueprint."""
    subsystem = _actor_subsystem()
    selected = subsystem.get_selected_level_actors()
    if not selected:
        _warn("Nothing selected in the level editor.")
        return []

    replacements, skipped = plan_replacements(selected)

    spawned = []
    with unreal.ScopedEditorTransaction("Replace Meshes With Blueprints"):
        for actor, mesh, blueprint_class in replacements:
            label = actor.get_actor_label()
            new_actor = _replace(actor, mesh, blueprint_class)
            if new_actor is None:
                skipped.append("%s (could not spawn %s)" % (label, blueprint_class.get_name()))
                continue
            spawned.append(new_actor)
            _log("%s -> %s" % (label, new_actor.get_actor_label()))

    for entry in skipped:
        _warn("  not replaced: %s" % entry)
    if spawned:
        subsystem.set_selected_level_actors(spawned)
    _log("Done: %d replaced, %d left as they were." % (len(spawned), len(skipped)))
    return spawned


def preview_selected():
    """Log what would be replaced, without touching the level."""
    selected = _actor_subsystem().get_selected_level_actors()
    if not selected:
        _warn("Nothing selected in the level editor.")
        return

    replacements, skipped = plan_replacements(selected)
    for actor, mesh, blueprint_class in replacements:
        _log("%s (%s) -> %s" % (actor.get_actor_label(), mesh.get_name(), blueprint_class.get_name()))
    for entry in skipped:
        _warn("  would not replace: %s" % entry)
    _log("%d would be replaced, %d left as they were." % (len(replacements), len(skipped)))
