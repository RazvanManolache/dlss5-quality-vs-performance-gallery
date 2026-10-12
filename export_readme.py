"""Verify sustained preset timing, curate real captures, and refresh the README table."""
import argparse,hashlib,json,re,shutil
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw,ImageFont
from gallery_catalog import ROOT

LABELS=['Very light','Middle','Beauty','Light + detail','Light + detail + soft shadows','Room bounce + contact','Sparse GI + reflections']
MODE_LABELS={'off':'Off','protected':'Protected surfaces','full_frame':'Full frame'}

def digest(p):
 with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()

def main():
 ap=argparse.ArgumentParser(description=__doc__)
 for k in ('run','build','screenshots','output'):ap.add_argument('--'+k,type=Path,required=True)
 a=ap.parse_args();run=a.run.resolve();build=a.build.resolve();screens=a.screenshots.resolve();out=a.output.resolve()
 r=json.loads((run/'run.json').read_text());protocol=r['preset_benchmark']
 assert r['passed'] and protocol['passed'] and protocol['moving_people'] and protocol['windows_hidden'] and not protocol['presentation'] and not protocol['capture_during_timing']
 assert r['kernel_count']==41 and not r['proprietary_dlss_dll_loaded']
 for f in r['frames']:
  assert f['own_launches']==(154 if f['network_enabled'] else 0) and f['d3d12_errors']==0 and f['exact_neural_resolution']
  assert f['width']==1920 and f['height']==1080 and f['character_detail']==0
 dependency=json.loads((build/'phoenix_dependency.json').read_text());old=json.loads((screens/'verification.json').read_text())
 assert old['passed'] and old['phoenix_commit']==dependency['commit']
 manifest=json.loads((build/'source_manifest.json').read_text())
 assert all(digest(Path(f['path']))==f['sha256'] for f in manifest['files'])
 stats=[]
 for q in range(7):
  for mode in MODE_LABELS:
   fs=[f for f in r['frames'] if f.get('measured') and f.get('quality_index')==q and f.get('phoenix_mode')==mode]
   assert len(fs)==144
   assert all(sum(f['repeat']==i for f in fs)==48 for i in range(3))
   wall=float(np.mean([f['frame_wall_ms'] for f in fs]));gpu=float(np.mean([f['processing_gpu_ms'] for f in fs]))
   stats.append({'quality_index':q,'quality':fs[0]['quality'],'label':LABELS[q],'mode':mode,'fps':1000/wall,'frame_wall_ms':wall,'gpu_ms':gpu,
    'network_gpu_ms':float(np.mean([f['network_gpu_ms'] for f in fs])),
    'repeat_fps':[1000/float(np.mean([f['frame_wall_ms'] for f in fs if f['repeat']==i])) for i in range(3)],
    'gpu_p95_ms':float(np.percentile([f['processing_gpu_ms'] for f in fs],95))})
 stats.sort(key=lambda s:s['fps'],reverse=True)
 out.mkdir(parents=True,exist_ok=False);(out/'images').mkdir();(out/'thumbnails').mkdir();sources=[]
 prefix=out.relative_to(ROOT).as_posix();source_hashes=json.loads((screens/'files-sha256.json').read_text())
 table=['| Quality preset | DLSS5 | FPS¹ ↓ | GPU ms | Frame ms¹ | Room | Face detail |','| --- | --- | ---: | ---: | ---: | --- | --- |']
 for s in stats:
  values=[s['label'],MODE_LABELS[s['mode']],f"{s['fps']:.1f}",f"{s['gpu_ms']:.2f}",f"{s['frame_wall_ms']:.2f}"]
  if s['mode']!='off':values=[f'**{v}**' for v in values];values[1]='🟣 '+values[1]
  for view,label in [('room0','Room view'),('person11_face','Face detail')]:
   file=s['quality']+'__'+s['mode']+'.png';source=screens/'images'/view/file
   assert digest(source)==source_hashes[source.relative_to(screens).as_posix()]
   name=view+'__'+source.stem+'.jpg';full=out/'images'/name;thumb=out/'thumbnails'/name
   with Image.open(source) as im:
    assert im.size==(1920,1080);im.save(full,quality=96,subsampling=0)
    # A fixed crop of the actual face-camera render, equal for all 21 rows.
    crop=(500,230,1230,960) if view=='person11_face' else (0,0,1920,1080)
    im.crop(crop).resize((360,360) if view=='person11_face' else (360,203),Image.Resampling.LANCZOS).save(thumb,quality=92,subsampling=0)
   alt=label+': '+s['label']+', DLSS5 '+MODE_LABELS[s['mode']]
   values.append(f'<a href="{prefix}/images/{name}"><img src="{prefix}/thumbnails/{name}" width="180" alt="{alt}"></a>')
   sources.append({'panel':'table','view':view,'quality':s['quality'],'mode':s['mode'],'thumbnail_crop':crop,'source_png':str(source),'source_sha256':digest(source)})
  table.append('| '+' | '.join(values)+' |')
 font=ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',25);small=ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',22)
 panels=[('scene','room0',[(1,'off'),(3,'full_frame'),(2,'off')]),
         ('clothing','person0_body',[(3,'off'),(3,'full_frame'),(2,'off')]),
         ('face','person11_face',[(3,'off'),(3,'protected'),(3,'full_frame')])]
 for name,view,choices in panels:
  if name=='scene':choices.sort(key=lambda c:next(s['fps'] for s in stats if s['quality_index']==c[0] and s['mode']==c[1]),reverse=True)
  panel=Image.new('RGB',(2520,552),(17,23,31));draw=ImageDraw.Draw(panel)
  for col,(q,mode) in enumerate(choices):
   s=next(s for s in stats if s['quality_index']==q and s['mode']==mode);source=screens/'images'/view/(s['quality']+'__'+mode+'.png')
   with Image.open(source) as im:
    assert im.size==(1920,1080);im.save(out/'images'/(view+'__'+source.stem+'.jpg'),quality=96,subsampling=0)
    panel.paste(im.resize((830,467),Image.Resampling.LANCZOS),(col*840+5,80))
   color='#bd91ff' if mode!='off' else '#a8b8cb'
   draw.rectangle((col*840,0,col*840+839,551),outline=color,width=4)
   draw.text((col*840+16,9),LABELS[q],font=font,fill='white')
   label='DLSS5 ON · '+MODE_LABELS[mode] if mode!='off' else 'DLSS5 off'
   if name=='scene':label+=f"  |  {s['fps']:.1f} FPS¹"
   draw.text((col*840+16,43),label,font=small,fill=color)
   sources.append({'panel':name,'view':view,'quality':s['quality'],'mode':mode,'source_png':str(source),'source_sha256':digest(source)})
  panel.save(out/(name+'.jpg'),quality=95,subsampling=0)
 verification={'passed':True,'hardware':'NVIDIA GeForce RTX 5080','resolution':[1920,1080],'measured_frames':3024,'repeats':3,'conditions':21,
  'fps_definition':'1000 / mean measured single-view offscreen frame wall time in ms, including CPU/driver work and GPU fence; excludes presentation and capture.',
  'moving_people':True,'full_body_meshes':True,'d3d12_errors':0,'windows_hidden':True,'presentation':False,'capture_during_timing':False,
  'phoenix_commit':dependency['commit'],'timing_build':build.name,'screenshot_build':old['build'],'run_sha256':digest(run/'run.json'),
  'timing_build_manifest_sha256':digest(build/'source_manifest.json'),'screenshots_verification_sha256':digest(screens/'verification.json'),
  'selected_sources':sources,'exporter_sha256':digest(Path(__file__)),'limits':'Room-camera FPS applies to the table and room panel. Character close-ups are appearance examples, without measured close-up FPS.'}
 (out/'measurements.json').write_text(json.dumps(stats,indent=2)+'\n',encoding='utf-8');(out/'verification.json').write_text(json.dumps(verification,indent=2)+'\n',encoding='utf-8')
 (out/'table.md').write_text(('\n'.join(table)+'\n').replace(prefix+'/',''),encoding='utf-8');shutil.copy2(build/'source_manifest.json',out/'build-manifest.json')
 readme=ROOT/'README.md';text=readme.read_text(encoding='utf-8');text,n=re.subn(r'<!-- PERFORMANCE_TABLE -->.*?<!-- /PERFORMANCE_TABLE -->','<!-- PERFORMANCE_TABLE -->\n'+'\n'.join(table)+'\n<!-- /PERFORMANCE_TABLE -->',text,flags=re.S);assert n==1
 readme.write_text(text,encoding='utf-8')
 (out/'files-sha256.json').write_text(json.dumps({p.relative_to(out).as_posix():digest(p) for p in sorted(out.rglob('*')) if p.is_file()},indent=2)+'\n',encoding='utf-8')
 print('Published 21 sorted timing rows, 42 linked table thumbnails, and three comparison panels:',out)

if __name__=='__main__':main()
