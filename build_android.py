from pathlib import Path
from datetime import datetime, timezone
import argparse
import hashlib
import json
import os
import re
import shutil
import struct
import subprocess

root = Path(__file__).resolve().parent
parser = argparse.ArgumentParser(description='Build the isolated drawing-only Android executable.')
parser.add_argument('--ndk', default=os.environ.get('ANDROID_NDK_HOME', 'D:/work-build/lsrael-spatial-20261116/android-ndk-r26d'))
parser.add_argument('--jobs', type=int, default=4)
parser.add_argument('--stage-root', default='D:/work-build/lsrael-draw-only-139')
default_project = next(root / name for name in ('app-draw-final', 'app-draw-clean', 'app') if (root / name).is_dir())
parser.add_argument('--project', type=Path, default=default_project)
args = parser.parse_args()
project = args.project.resolve()
assert (project / 'jni/Android.mk').is_file(), project
ndk = Path(args.ndk).resolve()
assert (ndk / 'ndk-build.cmd').is_file(), ndk
stage_root = Path(args.stage_root).resolve()
assert str(stage_root).isascii(), 'Use an ASCII-only build stage path.'
stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%f')
stage = stage_root / ('build-' + stamp) / 'app'
stage.parent.mkdir(parents=True)
shutil.copytree(project, stage, ignore=shutil.ignore_patterns('obj', 'libs'))
# Windows NDK make cannot stat non-ASCII translation unit names in an explicit
# source list. Compile byte-identical copies beside their original source so
# relative includes keep their meaning. The original project is never renamed.
makefile = stage / 'jni/Android.mk'
make_text = makefile.read_text('utf-8-sig')
source_aliases = {}
for match in re.finditer(r'(?<!\S)(src/[^\s\\]+\.(?:cpp|c))(?=\s|$)', make_text):
    original = match.group(1)
    if original.isascii():
        continue
    source = Path(original)
    alias = source.with_name('build_unit_' + hashlib.sha256(original.encode('utf-8')).hexdigest()[:12] + source.suffix)
    shutil.copy2(stage / 'jni' / source, stage / 'jni' / alias)
    source_aliases[original] = alias.as_posix()
for original, alias in source_aliases.items():
    make_text = make_text.replace(original, alias)
makefile.write_text(make_text, 'utf-8')
out = root / 'build'
out.mkdir(exist_ok=True)
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
inputs = {p.relative_to(project).as_posix(): sha(p)
          for p in sorted((project / 'jni').rglob('*')) if p.is_file()}
command = [str(ndk / 'ndk-build.cmd'), '-B', '-j' + str(args.jobs)]
print('Building in ' + str(stage), flush=True)
with (out / 'android-build.log').open('w', encoding='utf-8') as log:
    result = subprocess.run(command, cwd=stage, stdout=log, stderr=subprocess.STDOUT)
assert result.returncode == 0, 'Build failed; inspect build/android-build.log.'
binary = stage / 'libs/arm64-v8a/lsrael_DrawOnly_1_39.sh'
assert binary.is_file(), binary
data = binary.read_bytes()
assert data[:6] == b'\x7fELF\x02\x01'
assert struct.unpack_from('<H', data, 18)[0] == 183
assert struct.unpack_from('<H', data, 16)[0] == 3
offset = struct.unpack_from('<Q', data, 32)[0]
size, count = struct.unpack_from('<HH', data, 54)
alignments = [struct.unpack_from('<Q', data, offset + i * size + 48)[0]
              for i in range(count) if struct.unpack_from('<I', data, offset + i * size)[0] == 1]
assert alignments and all(n >= 16384 for n in alignments)
target = out / binary.name
shutil.copy2(binary, target)
assert {p.relative_to(project).as_posix(): sha(p)
        for p in sorted((project / 'jni').rglob('*')) if p.is_file()} == inputs, 'Sources changed during build.'
readelf = ndk / 'toolchains/llvm/prebuilt/windows-x86_64/bin/llvm-readelf.exe'
dynamic = subprocess.run([str(readelf), '-d', str(target)], check=True, capture_output=True, text=True).stdout
(out / 'elf-dynamic.txt').write_text(dynamic, 'utf-8')
assert 'libc++_shared' not in dynamic
report = {
    'edition': 'drawing-only', 'game_version': '1.39', 'abi': 'arm64-v8a',
    'minimum_api': 31, 'ndk': str(ndk), 'command': command, 'stage': str(stage), 'project': str(project),
    'success': True, 'binary': str(target), 'bytes': len(data), 'sha256': sha(target),
    'load_alignments': alignments, 'source_sha256': inputs,
    'build_translation_unit_aliases': source_aliases,
    'built_at': datetime.now(timezone.utc).isoformat(), 'production_executed': False
}
(out / 'build-info.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), 'utf-8')
print(json.dumps({k: v for k, v in report.items() if k != 'source_sha256'}, ensure_ascii=True, indent=2))
