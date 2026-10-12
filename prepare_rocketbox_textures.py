from pathlib import Path
from PIL import Image
import numpy as np,struct,json
ROOT=Path(__file__).resolve().parent;OUT=ROOT/'assets/people';OUT.mkdir(exist_ok=True)
meta={}
from gallery_catalog import actors
for visitor in actors():
    name=visitor['name']
    if all((OUT/(name+'_'+kind+'.rgba')).exists() for kind in ('color','normal','rough')):continue
    source=ROOT/'assets/rocketbox_sources/Assets'/visitor['category']/name/'Textures'
    animal=visitor['role']=='animal'
    prefix=next(source.glob('*_color.tga' if animal else '*_head_color.tga')).name.split('_color')[0] if animal else next(source.glob('*_head_color.tga')).name.split('_head')[0]
    for kind,size in [('color',2048),('normal',1024),('rough',1024)]:
        atlas=Image.new('RGBA',(size*3,size))
        for slot,part in enumerate(['head','body','opacity']):
            suffix='specular' if kind=='rough' else kind
            path=source/(prefix+'_'+suffix+'.tga' if animal else prefix+'_'+part+'_'+suffix+'.tga')
            if not path.exists() and part=='opacity':path=source/(prefix+'_equipment_'+suffix+'.tga')
            if path.exists():im=Image.open(path).convert('RGBA').resize((size,size),Image.Resampling.LANCZOS)
            else:im=Image.new('RGBA',(size,size),(128,128,255,255) if kind=='normal' else (170,170,170,255))
            if kind=='rough':
                a=np.array(im);rough=np.clip(190-a[:,:,:3].mean(axis=2)*.45,70,210).astype('uint8');a[:,:,:3]=rough[:,:,None];a[:,:,3]=255;im=Image.fromarray(a)
            if kind!='color' or part!='opacity':im.putalpha(255)
            atlas.paste(im,(slot*size,0))
        with (OUT/(name+'_'+kind+'.rgba')).open('wb') as f:f.write(struct.pack('<II',*atlas.size));f.write(atlas.tobytes())
        if kind=='color':
            atlas.resize((1536,512)).save(OUT/(name+'_atlas_preview.png'))
    meta[name]={'texture_prefix':prefix,'license':'MIT','color_pixels':[6144,2048],'normal_rough_pixels':[3072,1024],'material_order':['head','body','opacity'],'hair_alpha_preserved':True}
(OUT/'crowd_texture_manifest.json').write_text(json.dumps(meta,indent=2))
