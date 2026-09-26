#!/usr/bin/env python3
"""Clean-install acceptance test for the Android runtime-permission dialogs.

Emulator only (refuses physical devices). Requires the APK to be installed on a clean app state.
Asserts that the REAL Android system dialogs appear for Location, SMS and CALL_PHONE, that a
denied permission does not crash the app, that a permanent denial leads to App Settings, and
that the state refreshes when returning to the app. Evidence: XML dumps + results.json.
"""
import json
import os
import re
import subprocess
import sys
import time
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'docs/evidence/permission-dialog'
OUT.mkdir(parents=True, exist_ok=True)
ADB = os.environ.get('ADB', '/home/pdd/Android/Sdk/platform-tools/adb')
SERIAL = os.environ.get('ANDROID_SERIAL', 'emulator-5554')
if not SERIAL.startswith('emulator-'):
    raise SystemExit('This runner refuses physical devices')
APP = 'vn.nckh27pa.fallsafe'
PERMS = ['ACCESS_FINE_LOCATION', 'ACCESS_COARSE_LOCATION', 'SEND_SMS', 'CALL_PHONE']
results = []
dumps = {'n': 0}


def adb(*args, check=True):
    p = subprocess.run([ADB, '-s', SERIAL, *args], capture_output=True, text=True, timeout=90)
    if check and p.returncode:
        raise RuntimeError(' '.join(args) + ' -> ' + p.stdout + p.stderr)
    return p.stdout


def sh(cmd):
    return adb('shell', *cmd.split(' '))


def dump(name=None):
    dumps['n'] += 1
    tag = name or f'auto-{dumps["n"]}'
    adb('shell', 'uiautomator', 'dump', '/sdcard/acc.xml', check=False)
    adb('pull', '/sdcard/acc.xml', str(OUT / f'{tag}.xml'), check=False)
    try:
        return list(ET.parse(OUT / f'{tag}.xml').iter('node'))
    except ET.ParseError:
        return []


def texts(nodes):
    return '\n'.join((n.get('text') or '') + '|' + (n.get('content-desc') or '') for n in nodes)


def center(node):
    x1, y1, x2, y2 = map(int, re.findall(r'\d+', node.get('bounds')))
    return (x1 + x2) // 2, (y1 + y2) // 2


def scroll_up():
    adb('shell', 'input', 'swipe', '540', '600', '540', '1500', '300')
    time.sleep(0.4)


def scroll_down():
    adb('shell', 'input', 'swipe', '540', '1500', '540', '600', '400')
    time.sleep(0.4)


def tap_text(label, tries=6):
    for i in range(tries):
        for n in dump():
            if (n.get('text') or '') == label:
                adb('shell', 'input', 'tap', *map(str, center(n)))
                return True
        scroll_up()
    return False


def tap_desc(fragment, tries=8, tag='search'):
    for i in range(tries):
        for n in dump(f'{tag}-{i}'):
            if (n.get('content-desc') or '').startswith(fragment):
                adb('shell', 'input', 'tap', *map(str, center(n)))
                return True
        scroll_up()
        scroll_down()
    return False


def find_desc(fragment, tries=8, tag='find'):
    for i in range(tries):
        for n in dump(f'{tag}-{i}'):
            if (n.get('content-desc') or '').startswith(fragment):
                return True
        scroll_up()
        scroll_down()
    return False


def permission_dialog(tag):
    body = texts(dump(tag))
    if 'access this device' in body or 'make and manage phone calls' in body or 'send and view SMS' in body:
        return body
    return ''


def grant_log_count():
    out = adb('logcat', '-d', check=False)
    return sum(1 for l in out.splitlines() if 'GrantPermissionsActivity' in l and 'START' in l)


def dismiss_system_dialog():
    """A leftover Android permission dialog hides the app UI; dismiss it before driving the app."""
    body = texts(dump('z-leftover'))
    if 'access this device' in body or 'make and manage phone calls' in body or 'send and view SMS' in body:
        for label in ('Deny', 'DENY', 'Don\u2019t allow', "Don't allow", 'Không cho phép'):
            if tap_text(label, tries=1):
                time.sleep(1.2)
                return True
    return False


def grants():
    d = adb('shell', 'dumpsys', 'package', APP)
    out = {}
    for p in PERMS:
        m = re.search(r'android\.permission\.' + p + r': granted=(\w+)', d)
        out[p] = m.group(1) if m else '?'
    return out


def alive():
    return bool(sh('pidof ' + APP).strip())


def row_state(title, tag):
    for attempt in range(6):
        lines = texts(dump(f'{tag}-{attempt}')).split('\n')
        for i, line in enumerate(lines):
            if line.startswith(title):
                for follow in lines[i + 1:i + 6]:
                    f = follow.strip('|').strip()
                    if f in ('Đã cấp', 'Đã cấp (vị trí gần đúng)', 'Chưa cấp') or f.startswith('Bị từ chối'):
                        return f
        if attempt == 2:
            scroll_up()
            scroll_up()
        else:
            scroll_down()
    return ''


def current_activity():
    for line in sh('dumpsys activity activities').splitlines():
        if 'topResumedActivity' in line and 'ActivityRecord{' in line:
            inner = line.split('ActivityRecord{')[1]           # e.g. '46033707 u0 vn.../.MainActivity t71}'
            inner = inner.split('}', 1)[0].replace('t', 't')     # keep as-is
            parts = inner.split()
            if len(parts) >= 3:
                return parts[2]
            return inner
    return ''


def open_settings_tab(tries=4):
    '''Switch to the Settings tab and prove the permission section is on screen.'''
    for i in range(tries):
        body = texts(dump(f'settings-tab-{i}'))
        if 'TIẾP TỤC CẤP QUYỀN' in body:
            tap_text('ĐỂ SAU', tries=2)
            time.sleep(0.6)
            continue
        if 'QUYỀN ỨNG DỤNG' in body:
            return
        tap_text('Cài đặt', tries=2)
        time.sleep(1.1)
    raise AssertionError('Settings tab with QUYỀN ỨNG DỤNG did not open')


def open_app():
    sh('am force-stop ' + APP)
    adb('shell', 'am', 'start', '-W', '-n', APP + '/.MainActivity')
    time.sleep(2.6)


def check_clean_state():
    g = grants()
    assert all(v == 'false' for v in g.values()), 'app is not in a clean permission state: ' + json.dumps(g)


def first_run_location_dialog():
    open_app()
    assert 'TIẾP TỤC CẤP QUYỀN' in texts(dump('a1-first-run')), 'first-run explanation dialog missing'
    before = grant_log_count()
    assert tap_text('TIẾP TỤC CẤP QUYỀN'), 'could not tap the first-run continue button'
    time.sleep(2.2)
    body = permission_dialog('a2-location-dialog')
    assert body and 'location' in body, 'no Android location dialog: ' + texts(dump('a2-location-dialog'))[:200]
    assert grant_log_count() > before, 'no GrantPermissionsActivity in logcat for the location request'
    assert tap_text('While using the app'), 'could not grant location from the system dialog'
    time.sleep(1.8)
    g = grants()
    assert g['ACCESS_FINE_LOCATION'] == 'true' and g['ACCESS_COARSE_LOCATION'] == 'true', 'location not granted: ' + json.dumps(g)


def center_phone_dialog():
    open_app()
    open_settings_tab()
    before = grant_log_count()
    assert tap_desc('CẤP QUYỀN cho 📞 Điện thoại'), 'phone CẤP QUYỀN button not found'
    time.sleep(2.2)
    body = permission_dialog('a3-phone-dialog')
    assert body and 'phone calls' in body, 'no Android phone-call dialog: ' + str(body)[:160]
    assert grant_log_count() > before, 'no GrantPermissionsActivity for CALL_PHONE'
    assert tap_text('Allow'), 'could not grant CALL_PHONE'
    time.sleep(1.8)
    assert grants()['CALL_PHONE'] == 'true', 'CALL_PHONE not granted'


def center_sms_dialog():
    dismiss_system_dialog()
    open_settings_tab()
    before = grant_log_count()
    assert tap_desc('CẤP QUYỀN cho 💬 SMS'), 'SMS CẤP QUYỀN button not found'
    time.sleep(2.2)
    body = permission_dialog('a4-sms-dialog')
    assert body and 'SMS' in body, 'no Android SMS dialog: ' + str(body)[:160]
    assert grant_log_count() > before, 'no GrantPermissionsActivity for SEND_SMS'
    assert tap_text('Allow'), 'could not grant SEND_SMS'
    time.sleep(1.8)
    assert grants()['SEND_SMS'] == 'true', 'SEND_SMS not granted'


def approve_then_deny(steps):
    """Deny CALL_PHONE through the system dialog: `steps` denials, then expect the App Settings path."""
    adb('shell', 'pm', 'revoke', APP, 'android.permission.CALL_PHONE')
    time.sleep(0.6)
    dismiss_system_dialog()
    open_app()
    open_settings_tab()
    denied = 0
    for attempt in range(steps):
        if not tap_desc('CẤP QUYỀN cho 📞 Điện thoại', tag=f'b-denial{attempt}'):
            break
        time.sleep(2.0)
        if not permission_dialog(f'b-denial-dialog{attempt}'):
            break
        if not tap_text('Don\u2019t allow'):
            tap_text("Don't allow")
        denied += 1
        time.sleep(1.6)
        assert alive(), 'app died after denial ' + str(attempt + 1)
    state = row_state('📞 Điện thoại', 'c-after-denials')
    body = texts(dump('c-message'))
    if steps >= 2 or state.startswith('Bị từ chối'):
        assert state.startswith('Bị từ chối'), 'no permanent-denial state: ' + state
        assert 'Quyền này đang bị tắt. Hãy bật trong Cài đặt.' in body, 'missing exact settings message: ' + body[:300]
        assert find_desc('MỞ CÀI ĐẶT ỨNG DỤNG cho 📞 Điện thoại', tag='c-button'), 'no App Settings button'
    else:
        assert denied >= 1, f'expected at least one system dialog, saw {denied}'
        assert not state.startswith('Đã cấp'), 'denied permission shown as granted: ' + state


def settings_round_trip():
    assert tap_desc('MỞ CÀI ĐẶT ỨNG DỤNG cho 📞 Điện thoại', tag='d-open'), 'could not tap App Settings'
    time.sleep(2.8)
    screen = texts(dump('d-app-settings'))
    assert 'Uninstall' in screen or 'Force stop' in screen, 'App Settings (App info) did not open: ' + screen[:200]
    assert not current_activity().startswith(APP), 'the app is still in front after opening App Settings'
    adb('shell', 'input', 'keyevent', 'KEYCODE_BACK')
    time.sleep(1.8)
    assert alive(), 'app died returning from Android Settings'
    assert current_activity().startswith(APP), 'the app did not resume: ' + current_activity()
    state = row_state('📞 Điện thoại', 'd-back')
    assert state.startswith('Bị từ chối'), 'state lost after Settings round trip: ' + state


def resume_refresh():
    adb('shell', 'pm', 'grant', APP, 'android.permission.CALL_PHONE')
    adb('shell', 'input', 'keyevent', 'KEYCODE_HOME')
    time.sleep(1.0)
    open_app()
    open_settings_tab()
    state = row_state('📞 Điện thoại', 'e-granted')
    assert state.startswith('Đã cấp'), 'grant inside Android Settings not reflected on resume: ' + state


CHECKS = {
    'clean-state': check_clean_state,
    'first-run-location-dialog': first_run_location_dialog,
    'center-phone-dialog': center_phone_dialog,
    'center-sms-dialog': center_sms_dialog,
    'deny-once': lambda: approve_then_deny(1),
    'deny-twice-and-settings': lambda: (approve_then_deny(2), settings_round_trip()),
    'resume-refresh': resume_refresh,
}


def run(name, body):
    start = time.monotonic()
    try:
        body()
        results.append({'name': name, 'passed': True, 'elapsedSeconds': round(time.monotonic() - start, 2)})
        print('PASS', name, flush=True)
    except Exception as e:  # noqa: BLE001
        results.append({'name': name, 'passed': False, 'error': str(e)[:1500]})
        print('FAIL', name, str(e)[:1500], flush=True)
        try:
            dump(name + '-FAILURE')
        except Exception:
            pass
    finally:
        (OUT / 'acceptance-results.json').write_text(json.dumps(results, ensure_ascii=False, indent=2) + '\n')


if __name__ == '__main__':
    assert sh('getprop ro.kernel.qemu').strip() == '1', 'emulator required'
    selected = sys.argv[1:] or list(CHECKS)
    for name in selected:
        run(name, CHECKS[name])
    passed = sum(1 for r in results if r['passed'])
    print(f'\n{passed}/{len(results)} checks passed', flush=True)
    raise SystemExit(0 if passed == len(results) else 1)
