"""Validate renderer readbacks and export matched images, measurements and motion."""
import argparse,hashlib,json,csv
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw,ImageFont

def gaussian_filter(a,sigma):
    # Fixed sigma=1 diagnostic kernel; avoid an extra analysis dependency.
    assert sigma==1
    x=np.arange(-3,4);kernel=np.exp(-x*x/2);kernel/=kernel.sum()
    for axis in (0,1):
        pad=[(0,0),(0,0)];pad[axis]=(3,3);p=np.pad(a,pad,mode='edge')
        a=sum(w*np.take(p,np.arange(a.shape[axis])+i,axis=axis) for i,w in enumerate(kernel))
    return a
def binary_erosion(mask,iterations):
    for _ in range(iterations):
        p=np.pad(mask,1);mask=np.logical_and.reduce([p[y:y+mask.shape[0],x:x+mask.shape[1]] for y in range(3) for x in range(3)])
    return mask
def map_coordinates(a,coords,order,mode):
    assert mode=='nearest'
    y=np.clip(coords[0],0,a.shape[0]-1);x=np.clip(coords[1],0,a.shape[1]-1)
    if order==0:return a[np.rint(y).astype(int),np.rint(x).astype(int)]
    y0=y.astype(int);x0=x.astype(int);y1=np.minimum(y0+1,a.shape[0]-1);x1=np.minimum(x0+1,a.shape[1]-1);fy=y-y0;fx=x-x0
    return (a[y0,x0]*(1-fx)+a[y0,x1]*fx)*(1-fy)+(a[y1,x0]*(1-fx)+a[y1,x1]*fx)*fy

ROOT=Path(__file__).resolve().parent
FONT='C:/Windows/Fonts/segoeui.ttf'
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def linear(path,w=1920,h=1080):
    a=np.fromfile(path,np.float16).reshape(h,w,4).astype(np.float32)
    assert np.isfinite(a).all(),path
    return a[...,:3]
def srgb(a):return np.clip(np.where(a<=.0031308,a*12.92,1.055*np.maximum(a,0)**(1/2.4)-.055),0,1)
def picture(a):return Image.fromarray(np.uint8(srgb(a)*255+.5))
def measure(run):
    r=json.loads((run/'run.json').read_text());assert r['passed'] and r['native_generated_bundle'] and r['kernel_count']==41
    assert not r['proprietary_dlss_dll_loaded']
    for f in r['frames']:assert f['own_launches']==(154 if f['network_enabled'] else 0) and f['d3d12_errors']==0
    assert r['detail_protocol']['windows_hidden'] and not r['detail_protocol']['presentation']
    stats={}
    keys=['processing_gpu_ms','network_gpu_ms','gbuffer_gpu_ms','ray_lighting_gpu_ms','postprocess_gpu_ms','frame_wall_ms']
    for c in sorted({f['condition'] for f in r['frames'] if f.get('measured')}):
        fs=[f for f in r['frames'] if f.get('measured') and f.get('condition')==c];assert len(fs)==192
        stats[c]={k:float(np.mean([f[k] for f in fs])) for k in keys}
        stats[c]['gpu_p95_ms']=float(np.percentile([f['processing_gpu_ms'] for f in fs],95))
        stats[c]['repeat_gpu_means']=[float(np.mean([f['processing_gpu_ms'] for f in fs if f['repeat']==i])) for i in range(3)]
        stats[c]['offscreen_frames_per_second']=1000/stats[c]['frame_wall_ms']
    return r,stats
def stills(run):
    imgs={};arrays={};inputs={};positions={}
    for d in (run/'stills').iterdir():
        inputs[d.name]=linear(d/'input.rgba16f');arrays[d.name]=linear(d/('output.rgba16f' if (d/'output.rgba16f').exists() else 'input.rgba16f'))
        imgs[d.name]=picture(arrays[d.name]);positions[d.name]=np.fromfile(d/'position.rgba32f',np.float32).reshape(1080,1920,4)
    return imgs,arrays,inputs,positions
def sheet(imgs,stats,choices,path,crop=(470,350,630,650)):
    font=ImageFont.truetype(FONT,18);width=320;height=600 if crop else 720
    canvas=Image.new('RGB',((width if crop else 1280)*len(choices),height+65),(14,18,24));draw=ImageDraw.Draw(canvas)
    for i,(c,label) in enumerate(choices):
        x=i*(width if crop else 1280);im=imgs[c].crop(crop).resize((width,height)) if crop else imgs[c].resize((1280,720))
        canvas.paste(im,(x,65));draw.text((x+8,5),label,font=font,fill='white');draw.text((x+8,31),f"{stats[c]['processing_gpu_ms']:.3f} ms GPU",font=font,fill=(107,215,223))
    canvas.save(path,quality=94)
def main():
    ap=argparse.ArgumentParser();ap.add_argument('--first',type=Path,required=True);ap.add_argument('--second',type=Path,required=True);ap.add_argument('--review',type=Path);ap.add_argument('--output',type=Path,required=True);a=ap.parse_args()
    a.output.mkdir(exist_ok=False,parents=True)
    report={};summary={};all_hashes=[]
    for name,run in [('fabric',a.first),('surfaces',a.second)]:
        r,stats=measure(run);imgs,arrays,inputs,positions=stills(run);summary[name]=stats
        baseline=positions['light_full'];body=baseline[...,3]==11
        for c in positions:
            if c!='beauty':assert np.array_equal(positions[c],baseline),('geometry changed',c)
        preserved={}
        for c in arrays:
            if c.endswith('_protected'):
                assert np.array_equal(arrays[c][body],inputs[c][body]),('protected garment was modified',c)
                preserved[c]=True
        beauty=srgb(arrays['beauty']);yy,xx=np.mgrid[:1080,:1920]
        mask=body&(xx>=490)&(xx<590)&(yy>=470)&(yy<620)&(beauty[...,0]>beauty[...,1]*1.3)&(beauty[...,2]>beauty[...,1]*1.1)
        mask=binary_erosion(mask,iterations=2);assert mask.sum()>1000
        luma=lambda rgb:srgb(rgb)@np.array([.2126,.7152,.0722])
        ref=luma(arrays['beauty']);ref_detail=ref-gaussian_filter(ref,1)
        metrics={}
        for c in arrays:
            lum=luma(arrays[c]);detail=lum-gaussian_filter(lum,1)
            metrics[c]={'shirt_detail_rms':float(np.sqrt(np.mean(detail[mask]**2))),'shirt_detail_correlation_to_beauty':float(np.corrcoef(detail[mask],ref_detail[mask])[0,1])}
        for c in arrays:
            f=a.output/f'{name}_{c}.jpg';imgs[c].save(f,quality=94)
            imgs[c].crop((470,350,630,650)).resize((320,600)).save(a.output/f'{name}_{c}_character.jpg',quality=96)
        report[name]={'passed':True,'frames':len(r['frames']),'learned_launches':sum(f['own_launches'] for f in r['frames']),'d3d12_errors':0,'material_geometry_identical_across_light_variants':True,'protected_clothing_copied_exactly':preserved,'shirt_pixels_measured':int(mask.sum()),'detail_metrics':metrics,'metrics_limit':'High-pass contrast and correlation describe this crop; they are not perceptual quality scores or proof of Beauty equivalence.'}
        all_hashes += [{'path':str(p.resolve()),'sha256':digest(p)} for p in sorted((run/'stills').glob('*/frame.json'))]
        if name=='surfaces':
            sheet(imgs,stats,[('beauty','Beauty / Phoenix off'),('light_protected','Original light + Phoenix'),('all_detail_protected','Light + detail + Phoenix'),('all_detail_shadows_protected','Detail + soft shadows')],a.output/'characters.jpg')
            sheet(imgs,stats,[('beauty','Beauty / Phoenix off'),('light_full','Original light / full Phoenix'),('all_detail_full','Light + detail / full Phoenix')],a.output/'characters_full.jpg')
            sheet(imgs,stats,[('beauty','Beauty / Phoenix off'),('light_protected','Original light + Phoenix'),('all_detail_protected','Light + detail + Phoenix')],a.output/'scene.jpg',None)
    if a.review:
        r=json.loads((a.review/'run.json').read_text());assert r['passed'] and r['detail_ui_check']['passed']
        for context in [r,r['peer']]:
            assert context['native_generated_bundle'] and context['kernel_count']==41 and not context['proprietary_dlss_dll_loaded']
            for f in context['frames']:assert f['d3d12_errors']==0 and f['own_launches']==(154 if f['network_enabled'] else 0)
        report['ui']=r['detail_ui_check'];clip_metrics={};clips={}
        for folder in sorted((a.review/'clips').iterdir()):
            frames=[];rows=[];previous=None;temporal=[]
            for i in range(48):
                d=folder/f'frame_{i}';rgb=linear(d/'color.rgba16f',400,700);pos=np.fromfile(d/'position.rgba32f',np.float32).reshape(700,400,4);motion=np.fromfile(d/'motion.rg16f',np.float16).reshape(700,400,2).astype(np.float32);row=json.loads((d/'frame.json').read_text());rows.append(row)
                assert row['d3d12_errors']==0 and row['own_launches']==(154 if row['network_enabled'] else 0)
                image=picture(rgb);frames.append(image.resize((300,525)))
                lum=srgb(rgb)@np.array([.2126,.7152,.0722]);body=pos[...,3]==11
                if previous:
                    y,x=np.mgrid[:700,:400];px=x+motion[...,0];py=y+motion[...,1]
                    valid=body&(px>2)&(px<397)&(py>2)&(py<697)&np.isfinite(px)&np.isfinite(py)
                    old_body=map_coordinates(previous[1].astype(float),[py,px],order=0,mode='nearest')>.5
                    valid=binary_erosion(valid&old_body,iterations=2)
                    warped=map_coordinates(previous[0],[py,px],order=1,mode='nearest')
                    if valid.sum()>100:temporal.append(float(np.mean(np.abs(lum[valid]-warped[valid]))))
                previous=(lum,body)
            clip_metrics[folder.name]={'frames':len(frames),'motion_compensated_body_luma_delta':float(np.mean(temporal)),'limit':'Includes lighting, pose, visibility, shading and neural changes; this is a stability diagnostic, not isolated flicker.'};clips[folder.name]=frames
            assert np.ptp([row['scene_time'] for row in rows])>5.9
        font=ImageFont.truetype(FONT,16)
        for mode,choices in [('protected',[('beauty','Beauty / off'),('light_protected','Original light + Phoenix'),('detail_protected','Light + detail + Phoenix'),('soft_protected','Detail + soft shadows')]),('full',[('beauty','Beauty / off'),('light_full','Original light + Phoenix'),('detail_full','Light + detail + Phoenix'),('soft_full','Detail + soft shadows')])]:
            frames=[]
            for i in range(48):
                im=Image.new('RGB',(1200,565),(14,18,24));draw=ImageDraw.Draw(im)
                for j,(key,label) in enumerate(choices):im.paste(clips[key][i],(j*300,40));draw.text((j*300+6,10),label,font=font,fill='white')
                frames.append(im)
            frames[0].save(a.output/f'motion_{mode}.webp',save_all=True,append_images=frames[1:],duration=125,loop=0,quality=86,method=4)
        report['motion']=clip_metrics
    report['passed']=True;report['raw_evidence']=all_hashes;report['image_processing']='FP16 linear to sRGB, matching crop and uniform resizing only. No sharpening, AI image editing or desktop capture.'
    (a.output/'validation.json').write_text(json.dumps(report,indent=2));(a.output/'measurements.json').write_text(json.dumps(summary,indent=2))
    with (a.output/'measurements.csv').open('w',newline='') as f:
        writer=csv.writer(f);writer.writerow(['experiment','condition','GPU ms','network GPU ms','CPU+GPU offscreen frame ms'])
        for exp,conditions in summary.items():
            for c,v in conditions.items():writer.writerow([exp,c,v['processing_gpu_ms'],v['network_gpu_ms'],v['frame_wall_ms']])
    print(json.dumps({k:{c:round(v['processing_gpu_ms'],4) for c,v in cs.items()} for k,cs in summary.items()},indent=2));print('Passed; exported',a.output)
if __name__=='__main__':main()
