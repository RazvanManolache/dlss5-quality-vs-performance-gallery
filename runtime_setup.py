"""Prepare a downloaded Gallery package without Visual Studio or CUDA tools."""
from pathlib import Path
import argparse,hashlib,json
from assets import prepare
ROOT=Path(__file__).resolve().parent
def digest(path):
    with path.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def verify():
    manifest=json.loads((ROOT/'BUILDINFO.json').read_text())
    for name,expected in manifest['files'].items():
        if digest(ROOT/name)!=expected:raise ValueError('Package checksum mismatch: '+name)
    bundle=ROOT/'kernels'
    data=json.loads((bundle/'native_manifest.json').read_text())
    assert data['passed'] and len(data['kernels'])==41
    assert digest(bundle/'native_graph.json')==data['native_graph_sha256']
    for row in data['kernels']:assert digest(bundle/row['binary'])==row['binary_sha256']
    assert (ROOT/'gallery_desktop/scene_lab.exe').read_bytes()[:2]==b'MZ'
    print('Executable, shaders and 41 kernel checksums verified')
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--model',type=Path,help='Path to your supported nvngx_dlssnr.dll (model data only)')
    parser.add_argument('--verify-package',action='store_true')
    parser.add_argument('--offline',action='store_true')
    parser.add_argument('--asset-pack')
    args=parser.parse_args()
    verify()
    if args.verify_package:return
    if not args.model:parser.error('Supply --model with your local supported nvngx_dlssnr.dll')
    model=args.model.expanduser().resolve()
    expected=json.loads((ROOT/'kernels/native_graph.json').read_text())['supported_dll_sha256']
    if digest(model)!=expected:parser.error('Model hash does not match this pinned runtime; see BUILDING.md')
    config=ROOT/'local.json'
    if config.exists():parser.error('local.json already exists; preserve or rename it before configuring again')
    prepare(source=args.asset_pack,offline=args.offline)
    config.write_text(json.dumps({'bundle':'kernels','weight_source':{'kind':'nvidia_dll','path':str(model),'sha256':expected}},indent=2)+'\n')
    print('Setup complete. Run RUN.cmd to open the gallery.')
if __name__=='__main__':main()
