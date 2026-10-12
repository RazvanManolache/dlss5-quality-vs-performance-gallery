"""Export the expanded gallery from native readbacks, never desktop captures."""
import argparse
import hashlib
import json
import shutil
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw, ImageFont
from analyze_detail import measure, linear, picture
from gallery_catalog import ROOT, people

def digest(path):
    with path.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--run',type=Path,required=True)
    ap.add_argument('--build',type=Path,required=True)
    ap.add_argument('--output',type=Path,required=True)
    args=ap.parse_args();run=args.run.resolve();out=args.output.resolve()
    report,stats=measure(run)
    assert report['crowd_protocol']['passed'] and report['crowd_ui']['passed']
    assert report['crowd_protocol']['visitors']==len(people())==12
    assert report['crowd_ui']['shared_textures']==56
    for frame in report['comparison_frames']:
        for f in frame['sides']:
            if f is not None:assert f['d3d12_errors']==0 and f['own_launches']==(154 if f['network_enabled'] else 0)
    out.mkdir(parents=True,exist_ok=False);images=out/'images';images.mkdir()
    checks={};views=[(f'room{a}',run/'room'/f'angle{a}') for a in range(3)]
    views += [(f'person{i}',run/'people'/f'person{i}') for i in range(12)]
    for view,base in views:
        reference=json.loads((base/'beauty_off/frame.json').read_text())
        for folder in sorted(base.iterdir()):
            row=json.loads((folder/'frame.json').read_text())
            assert row['camera_eye']==reference['camera_eye'] and row['camera_target']==reference['camera_target']
            native=linear(folder/'input.rgba16f');final=linear(folder/'output.rgba16f') if (folder/'output.rgba16f').exists() else native
            pos=np.fromfile(folder/'position.rgba32f',np.float32).reshape(1080,1920,4)
            assert np.isfinite(pos).all() and row['visitor_count']==12
            key=view+'_'+folder.name;checks[key]={'native_to_nr_linear_change':float(np.abs(final-native).mean())}
            if view.startswith('person'):
                tag=11+int(view[6:]);body=pos[...,3]==tag;face=pos[...,3]==tag+.25
                assert body.sum()>1000 and face.sum()>100,(key,int(body.sum()),int(face.sum()))
                checks[key].update(body_pixels=int(body.sum()),face_pixels=int(face.sum()))
            else:
                checks[key]['visible_visitors']=[i for i in range(12) if np.isin(pos[...,3],[11+i,11.25+i]).sum()>150]
            if row['network_enabled']:assert checks[key]['native_to_nr_linear_change']>1e-5
            picture(final).save(images/(key+'.jpg'),quality=94)
            if row['network_enabled']:picture(native).save(images/(key+'_input.jpg'),quality=94)
    font=ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',18)
    sheet=Image.new('RGB',(1920,930),(14,18,24));draw=ImageDraw.Draw(sheet)
    for i,p in enumerate(people()):
        x=i%4*480;y=i//4*310
        with Image.open(images/f'person{i}_proxy_contact_nr.jpg') as im:sheet.paste(im.resize((480,270)),(x,y))
        draw.text((x+9,y+277),p['label'],font=font,fill='white')
    sheet.save(out/'people.jpg',quality=94)
    pair=Image.new('RGB',(1920,590),(14,18,24));draw=ImageDraw.Draw(pair)
    for i,key in enumerate(('beauty_off','proxy_contact_nr')):
        with Image.open(images/f'room0_{key}.jpg') as im:pair.paste(im.resize((960,540)),(i*960,50))
        draw.text((i*960+12,12),('Beauty / Phoenix off' if i==0 else 'Room bounce + contact / Phoenix on')+f"  {stats[key]['processing_gpu_ms']:.2f} ms GPU",font=font,fill='white')
    pair.save(out/'scene.jpg',quality=94)
    clips={}
    for clip in ('room','person'):
        frames=[];motion=[];deltas=[];previous=None
        for i in range(48):
            folder=run/'clips'/clip/f'frame{i}';row=json.loads((folder/'frame.json').read_text())
            native=linear(folder/'input.rgba16f');final=linear(folder/'output.rgba16f')
            vectors=np.fromfile(folder/'motion.rg16f',np.float16).astype(np.float32).reshape(1080,1920,2)
            assert np.isfinite(vectors).all() and row['own_launches']==154
            motion.append(float(np.linalg.norm(vectors,axis=2).mean()))
            if previous is not None:deltas.append(float(np.abs(final-previous).mean()))
            previous=final;frames.append(picture(final).resize((960,540)))
        assert np.mean(motion)>.001 and np.mean(deltas)>1e-5
        frames[0].save(out/(clip+'.webp'),save_all=True,append_images=frames[1:],duration=[42,42,41]*16,loop=0,quality=82,method=4)
        clips[clip]={'frames':48,'fps':24,'mean_motion_pixels':float(np.mean(motion)),'mean_frame_difference':float(np.mean(deltas))}
    assets=json.loads((ROOT/'gallery_assets.json').read_text())
    provenance={'passed':True,'build':args.build.name,'visitors':12,'objects':report['crowd_protocol']['objects'],
                'timed_frames':960,'rendered_primary_frames':len(report['frames']),'comparison_frames':len(report['comparison_frames']),
                'learned_launches':sum(f['own_launches'] for f in report['frames']),
                'shared_comparison_textures':56,'d3d12_errors':0,'desktop_capture':False,'presentation':False,'hidden_windows':True,
                'checks':checks,'clips':clips,'asset_pack_bytes':assets['archive']['bytes'],
                'installed_asset_bytes':sum(e['bytes'] for e in assets['files']),
                'source_commit':json.loads((ROOT/'gallery_people.json').read_text())['source_commit'],
                'build_manifest_sha256':digest(args.build/'source_manifest.json'),'run_sha256':digest(run/'run.json'),
                'quality_limit':'Matched appearances and motion are for inspection; image change is not a perceptual quality score.'}
    (out/'measurements.json').write_text(json.dumps(stats,indent=2)+'\n')
    (out/'verification.json').write_text(json.dumps(provenance,indent=2)+'\n')
    shutil.copyfile(ROOT/'crowd_viewer.html',out/'index.html')
    payload={'people':people(),'stats':stats,'verification':{k:v for k,v in provenance.items() if k not in ('checks','clips')}}
    (out/'data.json').write_text(json.dumps(payload,indent=2)+'\n')
    print(json.dumps({k:round(s['processing_gpu_ms'],3) for k,s in stats.items()},indent=2))
    print('Exported 12 inspected visitors, matched room views and moving readbacks.')

if __name__=='__main__':main()
