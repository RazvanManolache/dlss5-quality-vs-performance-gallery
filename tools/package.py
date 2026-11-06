"""Create a relocatable binary package containing only public runtime inputs."""
from pathlib import Path
import hashlib,json,shutil,subprocess,sys,zipfile
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from phoenix_dependency import identity
ROOT=Path(__file__).resolve().parents[1]
out=ROOT/'dist';out.mkdir(exist_ok=True)
stage=out/'DLSS5-Gallery';stage.mkdir(exist_ok=False)
build=ROOT/'gallery_desktop'
dependency=identity(ROOT/'external/phoenix')
bundle=ROOT/'external/phoenix/build'/dependency['kernel_build']
def copy(source,name):
    dest=stage/name;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,dest)
for name in ['scene_lab.exe','ray_lighting.dxil']:copy(build/name,'gallery_desktop/'+name)
for path in ROOT.glob('*.hlsl'):copy(path,path.name)
for path in ROOT.glob('*.hlsli'):copy(path,path.name)
for name in ['assets.py','runtime_setup.py','gallery_assets.json','gallery_people.json','BUILDING.md','LICENSE','CREDITS.md','THIRD_PARTY_NOTICES.md']:
    copy(ROOT/name,name)
shutil.copytree(ROOT/'licenses',stage/'licenses',dirs_exist_ok=True)
copy(ROOT/'external/phoenix/LICENSE','licenses/Phoenix-LICENSE')
copy(ROOT/'external/phoenix/THIRD_PARTY.md','licenses/Phoenix-THIRD_PARTY.md')
data=json.loads((bundle/'native_manifest.json').read_text())
assert data['passed'] and len(data['kernels'])==41
for name in ['native_manifest.json','native_graph.json']+[row['binary'] for row in data['kernels']]:copy(bundle/name,'kernels/'+name)
(stage/'RUN.cmd').write_text('@echo off\ncd /d "%~dp0"\nif not exist local.json (\n echo Run SETUP.cmd with the path to your model DLL first. See BUILDING.md.\n pause\n exit /b 1\n)\ngallery_desktop\\scene_lab.exe %*\n')
(stage/'SETUP.cmd').write_text('@echo off\ncd /d "%~dp0"\npython runtime_setup.py --model %*\nif errorlevel 1 pause\n')
files={p.relative_to(stage).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(stage.rglob('*')) if p.is_file()}
assert not any(Path(n).suffix.lower() in {'.dll','.bin','.db'} for n in files), 'Unexpected private/runtime input'
metadata={'schema':'gallery-download/1','commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
 'phoenix_commit':dependency['commit'],'phoenix_source_sha256':dependency['source_sha256'],
 'architecture':'sm_120','gpu_tested':False,'model_weights_included':False,'scene_assets_included':False,'files':files}
(stage/'BUILDINFO.json').write_text(json.dumps(metadata,indent=2)+'\n')
subprocess.run([sys.executable,str(stage/'runtime_setup.py'),'--verify-package'],check=True)
archive=out/'DLSS5-Gallery-windows-x64.zip'
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
    for p in sorted(stage.rglob('*')):
        if p.is_file() and '__pycache__' not in p.parts:z.write(p,'DLSS5-Gallery/'+p.relative_to(stage).as_posix())
with zipfile.ZipFile(archive) as z:assert z.testzip() is None
(out/'SHA256SUMS.txt').write_text(hashlib.sha256(archive.read_bytes()).hexdigest()+'  '+archive.name+'\n')
print('Packaged public executable, shaders and 41 kernels; model and scene assets remain separate')
