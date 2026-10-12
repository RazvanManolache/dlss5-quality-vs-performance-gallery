"""Exercise the actual release pack over loopback HTTP and from an offline cache."""
import argparse
import functools
import http.server
import json
from pathlib import Path
import threading
from unittest.mock import patch

import assets


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path, help='Fresh local evidence directory')
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    manifest = assets.read_manifest(assets.MANIFEST)
    name = manifest['id'] + '.zip'
    pack = assets.ROOT / 'asset-packs' / name
    assert assets.valid(pack, manifest['archive'])

    class Handler(http.server.SimpleHTTPRequestHandler):
        requests = 0

        def do_GET(self):
            type(self).requests += 1
            super().do_GET()

        def log_message(self, *args):
            pass

    server = http.server.ThreadingHTTPServer(('127.0.0.1', 0),
              functools.partial(Handler, directory=str(pack.parent)))
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        downloaded = assets.prepare(out / 'fresh-checkout', cache=out / 'cache',
                                    source=f'http://127.0.0.1:{server.server_port}/{name}')
    finally:
        server.shutdown()
        server.server_close()
        thread.join()
    assert Handler.requests == 1
    with patch('assets.urllib.request.urlopen', side_effect=AssertionError('Network forbidden')):
        cached = assets.prepare(out / 'offline-checkout', cache=out / 'cache', offline=True)
        reused = assets.prepare(out / 'fresh-checkout', cache=out / 'empty-cache', offline=True)
        # Existing borrowed asset junctions are read-only; only new license files install locally.
        current = assets.prepare(cache=out / 'cache', offline=True)
    assert downloaded['installed'] == cached['installed'] == len(manifest['files'])
    assert reused['installed'] == 0
    for checkout in ('fresh-checkout', 'offline-checkout'):
        assert all(assets.valid(out / checkout / assets.safe_path(e['path']), e) for e in manifest['files'])
    result = {'passed': True, 'archive_bytes': manifest['archive']['bytes'],
              'installed_bytes': sum(e['bytes'] for e in manifest['files']),
              'files': len(manifest['files']), 'http_requests': Handler.requests,
              'fresh_download': downloaded, 'fresh_offline_cache_install': cached,
              'existing_offline_no_cache': reused, 'current_checkout': current,
              'all_files_bit_identical': True, 'gpu_run': False, 'public_upload': False}
    (out / 'verification.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
