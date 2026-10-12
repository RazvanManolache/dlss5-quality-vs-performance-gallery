"""Export reproducible cost ablations, matched images and motion diagnostics."""
from pathlib import Path
import argparse
import csv
import hashlib
import json
import numpy as np
from PIL import Image, ImageDraw, ImageFont
from analyze_detail import measure, linear, picture, srgb, gaussian_filter, map_coordinates

ROOT = Path(__file__).resolve().parent
LABELS = {'beauty_off':'Beauty / NR off', 'beauty_nr':'Beauty + NR',
 'beauty_direct1':'Beauty / 1 direct sample', 'beauty_gi0':'Beauty / indirect off',
 'beauty_no_second':'Beauty / second bounce off', 'beauty_reflect1':'Beauty / 1 reflection sample',
 'beauty_sss_off':'Beauty / skin diffusion off', 'beauty_geometry_low':'Beauty / cheaper exhibit meshes',
 'middle_off':'Middle / NR off', 'detail_off':'Light + detail / NR off',
 'light_nr':'Very light + NR', 'texture_nr':'Light / texture sampling + NR',
 'normal_nr':'Light / normal strength + NR', 'filter_nr':'Light / preserve filtered detail + NR',
 'detail_nr':'Light + detail + NR', 'soft_nr':'Detail + soft shadows + NR',
 'gi1_nr':'Detail / 1 indirect sample + NR', 'reflect_nr':'Detail / traced reflections + NR',
 'proxy_nr':'Detail / room bounce + NR', 'contact_nr':'Detail / contact shading + NR',
 'proxy_contact_nr':'Room bounce + contact + NR', 'proxy_contact_off':'Room bounce + contact / NR off',
 'sparse_nr':'Sparse GI + reflections + NR', 'sparse_off':'Sparse GI + reflections / NR off',
 'sparse_shadowed_nr':'Sparse / secondary visibility + NR'}
DISPLAY = ['beauty_off','detail_nr','soft_nr','proxy_contact_nr','sparse_nr','sparse_shadowed_nr']


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def read_still(folder):
    native = linear(folder/'input.rgba16f')
    final = linear(folder/'output.rgba16f') if (folder/'output.rgba16f').exists() else native
    pos = np.fromfile(folder/'position.rgba32f', np.float32).reshape(1080,1920,4)
    assert np.isfinite(pos).all()
    return native, final, pos, json.loads((folder/'frame.json').read_text())


def validate_panel(report):
    # The panel test selects Right only; its renderer rows live in sides[1],
    # while the owner's research_ui.frames is correctly empty.
    frames=report['comparison_frames'];assert len(frames)==34
    for i,frame in enumerate(frames):
        assert frame['view']==1 and frame['sides'][0] is None
        side=frame['sides'][1]
        assert side['d3d12_errors']==0 and side['own_launches']==154 and side['network_enabled']
        assert side['detail_flags']==(1663 if i<17 else 17919)
        assert side['gi_samples']==(0 if i<17 else 1) and side['reflection_samples']==1
        assert not side['material_scoped_composite']
    return len(frames)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run',type=Path,default=ROOT/'evidence/research001/run')
    parser.add_argument('--output',type=Path,default=ROOT/'docs/research001')
    parser.add_argument('--build',type=Path,default=ROOT/'build005')
    args=parser.parse_args();run=args.run.resolve();out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    report,stats=measure(run)
    assert set(stats)==set(LABELS) and report['research_protocol']['passed']
    assert report['research_ui']['passed']
    panel_frames=validate_panel(report)
    for context in [report]+([report['peer']] if 'peer' in report else []):
        for frame in context['frames']:
            assert frame['d3d12_errors']==0 and frame['own_launches']==(154 if frame['network_enabled'] else 0)
    # Keep every benchmark repeat, and distinguish scene time from inference/bridge cost.
    for name,s in stats.items():
        frames=[f for f in report['frames'] if f.get('condition')==name and f.get('measured')]
        for k in ('encode_gpu_ms','decode_gpu_ms','direct_samples','gi_samples','reflection_samples','diffuse_bounce_depth','geometry_quality'):
            s[k]=float(np.mean([f[k] for f in frames]))
        s['label']=LABELS[name]
        s['scene_gpu_ms']=s['gbuffer_gpu_ms']+s['ray_lighting_gpu_ms']+s['postprocess_gpu_ms']
    (out/'measurements.json').write_text(json.dumps(stats,indent=2)+'\n')
    keys=['condition','label','processing_gpu_ms','scene_gpu_ms','gbuffer_gpu_ms','ray_lighting_gpu_ms','postprocess_gpu_ms','network_gpu_ms','encode_gpu_ms','decode_gpu_ms','gpu_p95_ms']
    with (out/'measurements.csv').open('w',newline='',encoding='utf-8') as f:
        writer=csv.DictWriter(f,fieldnames=keys);writer.writeheader()
        for name,s in stats.items():writer.writerow({k:(name if k=='condition' else s[k]) for k in keys})
    expected={'beauty_direct1':('direct_samples',1),'beauty_gi0':('gi_samples',0),'beauty_reflect1':('reflection_samples',1),'beauty_geometry_low':('geometry_quality',0),'sparse_nr':('gi_samples',1)}
    for name,(key,value) in expected.items():assert stats[name][key]==value
    ablations={name:{'saved_gpu_ms':stats['beauty_off']['processing_gpu_ms']-stats[name]['processing_gpu_ms'], 'repeat_savings_ms':[a-b for a,b in zip(stats['beauty_off']['repeat_gpu_means'],stats[name]['repeat_gpu_means'])]} for name in stats if name.startswith('beauty_') and name not in ('beauty_off','beauty_nr')}
    (out/'ablations.json').write_text(json.dumps(ablations,indent=2)+'\n')
    image_checks={};image_dir=out/'images';image_dir.mkdir()
    for view in ('room','person0','person1','person2'):
        base=run/'stills' if view=='room' else run/'people'/view
        reference_native,reference,reference_pos,_=read_still(base/'beauty_off')
        conditions=list(stats) if view in ('room','person0') else DISPLAY
        for name in conditions:
            native,final,pos,row=read_still(base/name)
            assert row['camera_eye']==json.loads((base/'beauty_off/frame.json').read_text())['camera_eye']
            assert row['camera_target']==json.loads((base/'beauty_off/frame.json').read_text())['camera_target']
            if view!='room':
                tag=11+int(view[-1]);assert (pos[...,3]==tag).sum()>1000 and (pos[...,3]==tag+.25).sum()>100
            final_rgb=srgb(final);reference_rgb=srgb(reference)
            same=(pos[...,3]==reference_pos[...,3])&(np.linalg.norm(pos[...,:3]-reference_pos[...,:3],axis=2)<.002)&(pos[...,3]>0)
            assert same.sum()>10000
            # A distance to Beauty documents appearance changes, not perceived quality.
            diagnostics={'beauty_rgb_distance':float(np.mean(np.abs(final_rgb-reference_rgb)[same])),
                         'compared_pixels':int(same.sum()),'native_to_nr_linear_change':float(np.mean(np.abs(final-native)))}
            for region,mask in [('clothing',np.isin(pos[...,3],[11.,12.,13.])),('face',(pos[...,3]>=11)&(np.mod(pos[...,3],1)>.1))]:
                if mask.sum():
                    luma=final_rgb@np.array([.2126,.7152,.0722]);blur=gaussian_filter(luma,1)
                    diagnostics[region]={'pixels':int(mask.sum()),'high_frequency_activity':float(np.abs(luma-blur)[mask].mean()),'nr_linear_change':float(np.abs(final-native)[mask].mean())}
            image_checks[view+'/'+name]=diagnostics
            picture(final).save(image_dir/f'{view}_{name}.jpg',quality=93)
            if name in DISPLAY:picture(native).save(image_dir/f'{view}_{name}_input.jpg',quality=93)
        del reference,reference_native,reference_pos
    # Each feature must affect the input, independent of the model's output.
    ref=linear(run/'people/person0/light_nr/input.rgba16f')
    for name in ('texture_nr','normal_nr','filter_nr','proxy_nr','contact_nr'):
        assert np.abs(linear(run/f'people/person0/{name}/input.rgba16f')-ref).mean()>1e-7,name
    font=ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',20)
    for view in ('room','person0'):
        sheet=Image.new('RGB',(1920,1180),(14,18,24));draw=ImageDraw.Draw(sheet)
        for i,name in enumerate(('beauty_off','detail_nr','proxy_contact_nr','sparse_nr')):
            x=i%2*960;y=i//2*590;draw.text((x+12,y+8),f'{LABELS[name]} / {stats[name]["processing_gpu_ms"]:.2f} ms GPU',font=font,fill='white')
            sheet.paste(Image.open(image_dir/f'{view}_{name}.jpg').resize((960,540)),(x,y+50))
        sheet.save(out/f'{view}.jpg',quality=94)
    motion={};reference_camera=None;clip_images={}
    yy,xx=np.mgrid[:1080,:640]
    for name in DISPLAY:
        frames=[];values=[];previous=None;camera=[];face_counts=[]
        for i in range(48):
            folder=run/'clips'/name/f'frame_{i}';row=json.loads((folder/'frame.json').read_text())
            native=linear(folder/'input.rgba16f',640,1080)
            final=linear(folder/'output.rgba16f',640,1080) if (folder/'output.rgba16f').exists() else native
            pos=np.fromfile(folder/'position.rgba32f',np.float32).reshape(1080,640,4)
            expected_pos=np.fromfile(folder/'previous.rgba32f',np.float32).reshape(1080,640,4)
            vectors=np.fromfile(folder/'motion.rg16f',np.float16).reshape(1080,640,2).astype(np.float32)
            assert np.isfinite(vectors).all() and np.isfinite(pos).all()
            camera.append([row['camera_eye'],row['camera_target'],row['scene_time'],row['light_phase']])
            face_counts.append(int((pos[...,3]==11.25).sum()));assert face_counts[-1]>100
            if previous is not None:
                px=xx+vectors[...,0];py=yy+vectors[...,1];coords=[py,px]
                old_pos=np.stack([map_coordinates(previous['pos'][...,c],coords,0,'nearest') for c in range(4)],axis=-1)
                mask=(pos[...,3]>=11)&(pos[...,3]==old_pos[...,3])&(np.linalg.norm(old_pos[...,:3]-expected_pos[...,:3],axis=-1)<.025)&(px>=1)&(px<638)&(py>=1)&(py<1078)
                assert mask.sum()>1000
                sample=lambda a:np.stack([map_coordinates(a[...,c],coords,1,'nearest') for c in range(3)],axis=-1)
                values.append({'pixels':int(mask.sum()),'native_residual':float(np.abs(srgb(native)-sample(previous['native']))[mask].mean()),'nr_residual':float(np.abs(srgb(final)-sample(previous['final']))[mask].mean())})
            previous={'pos':pos,'native':srgb(native),'final':srgb(final)}
            frames.append(picture(final).resize((320,540)))
        if reference_camera is None:reference_camera=camera
        else:assert camera==reference_camera
        motion[name]={'frames':48,'source_fps':24,'minimum_visible_face_pixels':min(face_counts),
                      'native_reprojection_residual':float(np.mean([v['native_residual'] for v in values])),
                      'nr_reprojection_residual':float(np.mean([v['nr_residual'] for v in values])),
                      'valid_pixels_min':min(v['pixels'] for v in values)}
        clip_images[name]=frames
    for suffix,choices in [('proxy', ['beauty_off','detail_nr','proxy_contact_nr']),('sparse',['beauty_off','detail_nr','sparse_nr'])]:
        sequence=[]
        for i in range(48):
            canvas=Image.new('RGB',(960,590),(14,18,24));draw=ImageDraw.Draw(canvas)
            for col,name in enumerate(choices):
                draw.text((col*320+8,8),LABELS[name],font=font,fill='white');canvas.paste(clip_images[name][i],(col*320,50))
            sequence.append(canvas)
        sequence[0].save(out/f'motion_{suffix}.webp',save_all=True,append_images=sequence[1:],duration=42,loop=0,quality=85,method=4)
    (out/'motion.json').write_text(json.dumps(motion,indent=2)+'\n')
    (out/'image_diagnostics.json').write_text(json.dumps(image_checks,indent=2)+'\n')
    validation={'passed':True,'conditions':len(stats),'timed_frames':len(stats)*192,'network_launches':154,'network_kernels':41,'d3d12_errors':0,'presentation':False,'windows_hidden':True,
                'actual_panel_presets_tested':[5,6],'actual_panel_neural_frames_checked':panel_frames,'matched_motion_cameras':True,'motion_frames_per_variant':48,
                'limits':'Beauty distances and high-frequency activity are diagnostics, not perceptual quality scores. Reprojection residual contains genuine shading changes and approximation errors. Two-second 24 fps motion is limited coverage.',
                'executable_sha256':digest(args.build/'scene_lab.exe'),'source_manifest_sha256':digest(args.build/'source_manifest.json'),'phoenix':json.loads((args.build/'phoenix_dependency.json').read_text())}
    (out/'validation.json').write_text(json.dumps(validation,indent=2)+'\n')
    (out/'index.html').write_bytes((ROOT/'research_viewer.html').read_bytes())
    (out/'provenance.json').write_text(json.dumps({'run_sha256':digest(run/'run.json'),'exporter_sha256':digest(Path(__file__)),
        'files':[{'path':p.relative_to(out).as_posix(),'sha256':digest(p)} for p in sorted(out.rglob('*')) if p.is_file()]},indent=2)+'\n')
    print(json.dumps({'passed':True,'presets':len(stats),'ablations':ablations,'motion':motion},indent=2))


if __name__=='__main__':main()
