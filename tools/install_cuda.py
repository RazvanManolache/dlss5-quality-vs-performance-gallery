"""Install the minimal CUDA 12.8.1 compiler from NVIDIA's hash-pinned archives."""
from pathlib import Path
import hashlib,json,os,shutil,sys,urllib.request,zipfile
base='https://developer.download.nvidia.com/compute/cuda/redist/'
pins={
 'cuda_nvcc':('12.8.93','9fdc70b4271ed9aad4d64cd7076a7d96ec36512d074b9995fe638de669197391'),
 'cuda_cudart':('12.8.90','4a39058fd8519444a81cfc7ae055d136f48d1a31ffa41ae255b35b2edd61e13b'),
 'cuda_cccl':('12.8.90','bd8548fa1ae82f92910bebc3079e14bd58c5a92aa64596d46bd610a478cb39d7')}
target=Path(sys.argv[1]).resolve();target.mkdir(parents=True,exist_ok=True)
for name,(version,expected) in pins.items():
    stem=f'{name}-windows-x86_64-{version}-archive'
    archive=target/(stem+'.zip')
    urllib.request.urlretrieve(base+f'{name}/windows-x86_64/{archive.name}',archive)
    assert hashlib.file_digest(archive.open('rb'),'sha256').hexdigest()==expected, name+' checksum mismatch'
    with zipfile.ZipFile(archive) as z:z.extractall(target/'packages')
    shutil.copytree(target/'packages'/stem,target,dirs_exist_ok=True)
assert (target/'bin/nvcc.exe').is_file() and (target/'bin/ptxas.exe').is_file()
if os.environ.get('GITHUB_PATH'):
    with open(os.environ['GITHUB_PATH'],'a') as f:f.write(str(target/'bin')+'\n')
print('Installed hash-verified CUDA 12.8.1 compiler')
