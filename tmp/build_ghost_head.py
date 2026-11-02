import bpy, math, random, os, json
from mathutils import Vector
from math import sin, cos, pi, exp, sqrt

OUT = r'C:/Users/user/Documents/GitHub/TheLastCheckIn/output/ghost_head'
os.makedirs(OUT, exist_ok=True)
random.seed(24)
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)

def material(name, color, roughness=.5, metallic=0):
    m=bpy.data.materials.new(name); m.use_nodes=True
    p=m.node_tree.nodes.get('Principled BSDF')
    p.inputs['Base Color'].default_value=(*color,1)
    p.inputs['Roughness'].default_value=roughness
    p.inputs['Metallic'].default_value=metallic
    return m
skin=material('Skin_PalePorcelain',(.43,.47,.43),.58)
lip=material('Lips_AshenMauve',(.26,.19,.19),.45)
socket=material('Eyelids_Shadow',(.15,.13,.14),.65)
black=material('Eyes_Obsidian',(.0015,.002,.002),.12)
mouth=material('Mouth_Interior',(.016,.006,.009),.75)
tooth=material('Teeth_AgedIvory',(.66,.64,.49),.37)
hair=material('Hair_DeepBrown',(.013,.008,.009),.38)
hair2=material('Hair_SubtleHighlights',(.025,.015,.016),.42)
# Procedural microtexture remains editable in the Blender source.
n=skin.node_tree.nodes; noise=n.new('ShaderNodeTexNoise'); noise.inputs['Scale'].default_value=95
bump=n.new('ShaderNodeBump'); bump.inputs['Strength'].default_value=.10; bump.inputs['Distance'].default_value=.002
skin.node_tree.links.new(noise.outputs['Fac'],bump.inputs['Height'])
skin.node_tree.links.new(bump.outputs['Normal'],n.get('Principled BSDF').inputs['Normal'])

model=[]
def finish(o,mat):
    o.data.materials.append(mat); model.append(o)
    if o.type=='MESH':
        for p in o.data.polygons:p.use_smooth=True
    return o

def ellipsoid(name,loc,scale,mat,seg=48,rings=32):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=seg,ring_count=rings,location=loc)
    o=bpy.context.object;o.name=name;o.scale=scale
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    return finish(o,mat)

def mesh(name,verts,faces,mat):
    d=bpy.data.meshes.new(name);d.from_pydata(verts,[],faces);d.update()
    o=bpy.data.objects.new(name,d);bpy.context.collection.objects.link(o)
    return finish(o,mat)

def tube(name,pts,radii,mat,sides=8):
    vs=[];fs=[]
    for i,p in enumerate(pts):
        p=Vector(p);t=Vector(pts[min(i+1,len(pts)-1)])-Vector(pts[max(i-1,0)])
        t.normalize();ref=Vector((0,1,0))
        if abs(t.dot(ref))>.95:ref=Vector((1,0,0))
        u=t.cross(ref).normalized();v=t.cross(u).normalized()
        r=radii[i] if isinstance(radii,list) else radii
        for j in range(sides):vs.append(tuple(p+r*(u*cos(j*2*pi/sides)+v*sin(j*2*pi/sides))))
        if i:
            for j in range(sides):
                a=(i-1)*sides+j;b=(i-1)*sides+(j+1)%sides
                fs.append((a,b,i*sides+(j+1)%sides,i*sides+j))
    fs += [tuple(reversed(range(sides))),tuple((len(pts)-1)*sides+j for j in range(sides))]
    return mesh(name,vs,fs,mat)

def profile(z):
    q=(z-2.12)/1.22
    r=sqrt(max(0,1-q*q))
    jaw=.78+.22*min(1,max(0,(z-.9)/1.12))
    return .82*r*jaw,.66*r

def front(x,z):
    rx,ry=profile(z)
    y=-ry*sqrt(max(.02,1-(x/max(rx,.001))**2))
    for s in [-1,1]:
        y+=.09*exp(-((x-s*.34)/.25)**2-((z-2.30)/.24)**2)
        y-=.035*exp(-((x-s*.42)/.3)**2-((z-1.87)/.25)**2)
    y-=.07*exp(-(x/.13)**2-((z-2.12)/.39)**2)
    return y

verts=[];faces=[];nr=100;ns=128
for i in range(nr+1):
    z=.901+2.438*i/nr;rx,ry=profile(z)
    for j in range(ns):
        a=2*pi*j/ns;x=rx*sin(a);y=-ry*cos(a)
        if cos(a)>0:y=front(x,z)
        verts.append((x,y,z))
for i in range(nr):
    for j in range(ns):
        a=i*ns+j;b=i*ns+(j+1)%ns;faces.append((a,b,b+ns,a+ns))
faces.extend([tuple(reversed(range(ns))),tuple(nr*ns+j for j in range(ns))])
head=mesh('Head_Sculpt',verts,faces,skin)

# Short neck with a closed base, suitable for placing on a body later.
vs=[];fs=[]
for i in range(17):
    z=.40+i*.075;r=.24+.04*(i/16)+.045*exp(-((i/16-.8)/.3)**2)
    for j in range(48):
        a=j*2*pi/48;vs.append((r*cos(a),.12+r*.85*sin(a),z))
for i in range(16):
    for j in range(48):fs.append((i*48+j,i*48+(j+1)%48,(i+1)*48+(j+1)%48,(i+1)*48+j))
fs.append(tuple(reversed(range(48))));fs.append(tuple(16*48+j for j in range(48)))
mesh('Neck_ClosedBase',vs,fs,skin)

for s in [-1,1]:
    ellipsoid('Ear_'+str(s),(s*.755,.02,2.06),(.12,.10,.25),skin)
    ellipsoid('EarInner_'+str(s),(s*.80,-.065,2.07),(.06,.025,.15),lip)
    x=s*.345;z=2.30;y=front(x,z)
    ellipsoid('BlackEye_'+str(s),(x,y+.025,z),(.242,.145,.207),black)
    for upper in [True,False]:
        pts=[]
        for i in range(33):
            t=pi*i/32+(0 if upper else pi)
            xx=x+.251*cos(t);zz=z+.209*sin(t)
            yy=front(xx,zz)-.016
            pts.append((xx,yy,zz))
        tube('Lid_'+str(s)+'_'+str(upper),pts,.020 if upper else .015,socket,8)
    pts=[]
    for i in range(25):
        t=i/24;xx=s*(.13+.43*t);zz=2.64+.07*sin(t*pi)-.045*t
        pts.append((xx,front(xx,zz)-.012,zz))
    tube('Brow_'+str(s),pts,[.003+.012*sin(pi*i/24) for i in range(25)],hair2,6)

# Continuous tapered nose, blended visually into the head.
ellipsoid('Nose_Bridge',(0,-.635,2.12),(.095,.10,.285),skin)
ellipsoid('Nose_Tip',(0,-.755,1.91),(.125,.13,.105),skin)
for s in [-1,1]:
    ellipsoid('Nose_Wing'+str(s),(s*.105,-.695,1.88),(.064,.075,.063),skin)
    ellipsoid('Nostril'+str(s),(s*.079,-.754,1.853),(.034,.013,.020),mouth,24,16)

# Wide restrained grin: dark mouth opening, individual teeth, and thin lips.
vs=[];fs=[];N=64
for i in range(N+1):
    t=-1+2*i/N;x=.49*t
    for zz in [1.55+.20*t*t,1.28+.47*t*t]:
        vs.append((x,front(x,zz)-.025,zz))
for i in range(N):fs.append((2*i,2*i+1,2*i+3,2*i+2))
mesh('Grin_Interior',vs,fs,mouth)
for top in [True,False]:
    pts=[];rs=[]
    for i in range(N+1):
        t=-1+2*i/N;x=.49*t;z=(1.55+.20*t*t) if top else (1.28+.47*t*t)
        pts.append((x,front(x,z)-.040,z));rs.append(.008+.015*(1-t*t))
    tube('UpperLip' if top else 'LowerLip',pts,rs,lip,8)
for i in range(12):
    x=(i-5.5)*.073;t=x/.49;z=1.55+.20*t*t
    height=.105*(1-.45*abs(t));zc=z-height/2-.012
    bpy.ops.mesh.primitive_cube_add(size=1,location=(x,front(x,zc)-.041,zc))
    o=bpy.context.object;o.name='UpperTooth_%02d'%i;o.scale=(.065,.045,height)
    o.rotation_euler[1]=-.35*t
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    bevel=o.modifiers.new('Soft tooth edges','BEVEL');bevel.width=.011;bevel.segments=3
    bpy.context.view_layer.objects.active=o;bpy.ops.object.modifier_apply(modifier=bevel.name)
    finish(o,tooth)
for i in range(10):
    x=(i-4.5)*.069;t=x/.49;z=1.30+.47*t*t
    ellipsoid('LowerTooth_%02d'%i,(x,front(x,z)-.040,z+.023),(.029,.024,.044),tooth,20,12)

# Scalp cap and many tapered polygonal locks. No external groom dependency.
vs=[];fs=[];rings=24;segments=96
for i in range(rings+1):
    for j in range(segments):
        a=j*2*pi/segments
        # Front hairline high, back scalp extends down to the nape.
        end=.72+1.55*(1-cos(a))/2
        p=.002+(end-.002)*i/rings
        vs.append((.85*sin(p)*sin(a),-.69*sin(p)*cos(a)+.04,2.12+1.25*cos(p)))
for i in range(rings):
    for j in range(segments):
        a=i*segments+j;b=i*segments+(j+1)%segments;fs.append((a,b,b+segments,a+segments))
mesh('Hair_Scalp',vs,fs,hair)
for k in range(170):
    a=.62+(2*pi-1.24)*(k+.25*random.random())/170
    length=random.uniform(2.40,3.18);pts=[];rs=[]
    for i in range(33):
        t=i/32
        p=.10+1.47*min(t/.42,1)
        radial=sin(p)
        z=2.12+1.28*cos(p) if t<=.42 else 2.12+(3.40-length-2.12)*(t-.42)/.58
        spread=.025+.06*t
        x=(.85*radial+spread)*sin(a)+.025*sin(t*8+k)*t
        y=-(.72*radial+spread*.35)*cos(a)+.04+.015*sin(t*11+k)*t
        pts.append((x,y,z));rs.append((.013+random.random()*.006)*(sin(pi*(.06+.94*t))**.35))
    tube('HairLock_%03d'%k,pts,rs,hair2 if k%9==0 else hair,6)
# Thin forward locks crossing the forehead and face, leaving eyes and grin legible.
for k in range(22):
    x0=random.uniform(-.58,.58);s=1 if x0>0 else -1
    end=random.uniform(1.05,2.02);pts=[];rs=[]
    for i in range(29):
        t=i/28;z=3.30-(3.30-end)*t
        x=x0+s*.15*t+.04*sin(t*5+k)
        rx,ry=profile(z);x=max(-rx*.96,min(rx*.96,x))
        y=front(x,z)-.055-.055*sin(pi*t)
        pts.append((x,y,z));rs.append(.002+.006*(1-t)**.55)
    tube('FaceStrand_%02d'%k,pts,rs,hair,5)

# Real-world head size: 12 cm per design unit, feet excluded (head asset only).
for o in model:
    for v in o.data.vertices:v.co*=.12
    o.location*=.12
bpy.context.scene.unit_settings.system='METRIC'
bpy.context.scene.unit_settings.scale_length=1.0
bpy.ops.object.select_all(action='DESELECT')
for o in model:o.select_set(True)
bpy.context.view_layer.objects.active=head
# UVs for future texturing; materials are currently solid color PBR.
bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT')
bpy.ops.uv.smart_project(angle_limit=1.15192,island_margin=.01)
bpy.ops.object.mode_set(mode='OBJECT')
# Export a single mesh for straightforward Unreal static-mesh import.
# Keep the independently editable components in the Blender source.
bpy.ops.object.duplicate()
bpy.ops.object.join()
export_mesh=bpy.context.object;export_mesh.name='GhostHead'
bpy.ops.export_scene.fbx(filepath=os.path.join(OUT,'GhostHead.fbx'),use_selection=True,object_types={'MESH'},apply_unit_scale=True,apply_scale_options='FBX_SCALE_UNITS',axis_forward='-Z',axis_up='Y',bake_anim=False,add_leaf_bones=False,path_mode='AUTO',mesh_smooth_type='FACE')
bpy.ops.object.delete(use_global=False)

def track(o,target):o.rotation_euler=(Vector(target)-o.location).to_track_quat('-Z','Y').to_euler()
world=bpy.context.scene.world;world.use_nodes=True;world.node_tree.nodes['Background'].inputs[0].default_value=(.045,.05,.06,1);world.node_tree.nodes['Background'].inputs[1].default_value=.35
def area(name,loc,power,size,color):
    bpy.ops.object.light_add(type='AREA',location=loc);o=bpy.context.object;o.name=name;o.data.energy=power;o.data.shape='DISK';o.data.size=size;o.data.color=color;track(o,(0,0,.25))
area('Key',(-.55,-.6,.75),40,.5,(.82,.9,1))
area('Fill',(.5,-.4,.3),18,.35,(1,.83,.72))
area('HairRim',(.2,.35,.65),65,.35,(.63,.75,1))
bpy.ops.object.camera_add(location=(.37,-1.10,.35));cam=bpy.context.object;track(cam,(0,-.005,.235));cam.data.type='ORTHO';cam.data.ortho_scale=.49;bpy.context.scene.camera=cam
scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.samples=48
scene.render.resolution_x=1000;scene.render.resolution_y=1100;scene.render.resolution_percentage=100
scene.view_settings.view_transform='AgX';scene.render.image_settings.file_format='PNG'
scene.render.filepath=os.path.join(OUT,'GhostHead_Preview.png')
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT,'GhostHead.blend'))
bpy.ops.render.render(write_still=True)
stats={'objects':len(model),'vertices':sum(len(o.data.vertices) for o in model),'triangles':sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in model),'rigged':False,'unit':'meters','dimensions_m':{'head_height':.293,'with_hair_height':.405}}
with open(os.path.join(OUT,'model_info.json'),'w') as f:json.dump(stats,f,indent=2)
print('GHOST_HEAD_DONE',json.dumps(stats))
