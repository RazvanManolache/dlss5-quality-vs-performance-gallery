"""Download and verify the gallery's separate, versioned runtime asset pack."""
from pathlib import Path, PurePosixPath
import argparse
import hashlib
import json
import os
import shutil
import tempfile
import urllib.parse
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parent
MANIFEST = ROOT / 'gallery_assets.json'
DEFAULT_URL = ''  # Set a release URL in the manifest after choosing the gallery repository.
CHUNK = 1024 * 1024


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def default_cache():
    return Path(os.environ.get('LOCALAPPDATA', Path.home() / '.cache')) / 'Phoenix/gallery_lab'


def safe_path(name):
    if not isinstance(name, str):
        raise ValueError(f'Unsafe asset path: {name!r}')
    path = PurePosixPath(name)
    reserved = {'CON', 'PRN', 'AUX', 'NUL'} | {f'{kind}{n}' for kind in ('COM', 'LPT') for n in range(1, 10)}
    if ('\\' in name or ':' in name or
            path.is_absolute() or len(path.parts) < 2 or
            path.parts[0] not in ('assets', 'base_assets', 'asset_licenses') or
            any(part in ('', '.', '..') or part.endswith((' ', '.')) or
                part.split('.')[0].upper() in reserved for part in name.split('/'))):
        raise ValueError(f'Unsafe asset path: {name!r}')
    return Path(*path.parts)


def read_manifest(path):
    manifest = json.loads(Path(path).read_text(encoding='utf-8'))
    if manifest['schema'] != 'gallery-assets/1' or not manifest['files']:
        raise ValueError('Unsupported or empty gallery asset manifest')
    names = set()
    for entry in manifest['files']:
        safe_path(entry['path'])
        key = entry['path'].casefold()
        if key in names or entry['bytes'] < 0 or len(entry['sha256']) != 64:
            raise ValueError('Invalid or duplicate manifest entry')
        names.add(key)
    return manifest


def valid(path, entry):
    return (path.is_file() and path.stat().st_size == entry['bytes']
            and digest(path) == entry['sha256'])


def archive_for(manifest, cache, source, offline):
    expected = manifest['archive']
    cache.mkdir(parents=True, exist_ok=True)
    cached = cache / (expected['sha256'] + '.zip')
    if valid(cached, expected):
        print('Using verified cached asset pack.', flush=True)
        return cached
    if offline and not source:
        raise RuntimeError('Assets missing and no verified cached pack. Run once online or supply --asset-pack PATH.')
    source = str(source or expected['url'])
    if not source:
        raise RuntimeError('The standalone asset release is not published yet. Supply --asset-pack PATH or HTTPS_URL.')
    url = urllib.parse.urlsplit(source)
    is_url = url.scheme in ('https', 'http')
    if is_url and url.scheme == 'http' and url.hostname not in ('127.0.0.1', 'localhost', '::1'):
        raise ValueError('Remote asset downloads require HTTPS')
    if offline and is_url:
        raise RuntimeError('--offline prevents downloads; --asset-pack must be a local file.')
    print(f'Fetching gallery asset pack ({expected["bytes"] / 2**20:.1f} MiB): {source}', flush=True)
    part = None
    try:
        with tempfile.NamedTemporaryFile(dir=cache, suffix='.part', delete=False) as output:
            part = Path(output.name)
            if is_url:
                request = urllib.request.Request(source, headers={'User-Agent': 'Phoenix-Gallery-Assets/1'})
                stream = urllib.request.urlopen(request, timeout=60)
            else:
                stream = Path(source).expanduser().open('rb')
            with stream:
                count = 0
                while block := stream.read(CHUNK):
                    count += len(block)
                    if count > expected['bytes']:
                        raise ValueError('Asset archive exceeds its pinned size')
                    output.write(block)
        if not valid(part, expected):
            raise ValueError('Asset archive size/SHA-256 mismatch; nothing was installed')
        os.replace(part, cached)
        return cached
    finally:
        if part is not None:
            part.unlink(missing_ok=True)


def prepare(root=ROOT, manifest_path=MANIFEST, cache=None, source=None, offline=False):
    root = Path(root).resolve()
    manifest = read_manifest(manifest_path)
    missing = []
    for entry in manifest['files']:
        target = root / safe_path(entry['path'])
        if target.exists():
            if not valid(target, entry):
                raise ValueError(f'Existing asset differs from the pinned version: {target}. Move it aside before preparing assets.')
        else:
            # Existing junctions are borrowed read-only assets, not install targets.
            if not target.resolve().is_relative_to(root):
                raise ValueError(f'Refusing to write through an external asset junction: {target}')
            missing.append(entry)
    if not missing:
        print(f'All {len(manifest["files"])} gallery assets verified; no download needed.', flush=True)
        return {'download_needed': False, 'installed': 0, 'verified': len(manifest['files'])}
    cache = Path(cache or default_cache()).resolve()
    archive = archive_for(manifest, cache, source, offline)
    with zipfile.ZipFile(archive) as pack:
        infos = pack.infolist()
        expected = {entry['path'] for entry in manifest['files']}
        if len(infos) != len(expected) or {info.filename for info in infos} != expected:
            raise ValueError('Archive contents do not match the pinned manifest')
        for info in infos:
            safe_path(info.filename)
            if (info.external_attr >> 16) & 0o170000 == 0o120000:
                raise ValueError('Asset archive must not contain symlinks')
        # Verify every extracted file before installing any of them.
        with tempfile.TemporaryDirectory(prefix='gallery-unpack-', dir=cache) as staging:
            staging = Path(staging)
            for entry in manifest['files']:
                staged = staging / safe_path(entry['path'])
                info = pack.getinfo(entry['path'])
                if info.file_size != entry['bytes']:
                    raise ValueError(f'Wrong unpacked size: {entry["path"]}')
                staged.parent.mkdir(parents=True, exist_ok=True)
                with pack.open(info) as src, staged.open('wb') as dst:
                    shutil.copyfileobj(src, dst, CHUNK)
                if not valid(staged, entry):
                    raise ValueError(f'Unpacked asset checksum mismatch: {entry["path"]}')
            for entry in missing:
                target = root / safe_path(entry['path'])
                if target.exists() or not target.resolve().is_relative_to(root):
                    raise ValueError(f'Install target changed while preparing assets: {target}')
                target.parent.mkdir(parents=True, exist_ok=True)
                # Copy via a temporary sibling so interrupted copies are never assets.
                with tempfile.NamedTemporaryFile(dir=target.parent, suffix='.part', delete=False) as dst:
                    temporary = Path(dst.name)
                    try:
                        with (staging / safe_path(entry['path'])).open('rb') as src:
                            shutil.copyfileobj(src, dst, CHUNK)
                    except BaseException:
                        dst.close()
                        temporary.unlink(missing_ok=True)
                        raise
                try:
                    os.replace(temporary, target)
                finally:
                    temporary.unlink(missing_ok=True)
    print(f'Installed {len(missing)} verified gallery assets.', flush=True)
    return {'download_needed': True, 'installed': len(missing), 'verified': len(manifest['files'])}


def runtime_files(root):
    from gallery_catalog import actors
    names = ['horse_statue_01', 'antique_ceramic_vase_01', 'chinese_armchair', 'vintage_video_camera']
    names += [p['name'] for p in actors(root)]
    paths = ['assets/marble_medium.mesh']
    paths += [f'base_assets/{name}.mesh' for name in ('head_high', 'head_low', 'marble_high', 'marble_low')]
    textures = [f'base_assets/{name}.rgba' for name in ('skin_color', 'skin_color_low', 'skin_normal',
                'skin_spec', 'marble_color', 'marble_color_low', 'marble_normal', 'marble_rough')]
    for i, name in enumerate(names):
        folder = 'assets/variety' if i < 4 else 'assets/people'
        if i < 4:
            paths += [f'{folder}/{name}_{lod}.mesh' for lod in ('high', 'low')]
        else:
            paths += [f'assets/living_people/{name}_lod{lod}.mesh' for lod in range(4)]
            paths += [f'assets/living_people/{name}{suffix}' for suffix in ('.skin', '.bones', '_rig.json')]
        textures += [f'{folder}/{name}_{kind}.rgba' for kind in ('color', 'normal', 'rough')]
    paths += textures
    # Every prepared mip chain is required to reproduce the validated appearance.
    paths += [f'assets/beauty_maps002/{Path(name).name}.mips' for name in textures]
    return {name: root / name for name in paths}


def make_pack(output, manifest_path, url=DEFAULT_URL, root=ROOT):
    output, manifest_path, root = Path(output), Path(manifest_path), Path(root)
    if output.exists() or manifest_path.exists():
        raise ValueError('Pack and manifest must have fresh names; version assets instead of overwriting them')
    files = runtime_files(root)
    for name in ('CREDITS.md', 'licenses/Rocketbox-MIT.md', 'licenses/Exhibit-credits.md',
                 'licenses/Rocketbox-source-manifest.json', 'licenses/Rocketbox-crowd-sources.json'):
        files[f'asset_licenses/{Path(name).name}'] = root / name
    files['asset_licenses/LeePerrySmith-License.txt'] = root / 'licenses/LeePerrySmith-License.txt'
    output.parent.mkdir(parents=True, exist_ok=True)
    entries = []
    try:
        with zipfile.ZipFile(output, 'x', compression=zipfile.ZIP_DEFLATED, compresslevel=6) as pack:
            for name, path in sorted(files.items()):
                print(f'Packing {name}', flush=True)
                checksum, size = hashlib.sha256(), 0
                info = zipfile.ZipInfo(name, (1980, 1, 1, 0, 0, 0))
                info.compress_type = zipfile.ZIP_DEFLATED
                info.external_attr = 0o100644 << 16
                with path.open('rb') as src, pack.open(info, 'w', force_zip64=True) as dst:
                    while block := src.read(CHUNK):
                        checksum.update(block)
                        size += len(block)
                        dst.write(block)
                entries.append({'path': name, 'bytes': size, 'sha256': checksum.hexdigest()})
        manifest = {'schema': 'gallery-assets/1', 'id': output.stem,
                    'archive': {'url': url, 'bytes': output.stat().st_size, 'sha256': digest(output)},
                    'files': entries}
        manifest_path.write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
        print(f'Pack ready: {output} ({output.stat().st_size / 2**20:.1f} MiB), '
              f'{sum(e["bytes"] for e in entries) / 2**20:.1f} MiB installed', flush=True)
    except BaseException:
        output.unlink(missing_ok=True)
        raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='command', required=True)
    get = sub.add_parser('prepare', help='Install only missing assets; verify existing files')
    get.add_argument('--root', type=Path, default=ROOT)
    get.add_argument('--manifest', type=Path, default=MANIFEST)
    get.add_argument('--cache', type=Path)
    get.add_argument('--asset-pack', help='Pinned archive from a local file or HTTPS URL')
    get.add_argument('--offline', action='store_true')
    create = sub.add_parser('pack', help='Maintainer: create a separate release asset and pinned manifest')
    create.add_argument('--output', type=Path, default=ROOT / 'asset-packs/gallery-assets-v3.zip')
    create.add_argument('--manifest', type=Path, default=MANIFEST)
    create.add_argument('--url', default=DEFAULT_URL)
    args = parser.parse_args()
    try:
        if args.command == 'pack':
            make_pack(args.output, args.manifest, args.url)
        else:
            prepare(args.root, args.manifest, args.cache, args.asset_pack, args.offline)
    except (OSError, ValueError, RuntimeError, zipfile.BadZipFile) as error:
        parser.exit(1, f'Asset preparation failed: {error}\n')


if __name__ == '__main__':
    main()
