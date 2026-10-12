"""Publish and verify the complete native screenshot matrix, without desktop capture."""
import argparse,hashlib,json,shutil
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw,ImageFont
from gallery_catalog import ROOT,people

QUALITIES=['Very light','Middle','Beauty','Light + detail','Light + detail + soft shadows','Room bounce + contact','Sparse GI + reflections']
MODES={'off':'Phoenix off','protected':'Phoenix on · protected surfaces','full_frame':'Phoenix on · full frame'}

def digest(path):
 with path.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()

def main():
 ap=argparse.ArgumentParser(description=__doc__)
 for key in ('run','build','output'):ap.add_argument('--'+key,type=Path,required=True)
 a=ap.parse_args();run=a.run.resolve();out=a.output.resolve();build=a.build.resolve()
 result=json.loads((run/'run.json').read_text());matrix=result['screenshot_matrix']
 assert result['passed'] and matrix['passed'] and matrix['windows_hidden'] and not matrix['presentation'] and not matrix['desktop_capture']
 assert not result['proprietary_dlss_dll_loaded'] and result['kernel_count']==41
 rows=matrix['captures'];assert len(rows)==714
 for frame in result['frames']:
  assert frame['d3d12_errors']==0 and frame['own_launches']==(154 if frame['network_enabled'] else 0) and frame['exact_neural_resolution']
 manifest=json.loads((build/'source_manifest.json').read_text())
 assert all(digest(Path(f['path']))==f['sha256'] for f in manifest['files'])
 views=[{'key':'room0','label':'Gallery overview'},{'key':'room1','label':'Side aisle and seating'}]
 for i,p in enumerate(people()):
  for frame,label in [('body','Upper body'),('face','Face detail')]:views.append({'key':f'person{i}_{frame}','label':p['label']+' · '+label})
 assert len(views)==34
 out.mkdir(parents=True,exist_ok=False);(out/'sheets').mkdir();(out/'metadata').mkdir()
 font=ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',17)
 comparisons={};images={};hashes={}
 for v in views:
  group=[f for f in rows if f['view']==v['key']];assert len(group)==21
  assert len({(f['quality_index'],f['phoenix_mode']) for f in group})==21
  assert all(f['camera_eye']==group[0]['camera_eye'] and f['camera_target']==group[0]['camera_target'] and f['scene_time']==8 and f['character_detail']==0 for f in group)
  sheet=Image.new('RGB',(1440,7*310+44),(15,19,25));draw=ImageDraw.Draw(sheet);draw.text((12,12),v['label']+' · matched native 1920 × 1080 captures',font=font,fill='white')
  for q in range(7):
   native=None
   for m,mode in enumerate(MODES):
    row=next(f for f in group if f['quality_index']==q and f['phoenix_mode']==mode)
    assert row['network_enabled']==(mode!='off') and row['material_scoped_composite']==(mode=='protected')
    assert row['width']==1920 and row['height']==1080
    source=run/row['image'];dest=out/row['image'];dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,dest)
    with Image.open(dest) as im:
     assert im.size==(1920,1080) and im.mode=='RGB';pixels=np.asarray(im).astype(np.float32)
     assert pixels.std()>2,(v,q,mode,'Blank capture')
     if mode=='off':native=pixels
     else:
      change=float(np.abs(pixels-native).mean());assert change>.001,(v,q,mode,'No visible output change')
      comparisons[f"{v['key']}/{q}/{mode}"]=change
     thumb=im.resize((480,270),Image.Resampling.LANCZOS)
     thumbnail=Path('thumbnails')/v['key']/(dest.stem+'.jpg');(out/thumbnail).parent.mkdir(parents=True,exist_ok=True);thumb.save(out/thumbnail,quality=90)
     x,y=m*480,q*310+44;sheet.paste(thumb,(x,y));draw.text((x+8,y+275),QUALITIES[q]+' · '+('Off' if m==0 else 'Protected' if m==1 else 'Full frame'),font=font,fill='white')
    key=f"{v['key']}/{q}/{mode}";images[key]={'image':row['image'],'thumbnail':thumbnail.as_posix()}
    (out/'metadata'/(v['key']+f'__{q}__{mode}.json')).write_text(json.dumps(row,indent=2)+'\n')
   del native
  sheet.save(out/'sheets'/(v['key']+'.jpg'),quality=94)
  print('Verified',v['key'],flush=True)
 verification={'passed':True,'screenshots':len(rows),'views':34,'quality_presets':7,'phoenix_modes':3,'resolution':[1920,1080],'warmup_frames_per_capture':12,
  'matched_cameras_and_poses':True,'body_detail':'Full meshes for every preset; facial animation enabled, pose frozen at 8 seconds.',
  'rendered_frames':len(result['frames']),'d3d12_errors':0,'desktop_capture':False,'presentation':False,'windows_hidden':True,
  'display_transfer':'The same display shader and sRGB conversion as the application. Lossless 24-bit PNG, no labels in full images.',
  'phoenix_commit':json.loads((build/'phoenix_dependency.json').read_text())['commit'],'build':build.name,
  'run_sha256':digest(run/'run.json'),'build_manifest_sha256':digest(build/'source_manifest.json'),'mean_absolute_display_change_8bit':comparisons,
  'limits':'Frozen stills after 12 recurrent warmup frames; these captures do not measure FPS, temporal stability, or perceptual equivalence.'}
 (out/'verification.json').write_text(json.dumps(verification,indent=2)+'\n')
 (out/'data.json').write_text(json.dumps({'views':views,'qualities':QUALITIES,'modes':MODES,'images':images,'verification':{k:v for k,v in verification.items() if k!='mean_absolute_display_change_8bit'}},indent=2)+'\n')
 shutil.copy2(ROOT/'screenshot_viewer.html',out/'index.html');shutil.copy2(build/'source_manifest.json',out/'build-manifest.json')
 (out/'files-sha256.json').write_text(json.dumps({p.relative_to(out).as_posix():digest(p) for p in sorted(out.rglob('*')) if p.is_file()},indent=2)+'\n')
 print(f'Published {len(rows)} PNGs in {out}',flush=True)

if __name__=='__main__':main()
