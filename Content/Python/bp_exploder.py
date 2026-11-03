"""Explode Blueprint actors in the level back into individual actors.

Works on whatever is selected in the level editor. For every selected actor it
walks the component tree (including nested Child Actor Components and instanced
static mesh components) and spawns one StaticMeshActor per static mesh and one
light actor per light component, keeping the world transform and the source
component's own settings.

A nested Blueprint - a Child Actor Component whose class is a Blueprint - comes
out as that Blueprint actor, not as loose meshes, so the nesting survives one
level down. Child actors of a plain engine class (StaticMeshActor, PointLight)
carry no logic of their own, so those are taken apart as usual. Pass
keep_blueprints=False to flatten nested Blueprints into meshes and lights too.

Usage from the Output Log console (Cmd: Python):
    import bp_exploder; bp_exploder.explode_selected()
    import bp_exploder; bp_exploder.explode_selected(delete_original=True)

Menu entries are registered by init_unreal.py.
"""

import unreal

LOG_PREFIX = "[BPExploder] "

# Mesh component properties copied onto the new actor's mesh component.
MESH_PROPERTIES = (
    "mobility",
    "cast_shadow",
    "visible",
    "hidden_in_game",
    "collision_profile_name",
    "collision_enabled",
    "bounds_scale",
)

# Light component properties copied onto the new light actor's component.
# Names that do not exist on a given light class are skipped.
LIGHT_PROPERTIES = (
    "mobility",
    "intensity",
    "light_color",
    "attenuation_radius",
    "source_radius",
    "soft_source_radius",
    "source_length",
    "use_inverse_squared_falloff",
    "light_falloff_exponent",
    "intensity_units",
    "temperature",
    "use_temperature",
    "cast_shadows",
    "cast_static_shadows",
    "cast_dynamic_shadows",
    "cast_volumetric_shadow",
    "volumetric_scattering_intensity",
    "specular_scale",
    "indirect_lighting_intensity",
    "affects_world",
    "shadow_bias",
    "shadow_slope_bias",
    "shadow_resolution_scale",
    "contact_shadow_length",
    "inner_cone_angle",
    "outer_cone_angle",
    "source_width",
    "source_height",
    "barn_door_angle",
    "barn_door_length",
    "light_function_material",
    "ies_texture",
)

MAX_DEPTH = 8


def _log(message):
    unreal.log(LOG_PREFIX + message)


def _warn(message):
    unreal.log_warning(LOG_PREFIX + message)


def _actor_subsystem():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def _get(obj, name, default=None):
    try:
        return obj.get_editor_property(name)
    except Exception:
        return default


def _light_actor_class(component):
    """The level actor class matching a light component, most derived first."""
    pairs = (
        (unreal.SpotLightComponent, unreal.SpotLight),
        (unreal.RectLightComponent, unreal.RectLight),
        (unreal.PointLightComponent, unreal.PointLight),
        (unreal.DirectionalLightComponent, unreal.DirectionalLight),
    )
    for component_class, actor_class in pairs:
        if isinstance(component, component_class):
            return actor_class
    return None


def _is_blueprint_class(actor_class):
    """True for a Blueprint-generated class, false for a native one like StaticMeshActor."""
    if actor_class is None:
        return False
    try:
        if actor_class.get_class().get_name() == "BlueprintGeneratedClass":
            return True
    except Exception:
        pass
    try:
        return actor_class.get_name().endswith("_C")
    except Exception:
        return False


class Source(object):
    """One spawnable thing found inside an actor, resolved to world space."""

    def __init__(self, label, world_transform, component, mesh=None, actor_class=None):
        self.label = label
        self.world_transform = world_transform
        self.component = component
        self.mesh = mesh
        self.actor_class = actor_class


def collect_sources(actor, include_instances=True, include_lights=True,
                    keep_blueprints=True, depth=0, prefix=""):
    """Return (meshes, lights, blueprints, skipped) for an actor.

    Recurses into child actors, except the Blueprint ones kept whole.
    """
    meshes = []
    lights = []
    blueprints = []
    skipped = []

    if depth > MAX_DEPTH:
        _warn("Max nesting depth reached at %s, stopping recursion." % actor.get_actor_label())
        return meshes, lights, blueprints, skipped

    for component in actor.get_components_by_class(unreal.StaticMeshComponent):
        mesh = _get(component, "static_mesh")
        label = "%s%s" % (prefix, component.get_name())

        if mesh is None:
            skipped.append("%s (no static mesh assigned)" % label)
            continue

        if include_instances and isinstance(component, unreal.InstancedStaticMeshComponent):
            for index in range(component.get_instance_count()):
                found, instance_transform = component.get_instance_transform(index, world_space=True)
                if not found:
                    continue
                meshes.append(Source(
                    "%s_%03d" % (label, index), instance_transform, component, mesh))
            continue

        meshes.append(Source(label, component.get_world_transform(), component, mesh))

    if include_lights:
        for component in actor.get_components_by_class(unreal.LightComponent):
            label = "%s%s" % (prefix, component.get_name())
            if _light_actor_class(component) is None:
                skipped.append("%s (%s, no matching light actor)"
                               % (label, component.get_class().get_name()))
                continue
            lights.append(Source(label, component.get_world_transform(), component))

    # Nested Blueprints appear as Child Actor Components holding a separate actor.
    for child_component in actor.get_components_by_class(unreal.ChildActorComponent):
        child_actor = _get(child_component, "child_actor")
        label = "%s%s" % (prefix, child_component.get_name())
        child_class = _get(child_component, "child_actor_class")

        if keep_blueprints and _is_blueprint_class(child_class):
            # Spawn the Blueprint itself instead of taking it apart.
            transform = (child_actor.get_actor_transform() if child_actor is not None
                         else child_component.get_world_transform())
            blueprints.append(Source(label, transform, child_component, actor_class=child_class))
            continue

        if child_actor is None:
            skipped.append("%s (child actor not spawned)" % label)
            continue

        child_meshes, child_lights, child_blueprints, child_skipped = collect_sources(
            child_actor,
            include_instances=include_instances,
            include_lights=include_lights,
            keep_blueprints=keep_blueprints,
            depth=depth + 1,
            prefix="%s_" % label,
        )
        meshes.extend(child_meshes)
        lights.extend(child_lights)
        blueprints.extend(child_blueprints)
        skipped.extend(child_skipped)

    # Anything that is not geometry or light is lost by this conversion - report it.
    for component in actor.get_components_by_class(unreal.ActorComponent):
        if isinstance(component, (unreal.StaticMeshComponent, unreal.ChildActorComponent)):
            continue
        if include_lights and isinstance(component, unreal.LightComponent):
            continue
        if type(component) is unreal.SceneComponent:  # plain transform/scene roots
            continue
        skipped.append("%s%s (%s)" % (prefix, component.get_name(), component.get_class().get_name()))

    return meshes, lights, blueprints, skipped


def _copy_properties(source, target, names):
    for name in names:
        value = _get(source, name)
        if value is None:
            continue
        try:
            target.set_editor_property(name, value)
        except Exception:
            pass


def _finish(new_actor, source, folder_path):
    new_actor.set_actor_transform(source.world_transform, False, False)
    new_actor.set_actor_label(source.label)
    if folder_path:
        new_actor.set_folder_path(folder_path)


def _spawn_mesh(source, folder_path):
    subsystem = _actor_subsystem()
    transform = source.world_transform

    new_actor = subsystem.spawn_actor_from_class(
        unreal.StaticMeshActor, transform.translation, transform.rotation.rotator())
    if new_actor is None:
        return None

    mesh_component = _get(new_actor, "static_mesh_component")
    if mesh_component is None:
        mesh_component = new_actor.get_component_by_class(unreal.StaticMeshComponent)

    # Mobility first: a Static component refuses later transform changes at runtime.
    _copy_properties(source.component, mesh_component, MESH_PROPERTIES)
    mesh_component.set_static_mesh(source.mesh)

    for index, material in enumerate(_get(source.component, "override_materials") or []):
        if material is not None:
            mesh_component.set_material(index, material)

    _finish(new_actor, source, folder_path)
    return new_actor


def _spawn_light(source, folder_path):
    subsystem = _actor_subsystem()
    transform = source.world_transform

    new_actor = subsystem.spawn_actor_from_class(
        _light_actor_class(source.component), transform.translation, transform.rotation.rotator())
    if new_actor is None:
        return None

    light_component = new_actor.get_component_by_class(unreal.LightComponent)
    _copy_properties(source.component, light_component, LIGHT_PROPERTIES)

    _finish(new_actor, source, folder_path)
    return new_actor


def _spawn_blueprint(source, folder_path):
    """Place the nested Blueprint as its own actor, left whole."""
    transform = source.world_transform

    new_actor = _actor_subsystem().spawn_actor_from_class(
        source.actor_class, transform.translation, transform.rotation.rotator())
    if new_actor is None:
        return None

    _finish(new_actor, source, folder_path)
    return new_actor


def explode_actor(actor, delete_original=False, include_instances=True,
                  include_lights=True, keep_blueprints=True, folder_root=""):
    """Explode a single actor. Returns the list of spawned actors."""
    label = actor.get_actor_label()
    meshes, lights, blueprints, skipped = collect_sources(
        actor, include_instances=include_instances, include_lights=include_lights,
        keep_blueprints=keep_blueprints)

    if not meshes and not lights and not blueprints:
        _warn("%s has nothing to explode - skipped." % label)
        return []

    base = folder_root or str(actor.get_folder_path() or "")
    folder_path = "%s/%s_Exploded" % (base, label) if base else "%s_Exploded" % label

    spawned = []
    for source in meshes:
        new_actor = _spawn_mesh(source, folder_path)
        if new_actor is not None:
            spawned.append(new_actor)
    for source in lights:
        new_actor = _spawn_light(source, folder_path)
        if new_actor is not None:
            spawned.append(new_actor)
    for source in blueprints:
        new_actor = _spawn_blueprint(source, folder_path)
        if new_actor is not None:
            spawned.append(new_actor)

    _log("%s -> %d mesh actor(s) + %d light(s) + %d blueprint(s) in folder %s."
         % (label, len(meshes), len(lights), len(blueprints), folder_path))
    for entry in skipped:
        _warn("  not converted: %s" % entry)

    if delete_original:
        _actor_subsystem().destroy_actor(actor)
        _log("  original actor %s deleted." % label)

    return spawned


def explode_selected(delete_original=False, include_instances=True,
                     include_lights=True, keep_blueprints=True, folder_root=""):
    """Explode every actor currently selected in the level editor."""
    subsystem = _actor_subsystem()
    selected = subsystem.get_selected_level_actors()

    if not selected:
        _warn("Nothing selected in the level editor.")
        return []

    spawned = []
    with unreal.ScopedEditorTransaction("Explode Blueprint Actors"):
        for actor in selected:
            spawned.extend(explode_actor(
                actor,
                delete_original=delete_original,
                include_instances=include_instances,
                include_lights=include_lights,
                keep_blueprints=keep_blueprints,
                folder_root=folder_root,
            ))

    if spawned:
        subsystem.set_selected_level_actors(spawned)
    _log("Done: %d actor(s) created from %d selected actor(s)." % (len(spawned), len(selected)))
    return spawned


def explode_selected_and_delete():
    return explode_selected(delete_original=True)


def explode_selected_meshes_only():
    return explode_selected(include_lights=False)


def explode_selected_flattened():
    """Take nested Blueprints apart as well, instead of keeping them whole."""
    return explode_selected(keep_blueprints=False)


def preview_selected():
    """Log what would be created, without touching the level."""
    subsystem = _actor_subsystem()
    selected = subsystem.get_selected_level_actors()
    if not selected:
        _warn("Nothing selected in the level editor.")
        return

    for actor in selected:
        meshes, lights, blueprints, skipped = collect_sources(actor)
        _log("%s: %d mesh(es) + %d light(s) + %d blueprint(s) would be created."
             % (actor.get_actor_label(), len(meshes), len(lights), len(blueprints)))
        for source in meshes:
            _log("  %s <- %s" % (source.label, source.mesh.get_path_name()))
        for source in lights:
            _log("  %s <- %s" % (source.label, source.component.get_class().get_name()))
        for source in blueprints:
            _log("  %s <- blueprint %s" % (source.label, source.actor_class.get_name()))
        for entry in skipped:
            _warn("  not converted: %s" % entry)
