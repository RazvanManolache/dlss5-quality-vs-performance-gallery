"""Maintainer-only: fetch pinned MIT Rocketbox sources and prepare runtime assets.

Normal builds download the smaller prepared pack; Blender is not a build dependency.
Run this with Python, then Blender --background --factory-startup --python bake_living_rigs.py.
"""
import argparse
import concurrent.futures
import hashlib
import json
import shutil
import subprocess
import sys
import urllib.request
import urllib.parse
from pathlib import Path
from gallery_catalog import ROOT, people, actors, generate

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-cache',type=Path,help='Optional read-only existing Rocketbox Assets directory')
    args=parser.parse_args()
    commit=json.loads((ROOT/'gallery_people.json').read_text())['source_commit']
    cache=ROOT/'assets/rocketbox_sources';cache.mkdir(parents=True,exist_ok=True)
    tree_path=cache/'tree.json'
    if not tree_path.exists():
        request=urllib.request.Request(f'https://api.github.com/repos/microsoft/Microsoft-Rocketbox/git/trees/{commit}?recursive=1',headers={'User-Agent':'Phoenix-Gallery'})
        tree_path.write_bytes(urllib.request.urlopen(request,timeout=120).read())
    tree=json.loads(tree_path.read_text())
    if tree.get('sha')!=commit or tree.get('truncated'):raise ValueError('Source tree is not the complete pinned commit')
    entries={e['path']:e for e in tree['tree'] if e['type']=='blob'}
    selected=set()
    for p in actors():
        prefix=f"Assets/{p['category']}/{p['name']}/"
        selected.add(prefix+f"Export/{p['name']}.fbx")
        if p.get('facial'):selected.add(prefix+f"Export/{p['name']}_facial.fbx")
        selected.update(n for n in entries if n.startswith(prefix+'Textures/') and n.endswith('.tga'))
        if p['role']=='animal':continue
        for directory,clip in [('xy','walk_neutral'),('static',p['activity'])]:
            filename=f"Assets/Animations/all_animations_max_motextr_{directory}/{p['gender']}_{clip}"
            candidate=filename+'_01.max.fbx' if filename+'_01.max.fbx' in entries else filename+'.max.fbx'
            if candidate not in entries:raise ValueError('Missing clip '+candidate)
            selected.add(candidate)
    def valid(path,e):
        if not path.is_file() or path.stat().st_size!=e['size']:return False
        data=path.read_bytes()
        return hashlib.sha1(f'blob {len(data)}\0'.encode()+data).hexdigest()==e['sha']
    def fetch(name):
        e=entries[name];dest=cache/name;dest.parent.mkdir(parents=True,exist_ok=True)
        if not valid(dest,e):
            candidate=args.source_cache/Path(name).relative_to('Assets') if args.source_cache else None
            if candidate and valid(candidate,e):shutil.copyfile(candidate,dest)
            else:
                url=f'https://raw.githubusercontent.com/microsoft/Microsoft-Rocketbox/{commit}/{urllib.parse.quote(name)}'
                for attempt in range(3):
                    try:
                        with urllib.request.urlopen(url,timeout=120) as source,dest.open('wb') as output:shutil.copyfileobj(source,output)
                        if not valid(dest,e):raise ValueError('Git blob verification failed: '+name)
                        break
                    except (OSError,ValueError):
                        if attempt==2:raise
        print('Verified '+name,flush=True)
        return {'path':name,'bytes':e['size'],'git_blob_sha1':e['sha'],'sha256':hashlib.sha256(dest.read_bytes()).hexdigest(),'url':f'https://raw.githubusercontent.com/microsoft/Microsoft-Rocketbox/{commit}/{urllib.parse.quote(name)}'}
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:files=list(pool.map(fetch,sorted(selected)))
    manifest={'repository':'https://github.com/microsoft/Microsoft-Rocketbox','commit':commit,'license':'MIT','people':[p['name'] for p in actors()],'files':files}
    (ROOT/'licenses/Rocketbox-crowd-sources.json').write_text(json.dumps(manifest,indent=2)+'\n')
    generate()
    for script in ('prepare_rocketbox_textures.py','prepare_rocketbox_mips.py'):
        subprocess.run([sys.executable,str(ROOT/script)],check=True)
    print('Sources and textures ready. Bake compact rigs with background Blender.',flush=True)

if __name__=='__main__':main()
