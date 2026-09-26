#!/usr/bin/env python3
"""LOC-01 acceptance on the project emulator only; never physical devices.

Owner-reported defect: with Android Location Services OFF at launch the app reports no location, and after the
user turns Location ON (Quick Settings / Settings) and returns to the app WITHOUT restarting it, the app still
reports no location. Enabling Location and restarting the app "fixes" it.

This runner proves, end to end on a booted emulator, that:
  A. Location ON at startup        -> the app acquires a location (no emergency needed);
  B. Location OFF at startup       -> honest "Vị trí đang tắt" card, ZERO provider lookups;
  C. OFF -> ON after launch        -> recovery WITHOUT restarting the app, exactly ONE new lookup;
  D. resume with a fresh fix       -> no extra lookup (no duplicate work);
  E. repeated resumes              -> no duplicate lookups;
  F. location permission denied    -> "Chưa cấp quyền vị trí", zero provider lookups, and granting the
                                      permission later recovers the same way as C.

Honest limits (recorded, not hidden): the emulator's Location toggle is driven through
`settings put secure location_mode`, not a real Settings/Quick Settings UI, and the emulator injects a synthetic
GPS fix. Real Quick Settings, real GNSS and a real phone remain unverified here.
"""
import json
import os
import re
import runpy
import subprocess
import threading
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'docs/evidence/loc01'
OUT.mkdir(parents=True, exist_ok=True)
ADB = os.environ.get('ADB', '/home/pdd/Android/Sdk/platform-tools/adb')
SERIAL = os.environ.get('ANDROID_SERIAL', 'emulator-5554')
if not SERIAL.startswith('emulator-'):
    raise SystemExit('This runner refuses physical devices')
APP = 'vn.nckh27pa.fallsafe'
COMPONENT = APP + '/.MainActivity'
LAT, LON = 10.8231, 106.6297
results = []


def adb(*args, stdin=None, check=True, timeout=120):
    p = subprocess.run([ADB, '-s', SERIAL, *args], capture_output=True, text=True,
                       timeout=timeout, input=stdin)
    if check and p.returncode:
        raise RuntimeError(' '.join(args) + ' -> ' + (p.stdout + p.stderr)[-400:])
    return p.stdout + p.stderr


m = runpy.run_path(str(ROOT / 'android/scripts/emulator-smoke.py'))
g = m['fresh'].__globals__
g['OUT'] = OUT
snapshot, text = m['snapshot'], m['text']
tap_label = m['tap_label']


def logcat_clear():
    adb('logcat', '-c', check=False)


def location_log():
    return adb('shell', 'logcat', '-d', '-s', 'FallSafe/Location:I', '*:S', check=False).splitlines()


def recovery():
    return [l for l in location_log() if 'recovery ' in l]


REAL_SOURCE = re.compile(r'source=(FUSED|GPS|NETWORK|CACHED|PHONE|ESP32_GNSS)\b')


def fixes():
    """Real acquired/reported fixes only: `source=null` is the failure log line, not a fix."""
    return [l for l in location_log() if REAL_SOURCE.search(l)]


def set_location(enabled):
    adb('shell', 'settings', 'put', 'secure', 'location_mode', '3' if enabled else '0')
    adb('shell', 'settings', 'put', 'secure', 'location_providers_allowed',
        '+gps,+network' if enabled else '', check=False)
    time.sleep(1.5)


def grant_location():
    for p in ('ACCESS_FINE_LOCATION', 'ACCESS_COARSE_LOCATION'):
        adb('shell', 'pm', 'grant', APP, 'android.permission.' + p, check=False)
    time.sleep(0.5)


def revoke_location():
    for p in ('ACCESS_FINE_LOCATION', 'ACCESS_COARSE_LOCATION'):
        adb('shell', 'pm', 'revoke', APP, 'android.permission.' + p, check=False)
    time.sleep(0.5)


def launch_fresh():
    adb('shell', 'am', 'force-stop', APP)
    time.sleep(1.5)
    adb('shell', 'am', 'start', '-W', '-n', COMPONENT, check=False)
    time.sleep(3.5)


def resume():
    """Leave the app (HOME) and bring it back: exactly what returning from Android Settings does."""
    adb('shell', 'input', 'keyevent', 'KEYCODE_HOME')
    time.sleep(1.5)
    adb('shell', 'am', 'start', '-W', '-n', COMPONENT, check=False)
    time.sleep(3.5)


def wait_for(pred, seconds, step=1.0):
    end = time.monotonic() + seconds
    while time.monotonic() < end:
        value = pred()
        if value:
            return value
        time.sleep(step)
    return None


_stop = False


def geo_pump():
    while not _stop:
        adb('emu', 'geo', 'fix', str(LON), str(LAT), check=False)
        time.sleep(0.5)


def decisions(kind):
    return [l for l in recovery() if 'decision=' + kind in l]


def run(name, body):
    start = time.monotonic()
    try:
        body()
        results.append({'name': name, 'passed': True, 'elapsedSeconds': round(time.monotonic() - start, 2)})
        print('PASS', name, flush=True)
    except Exception as e:
        results.append({'name': name, 'passed': False, 'error': str(e),
                        'elapsedSeconds': round(time.monotonic() - start, 2)})
        print('FAIL', name, str(e), flush=True)
    finally:
        (OUT / 'results.json').write_text(json.dumps(results, ensure_ascii=False, indent=2) + '\n')


def case_a_startup_enabled():
    grant_location()
    set_location(True)
    logcat_clear()
    launch_fresh()
    line = wait_for(lambda: decisions('Acquire'), 15)
    assert line, 'no recovery Acquire decision at startup with Location ON:\n' + '\n'.join(recovery())
    fix = wait_for(fixes, 15)
    assert fix, 'no location acquired at startup with Location ON:\n' + '\n'.join(location_log()[-20:])
    (OUT / 'A-logcat.txt').write_text('\n'.join(location_log()))
    body = text(snapshot('loc01-A-card'))
    (OUT / 'A-card.txt').write_text(body[:2000])
    assert 'Đã xác định' in body or 'Vị trí gần đúng' in body, \
        'location card did not show a fix at startup: ' + body[:400]


def case_b_startup_disabled():
    set_location(False)
    logcat_clear()
    launch_fresh()
    line = wait_for(lambda: decisions('ProviderDisabled'), 15)
    assert line, 'no ProviderDisabled decision with Location OFF at startup:\n' + '\n'.join(recovery())
    assert not fixes(), 'a provider was queried while Location Services was OFF:\n' + '\n'.join(fixes()[-5:])
    (OUT / 'B-logcat.txt').write_text('\n'.join(location_log()))
    body = text(snapshot('loc01-B-card'))
    (OUT / 'B-card.txt').write_text(body[:2000])
    assert 'Vị trí đang tắt' in body, 'card did not report Location Services OFF: ' + body[:400]


def case_c_off_to_on_without_restart():
    logcat_clear()
    set_location(True)   # Quick Settings / Settings path: the app keeps running, it is NOT restarted
    resume()             # ... and the user returns to the app
    line = wait_for(lambda: decisions('Acquire'), 20)
    assert line, 'enabling Location did not trigger recovery without a restart:\n' + '\n'.join(recovery())
    assert len(decisions('Acquire')) == 1, \
        'expected exactly ONE recovery lookup for OFF -> ON, got %d:\n%s' % (
            len(decisions('Acquire')), '\n'.join(recovery()))
    fix = wait_for(fixes, 20)
    if not fix:
        # The bounded attempt may time out on the first try (emulator geo injection). A later resume after the
        # recovery cooldown is a fresh bounded attempt, never an overlapping one - still no app restart.
        time.sleep(16.0)
        resume()
        fix = wait_for(fixes, 20)
    assert fix, 'no location recovered after enabling Location without a restart:\n' + '\n'.join(location_log()[-20:])
    (OUT / 'C-logcat.txt').write_text('\n'.join(location_log()))
    body = text(snapshot('loc01-C-card'))
    (OUT / 'C-card.txt').write_text(body[:2000])
    assert 'Vị trí đang tắt' not in body, 'card still reports Location Services OFF: ' + body[:400]
    assert 'Đã xác định' in body or 'Vị trí gần đúng' in body, \
        'card did not show a determined location after OFF -> ON: ' + body[:400]


def case_d_resume_with_fresh_fix():
    assert fixes(), 'case D is only meaningful with a fresh fix from case C'
    logcat_clear()
    resume()
    time.sleep(3.0)
    assert not decisions('Acquire'), 'resume with a fresh fix must not start another lookup:\n' + '\n'.join(recovery())
    assert not fixes(), 'duplicate lookup on a plain resume:\n' + '\n'.join(fixes()[-5:])
    (OUT / 'D-logcat.txt').write_text('\n'.join(location_log()))


def case_e_repeated_resume():
    logcat_clear()
    for _ in range(3):
        resume()
    # A fresh fix is in state: repeated resumes must not start a lookup or re-publish a fix.
    assert not decisions('Acquire'), 'repeated resumes started a duplicate lookup:\n' + '\n'.join(recovery())
    assert not fixes(), 'repeated resumes created duplicate lookups:\n' + '\n'.join(fixes()[-5:])
    (OUT / 'E-logcat.txt').write_text('\n'.join(location_log()))


def case_f_permission_denied():
    revoke_location()
    set_location(True)
    logcat_clear()
    launch_fresh()
    line = wait_for(lambda: decisions('PermissionDenied'), 15)
    assert line, 'no PermissionDenied decision without location permission:\n' + '\n'.join(recovery())
    assert not fixes(), 'providers were queried without location permission:\n' + '\n'.join(fixes()[-5:])
    (OUT / 'F-logcat.txt').write_text('\n'.join(location_log()))
    body = text(snapshot('loc01-F-card'))
    if 'ĐỂ SAU' in body:
        # Revoking the permission brings back the first-run permission setup overlay; dismiss it so the
        # location card is actually visible before asserting on it.
        tap_label('ĐỂ SAU', 'dismiss-permission-setup')
        body = text(snapshot('loc01-F-card'))
    (OUT / 'F-card.txt').write_text(body[:2000])
    assert 'Chưa cấp quyền vị trí' in body, 'card did not report the missing permission: ' + body[:400]
    # Granting the permission later must recover exactly like C (no restart, real fix).
    logcat_clear()
    grant_location()
    resume()
    line = wait_for(lambda: decisions('Acquire'), 20)
    assert line, 'granting location permission did not trigger recovery:\n' + '\n'.join(recovery())
    fix = wait_for(fixes, 20)
    assert fix, 'no fix after granting the location permission:\n' + '\n'.join(location_log()[-20:])
    (OUT / 'F-after-grant-logcat.txt').write_text('\n'.join(location_log()))


if __name__ == '__main__':
    assert adb('shell', 'getprop', 'ro.kernel.qemu').strip() == '1', 'emulator required'
    assert APP in adb('shell', 'pm', 'list', 'packages', APP, check=False), 'app not installed'
    threading.Thread(target=geo_pump, daemon=True).start()
    for name, body in [
        ('A-startup-location-on', case_a_startup_enabled),
        ('B-startup-location-off', case_b_startup_disabled),
        ('C-off-to-on-without-restart', case_c_off_to_on_without_restart),
        ('D-resume-with-fresh-fix', case_d_resume_with_fresh_fix),
        ('E-repeated-resume', case_e_repeated_resume),
        ('F-permission-denied-then-granted', case_f_permission_denied),
    ]:
        run(name, body)
    _stop = True
    set_location(True)
    grant_location()
    raise SystemExit(0 if all(r['passed'] for r in results) else 1)
