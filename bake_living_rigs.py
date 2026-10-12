"""Prepare compact Rocketbox skeletons, real facial shapes and face-preserving LODs.

Run with Blender --background --factory-startup. Derived LODs use Blender's
decimator; the source FBXs contain the high mesh only, not authored LOD meshes.
"""
import bpy, json, math, struct, sys
import numpy as np
from pathlib import Path
from mathutils import Matrix, Vector, Quaternion
ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT))
from gallery_catalog import actors
SRC=ROOT/'assets/rocketbox_sources/Assets'
OUT=ROOT/'assets/living_people';OUT.mkdir(exist_ok=True)
C=Matrix(((1,0,0,0),(0,0,1,0),(0,-1,0,0),(0,0,0,1)))

def export_actor(actor):
 name=actor['name'];human=actor['role']!='animal'
 if (OUT/(name+'_rig.json')).exists():return
 bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
 folder=SRC/actor['category']/name/'Export'
 bpy.ops.import_scene.fbx(filepath=str(folder/(name+('_facial' if human else '')+'.fbx')))
 arm=next(o for o in bpy.context.scene.objects if o.type=='ARMATURE')
 obj=next(o for o in bpy.context.scene.objects if o.type=='MESH')
 arm.animation_data_clear()
 for p in arm.pose.bones:p.matrix_basis=Matrix.Identity(4)
 bpy.context.view_layer.update()
 origin=Vector((0,0,min((obj.matrix_world@v.co).z for v in obj.data.vertices)))
 native=C@Matrix.Translation(-origin)
 world=native@obj.matrix_world
 skeleton=native@arm.matrix_world
 inv_skeleton=skeleton.inverted()
 bones=list(arm.data.bones);indices={b.name:i for i,b in enumerate(bones)}
 slots=[0 if '_head' in m.name else 2 if ('_opacity' in m.name or '_equipment' in m.name) else 1 for m in obj.data.materials]
 if not human:slots=[1]*len(slots)
 groups={g.index:indices[g.name] for g in obj.vertex_groups if g.name in indices}
 keys=obj.data.shape_keys
 shape_names=[['AK_09_EyeBlinkLeft','AK_10_EyeBlinkRight'],['AK_44_MouthSmileLeft','AK_45_MouthSmileRight'],['AK_25_JawOpen']]
 morph=np.zeros((len(obj.data.vertices),3,6),dtype=np.float32)
 obj.data.calc_loop_triangles();obj.data.calc_tangents()
 if human:
  assert keys and all(k in keys.key_blocks for channel in shape_names for k in channel),name
  basis=keys.key_blocks[0]
  for ch,channel in enumerate(shape_names):
   for key in channel:
    for i,v in enumerate(keys.key_blocks[key].data):
     morph[i,ch,:3]+=np.array(world.to_3x3()@(v.co-basis.data[i].co))
 # Preserve complete original head/hair triangles at all lower body levels.
 streams=[];lod_counts=[];lod_offsets=[]
 for lod,ratio in enumerate((1,.55,.28,.1)):
  parts=[(obj,True)] if not lod else []
  if lod:
   face=obj.copy();face.data=obj.data.copy();bpy.context.collection.objects.link(face)
   body=obj.copy();body.data=obj.data.copy();bpy.context.collection.objects.link(body)
   for part,keep_head in ((face,True),(body,False)):
    bpy.context.view_layer.objects.active=part;part.select_set(True)
    if part.data.shape_keys:part.shape_key_clear()
    import bmesh
    bm=bmesh.new();bm.from_mesh(part.data)
    # Hair opacity stays high too; avoid transparent hair changing silhouette.
    remove=[f for f in bm.faces if ((slots[f.material_index] in (0,2)) != keep_head)]
    bmesh.ops.delete(bm,geom=remove,context='FACES');bm.to_mesh(part.data);bm.free()
   dec=body.modifiers.new('Body detail reduction','DECIMATE');dec.ratio=ratio
   # Disable armature while applying only decimation to the bind pose.
   for modifier in body.modifiers:
    if modifier.type=='ARMATURE':modifier.show_viewport=False
   bpy.context.view_layer.objects.active=body
   bpy.ops.object.modifier_apply(modifier=dec.name)
   parts=[(face,True),(body,False)] if human else [(body,False)]
   # For animals, all polygons are body, so the same reducer covers the animal.
  rows=[];skin=[]
  for part,has_morph in parts:
   mesh=part.data;mesh.calc_loop_triangles();mesh.calc_tangents()
   # Head copies retain original coordinates; recover exact original vertex IDs.
   original={tuple(v.co):v.index for v in obj.data.vertices} if has_morph else {}
   for tri in mesh.loop_triangles:
    slot=slots[tri.material_index]
    for li in tri.loops:
     loop=mesh.loops[li];v=mesh.vertices[loop.vertex_index]
     p=world@v.co;n=(world.to_3x3()@loop.normal).normalized();t=(world.to_3x3()@loop.tangent).normalized()
     uv=mesh.uv_layers.active.data[li].uv
     vertex=(*p,*n,(min(.9995,max(.0005,uv.x))+slot)/3,1-uv.y,*t,-loop.bitangent_sign)
     weights=sorted([(groups[g.group],g.weight) for g in v.groups if g.group in groups and g.weight>1e-6],key=lambda a:-a[1])[:4]
     if not weights:weights=[(0,1)]
     total=sum(w for _,w in weights);weights=[(i,w/total) for i,w in weights]+[(0,0)]*(4-len(weights))
     delta=morph[original[tuple(v.co)]].ravel() if has_morph and tuple(v.co) in original else np.zeros(18,dtype='<f4')
     rows.append(vertex)
     skin.append(struct.pack('<12f4I4f18f',*vertex,*[i for i,w in weights],*[w for i,w in weights],*delta))
  data=np.array(rows,dtype='<f4');assert len(data)>0 and np.isfinite(data).all(),name
  with (OUT/f'{name}_lod{lod}.mesh').open('wb') as f:f.write(struct.pack('<I',len(data)));f.write(data.tobytes())
  lod_offsets.append(sum(lod_counts));lod_counts.append(len(data));streams.extend(skin)
  if lod:
   bpy.data.objects.remove(face,do_unlink=True);bpy.data.objects.remove(body,do_unlink=True)
 with (OUT/(name+'.skin')).open('wb') as f:f.write(b''.join(streams))
 frames=[];clips=[];stride=1;validation=[]
 for clip,directory in ([('walk_neutral','xy'),(actor['activity'],'static')] if human else [('procedural_idle',None)]):
  added=set();source=None
  if human:
   before=set(bpy.context.scene.objects)
   folder=SRC/'Animations'/('all_animations_max_motextr_'+directory)
   path=folder/(actor['gender']+'_'+clip+'_01.max.fbx')
   if not path.exists():path=folder/(actor['gender']+'_'+clip+'.max.fbx')
   bpy.ops.import_scene.fbx(filepath=str(path));added=set(bpy.context.scene.objects)-before
   source=next(o for o in added if o.type=='ARMATURE')
   start,end=source.animation_data.action.frame_range
   duration=(end-start)/(bpy.context.scene.render.fps/bpy.context.scene.render.fps_base)
   bpy.context.scene.frame_set(int(start));bpy.context.view_layer.update()
   base=(source.matrix_world@source.pose.bones['Bip01 Pelvis'].matrix).translation.copy()
   bpy.context.scene.frame_set(int(end));bpy.context.view_layer.update()
   last=(source.matrix_world@source.pose.bones['Bip01 Pelvis'].matrix).translation.copy()
   if directory=='xy':stride=Vector((last.x-base.x,last.y-base.y,0)).length
  else:start,end,duration=0,96,4
  count=48;offset=len(frames)
  for k in range(count):
   frame=start+(end-start)*k/(count if directory=='xy' or not human else count-1)
   bpy.context.scene.frame_set(int(frame),subframe=frame-int(frame))
   for bone in arm.pose.bones:
    bone.rotation_mode='QUATERNION';bone.location=(0,0,0);bone.scale=(1,1,1);bone.rotation_quaternion=Quaternion()
    s=source.pose.bones.get(bone.name) if source else None
    if s:
     desired=(source.matrix_world@s.matrix).to_quaternion()
     if bone.parent and s.parent:
      rest=bone.parent.bone.matrix_local.to_quaternion().inverted()@bone.bone.matrix_local.to_quaternion()
      relative=(source.matrix_world@s.parent.matrix).to_quaternion().inverted()@desired
      bone.rotation_quaternion=rest.inverted()@relative
     else:bone.rotation_quaternion=bone.bone.matrix_local.to_quaternion().inverted()@arm.matrix_world.to_quaternion().inverted()@desired
     if not bone.parent:
      delta=(source.matrix_world@s.matrix).translation-base;delta.x=delta.y=0
      bone.location=arm.matrix_world.to_3x3().inverted()@delta
    elif not human:
     angle=(.12 if 'tail' in bone.name.lower() else .055)*math.sin(k/count*2*math.pi)
     if 'head' in bone.name.lower() or 'tail' in bone.name.lower():bone.rotation_quaternion=Quaternion((0,1,0),angle)
   bpy.context.view_layer.update()
   evaluated=obj.evaluated_get(bpy.context.evaluated_depsgraph_get());deformed=evaluated.to_mesh()
   ground=min((native@obj.matrix_world@v.co).y for v in deformed.vertices)
   matrices=[Matrix.Translation((0,-ground,0))@skeleton@arm.pose.bones[b.name].matrix@b.matrix_local.inverted()@inv_skeleton for b in bones]
   frames.append(np.array(matrices,dtype='<f4'))
   # Compare compact four-weight skinning to Blender, sampling 100 source vertices.
   if k==count//2:
    errors=[]
    for v in list(obj.data.vertices)[::max(1,len(obj.data.vertices)//100)]:
     weighted=sorted([(groups[g.group],g.weight) for g in v.groups if g.group in groups],key=lambda a:-a[1])[:4]
     total=sum(w for i,w in weighted)
     if not total:continue
     p=world@v.co;actual=sum(((matrices[i]@p)*(w/total) for i,w in weighted),Vector((0,0,0)))
     expected=native@obj.matrix_world@deformed.vertices[v.index].co;expected.y-=ground
     errors.append((actual-expected).length)
    validation.append({'clip':clip,'maximum_position_error_m':max(errors),'sampled_vertices':len(errors)})
   evaluated.to_mesh_clear()
  clips.append({'name':clip,'offset':offset,'frames':count,'seconds':duration})
  for o in added:bpy.data.objects.remove(o,do_unlink=True)
 if not human:clips.append(dict(clips[0]))
 np.array(frames,dtype='<f4').tofile(OUT/(name+'.bones'))
 head=next((b for b in bones if b.name=='Bip01 Head' or 'head' in b.name.lower()),bones[0])
 anchor=skeleton@head.head_local
 # Eye level, slightly above the neck pivot; use actual animated head bone.
 anchor.y+=.11 if human else .02
 hands={}
 for hand in ('Bip01 L Hand','Bip01 R Hand'):
  if hand in indices:hands[hand]={'bone':indices[hand],'rest_matrix':np.array(skeleton@arm.data.bones[hand].matrix_local).tolist()}
 info={'name':name,'role':actor['role'],'bone_count':len(bones),'bone_names':[b.name for b in bones],
       'skin_stride':152,'lod_counts':lod_counts,'lod_offsets':lod_offsets,'clips':clips,'stride':stride,
       'head_bone':indices[head.name],'head_anchor':list(anchor),'hands':hands,
       'facial_shapes':shape_names if human else [],'lod_method':'Blender decimation of body; original head and hair retained',
       'validation':validation}
 assert max(v['maximum_position_error_m'] for v in validation)<.025,(name,validation)
 (OUT/(name+'_rig.json')).write_text(json.dumps(info,indent=2)+'\n')
 print('LIVING_EXPORTED',name,lod_counts,len(bones),validation,flush=True)

for actor in actors():export_actor(actor)
