"""Changes to the submodule's native sources must invalidate kernel caches."""
from pathlib import Path
import subprocess
import tempfile
import unittest

from phoenix_dependency import identity


class DependencyTests(unittest.TestCase):
    def test_missing_submodule_has_setup_instruction(self):
        with tempfile.TemporaryDirectory() as folder:
            with self.assertRaisesRegex(RuntimeError, 'submodule update --init'):
                identity(folder)

    def test_source_change_invalidates_cache_but_generated_files_do_not(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            def git(*args):
                subprocess.run(['git', '-C', str(root), *args], check=True, capture_output=True)
            git('init')
            source = root / 'src/native/test.h'
            source.parent.mkdir(parents=True)
            source.write_text('original')
            git('add', 'src')
            git('-c', 'user.name=Test', '-c', 'user.email=test@example.invalid', 'commit', '-m', 'fixture')
            first = identity(root)
            output = root / 'build/generated'
            output.parent.mkdir()
            output.write_text('compiled output')
            self.assertEqual(first, identity(root))
            source.write_text('updated implementation')
            updated = identity(root)
            self.assertEqual(first['commit'], updated['commit'])
            self.assertNotEqual(first['kernel_build'], updated['kernel_build'])
            git('add', 'src')
            git('-c', 'user.name=Test', '-c', 'user.email=test@example.invalid', 'commit', '-m', 'updated fixture')
            self.assertEqual(updated['kernel_build'], identity(root)['kernel_build'])
            self.assertNotEqual(first['commit'], identity(root)['commit'])


if __name__ == '__main__':
    unittest.main()
