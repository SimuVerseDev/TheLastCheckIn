import bpy, json, math, os
from mathutils import Vector
root = r'C:/Users/user/Documents/GitHub/TheLastCheckIn/output/ghost_head'
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
bpy.ops.import_scene.fbx(filepath=os.path.join(root, 'GhostHead.fbx'))
meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
assert len(meshes) == 1, f'Expected one mesh, got {len(meshes)}'
o = meshes[0]
assert len(o.data.vertices) > 10000
assert all(math.isfinite(c) for v in o.data.vertices for c in v.co)
assert len(o.data.uv_layers) > 0
assert not any(x.type == 'ARMATURE' for x in bpy.context.scene.objects)
points = [o.matrix_world @ Vector(c) for c in o.bound_box]
extent = [max(p[i] for p in points)-min(p[i] for p in points) for i in range(3)]
assert .2 < extent[2] < .6, extent
result = {'fbx_reimport': 'PASS', 'mesh_count': len(meshes), 'vertices':len(o.data.vertices),
          'triangles':sum(len(p.vertices)-2 for p in o.data.polygons),
          'materials':[m.name for m in o.data.materials], 'dimensions_m':extent,
          'uv_layers':len(o.data.uv_layers), 'rigged':False}
with open(os.path.join(root,'validation.json'), 'w', encoding='utf-8') as f:
    json.dump(result,f,indent=2)
print(json.dumps(result))
