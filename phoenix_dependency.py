"""Resolve the Phoenix submodule and key compiled bundles to its actual sources."""
from pathlib import Path
import hashlib
import subprocess


def identity(path):
    path = Path(path).resolve()
    if not (path / '.git').exists():
        raise RuntimeError('Initialize Phoenix first: git submodule update --init --recursive')
    commit = subprocess.check_output(['git', '-C', str(path), 'rev-parse', 'HEAD'], text=True).strip()
    sources = subprocess.check_output([
        'git', '-C', str(path), 'ls-files', '-z', '--', 'src/native',
        'third_party/nvapi', 'third_party/nlohmann', 'third_party/xess',
        'tools/production_build_native_kernels.py',
    ]).decode('utf-8').split('\0')
    digest = hashlib.sha256()
    for name in sorted(filter(None, sources)):
        digest.update(name.encode('utf-8') + b'\0')
        digest.update((path / name).read_bytes())
        digest.update(b'\0')
    return {'commit': commit, 'source_sha256': digest.hexdigest(),
            'path': str(path), 'kernel_build': 'kernels_' + digest.hexdigest()[:16]}
