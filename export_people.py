"""Export full-resolution person-camera renders, not crops of gallery images."""
from pathlib import Path
import json,sys,hashlib
import numpy as np
from PIL import Image,ImageDraw,ImageFont
root=Path(__file__).resolve().parent
source=root/sys.argv[1];output=root/sys.argv[2];output.mkdir(parents=True,exist_ok=False)
r=json.loads((source/'run.json').read_text());assert r['passed'] and r['person_check']['passed']
for context in [r,r['peer']]:
 assert context['native_generated_bundle'] and context['kernel_count']==41 and not context['proprietary_dlss_dll_loaded']
 for f in context['frames']:assert f['d3d12_errors']==0 and f['own_launches']==(154 if f['network_enabled'] else 0)
names=['Woman - pink shirt','Man - striped polo','Woman - grey top'];frames=['Whole person','Upper body','Face'];data=[];hero=[]
for person in range(3):
 for framing in range(3):
  item={'person':person,'framing':framing,'name':names[person],'framing_name':frames[framing],'images':{},'gpu_ms':{},'visibility':{}}
  for pair in ['beauty','light']:
   d=source/f'person_{person}_framing_{framing}_{pair}';comparison=json.loads((d/'comparison.json').read_text());sides=comparison['sides'];assert sides[0]['camera_eye']==sides[1]['camera_eye'] and sides[0]['camera_target']==sides[1]['camera_target']
   assert sides[0]['focus_person']==person and sides[1]['focus_person']==person and sides[0]['focus_framing']==framing
   # The selected person must actually appear in the new camera, including a visible face.
   p=np.fromfile(d/'position.rgba32f',np.float32).reshape(1080,1920,4);tag=p[...,3];body=(np.floor(tag)==11+person);face=(tag==11.25+person)
   assert body.sum()>(50000 if framing==0 else 150000 if framing==1 else 100000),(d,'selected person obscured');assert face.sum()>400,(d,'selected face obscured')
   item['visibility'][pair]={'person_pixels':int(body.sum()),'face_pixels':int(face.sum())}
   item['camera_eye']=sides[0]['camera_eye'];item['camera_target']=sides[0]['camera_target']
   for side,key in [(0,pair),(1,'detail')]:
    raw=d/('right.rgba16f' if side else 'left.rgba16f');a=np.fromfile(raw,np.float16).reshape(1080,1920,4).astype(np.float32)[...,:3];assert np.isfinite(a).all()
    if key=='detail' and key in item['images']:
     reference=np.fromfile(source/f'person_{person}_framing_{framing}_beauty'/'right.rgba16f',np.float16).astype(np.float32)
     repeated=np.fromfile(raw,np.float16).astype(np.float32)
     item['repeat_detail_difference']={'max_linear_rgb':float(np.max(np.abs(reference-repeated))),'mean_linear_rgb':float(np.mean(np.abs(reference-repeated))),'limit':'Independent recurrent runs are recorded rather than assumed bit-exact.'}
     continue
    rgb=np.clip(np.where(a<=.0031308,a*12.92,1.055*np.maximum(a,0)**(1/2.4)-.055),0,1)
    image=Image.fromarray(np.uint8(rgb*255+.5));name=f'person_{person}_framing_{framing}_{key}.jpg';image.save(output/name,quality=95);item['images'][key]=name
    # These fresh camera-view samples are separate from the prior gallery benchmark.
    matching=[f['sides'][side] for f in r['comparison_frames'] if f['sides'][side] is not None and f['sides'][side]['focus_person']==person and f['sides'][side]['focus_framing']==framing and f['sides'][side]['rendering_level']==sides[side]['rendering_level'] and f['sides'][side]['network_enabled']==sides[side]['network_enabled'] and f['sides'][side]['detail_flags']==sides[side]['detail_flags'] and f['sides'][side]['material_scoped_composite']==sides[side]['material_scoped_composite'] and abs(f['sides'][side]['scene_time']-.9)<1e-5]
    item['gpu_ms'][key]=float(np.median([f['processing_gpu_ms'] for f in matching[-8:]]))
    if key=='detail':item['detail_raw_sha256']=hashlib.sha256(raw.read_bytes()).hexdigest()
    if framing==2 and key=='detail':hero.append(image.resize((640,360)))
  data.append(item)
canvas=Image.new('RGB',(1920,410),(14,18,24));draw=ImageDraw.Draw(canvas);font=ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',23)
for i,image in enumerate(hero):canvas.paste(image,(640*i,50));draw.text((640*i+14,12),names[i],font=font,fill='white')
canvas.save(output/'people.jpg',quality=94)
validation={'passed':True,'checks':r['person_check'],'views':data,'processing':'Full 1920x1080 world-camera renders; FP16 linear-to-sRGB conversion and JPEG encoding only. Hero is uniformly resized. No crops or desktop capture.','timing':'Median of the last eight corresponding warm camera-view GPU samples, not a counterbalanced performance benchmark.'}
(output/'people.json').write_text(json.dumps(validation,indent=2))
print('Passed:',len(data),'camera views,',len(list(output.glob('*.jpg'))),'images; selected visitor and face visible in every view')
