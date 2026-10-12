"""Bake MIT Rocketbox rigs to stable native triangle streams with real mocap."""
import bpy, json, struct, math
import numpy as np
from pathlib import Path
from mathutils import Vector, Quaternion
ROOT=Path(__file__).resolve().parent
SRC=ROOT/'assets/rocketbox_sources/Assets'
OUT=ROOT/'assets/people';OUT.mkdir(exist_ok=True)
metadata={}
import sys
sys.path.insert(0,str(ROOT))
from gallery_catalog import people
for visitor in people():
    name,gender=visitor["name"],visitor["gender"]
    if (OUT/(name+".poses")).exists():continue
    walkFrames,idleFrames=(60,60) if visitor.get("existing") else (32,48)
    # Bake only missing derivatives. Existing validated avatars remain exact.

    bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
    bpy.ops.import_scene.fbx(filepath=str(SRC/'Avatars/Adults'/name/'Export'/(name+'.fbx')))
    arm=next(o for o in bpy.context.scene.objects if o.type=='ARMATURE')
    obj=next(o for o in bpy.context.scene.objects if o.type=='MESH')
    arm.animation_data_clear()
    origin=Vector((0,0,min((obj.matrix_world@v.co).z for v in obj.data.vertices)))
    mat_slots=[0 if '_head' in m.name else 2 if '_opacity' in m.name else 1 for m in obj.data.materials]
    clips=[];durations=[];stride=0
    for clip,directory in [('walk_neutral','xy'),(visitor['idle'],'static')]:
        before=set(bpy.context.scene.objects)
        bpy.ops.import_scene.fbx(filepath=str(SRC/'Animations'/('all_animations_max_motextr_'+directory)/(gender+'_'+clip+'_01.max.fbx')))
        added=set(bpy.context.scene.objects)-before
        source=next(o for o in added if o.type=='ARMATURE')
        action=source.animation_data.action;start,end=action.frame_range
        fps=bpy.context.scene.render.fps/bpy.context.scene.render.fps_base
        duration=(end-start)/fps
        bpy.context.scene.frame_set(int(start));bpy.context.view_layer.update()
        base=(source.matrix_world@source.pose.bones['Bip01 Pelvis'].matrix).translation.copy()
        bpy.context.scene.frame_set(int(end));bpy.context.view_layer.update()
        last=(source.matrix_world@source.pose.bones['Bip01 Pelvis'].matrix).translation.copy()
        if clip.startswith('walk'):stride=Vector((last.x-base.x,last.y-base.y,0)).length
        poses=[];bounds=[]
        frames=walkFrames if directory=="xy" else idleFrames
        for k in range(frames):
            frame=start+(end-start)*k/frames
            bpy.context.scene.frame_set(int(frame),subframe=frame-int(frame))
            for bone in arm.pose.bones:
                s=source.pose.bones.get(bone.name)
                bone.rotation_mode='QUATERNION'
                bone.location=(0,0,0);bone.scale=(1,1,1)
                if s:
                    desired=(source.matrix_world@s.matrix).to_quaternion()
                    if bone.parent and s.parent:
                        rest=bone.parent.bone.matrix_local.to_quaternion().inverted()@bone.bone.matrix_local.to_quaternion()
                        relative=(source.matrix_world@s.parent.matrix).to_quaternion().inverted()@desired
                        bone.rotation_quaternion=rest.inverted()@relative
                    else:
                        bone.rotation_quaternion=bone.bone.matrix_local.to_quaternion().inverted()@arm.matrix_world.to_quaternion().inverted()@desired
                    if not bone.parent:
                        # Keep pelvis bob, strip XY root extraction for runtime paths.
                        delta=(source.matrix_world@s.matrix).translation-base
                        delta.x=delta.y=0
                        bone.location=arm.matrix_world.to_3x3().inverted()@delta
            if clip.startswith('idle'):
                head=arm.pose.bones['Bip01 Head']
                axis=head.bone.matrix_local.to_3x3().inverted()@Vector((0,0,1))
                head.rotation_quaternion=head.rotation_quaternion@Quaternion(axis.normalized(),.13*math.sin(k/frames*2*math.pi))
            bpy.context.view_layer.update()
            evaluated=obj.evaluated_get(bpy.context.evaluated_depsgraph_get());mesh=evaluated.to_mesh()
            mesh.calc_loop_triangles();mesh.calc_tangents()
            rows=[];transform=obj.matrix_world.to_3x3()
            for tri in mesh.loop_triangles:
                slot=mat_slots[tri.material_index]
                for li in tri.loops:
                    loop=mesh.loops[li];p=obj.matrix_world@mesh.vertices[loop.vertex_index].co-origin
                    n=(transform@loop.normal).normalized();t=(transform@loop.tangent).normalized()
                    uv=mesh.uv_layers.active.data[li].uv
                    u=(min(.9995,max(.0005,uv.x))+slot)/3
                    rows.append((p.x,p.z,-p.y,n.x,n.z,-n.y,u,1-uv.y,t.x,t.z,-t.y,-loop.bitangent_sign))
            data=np.array(rows,dtype='<f4');assert np.isfinite(data).all()
            # Retargeted proportions need a small sole-height correction.
            data[:,1]-=data[:,1].min()
            bounds.append([data[:,:3].min(axis=0).tolist(),data[:,:3].max(axis=0).tolist()])
            poses.append(data);evaluated.to_mesh_clear()
        clips.append(poses);durations.append(duration)
        for o in added:bpy.data.objects.remove(o,do_unlink=True)
        metadata.setdefault(name,{})[clip]={'seconds':duration,'bounds':bounds}
    count=len(clips[0][0]);assert all(len(p)==count for clip in clips for p in clip)
    for lod in ['high','low']:
        with (OUT/(name+'_'+lod+'.mesh')).open('wb') as f:f.write(struct.pack('<I',count));f.write(clips[0][0].tobytes())
    with (OUT/(name+'.poses')).open('wb') as f:
        f.write(struct.pack('<IIIfff',walkFrames,idleFrames,count,*durations,stride))
        for clip in clips:
            for p in clip:f.write(p.tobytes())
    metadata[name].update(vertices=count,triangles=count//3,stride=stride,material_slots=mat_slots)
    print('EXPORTED',name,count,durations,stride,flush=True)
(OUT/'crowd_geometry_manifest.json').write_text(json.dumps(metadata,indent=2))
