"""Exercise a real pack download, complete install and offline cache repair."""
import argparse,functools,http.server,json,threading
from pathlib import Path
from assets import ROOT,prepare,read_manifest,valid,digest

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--pack',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);a=ap.parse_args()
 pack=a.pack.resolve();out=a.output.resolve()
 if out.exists() or not out.is_relative_to(ROOT/'evidence'):raise ValueError('Fresh evidence directory inside gallery required')
 out.mkdir(parents=True);cache=out/'cache';installed=out/'installed';manifest=read_manifest(ROOT/'gallery_assets.json')
 class Handler(http.server.SimpleHTTPRequestHandler):
  def log_message(self,*args):pass
 server=http.server.ThreadingHTTPServer(('127.0.0.1',0),functools.partial(Handler,directory=str(pack.parent)))
 thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
 try:download=prepare(root=installed,cache=cache,source=f'http://127.0.0.1:{server.server_port}/{pack.name}')
 finally:server.shutdown();server.server_close();thread.join()
 assert all(valid(installed/e['path'],e) for e in manifest['files'])
 # Remove one owned fixture file to require the cached archive on the next run.
 repair=installed/manifest['files'][0]['path'];assert repair.is_relative_to(installed);repair.unlink()
 offline=prepare(root=installed,cache=cache,offline=True)
 assert all(valid(installed/e['path'],e) for e in manifest['files'])
 current=prepare(root=ROOT,cache=cache,offline=True)
 result={'passed':True,'archive_sha256':digest(pack),'archive_bytes':pack.stat().st_size,'files':len(manifest['files']),
  'download':download,'offline_cache_repair':offline,'current_install':current,'all_file_hashes_match':True}
 (out/'verification.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))

if __name__=='__main__':main()
