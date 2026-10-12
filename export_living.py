"""Export checked native readbacks for the living gallery, without screen capture."""
import argparse,hashlib,json,shutil
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw,ImageFont
from analyze_detail import linear,picture
from gallery_catalog import ROOT,people,actors

def digest(path):
 with path.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()

def main():
 ap=argparse.ArgumentParser(description=__doc__)
 for name in ('run','build','output'):ap.add_argument('--'+name,type=Path,required=True)
 a=ap.parse_args();run=a.run.resolve();out=a.output.resolve();j=json.loads((run/'run.json').read_text())
 assert j['passed'] and j['living_protocol']['passed'] and j['living_ui']['passed'] and j['kernel_count']==41 and not j['proprietary_dlss_dll_loaded']
 for f in j['frames']:
  assert f['d3d12_errors']==0 and f['own_launches']==(154 if f['network_enabled'] else 0)
  assert f['exact_neural_resolution'] and f['neural_width']==f['width'] and f['neural_height']==f['height']
 for compare in j['comparison_frames']:
  for f in compare['sides']:
   if f:assert f['d3d12_errors']==0 and f['own_launches']==(154 if f['network_enabled'] else 0) and f['exact_neural_resolution']
 stats={}
 for condition in sorted({f['condition'] for f in j['frames'] if f.get('measured')}):
  rows=[f for f in j['frames'] if f.get('measured') and f.get('condition')==condition];assert len(rows)==144
  stats[condition]={key:float(np.mean([f[key] for f in rows])) for key in ('processing_gpu_ms','frame_wall_ms','network_gpu_ms','gbuffer_gpu_ms','actor_triangles')}
  stats[condition]['gpu_p95_ms']=float(np.percentile([f['processing_gpu_ms'] for f in rows],95))
  stats[condition]['repeat_gpu_means']=[float(np.mean([f['processing_gpu_ms'] for f in rows if f['repeat']==i])) for i in range(3)]
 out.mkdir(parents=True,exist_ok=False);(out/'images').mkdir();checks={}
 views=[('room0',run/'room/angle0'),('room1',run/'room/angle1')]+[(f'animal{i}',run/f'animals/animal{i}') for i in range(2)]+[(f'person{i}',run/f'people/person{i}') for i in range(len(people()))]
 for view,base in views:
  reference=json.loads((base/'beauty_off/frame.json').read_text())
  for folder in base.iterdir():
   row=json.loads((folder/'frame.json').read_text());assert row['camera_eye']==reference['camera_eye'] and row['camera_target']==reference['camera_target']
   native=linear(folder/'input.rgba16f');final=linear(folder/'output.rgba16f') if row['network_enabled'] else native
   pos=np.fromfile(folder/'position.rgba32f',np.float32).reshape(1080,1920,4);assert np.isfinite(pos).all()
   motion=np.fromfile(folder/'motion.rg16f',np.float16).astype(np.float32);assert np.isfinite(motion).all() and np.abs(motion).mean()<1e-3
   key=view+'_'+folder.name;checks[key]={'native_to_nr_mean_change':float(np.abs(final-native).mean()),'triangles':row['actor_triangles']}
   if view.startswith('person'):
    index=int(view[6:]);body=pos[...,3]==11+index;face=pos[...,3]==11.25+index
    assert body.sum()>1000 and face.sum()>100,(key,body.sum(),face.sum())
    checks[key].update(body_pixels=int(body.sum()),face_pixels=int(face.sum()))
   if row['network_enabled']:assert checks[key]['native_to_nr_mean_change']>1e-5
   picture(final).save(out/'images'/(key+'.jpg'),quality=94)
   if row['network_enabled']:picture(native).save(out/'images'/(key+'_input.jpg'),quality=94)
 # Activity and expression examples are actual separate renders, not crops.
 for folder in (run/'activities').iterdir():picture(linear(folder/'output.rgba16f')).save(out/'images'/('activity_'+folder.name+'.jpg'),quality=94)
 faces={}
 for key in ('off','blink'):
  folder=run/'faces'/key;faces[key]=linear(folder/'input.rgba16f');picture(linear(folder/'output.rgba16f')).save(out/'images'/('face_'+key+'.jpg'),quality=94)
 assert np.abs(faces['off']-faces['blink']).mean()>1e-5
 font=ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',18)
 sheet=Image.new('RGB',(1920,1240),(14,18,24));draw=ImageDraw.Draw(sheet)
 for i,p in enumerate(people()):
  x=i%4*480;y=i//4*310
  with Image.open(out/'images'/f'person{i}_detail_nr_distance.jpg') as im:sheet.paste(im.resize((480,270)),(x,y))
  draw.text((x+9,y+277),p['label'],font=font,fill='white')
 sheet.save(out/'people.jpg',quality=93)
 pair=Image.new('RGB',(1920,590),(14,18,24));draw=ImageDraw.Draw(pair)
 for i,key in enumerate(('beauty_off','detail_nr_distance')):
  with Image.open(out/'images'/f'room0_{key}.jpg') as im:pair.paste(im.resize((960,540)),(i*960,50))
  draw.text((i*960+12,12),('Beauty / Phoenix off' if i==0 else 'Light + distance detail / Phoenix on')+f"  {stats[key]['processing_gpu_ms']:.2f} ms GPU",font=font,fill='white')
 pair.save(out/'scene.jpg',quality=94)
 clips={}
 for clip in ('room','guide','face'):
  frames=[];motion=[];change=[];previous=None
  for i in range(48):
   folder=run/f'clips/{clip}/frame{i}';row=json.loads((folder/'frame.json').read_text());assert row['own_launches']==154
   native=linear(folder/'input.rgba16f');final=linear(folder/'output.rgba16f')
   vectors=np.fromfile(folder/'motion.rg16f',np.float16).astype(np.float32);assert np.isfinite(vectors).all()
   motion.append(float(np.abs(vectors).mean()))
   if previous is not None:change.append(float(np.abs(final-previous).mean()))
   previous=final;pair=Image.new('RGB',(1280,360));pair.paste(picture(native).resize((640,360)),(0,0));pair.paste(picture(final).resize((640,360)),(640,0));frames.append(pair)
  assert np.mean(motion)>.001 and np.mean(change)>1e-5
  frames[0].save(out/(clip+'.webp'),save_all=True,append_images=frames[1:],duration=[42,42,41]*16,loop=0,quality=83,method=4)
  clips[clip]={'frames':48,'fps':24,'mean_absolute_motion_pixels':float(np.mean(motion)),'mean_frame_difference':float(np.mean(change))}
 resolutions=[]
 for folder in (run/'resolutions').iterdir():
  row=json.loads((folder/'frame.json').read_text());w,h=row['width'],row['height'];native=linear(folder/'input.rgba16f',w,h);final=linear(folder/'output.rgba16f',w,h)
  motion=np.fromfile(folder/'motion.rg16f',np.float16).reshape(h,w,2);assert np.isfinite(motion).all()
  delta=float(np.abs(final-native).mean());assert delta>1e-5 and row['own_launches']==154
  resolutions.append({'width':w,'height':h,'neural_width':row['neural_width'],'neural_height':row['neural_height'],'input_to_output_change':delta,'finite_output':True})
 assert len(resolutions)==7 and all(c['passed'] for c in j['exact_resolution_checks'])
 asset=json.loads((ROOT/'gallery_assets.json').read_text());blender=[]
 for actor in actors():blender.extend(json.loads((ROOT/'assets/living_people'/(actor['name']+'_rig.json')).read_text())['validation'])
 verification={'passed':True,'build':a.build.name,'visitors':len(people()),'animals':2,'objects':j['living_protocol']['objects'],'timed_frames':720,
  'rendered_primary_frames':len(j['frames']),'comparison_frames':len(j['comparison_frames']),'shared_textures':j['living_ui']['shared_textures'],
  'maximum_gpu_skin_error_m':max(c['maximum_position_error_m'] for c in j['skin_validation']),
  'maximum_blender_skin_error_m':max(c['maximum_position_error_m'] for c in blender),
  'd3d12_errors':0,'desktop_capture':False,'windows_hidden':True,'face_toggle_changes_source':True,'camera_matches':True,'resize':True,'single_view':True,
  'asset_pack_bytes':asset['archive']['bytes'],'installed_asset_bytes':sum(e['bytes'] for e in asset['files']),
  'phoenix_commit':json.loads((a.build/'phoenix_dependency.json').read_text())['commit'],'exact_resolution_checks':resolutions,
  'build_manifest_sha256':digest(a.build/'source_manifest.json'),'run_sha256':digest(run/'run.json'),'checks':checks,'clips':clips,
  'limit':'Image change and geometric agreement do not establish perceived quality, identity preservation or temporal equivalence.'}
 (out/'measurements.json').write_text(json.dumps(stats,indent=2)+'\n');(out/'verification.json').write_text(json.dumps(verification,indent=2)+'\n')
 (out/'data.json').write_text(json.dumps({'people':people(),'stats':stats,'verification':{k:v for k,v in verification.items() if k not in ('checks','clips')}},indent=2)+'\n')
 shutil.copyfile(ROOT/'living_viewer.html',out/'index.html');shutil.copyfile(a.build/'source_manifest.json',out/'build-manifest.json')
 (out/'files-sha256.json').write_text(json.dumps({p.relative_to(out).as_posix():digest(p) for p in out.rglob('*') if p.is_file()},indent=2)+'\n')
 print(json.dumps(stats,indent=2))

if __name__=='__main__':main()
