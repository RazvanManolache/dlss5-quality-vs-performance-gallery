"""Export one current-build evidence set; raw captures may be pruned afterwards."""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont
from analyze_detail import measure, stills, picture, linear, srgb

ROOT = Path(__file__).resolve().parent
LABELS = {'beauty': 'Beauty / Phoenix off', 'middle': 'Middle / Phoenix off',
          'light_off': 'Very light / Phoenix off', 'light_full': 'Very light + Phoenix',
          'light_protected': 'Very light + protected Phoenix',
          'all_detail_full': 'Light + detail + Phoenix',
          'all_detail_protected': 'Light + detail + protected Phoenix',
          'all_detail_shadows_full': 'Detail + soft shadows + Phoenix',
          'all_detail_shadows_protected': 'Detail + soft shadows + protected Phoenix'}


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def validate_run(path):
    report = json.loads((path / 'run.json').read_text())
    assert report['passed']
    for context in [report] + ([report['peer']] if 'peer' in report else []):
        assert context['native_generated_bundle'] and context['kernel_count'] == 41
        assert not context['proprietary_dlss_dll_loaded']
        for frame in context['frames']:
            assert frame['d3d12_errors'] == 0
            assert frame['own_launches'] == (154 if frame['network_enabled'] else 0)
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--evidence', type=Path, default=ROOT / 'evidence/current')
    parser.add_argument('--output', type=Path, default=ROOT / 'docs/current')
    selected = (ROOT / 'current_build.txt').read_text(encoding='utf-8').strip() if (ROOT / 'current_build.txt').exists() else 'build001'
    parser.add_argument('--build', type=Path, default=ROOT / selected)
    args = parser.parse_args()
    evidence, output, build = args.evidence.resolve(), args.output.resolve(), args.build.resolve()
    output.mkdir(parents=True, exist_ok=False)
    timing = validate_run(evidence / 'timing')
    people = validate_run(evidence / 'people')
    review = validate_run(evidence / 'controls_motion')
    assert people['person_check']['passed'] and review['detail_ui_check']['passed']
    _, stats = measure(evidence / 'timing')
    assert set(stats) == set(LABELS)
    images, arrays, inputs, positions = stills(evidence / 'timing')
    protected, response = {}, {}
    for name in stats:
        images[name].save(output / f'scene_{name}.jpg', quality=95)
        if name.startswith(('light_', 'all_detail')) and name != 'light_off':
            assert np.array_equal(positions[name], positions['light_full']), name
        if name.endswith('_protected'):
            clothing = np.isin(positions[name][..., 3], [11., 12., 13.])
            assert clothing.sum() > 1000
            assert np.array_equal(arrays[name][clothing], inputs[name][clothing])
            protected[name] = {'pixels': int(clothing.sum()), 'input_rgb_exact': True}
        if name.endswith('_full'):
            delta = np.abs(arrays[name] - inputs[name])
            assert np.mean(delta) > 1e-5
            response[name] = {'mean_linear_rgb_change': float(np.mean(delta))}
    font = ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf', 22)
    choices = ['beauty', 'middle', 'light_off', 'all_detail_full']
    sheet = Image.new('RGB', (1920, 1180), (14, 18, 24))
    draw = ImageDraw.Draw(sheet)
    for index, name in enumerate(choices):
        x, y = index % 2 * 960, index // 2 * 590
        draw.text((x + 12, y + 10), f'{LABELS[name]} · {stats[name]["processing_gpu_ms"]:.3f} ms GPU', font=font, fill='white')
        sheet.paste(images[name].resize((960, 540)), (x, y + 50))
    sheet.save(output / 'scene.jpg', quality=94)
    subprocess.run([sys.executable, str(ROOT / 'export_people.py'),
                    str(evidence / 'people'), str(output / 'people')], check=True)
    # Three independently reset pipelines see the same moving, followed visitor.
    crop = review['detail_clip']['crop']
    width, height = crop[2:]
    assert review['detail_clip']['actual_person_camera'] and (width, height) == (640, 1080)
    clips, motion_checks, clip_camera = {}, {}, None
    for folder in sorted((evidence / 'controls_motion/clips').iterdir()):
        frames, rows, garment_pixels = [], [], []
        for i in range(48):
            source = folder / f'frame_{i}'
            row = json.loads((source / 'frame.json').read_text())
            rgb = linear(source / 'color.rgba16f', width, height)
            position = np.fromfile(source / 'position.rgba32f', np.float32).reshape(height, width, 4)
            motion = np.fromfile(source / 'motion.rg16f', np.float16).reshape(height, width, 2)
            assert np.isfinite(motion).all() and np.isfinite(position).all()
            assert row['focus_person'] == 0 and row['focus_framing'] == 1
            assert row['d3d12_errors'] == 0 and row['own_launches'] == (154 if row['network_enabled'] else 0)
            garment_pixels.append(int((position[..., 3] == 11).sum()))
            frames.append(picture(rgb).resize((256, 432)))
            rows.append(row)
        camera = [(r['scene_time'], r['camera_eye'], r['camera_target']) for r in rows]
        if clip_camera is None:
            clip_camera = camera
        else:
            assert camera == clip_camera, 'Motion comparison cameras differ'
        assert np.ptp([r['scene_time'] for r in rows]) > 5.9
        assert min(garment_pixels) > 1000, 'Focused motion loses its selected visitor'
        clips[folder.name] = frames
        motion_checks[folder.name] = {'frames': 48, 'min_selected_clothing_pixels': min(garment_pixels),
                                     'moving_follow_camera': True, 'same_camera_all_presets': True}
    for scope in ('full', 'protected'):
        names = ['beauty', 'light_' + scope, 'detail_' + scope, 'soft_' + scope]
        labels = ['Beauty / off', 'Very light + Phoenix', 'Light + detail + Phoenix', 'Detail + soft shadows']
        result = []
        small_font = ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf', 15)
        for i in range(48):
            frame = Image.new('RGB', (1024, 472), (14, 18, 24))
            draw = ImageDraw.Draw(frame)
            for index, name in enumerate(names):
                frame.paste(clips[name][i], (index * 256, 40))
                draw.text((index * 256 + 6, 11), labels[index], font=small_font, fill='white')
            result.append(frame)
        result[0].save(output / f'motion_{scope}.webp', save_all=True,
                       append_images=result[1:], duration=125, loop=0, quality=85, method=4)
    controls = evidence / 'people/controls.png'
    if not controls.exists():
        controls = evidence / 'people/controls_hidden.png'
    assert controls.exists(), 'Current native control panel capture missing'
    shutil.copy2(controls, output / 'controls.png')
    measurements = {'surfaces': stats}
    (output / 'measurements.json').write_text(json.dumps(measurements, indent=2) + '\n')
    with (output / 'measurements.csv').open('w', newline='') as stream:
        writer = csv.writer(stream)
        writer.writerow(['Preset', 'GPU ms', 'Network GPU ms', 'Offscreen frame ms'])
        for name, values in stats.items():
            writer.writerow([name, values['processing_gpu_ms'], values['network_gpu_ms'], values['frame_wall_ms']])
    manifest = json.loads((build / 'source_manifest.json').read_text())
    assert all(sha(Path(item['path'])) == item['sha256'] for item in manifest['files'])
    raw = [{'path': str(path.relative_to(evidence)), 'bytes': path.stat().st_size, 'sha256': sha(path)}
           for path in sorted(evidence.rglob('*')) if path.is_file()]
    (evidence / 'raw_capture_manifest.json').write_text(json.dumps(raw, indent=2) + '\n')
    validation = {'passed': True, 'build': build.name, 'source_manifest_sha256': sha(build / 'source_manifest.json'),
                  'executable_sha256': sha(build / 'scene_lab.exe'), 'resolution': [1920, 1080],
                  'timing_protocol': timing['detail_protocol'], 'preset_count': len(stats),
                  'timing_frames': len(timing['frames']), 'person_checks': people['person_check'],
                  'ui_checks': review['detail_ui_check'], 'motion_checks': motion_checks,
                  'protected_clothing': protected, 'full_frame_neural_response': response,
                  'complete_learned_launches_per_enabled_frame': 154, 'generated_kernels': 41,
                  'd3d12_errors': 0, 'nvidia_nr_implementation_loaded': False,
                  'desktop_capture': False, 'presentation': False,
                  'raw_capture_manifest_sha256': sha(evidence / 'raw_capture_manifest.json'),
                  'processing': 'Renderer FP16 readback, linear-to-sRGB conversion, JPEG/WebP encoding. '
                                'Person stills are complete world-camera renders. Motion uses a centered '
                                'region of the actual follow camera, uniformly resized; overview sheets resize only.',
                  'limits': 'GPU timings exclude captures and presentation; they are not delivered FPS. '
                            'The 8 fps motion clip verifies matched motion and visibility, not high-rate flicker. '
                            'No claim of Beauty equivalence.'}
    (output / 'validation.json').write_text(json.dumps(validation, indent=2) + '\n')
    assets = json.loads((evidence / 'assets/verification.json').read_text())
    assert assets['passed']
    shutil.copy2(evidence / 'assets/verification.json', output / 'asset_setup.json')
    shutil.copy2(ROOT / 'viewer.html', output / 'index.html')
    (output / 'provenance.json').write_text(json.dumps({
        'build': build.name, 'executable_sha256': validation['executable_sha256'],
        'source_manifest_sha256': validation['source_manifest_sha256'],
        'asset_manifest_sha256': sha(ROOT / 'gallery_assets.json'),
        'export_sources': {name: sha(ROOT / name) for name in ('export_current.py', 'export_people.py', 'viewer.html')},
        'files': [{'path': str(path.relative_to(output)), 'sha256': sha(path)}
                  for path in sorted(output.rglob('*')) if path.is_file()]}, indent=2) + '\n')
    print('Current evidence passed:', len(stats), 'presets; 9 person views; 7 matched motion sequences.')
    print(json.dumps({name: round(values['processing_gpu_ms'], 3) for name, values in stats.items()}, indent=2))


if __name__ == '__main__':
    main()
