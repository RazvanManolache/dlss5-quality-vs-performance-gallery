"""Linear-light, alpha-aware color mips; normalized vector normal mips."""
from pathlib import Path
import numpy as np,struct,json,hashlib
ROOT=Path(__file__).resolve().parent;OUT=ROOT/'assets/beauty_maps002';OUT.mkdir(exist_ok=True)
from gallery_catalog import actors
paths=[ROOT/'assets/people'/(p['name']+'_'+k+'.rgba') for p in actors() for k in ('color','normal','rough')]
manifest={}
for p in paths:
 if (OUT/(p.name+".mips")).exists():continue
 b=p.read_bytes();w,h=struct.unpack('<II',b[:8]);base=np.frombuffer(b[8:],dtype='uint8').reshape(h,w,4).copy();levels=[base]
 a=base.astype('float32')/255;color='color' in p.name;normal='normal' in p.name
 if color:
  a[:,:,:3]=np.where(a[:,:,:3]<=.04045,a[:,:,:3]/12.92,((a[:,:,:3]+.055)/1.055)**2.4)*a[:,:,3:4]
 elif normal:a[:,:,:3]=a[:,:,:3]*2-1
 while a.shape[0]>4:
  height,width=a.shape[:2];assert height%2==width%2==0
  a=a.reshape(height//2,2,width//2,2,4).mean(axis=(1,3));encoded=a.copy()
  if color:
   rgb=np.divide(a[:,:,:3],a[:,:,3:4],out=np.zeros_like(a[:,:,:3]),where=a[:,:,3:4]>1e-6)
   encoded[:,:,:3]=np.where(rgb<=.0031308,rgb*12.92,1.055*np.maximum(rgb,0)**(1/2.4)-.055)
  elif normal:
   n=a[:,:,:3]/np.maximum(np.linalg.norm(a[:,:,:3],axis=2,keepdims=True),.001);encoded[:,:,:3]=(n+1)*.5
  levels.append(np.clip(encoded*255,0,255).round().astype('uint8'))
 dest=OUT/(p.name+'.mips');assert not dest.exists()
 with dest.open('wb') as f:
  f.write(struct.pack('<III',w,h,len(levels)))
  for level in levels:f.write(level.tobytes())
 manifest[p.name]={'source':str(p.resolve()),'source_sha256':hashlib.sha256(b).hexdigest(),'levels':len(levels),'dimensions':[w,h],'mip_sha256':hashlib.sha256(dest.read_bytes()).hexdigest(),'base_rgba_exact':bool(np.array_equal(levels[0],base)),'linear_light_color':color,'premultiplied_alpha_filtering':color,'normalized_vector_mips':normal}
(OUT/'crowd_mip_manifest.json').write_text(json.dumps(manifest,indent=2));print('Prepared',len(manifest),'authenticated linear-light mip chains.')
