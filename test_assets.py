"""Asset installation integrity, cache and filesystem boundary checks (CPU only)."""
import functools
import http.server
import json
import os
from pathlib import Path
import tempfile
import threading
import subprocess
import unittest
from unittest.mock import patch
import zipfile

import assets


class AssetTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.folder = Path(self.temporary.name)
        self.root = self.folder / 'checkout'
        self.cache = self.folder / 'cache'
        self.pack = self.folder / 'pack.zip'
        self.manifest = self.folder / 'manifest.json'
        self.payloads = {'assets/people/person.mesh': b'triangle data',
                         'base_assets/marble.rgba': b'texture data'}
        self.write_pack(self.payloads)

    def write_pack(self, payloads):
        with zipfile.ZipFile(self.pack, 'w', compression=zipfile.ZIP_DEFLATED) as archive:
            for name, payload in payloads.items():
                archive.writestr(name, payload)
        entries = [{'path': name, 'bytes': len(payload),
                    'sha256': assets.hashlib.sha256(payload).hexdigest()}
                   for name, payload in self.payloads.items()]
        self.data = {'schema': 'gallery-assets/1', 'id': 'test', 'files': entries,
                     'archive': {'bytes': self.pack.stat().st_size, 'sha256': assets.digest(self.pack),
                                 'url': 'https://example.invalid/not-needed.zip'}}
        self.manifest.write_text(json.dumps(self.data))

    def prepare(self, **kwargs):
        return assets.prepare(self.root, self.manifest, self.cache, **kwargs)

    def test_http_download_then_offline_cache_and_installed_reuse(self):
        class Handler(http.server.SimpleHTTPRequestHandler):
            requests = 0

            def do_GET(self):
                type(self).requests += 1
                super().do_GET()

            def log_message(self, *args):
                pass

        server = http.server.ThreadingHTTPServer(('127.0.0.1', 0),
                  functools.partial(Handler, directory=str(self.folder)))
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            result = self.prepare(source=f'http://127.0.0.1:{server.server_port}/pack.zip')
            self.assertEqual(result['installed'], 2)
            self.assertEqual(Handler.requests, 1)
        finally:
            server.shutdown()
            server.server_close()
            thread.join()
        with patch('assets.urllib.request.urlopen', side_effect=AssertionError('Network forbidden')):
            other = self.folder / 'second-checkout'
            assets.prepare(other, self.manifest, self.cache, offline=True)
            self.assertEqual(self.prepare(offline=True)['installed'], 0)
        for name, payload in self.payloads.items():
            self.assertEqual((other / name).read_bytes(), payload)

    def test_offline_missing_never_requests_network(self):
        with patch('assets.urllib.request.urlopen', side_effect=AssertionError('Network forbidden')):
            with self.assertRaisesRegex(RuntimeError, 'no verified cached pack'):
                self.prepare(offline=True)

    def test_corrupt_download_is_rejected_before_installation(self):
        self.pack.write_bytes(b'bad archive')
        with self.assertRaisesRegex(ValueError, 'mismatch'):
            self.prepare(source=self.pack)
        self.assertFalse(self.root.exists())
        self.assertEqual(list(self.cache.glob('*.part')), [])

    def test_tampered_file_is_rejected_before_any_file_is_installed(self):
        payloads = dict(self.payloads)
        payloads['base_assets/marble.rgba'] = b'wrong pixels'
        self.write_pack(payloads)
        with self.assertRaisesRegex(ValueError, 'checksum mismatch|Wrong unpacked size'):
            self.prepare(source=self.pack)
        self.assertFalse(self.root.exists())

    def test_archive_cannot_add_an_unlisted_traversal_path(self):
        self.write_pack({**self.payloads, '../escaped.txt': b'escape'})
        with self.assertRaisesRegex(ValueError, 'contents do not match'):
            self.prepare(source=self.pack)
        self.assertFalse((self.folder / 'escaped.txt').exists())
        self.assertFalse(self.root.exists())

    def test_manifest_cannot_escape_checkout(self):
        self.data['files'][0]['path'] = '../escaped.txt'
        self.manifest.write_text(json.dumps(self.data))
        with self.assertRaisesRegex(ValueError, 'Unsafe asset path'):
            self.prepare(source=self.pack)
        self.assertFalse(self.root.exists())

    def test_windows_device_and_alternate_stream_paths_are_rejected(self):
        for name in ('assets/NUL', 'assets/com1.rgba', 'assets/normal:stream',
                     'assets/../outside', 'assets\\file.mesh'):
            with self.subTest(name=name), self.assertRaisesRegex(ValueError, 'Unsafe asset path'):
                assets.safe_path(name)

    def test_modified_existing_asset_is_preserved(self):
        target = self.root / next(iter(self.payloads))
        target.parent.mkdir(parents=True)
        target.write_bytes(b'local edit')
        with self.assertRaisesRegex(ValueError, 'Existing asset differs'):
            self.prepare(source=self.pack)
        self.assertEqual(target.read_bytes(), b'local edit')

    def test_missing_asset_cannot_write_through_external_link(self):
        outside = self.folder / 'borrowed-assets'
        outside.mkdir()
        self.root.mkdir()
        link = self.root / 'assets'
        if os.name == 'nt':
            quote = lambda path: "'" + str(path).replace("'", "''") + "'"
            subprocess.run(['powershell.exe', '-NoProfile', '-NonInteractive', '-Command',
                f'New-Item -ItemType Junction -Path {quote(link)} -Target {quote(outside)} | Out-Null'],
                check=True, capture_output=True)
        else:
            link.symlink_to(outside, target_is_directory=True)
        try:
            with self.assertRaisesRegex(ValueError, 'external asset junction'):
                self.prepare(source=self.pack)
            self.assertEqual(list(outside.iterdir()), [])
        finally:
            if os.name == 'nt':
                link.rmdir()  # Remove the fixture junction itself, never its target.
            else:
                link.unlink()


if __name__ == '__main__':
    unittest.main()
