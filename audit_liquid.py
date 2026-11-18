"""Audit liquid UI, bone fixes, TWT/RT reads and mortar display against the preserved background release."""
from pathlib import Path
from datetime import datetime, timezone
import argparse
import hashlib
import json
import re
import struct
import runpy

root = Path(__file__).resolve().parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--baseline', type=Path)
default_project = next((root / name for name in ('app-draw-final', 'app-draw-clean', 'app')
                        if (root / name).is_dir()), root / 'app-draw-final')
parser.add_argument('--project', type=Path, default=default_project)
args = parser.parse_args()
project = args.project.resolve()
if args.baseline:
    baseline = args.baseline.resolve()
else:
    releases = root / 'release'
    candidates = sorted((p / 'lsrael_1.39_绘制专版_动态背景' / 'app'
                         for p in releases.iterdir() if p.is_dir()), key=lambda p: p.parts[-3])
    candidates = [p.resolve() for p in candidates if (p / 'jni/src/SpatialUI.h').is_file()]
    assert candidates, 'No preserved dynamic-background release found.'
    baseline = candidates[-1]
assert baseline != project and (baseline / 'jni/src/SpatialUI.h').is_file(), baseline
sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()

def inventory(folder):
    return {p.relative_to(folder).as_posix(): {'bytes': p.stat().st_size, 'sha256': sha(p)}
            for p in sorted(folder.rglob('*')) if p.is_file()
            and not any(part in {'obj', 'libs'} for part in p.relative_to(folder).parts)}

before, after = inventory(baseline), inventory(project)
changed = sorted(name for name in before.keys() & after.keys() if before[name] != after[name])
added, removed = sorted(after.keys() - before.keys()), sorted(before.keys() - after.keys())
allowed = sorted('jni/src/' + name for name in (
    'SpatialUI.h', 'SpatialChrome.h', 'SpatialBackdrop.h',
    'SpatialBackdropStudio.inl', 'SpatialControls.h',
    '骨骼.hpp', '绘制.cpp', '计算.cpp', 'DrawOnlyVisuals.h', '绘图.cpp',
    '辅助类.h', 'SpatialConsole.h', '结构体.h', 'SpatialLegacyPages.inl',
    '版本数据_1_39.h', 'PhysX.h', 'ReadGame.cpp', 'SpatialBackdropImage.h', '悬浮窗.cpp',
    'ImGui/TouchHelperA.cpp'))
allowed = sorted(allowed + ['jni/include/ImGui/ANativeWindowCreator.h'])
asset_names = (
    '6AECCD18E30048EFC2946A824BD6E089.jpg',
    '8A88047E005ECCFAACF29438427A3B14.jpg',
    'EA68D70D6F5E4CCF0854725F5ABE4048.jpg')
asset_hashes = (
    '36ef85e97c160963dfc33cbb700718e774e1c28c9ce16169df2dfabb4874cf07',
    '49e37038437308fb2d6db8b5937dbf052a125caa3ded34c4a9cc4718a995be9d',
    '14985616db185c96efa00fe1148eb9a017312e33e5f61b8e6e054eac6dcfbedf')
allowed_added = sorted(['jni/src/' + name for name in (
    'DrawOnlyBoneReader.h', 'DrawOnlyProjection.h', 'DrawOnlyDriver.h',
    'DrawOnlyMemoryBridge.h', 'DrawOnlyMortar.h', 'DrawOnlyMortarVisuals.h',
    'DrawOnlyActorRead.h', 'DrawOnlyFrameStability.h', 'DrawOnlyVisibleScene.h',
    'DrawOnlyPhysXThrottle.h', 'SpatialTouchRecovery.h', 'DrawOnlySceneBounds.h',
    'SpatialBackdropPresets.h', 'SpatialBackdropPresetData.h')]
    + ['jni/assets/backdrops/' + name for name in (*asset_names, 'generate_presets.py')])
assert changed == allowed, {'allowed': allowed, 'changed': changed}
assert added == allowed_added and not removed, {'added': added, 'removed': removed}

# The platform source may only add the preset callback and separate custom-image
# release. All remaining window/input/game integration is byte-for-byte stable.
platform_path = 'jni/src/悬浮窗.cpp'
old_callback = '    },spatial_backdrop_image::Release);\n'
new_callback = '''    },spatial_backdrop_image::ReleaseCustom,[](int index)->spatial::BackdropImageView{
        const auto& image=spatial_backdrop_image::GetPreset(index);
        return {image.texture,{(float)image.width,(float)image.height},image.error};
    });
'''
platform_before = (baseline / platform_path).read_text('utf-8-sig')
assert platform_before.count(old_callback) == 1
assert (project / platform_path).read_text('utf-8-sig') == platform_before.replace(old_callback, new_callback)
protected = ('jni/src/SpatialTheme.h', 'jni/src/SpatialMotion.h')
assert all(before[name] == after[name] for name in protected)
for name, digest in zip(asset_names, asset_hashes):
    assert sha(project / 'jni/assets/backdrops' / name) == digest, name
generator = runpy.run_path(str(project / 'jni/assets/backdrops/generate_presets.py'))
generated_path, generated_bytes = generator['generate']()
assert generated_path.read_bytes() == generated_bytes, 'Embedded bytes differ from the bundled original JPEGs.'
core = (project / 'jni/src/绘制.cpp').read_text('utf-8-sig')
assert '479.5f' not in core and 't_屏幕坐标.W <= 0.0f' not in core
assert 'draw_only_actor::ReadSnapshot' in core and 'draw_only_actor::ShouldSkip' in core
assert 'draw_only_projection::ProjectActorBounds' in core and 'g_actorList.Values()' in core
assert 'LogReadDiagnostics(ImGui::GetTime())' in core
assert 'CustomLineTrace' in core and '按钮.掩体识别' in core
assert '人物掩体漏打' in (project / 'jni/src/SpatialLegacyPages.inl').read_text('utf-8-sig')
assert re.search(r'bool\s+掩体识别\s*=\s*false', (project / 'jni/src/结构体.h').read_text('utf-8-sig'))
driver = (project / 'jni/src/辅助类.h').read_text('utf-8-sig')
console = (project / 'jni/src/SpatialConsole.h').read_text('utf-8-sig')
version = (project / 'jni/src/版本数据_1_39.h').read_text('utf-8-sig')
physics = (project / 'jni/src/PhysX.h').read_text('utf-8-sig')
assert 'TWT' in console and 'RT' in console
assert not re.search(r'DEV 驱动|HK 驱动|QX 驱动|RT / GT', console)
assert not re.search(r'/dev/TH520|/dev/niuto01|FindQxDevice|0x6661|0x6663', driver)
assert '迫击炮落点' in core and 'draw_only_mortar::ReadFrame' in core
assert re.search(r'bool\s+迫击炮落点\s*=\s*false', (project / 'jni/src/结构体.h').read_text('utf-8-sig'))
assert re.search(r'PhysX实例\s*=\s*0x175E6D98', version)
assert 'V139::PhysX实例' in physics and 'draw_only_memory::Read' in physics
assert 'process_vm_readv' not in physics
banned = re.compile(r'PTE追踪|PTE掩体漏打|自瞄|漏打开关|WriteFloat|arm64_hwbp|hwbp_add|AimBot')
for name in after:
    path = project / name
    if path.suffix.lower() in {'.cpp', '.c', '.h', '.hpp', '.inl', '.mk', '.sh'}:
        assert not banned.search(path.read_text('utf-8-sig', errors='replace')), name

build_dir = root / 'build'
build = json.loads((build_dir / 'build-info.json').read_text('utf-8-sig'))
current_jni = {p.relative_to(project).as_posix(): sha(p)
               for p in sorted((project / 'jni').rglob('*')) if p.is_file()}
assert build['success'] and build['source_sha256'] == current_jni, 'Build inputs differ from current JNI sources.'
assert build['edition'] == 'drawing-only' and build['game_version'] == '1.39'
assert build['abi'] == 'arm64-v8a' and build['minimum_api'] == 31
binary = build_dir / 'lsrael_DrawOnly_1_39.sh'
assert build['sha256'] == sha(binary)
assert not build['production_executed']
elf = binary.read_bytes()
assert elf[:6] == b'\x7fELF\x02\x01' and struct.unpack_from('<HH', elf, 16) == (3, 183)
for name in asset_names:
    assert (project / 'jni/assets/backdrops' / name).read_bytes() in elf, 'JPEG is missing from the executable: ' + name
program_offset = struct.unpack_from('<Q', elf, 32)[0]
program_size, program_count = struct.unpack_from('<HH', elf, 54)
alignments = [struct.unpack_from('<Q', elf, program_offset + i * program_size + 48)[0]
              for i in range(program_count)
              if struct.unpack_from('<I', elf, program_offset + i * program_size)[0] == 1]
assert alignments == build['load_alignments'] and all(alignment >= 16384 for alignment in alignments)
assert 'libc++_shared' not in (build_dir / 'elf-dynamic.txt').read_text('utf-8-sig')

preview = root / 'preview'
validation_path = preview / 'validation-summary.json'
validation = json.loads(validation_path.read_text('utf-8-sig'))
assert validation['cases'] and all(case['exit_code'] == 0 and case['passed'] > 0 for case in validation['cases'])
assert validation['total'] == sum(case['passed'] for case in validation['cases'])
case_names = {case['name'] for case in validation['cases']}
assert {'ui', 'gyro-ui', 'quotes-ui', 'scene', 'visuals', 'motion', 'quotes-unit',
        'background', 'background-images', 'background-ui', 'responsive', 'liquid',
        'bone-reader', 'projection', 'driver', 'console', 'mortar', 'mortar-visuals', 'memory-bridge',
        'actor-read', 'frame-stability', 'physx-concurrency', 'physx-throttle', 'touch-recovery',
        'scene-bounds'} <= case_names, case_names
logs = {}
resolution_hits = set()
glass_alpha_samples = []
for case in validation['cases']:
    assert Path(case['log']).name == case['log'], case
    log = preview / case['log']
    raw = log.read_bytes()
    text = raw.decode('utf-16' if raw[:2] in {b'\xff\xfe', b'\xfe\xff'} else 'utf-8-sig')
    assert len(re.findall(r'^PASS ', text, re.M)) == case['passed'], 'Log/report count mismatch: ' + case['name']
    logs[case['log']] = sha(log)
    if case['name'] == 'liquid':
        glass_alpha_samples = [dict(zip(('theme', 'quote', 'navigation', 'drawing', 'branding', 'emblem'), map(int, sample)))
            for sample in re.findall(r'GLASS_ALPHA theme=(\d+) quote=(\d+) navigation=(\d+) drawing=(\d+) branding=(\d+) emblem=(\d+) / 255', text)]
    if case['name'] == 'responsive':
        for width, height in re.findall(r'(?:RESPONSIVE_TESTED|responsive(?:\s+layout)?)[^\r\n]*?([0-9]{3,5})\s*[x×]\s*([0-9]{3,5})', text, re.I):
            resolution_hits.add((int(width), int(height)))
        for line in text.splitlines():
            if re.match(r'^PASS responsive layout ', line, re.I):
                resolution_hits.update((int(w), int(h)) for w, h in re.findall(r'([0-9]{3,5})\s*[x×]\s*([0-9]{3,5})', line))
assert resolution_hits, 'Responsive test log must state the actual tested pixel dimensions.'
assert {sample['theme'] for sample in glass_alpha_samples} == {0, 2}, 'Missing light/dark composed glass comparison.'
assert all(all(abs(sample[key]-sample['drawing']) <= 4 for key in ('quote', 'navigation', 'branding', 'emblem'))
           for sample in glass_alpha_samples), 'Quote/drawing/navigation/branding transparency differs.'
responsive_resolutions = [{'width': w, 'height': h} for w, h in sorted(resolution_hits)]

def preview_inventory():
    return {p.name: sha(p) for p in sorted(preview.iterdir()) if p.is_file()
            and (p.suffix.lower() in {'.cpp', '.h', '.hpp', '.inl', '.ps1'}
                 or p.name in {'prepare_draw_only.py', 'export_pngs.py'})}
preview_sources = preview_inventory()
assert validation.get('project_source_sha256') == current_jni, 'Desktop verification does not match current JNI inputs.'
assert validation.get('preview_sources_sha256') == preview_sources, 'Desktop preview inputs changed after verification.'
preview_executables = {p.name: sha(p) for p in sorted(preview.iterdir()) if p.is_file()
                       and (p.name == 'SpatialPreview.exe' or (p.name.startswith('Verify') and p.suffix.lower() == '.exe'))}
assert 'SpatialPreview.exe' in preview_executables
assert validation.get('preview_executables_sha256') == preview_executables, 'Desktop test binaries changed after verification.'

manifest_path = preview / 'capture-manifest.json'
manifest = json.loads(manifest_path.read_text('utf-8-sig'))
captures = manifest.get('captures', [])
assert captures, 'No exported captures.'
assert manifest.get('exporter_sha256') == sha(preview / 'export_pngs.py'), 'Screenshots were exported using a different script.'
capture_sources = manifest.get('sources', [])
assert capture_sources, 'Capture source hashes missing.'
current_capture_sources = sorted(p.name for p in preview.glob('draw-only-*')
                                 if p.is_file() and p.suffix.lower() in {'.ppm', '.tga'})
assert sorted(source['name'] for source in capture_sources) == current_capture_sources, 'Capture inventory is stale or incomplete.'
for source in capture_sources:
    assert Path(source['name']).name == source['name'] and (preview / source['name']).suffix.lower() in {'.ppm', '.tga'}, source
    assert sha(preview / source['name']) == source['sha256'], source['name']
for capture in captures:
    for field in ('png', 'png_sha256'):
        assert field in capture, capture
    assert Path(capture['png']).name == capture['png'] and capture['png'].endswith('.png'), capture
    png = preview / capture['png']
    assert sha(png) == capture['png_sha256'], capture['png']
    header = png.read_bytes()[:24]
    assert header[:8] == b'\x89PNG\r\n\x1a\n' and header[12:16] == b'IHDR', capture['png']
    assert struct.unpack('>II', header[16:24]) == (capture['width'], capture['height']), capture['png']
    if 'ppm' in capture:
        assert sha(preview / capture['ppm']) == capture['ppm_sha256'], capture['ppm']
liquid_captures = [c for c in captures if re.search(r'liquid|responsive', c.get('source', c.get('ppm', c.get('png', ''))), re.I)]
assert liquid_captures, 'Liquid/responsive screenshot captures missing.'
transparent_captures = [c for c in captures if c.get('mode') == 'RGBA' and
                        c.get('alpha_pixels', {}).get('clear', 0) > 0 and
                        c.get('alpha_pixels', {}).get('partial', 0) > 0]
assert transparent_captures, 'No captured PNG retains both clear and partial-alpha pixels.'
capture_image_hashes = {c['png']: sha(preview / c['png']) for c in captures}
assert {f'draw-only-background-preset-{i}.png' for i in range(3)} <= capture_image_hashes.keys()
overview_hashes = {item['name']: item['sha256'] for item in manifest.get('overviews', [])}
for name, digest in overview_hashes.items():
    assert sha(preview / name) == digest, name
matrix_path = preview / 'responsive-matrix.json'
assert matrix_path.is_file(), 'Responsive matrix artifact missing.'
matrix_report = json.loads(matrix_path.read_text('utf-8-sig'))
tested = matrix_report.get('tested_resolutions_px')
assert tested, 'Responsive matrix has no tested_resolutions_px.'
normalize_resolution = lambda pair: (int(pair[0]), int(pair[1])) if isinstance(pair, (list, tuple)) else (int(pair['width']), int(pair['height']))
assert all(width > 0 and height > 0 for width, height in map(normalize_resolution, tested))
assert {normalize_resolution(item) for item in tested} == resolution_hits, 'Test log and responsive matrix disagree.'
captured_resolution_hits = {(item['width'], item['height']) for item in captures
                            if 'responsive' in item.get('source', item.get('ppm', item.get('png', ''))).lower()}
assert captured_resolution_hits == resolution_hits, 'Responsive screenshot sizes differ from passed matrix.'
tooling_names = ('build_android.py', 'audit_liquid.py', 'package_release.py', 'transfer_liquid.py')

report = {
    'edition': 'lsrael 1.39 drawing-only / liquid glass',
    'created_at': datetime.now(timezone.utc).isoformat(),
    'baseline': str(baseline), 'baseline_files': before,
    'changed': changed, 'added': added, 'removed': removed,
    'allowed_changed_files': allowed,
    'allowed_added_files': allowed_added,
    'drawing_and_person_occlusion_logic_unchanged': False,
    'skeleton_read_projection_render_fixes': True,
    'checked_actor_metadata': True,
    'actor_identity_uses_speed_equality': False,
    'frame_and_position_grace_milliseconds': 120,
    'actor_list_header_rechecked_and_deduplicated': True,
    'physx_scene_updates_queries_synchronized': True,
    'ui_scene_queries_nonblocking': True,
    'physx_pressure_cooldown_recovers': True,
    'physx_remote_arrays_and_mesh_indices_bounds_checked': True,
    'physx_failed_mesh_reads_remain_retryable': True,
    'physx_incomplete_enumeration_preserves_scene': True,
    'physx_heightfield_compatibility_tested': ['512x512', '1024x1024'],
    'wireframe_attempted_triangle_budget': 4000,
    'touch_failure_release_and_backoff': True,
    'touch_slots_bounds_checked': True,
    'read_diagnostics_log_tag': 'lsrael-draw',
    'occlusion_sampling_change': 'Skip unavailable joints; use actor position when no valid torso joint exists.',
    'person_occlusion_preserved': True, '悬浮窗.cpp_unchanged': False,
    'platform_change_only_backdrop_callbacks': True,
    'embedded_backgrounds': [
        {'name': name, 'source': source, 'sha256': digest}
        for name, source, digest in zip(('星眸', '泡影', '蓝蝶'), asset_names, asset_hashes)],
    'embedded_backgrounds_fixed_and_rotation': True,
    'consistent_glass_surfaces': ['home_quotes', 'cards', 'theme_picker', 'background_studio', 'numeric_editor', 'notices', 'buttons', 'collapsed_restore', 'emblem'],
    'composed_glass_alpha_samples': glass_alpha_samples,
    'read_drivers': ['TWT', 'RT'],
    'mortar_landing_display': True,
    'mortar_chain': 'libUE4.so:bss + 0x42DAA8 -> 0x18 -> 0x9C8 -> 0xE24',
    'physx_instance_offset': '0x175E6D98',
    'physx_reads_selected_driver': True,
    'drawing_only_removed_features_absent': True,
    'protected_source_sha256': {name: after[name]['sha256'] for name in protected},
    'android_build_inputs_sha256': current_jni, 'binary_sha256': build['sha256'],
    'host_validation': validation, 'validation_logs_sha256': logs,
    'preview_sources_sha256': preview_sources, 'preview_executables_sha256': preview_executables,
    'validation_summary_sha256': sha(validation_path),
    'responsive_tested_resolutions_px': responsive_resolutions,
    'screenshot_sha256': capture_image_hashes, 'overview_sha256': overview_hashes,
    'capture_manifest_sha256': sha(manifest_path),
    'capture_sources_sha256': {item['name']: item['sha256'] for item in capture_sources},
    'responsive_matrix_sha256': sha(matrix_path),
    'tooling_sha256': {name: sha(root / name) for name in tooling_names},
    'build_records_sha256': {name: sha(build_dir / name) for name in
                             ('android-build.log', 'build-info.json', 'elf-dynamic.txt')},
    'production_executed': False, 'mobile_push_status': 'pending', 'final_source_files': after,
}
(root / 'audit').mkdir(exist_ok=True)
(root / 'audit/liquid-glass.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), 'utf-8')
print(json.dumps({key: report[key] for key in
                  ('changed', 'person_occlusion_preserved', 'binary_sha256', 'responsive_tested_resolutions_px')},
                 ensure_ascii=True, indent=2))
print('Host checks: ' + str(validation['total']))
