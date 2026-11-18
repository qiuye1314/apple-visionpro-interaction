"""Transfer the verified liquid-glass release over USB without launching it."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib
import json
import shlex
import shutil
import subprocess

root = Path(__file__).resolve().parent
delivery = root.parent / 'lsrael_1.39_绘制专版_液体玻璃'
summary_path = delivery / '交付校验.json'
summary = json.loads(summary_path.read_text('utf-8'))
adb = 'D:/re-tools/platform-tools/adb.exe'
serial = 'HA2CMW5Z'
remote = '/sdcard/Download/lsrael_DrawOnly_1_39_LiquidGlass'
runtime = '/data/local/tmp/lsrael_DrawOnly_1_39.sh'
sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()

def run(*args):
    completed = subprocess.run([adb, '-s', serial, *args], check=True,
                               capture_output=True, encoding='utf-8', errors='replace', timeout=180)
    return completed.stdout.strip()

def shell(*args):
    return run('shell', shlex.join(args))

archive, binary = Path(summary['archive']), Path(summary['binary'])
assert sha(archive) == summary['archive_sha256']
assert sha(binary) == summary['binary_sha256']
devices_result = subprocess.run([adb, 'devices'], check=True, capture_output=True,
                                encoding='utf-8', errors='replace', timeout=30)
devices = [line.split() for line in devices_result.stdout.splitlines()[1:] if line.strip()]
available = [parts[0] for parts in devices if len(parts) >= 2 and parts[1] == 'device']
if serial not in available and len(available) == 1:
    serial = available[0]
if serial not in available:
    stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%f')
    previous = delivery / '手机推送校验.json'
    if previous.is_file():
        preserved = root / 'revisions' / ('phone-transfer-' + stamp + '.json')
        preserved.parent.mkdir(exist_ok=True)
        shutil.copy2(previous, preserved)
    status = 'not_connected' if not devices else 'device_unavailable'
    report = {'status': status, 'executed': False, 'checked_at': datetime.now(timezone.utc).isoformat(),
              'devices': devices, 'expected_device': serial,
              'archive_sha256': summary['archive_sha256'], 'binary_sha256': summary['binary_sha256']}
    previous.write_text(json.dumps(report, ensure_ascii=False, indent=2), 'utf-8')
    for key in ('mobile_directory', 'mobile_runtime_binary', 'mobile_transfer_report_sha256'):
        summary.pop(key, None)
    summary.update(mobile_push_status=status, mobile_transfer_report_sha256=sha(previous))
    summary_path.write_text(json.dumps(summary, ensure_ascii=False, indent=2), 'utf-8')
    (root / 'build/phone-transfer-liquid.json').write_text(previous.read_text('utf-8'), 'utf-8')
    print(json.dumps(report, ensure_ascii=True, indent=2), flush=True)
    raise SystemExit(0)
assert run('get-state') == 'device'
assert shell('getprop', 'ro.product.cpu.abi') == 'arm64-v8a'
shell('mkdir', '-p', remote)
files = [archive, binary, delivery / '使用说明.txt', delivery / '液体玻璃总览.png',
         delivery / '分辨率适配总览.png']
verified = {}
for path in files:
    digest = sha(path)
    target = remote + '/' + path.name
    temporary = target + '.upload-' + digest[:12]
    print('Pushing ' + path.name, flush=True)
    run('push', str(path), temporary)
    assert shell('sha256sum', temporary).split()[0] == digest, target
    shell('mv', '-f', temporary, target)
    assert shell('sha256sum', target).split()[0] == digest, target
    verified[target] = {'bytes': path.stat().st_size, 'sha256': digest}

temporary = runtime + '.upload-' + summary['binary_sha256'][:12]
run('push', str(binary), temporary)
assert shell('sha256sum', temporary).split()[0] == summary['binary_sha256']
shell('chmod', '755', temporary)
shell('mv', '-f', temporary, runtime)
assert shell('sha256sum', runtime).split()[0] == summary['binary_sha256']
verified[runtime] = {'bytes': binary.stat().st_size, 'sha256': summary['binary_sha256']}

report = {'device': serial, 'directory': remote, 'runtime_binary': runtime,
          'status': 'verified', 'executed': False,
          'verified_at': datetime.now(timezone.utc).isoformat(), 'files': verified}
report_path = delivery / '手机推送校验.json'
report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2), 'utf-8')
report_remote = remote + '/' + report_path.name
run('push', str(report_path), report_remote)
assert shell('sha256sum', report_remote).split()[0] == sha(report_path)
summary.update(mobile_push_status='verified', mobile_directory=remote,
               mobile_runtime_binary=runtime, mobile_transfer_report_sha256=sha(report_path))
summary_path.write_text(json.dumps(summary, ensure_ascii=False, indent=2), 'utf-8')
summary_remote = remote + '/' + summary_path.name
run('push', str(summary_path), summary_remote)
assert shell('sha256sum', summary_remote).split()[0] == sha(summary_path)
(root / 'build/phone-transfer-liquid.json').write_text(report_path.read_text('utf-8'), 'utf-8')
print(json.dumps(report, ensure_ascii=True, indent=2), flush=True)
