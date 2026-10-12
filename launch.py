"""Launch the most recently built standalone gallery."""
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parent
selection = root / 'current_build.txt'
if not selection.is_file():
    sys.exit('Build the gallery first: python build.py build001')
name = selection.read_text(encoding='utf-8').strip()
if not name or not name.replace('_', '').isalnum():
    sys.exit('Invalid current_build.txt; rebuild the gallery.')
executable = root / name / 'scene_lab.exe'
if not executable.is_file():
    sys.exit('Selected application is missing; rebuild the gallery.')
sys.exit(subprocess.call([str(executable), *sys.argv[1:]], cwd=root))
