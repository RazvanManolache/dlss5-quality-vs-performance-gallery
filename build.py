from pathlib import Path
import argparse,json,os,subprocess,sys,shutil
from assets import prepare
from phoenix_dependency import identity
from gallery_catalog import generate
generate()
ROOT=Path(__file__).resolve().parent;EDITION=ROOT/'external/phoenix'
parser=argparse.ArgumentParser()
parser.add_argument('name', nargs='?')
parser.add_argument('--ngx-sdk',type=Path)
parser.add_argument('--cuda-compiler',type=Path,help='CUDA nvcc.exe; otherwise discover from PATH')
parser.add_argument('--assets-only',action='store_true',help='Download/verify scene assets without compiling')
parser.add_argument('--offline',action='store_true',help='Use installed assets or the verified cache without networking')
parser.add_argument('--asset-pack',help='Use a local pack or HTTPS mirror with the pinned checksum')
parser.add_argument('--asset-cache',type=Path)
args=parser.parse_args();name=args.name
local=json.loads((ROOT/'local.json').read_text()) if (ROOT/'local.json').exists() else {}
if not args.assets_only and not name:parser.error('Supply a fresh build name, or use --assets-only')
try:
    prepare(cache=args.asset_cache,source=args.asset_pack or local.get('asset_pack'),offline=args.offline)
except (OSError,ValueError,RuntimeError) as error:parser.exit(1,f'Asset preparation failed: {error}\n')
if args.assets_only:sys.exit(0)
sdk=args.ngx_sdk or os.environ.get('PHOENIX_NGX_SDK') or local.get('ngx_sdk')
if not sdk:parser.error('Supply --ngx-sdk with the NGX include directory, or configure PHOENIX_NGX_SDK/local.json ngx_sdk')
sdk=Path(sdk).resolve()
if not (sdk/'nvsdk_ngx.h').exists():parser.error('NGX include directory must contain nvsdk_ngx.h')
assert name.replace('_','').isalnum()
if (ROOT/name).exists():parser.error('Choose a fresh build name')
if 'weight_source' not in local:parser.error('Configure your external model in local.json; see local.example.json')
try:dependency=identity(EDITION)
except (OSError,RuntimeError,subprocess.CalledProcessError) as error:parser.exit(1,f'Phoenix dependency unavailable: {error}\n')
bundle=EDITION/'build'/dependency['kernel_build']
if not (bundle/'native_manifest.json').is_file():
    compiler=args.cuda_compiler or shutil.which('nvcc')
    if not compiler:parser.error('Install CUDA or supply --cuda-compiler to build Phoenix kernels')
    subprocess.run([sys.executable,str(EDITION/'tools/production_build_native_kernels.py'),bundle.name,'--compiler',str(compiler)],check=True)
manifest=json.loads((bundle/'native_manifest.json').read_text(encoding='utf-8'))
if not manifest.get('passed') or len(manifest.get('kernels',[]))!=41:parser.error('A complete compiled 41-kernel bundle is required')
local['bundle']=str(bundle.resolve())
(ROOT/'local.json').write_text(json.dumps(local,indent=2)+'\n',encoding='utf-8')
out=ROOT/name;out.mkdir(exist_ok=False)
dxc=Path(os.environ['ProgramFiles(x86)'])/'Windows Kits/10/bin/10.0.26100.0/x64/dxc.exe'
assert dxc.exists()
with (out/'ray_compile.log').open('w') as log:
    subprocess.run([str(dxc),'-T','cs_6_5','-E','main','-O3','-WX','-Fo',str(out/'ray_lighting.dxil'),str(ROOT/'ray_lighting.hlsl')],stdout=log,stderr=log,check=True)
vswhere=Path(os.environ['ProgramFiles(x86)'])/'Microsoft Visual Studio/Installer/vswhere.exe'
install=subprocess.check_output([str(vswhere),'-latest','-products','*','-requires','Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-property','installationPath'],text=True).strip()
assert install
batch='\n'.join(['@echo off','setlocal',f'call "{install}/VC/Auxiliary/Build/vcvars64.bat" > "{out}/toolchain.log" 2>&1','if errorlevel 1 exit /b 5',
 f'cl /nologo /std:c++17 /O2 /W4 /EHsc /MD /DNOMINMAX /I"{sdk}" /I"{EDITION}/third_party/nvapi" /sourceDependencies "{out}/dependencies.json" "{ROOT}/scene.cpp" /Fo:"{out}/scene.obj" /Fe:"{out}/scene_lab.exe" /link /INCREMENTAL:NO d3d12.lib dxgi.lib d3dcompiler.lib user32.lib bcrypt.lib psapi.lib gdi32.lib windowscodecs.lib ole32.lib uxtheme.lib > "{out}/compile.log" 2>&1',
 'exit /b %errorlevel%'])
(out/'build.cmd').write_text(batch)
r=subprocess.run(['cmd','/c',str(out/'build.cmd')],cwd=ROOT)
print((out/'compile.log').read_text(errors='replace'))
if r.returncode==0:
    (out/'phoenix_dependency.json').write_text(json.dumps(dependency,indent=2)+'\n',encoding='utf-8')
    (ROOT/'current_build.txt').write_text(name+'\n',encoding='utf-8')
sys.exit(r.returncode)
