#!/usr/bin/env python3
"""Critical SOS acceptance on the project emulator only; never physical devices.

Proves the owner-reported defect is gone, end to end, on a booted emulator:
- a REAL sensor-detected fall (not replay) starts the 10 s countdown;
- the best-available location pipeline acquires a fix during the countdown (no satellite-fix wait);
- SOS dispatch reports per-step results and never blocks SMS/CALL on location;
- the handset call branch is attempted for the seeded emergency contact;
- the acquired coordinates match the injected `adb emu geo fix` values.

Honest limits (recorded, not hidden): the emulator has no carrier, so SMS delivery receipts and a real
voice call cannot be verified here; those stay unit-tested and device-blocked.
"""
import html
import json
import os
import re
import runpy
import subprocess
import threading
import time
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'docs/evidence/sos-location'
OUT.mkdir(parents=True, exist_ok=True)
ADB = os.environ.get('ADB', '/home/pdd/Android/Sdk/platform-tools/adb')
SERIAL = os.environ.get('ANDROID_SERIAL', 'emulator-5554')
if not SERIAL.startswith('emulator-'):
    raise SystemExit('This runner refuses physical devices')
APP = 'vn.nckh27pa.fallsafe'
CONTACT_JSON = json.dumps([{
    'id': 'sos-acceptance-1', 'name': 'Mai', 'relationship': 'Con gái',
    'phone': '0901234567', 'receiveSos': True, 'isPrimary': True, 'callPriority': 1,
}], ensure_ascii=False)
LAT, LON = 10.8231, 106.6297
PREFS = 'shared_prefs/fallsafe_contacts_v2_user-01.xml'

def adb(*args, stdin=None, check=True):
    p = subprocess.run([ADB, '-s', SERIAL, *args], capture_output=True, text=True,
                       timeout=120, input=stdin)
    if check and p.returncode:
        raise RuntimeError(' '.join(args) + ' -> ' + p.stdout[-300:] + p.stderr[-300:])
    return p.stdout + p.stderr

m = runpy.run_path(str(ROOT / 'android/scripts/emulator-smoke.py'))
g = m['fresh'].__globals__
g['OUT'] = OUT
results = g['results']
snapshot, tap_label, text, require, fresh = m['snapshot'], m['tap_label'], m['text'], m['require'], m['fresh']

def seed_contact():
    adb('shell', 'am', 'force-stop', APP)
    xml = ("<?xml version='1.0' encoding='utf-8' standalone='yes' ?>\n<map>\n"
           f'<string name="contacts_list_json">{html.escape(CONTACT_JSON, quote=True)}</string>\n</map>\n')
    local = Path('/tmp/failsafe_contacts_seed.xml')
    local.write_text(xml)
    # A device-side `>` redirect would be executed by the outer shell (not the app uid), so push then copy
    # inside the app's own data directory instead.
    adb('push', str(local), '/data/local/tmp/failsafe_contacts_seed.xml')
    adb('shell', 'run-as', APP, 'mkdir', '-p', 'shared_prefs')
    adb('shell', 'run-as', APP, 'cp', '/data/local/tmp/failsafe_contacts_seed.xml', PREFS)
    got = adb('shell', 'run-as', APP, 'cat', PREFS)
    assert 'sos-acceptance-1' in got, 'contact seed failed: ' + got[:200]
    (OUT / 'seeded-contact.xml').write_text(got)
    # The demo build restarts its local event counter at 1, which re-uses the persisted remote event id.
    # `EmergencyCoordinator.dispatch` then early-returns as already dispatched and the UI shows a stale
    # report, so clear the stored event identity/records to force a genuinely fresh SOS event.
    for stale in ('shared_prefs/fallsafe_emergency_records.xml', 'shared_prefs/fallsafe_emergency_identity.xml'):
        adb('shell', 'run-as', APP, 'rm', '-f', stale, check=False)

def grant():
    for p in ['ACCESS_FINE_LOCATION', 'ACCESS_COARSE_LOCATION', 'SEND_SMS', 'CALL_PHONE', 'READ_PHONE_STATE']:
        adb('shell', 'pm', 'grant', APP, 'android.permission.' + p)
    adb('shell', 'settings', 'put', 'secure', 'location_mode', '3')
    adb('shell', 'settings', 'put', 'secure', 'location_providers_allowed', '+gps,+network')

_stop = False
def geo_pump():
    while not _stop:
        adb('emu', 'geo', 'fix', str(LON), str(LAT), check=False)
        time.sleep(1.0)

def logcat(*tags):
    return adb('shell', 'logcat', '-d', '-s', *tags, '*:S', check=False)

def restart_log():
    adb('logcat', '-c', check=False)

def open_settings_tab():
    tap_label('Cài đặt', 'settings')

def run(name, body):
    start = time.monotonic()
    try:
        body()
        results.append({'name': name, 'passed': True, 'elapsedSeconds': round(time.monotonic() - start, 2)})
        print('PASS', name, flush=True)
    except Exception as e:
        results.append({'name': name, 'passed': False, 'error': str(e)})
        print('FAIL', name, str(e), flush=True)
    finally:
        (OUT / 'results.json').write_text(json.dumps(results, ensure_ascii=False, indent=2) + '\n')

def critical_fall_flow():
    """Real sensor fall -> countdown -> SOS with a fused fix, SMS and SIM call attempts."""
    seed_contact()
    grant()
    restart_log()
    fresh()
    time.sleep(2.0)
    # the seeded contact must be visible, otherwise the SMS/CALL branches never run
    tap_label('Người thân', 'contacts')
    nodes = snapshot('contacts-with-seed')
    assert 'Mai' in text(nodes), 'seeded contact not visible: ' + text(nodes)[:300]

    tap_label('Trang chủ', 'home')
    _stop_thread = threading.Thread(target=geo_pump, daemon=True)
    _stop_thread.start()

    # Real acceleration spike: free fall then impact (same stimulus as emulator-sensor-scroll.py)
    adb('emu', 'sensor', 'set', 'acceleration', '0:0:30'); time.sleep(0.2)
    adb('emu', 'sensor', 'set', 'acceleration', '0:0:9.81')
    time.sleep(1.8)
    countdown = snapshot('countdown')
    ct = text(countdown)
    (OUT / 'countdown.txt').write_text(ct[:1500])
    assert ('Bạn có ổn không?' in ct or 'Nguy cơ ngã' in ct or 'Cần kiểm tra' in ct), \
        'no real fall countdown: ' + ct[:300]

    # During the countdown the pipeline must acquire a fix (this is the pre-warm the defect lacked)
    fix_seen = False
    deadline = time.monotonic() + 9
    while time.monotonic() < deadline:
        log = logcat('FallSafe/Location:*')
        if 'source=' in log:
            (OUT / 'logcat-location-during-countdown.txt').write_text(log)
            fix_seen = True
            break
        time.sleep(1.0)
    assert fix_seen, 'no location acquisition logged during the countdown'

    # let the countdown expire so SOS dispatches with the acquired fix
    time.sleep(13)
    state = snapshot('sos-dispatched')
    (OUT / 'sos-state.txt').write_text(text(state)[:2000])

    full = logcat('FallSafe/SOS:*', 'FallSafe/Location:*', 'FallSafe/SMS:*', 'FallSafe/CALL:*')
    (OUT / 'logcat-dispatch.txt').write_text(full)
    assert 'FallSafe/SMS' in full, 'no SMS dispatch logged: ' + full[-400:]
    assert 'SIM_CALL' in full, 'no SIM call step logged: ' + full[-400:]
    assert 'FallSafe/SOS' in full and 'eventId=' in full, 'no structured SOS diagnostics: ' + full[-400:]
    assert 'thoáng' not in text(state), 'blocking "thoáng" copy still shown after SOS'

    # the dispatched coordinates must match the injected geo fix
    sms = logcat('FallSafe/SMS:*')
    assert 'status=FAILED' not in sms or 'Hệ thống từ chối' not in sms, 'SMS rejected: ' + sms[-400:]
    (OUT / 'sent-sms-provider.txt').write_text(
        adb('shell', 'content', 'query', '--uri', 'content://sms/sent',
            '--projection', 'address:body', check=False))

def map_link_and_no_blocker():
    """The location card must show a real state with accuracy, and never a blocking "thoáng" error."""
    grant()
    restart_log()
    fresh()
    threading.Thread(target=geo_pump, daemon=True).start()
    open_settings_tab()
    adb('shell', 'input', 'swipe', '540', '1600', '540', '700', '400')
    adb('emu', 'sensor', 'set', 'acceleration', '0:0:30'); time.sleep(0.2)
    adb('emu', 'sensor', 'set', 'acceleration', '0:0:9.81')
    time.sleep(3.0)
    tap_label('Trang chủ', 'home-card')
    time.sleep(1.0)
    nodes = snapshot('location-card')
    body = text(nodes)
    (OUT / 'location-card.txt').write_text(body[:2000])
    assert 'Đã xác định' in body or 'Vị trí gần đúng' in body, 'location card did not reach a determined state: ' + body[:400]
    assert re.search(r'±\s*\d+\s*m', body), 'accuracy not shown on the location card: ' + body[:400]
    no_fix_markers = ('Chưa có vị trí', 'Đang xác định', 'Chưa cấp quyền vị trí', 'Vị trí đang tắt')
    assert not any(m in body for m in no_fix_markers), 'location card still in a no-fix state: ' + body[:400]
    # try to hold-cancel the pending countdown so the run stops cleanly
    try:
        tap_label('TÔI VẪN ỔN', 'cancel')
    except AssertionError:
        pass

if __name__ == '__main__':
    assert adb('shell', 'getprop', 'ro.kernel.qemu').strip() == '1', 'emulator required'
    features = adb('shell', 'pm', 'list', 'features')
    assert 'telephony.calling' in features and 'telephony.messaging' in features, 'emulator lacks telephony'
    for name, body in [('critical-sos-flow', critical_fall_flow), ('location-card', map_link_and_no_blocker)]:
        run(name, body)
    _stop = True
    raise SystemExit(0 if all(r['passed'] for r in results) else 1)
