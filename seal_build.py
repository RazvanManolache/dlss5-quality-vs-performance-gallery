"""Pin demo source, actual included Phoenix headers, shaders and executable."""
import hashlib,json,shutil,sys
from pathlib import Path
root=Path(__file__).resolve().parent
build=root/sys.argv[1]
snapshot=build/'source';snapshot.mkdir(exist_ok=False)
for source in root.iterdir():
    if source.is_file() and (source.suffix in ('.cpp','.inc','.hlsl','.hlsli','.py','.ps1','.md') or source.name in ('gallery_assets.json','gallery_people.json')):
        shutil.copy2(source,snapshot/source.name)
dependencies=json.loads((build/'dependencies.json').read_text(encoding='utf-8-sig'))['Data']['Includes']
paths=list(snapshot.iterdir())+[build/'scene_lab.exe',build/'ray_lighting.dxil']
paths += [build/'phoenix_dependency.json',root/'.gitmodules']
paths += [Path(f) for f in dependencies if Path(f).is_relative_to(root)]
local=json.loads((root/'local.json').read_text())
paths += [Path(local['bundle'])/name for name in ('native_manifest.json','native_graph.json')]
(build/'source_manifest.json').write_text(json.dumps({'schema':'gallery-build/1','runtime':'standalone-phoenix-gallery','files':[{'path':str(f.resolve()),'sha256':hashlib.sha256(f.read_bytes()).hexdigest()} for f in paths]},indent=2))
print('Pinned',len(paths),'files')
