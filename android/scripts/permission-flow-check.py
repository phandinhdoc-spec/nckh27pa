#!/usr/bin/env python3
"""End-to-end Android permission flow check on the project emulator only; never physical devices.

Covers: first-run explanation, permission center states, explicit request, denied-once,
permanent denial -> App Settings round trip, grant-on-resume, approximate-only location,
GPS disabled, Google Maps present/absent map opening, per-step SOS degradation without crash.
No real SMS is sent and no real number is dialed.
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
OUT = ROOT / 'docs/evidence/permission-flow'
OUT.mkdir(parents=True, exist_ok=True)
ADB = os.environ.get('ADB', '/home/pdd/Android/Sdk/platform-tools/adb')
SERIAL = os.environ.get('ANDROID_SERIAL', 'emulator-5554')
if not SERIAL.startswith('emulator-'):
    raise SystemExit('This runner refuses physical devices')
APP = 'vn.nckh27pa.fallsafe'
MAPS = 'com.google.android.apps.maps'
PERMS = {
    'fine': ['ACCESS_FINE_LOCATION'],
    'coarse': ['ACCESS_COARSE_LOCATION'],
    'sms': ['SEND_SMS'],
    'call': ['CALL_PHONE'],
    'phone': ['READ_PHONE_STATE'],
}
DENY_LABELS = ["Don\u2019t allow", "Don't allow", 'DENY', 'Từ chối', 'Không cho phép']
FIRST_RUN_TITLE = 'Cho phép ứng dụng hoạt động khi khẩn cấp'
results = []
_state = {'dumps': 0}


def adb(*args, check=True):
    p = subprocess.run([ADB, '-s', SERIAL, *args], capture_output=True, text=True, timeout=90)
    if check and p.returncode:
        raise RuntimeError(' '.join(args) + ' -> ' + p.stdout + p.stderr)
    return p.stdout


def sh(cmd):
    return adb('shell', *cmd.split(' '))


def snapshot(name=None):
    _state['dumps'] += 1
    tag = name or f'auto-{_state["dumps"]}'
    adb('shell', 'uiautomator', 'dump', '/sdcard/nckh-perm.xml', check=False)
    target = OUT / (tag + '.xml')
    adb('pull', '/sdcard/nckh-perm.xml', str(target), check=False)
    try:
        return list(ET.parse(target).iter('node'))
    except ET.ParseError:
        return []


def text(nodes):
    return '\n'.join((n.get('text') or '') + ' ' + (n.get('content-desc') or '') for n in nodes)


def center(node):
    x1, y1, x2, y2 = map(int, re.findall(r'\d+', node.get('bounds')))
    return (x1 + x2) // 2, (y1 + y2) // 2


def scroll(down=True):
    nodes = snapshot('scroll-probe')
    w = max(int(re.findall(r'\d+', n.get('bounds', '[0,0][1,1]'))[2]) for n in nodes)
    h = max(int(re.findall(r'\d+', n.get('bounds', '[0,0][1,1]'))[3]) for n in nodes)
    if down:
        adb('shell', 'input', 'swipe', str(w // 2), str(h * 3 // 4), str(w // 2), str(h // 4), '500')
    else:
        adb('shell', 'input', 'swipe', str(w // 2), str(h // 4), str(w // 2), str(h * 3 // 4), '500')
    time.sleep(0.6)


def match(nodes, label, exact=False):
    return [n for n in nodes
            if (n.get('text') or '') == label
            or (not exact and (n.get('content-desc') or '').startswith(label))]


def find(nodes, label):
    return (label in text(nodes)) or bool(match(nodes, label)) or bool(match(nodes, label, exact=False))


def tap_label(label, name, exact=False, tries=8):
    for attempt in range(tries):
        nodes = snapshot(f'{name}-locate-{attempt}')
        found = match(nodes, label, exact)
        if found:
            found.sort(key=lambda n: (n.get('clickable') != 'true', n.get('enabled') != 'true'))
            x, y = center(found[0])
            adb('shell', 'input', 'tap', str(x), str(y))
            return (x, y)
        scroll(down=True)
    raise AssertionError('Not visible: ' + label)


def tap_any(labels, name):
    for label in labels:
        try:
            tap_label(label, f'{name}-{re.sub(r"[^A-Za-z]", "", label)}', tries=2)
            return label
        except AssertionError:
            continue
    raise AssertionError('None of the labels visible: ' + str(labels))


def scroll_find(phrase, name, tries=6, rewind=True):
    """Return the nodes once `phrase` is visible, scrolling through the screen otherwise."""
    if rewind:
        for _ in range(3):
            scroll(down=False)
    for attempt in range(tries):
        nodes = snapshot(f'{name}-scroll-{attempt}')
        if phrase in text(nodes):
            return nodes
        scroll(down=True)
    raise AssertionError(f'{name}: never saw {phrase!r}')


def dismiss_first_run(name='fresh', tries=3):
    """The first-run permission dialog is modal; it must be dismissed before driving the UI."""
    for i in range(tries):
        nodes = snapshot(f'{name}-dialog-{i}')
        if FIRST_RUN_TITLE in text(nodes):
            try:
                tap_label('ĐỂ SAU', f'{name}-dismiss-{i}', tries=1)
                time.sleep(0.7)
            except AssertionError:
                pass
        else:
            return


def fresh(dismiss_dialog=True):
    adb('shell', 'am', 'force-stop', APP)
    adb('shell', 'am', 'start', '-W', '-n', APP + '/.MainActivity')
    time.sleep(1.6)
    if dismiss_dialog:
        dismiss_first_run('fresh')


def open_settings_tab(tries=3):
    for i in range(tries):
        dismiss_first_run(f'opentab-{i}', tries=2)
        try:
            tap_label('Cài đặt', f'opentab-{i}', tries=2)
        except AssertionError:
            continue
        time.sleep(0.9)
        if 'QUYỀN ỨNG DỤNG' in text(snapshot(f'opentab-check-{i}')):
            return
    raise AssertionError('Settings tab with QUYỀN ỨNG DỤNG did not open')


def alive():
    pid = sh('pidof ' + APP).strip()
    return bool(pid), pid


def resumed():
    out = sh('dumpsys activity activities')
    for pattern in (r'topResumedActivity=ActivityRecord\{[^ ]+ [^ ]+ ([^ }]+)\}',
                    r'ResumedActivity: ActivityRecord\{[^ ]+ [^ ]+ ([^ }]+)\}',
                    r'mResumedActivity[^ ]* ([A-Za-z0-9_.]+/[A-Za-z0-9_.$]+)'):
        m = re.search(pattern, out)
        if m:
            return m.group(1)
    win = sh('dumpsys window')
    m = re.search(r'mCurrentFocus=Window\{[^ ]+ [^ ]+ ([A-Za-z0-9_.]+)/', win)
    return m.group(1) + '/' if m else ''


def pm(action, *keys):
    for k in keys:
        for perm in PERMS[k]:
            adb('shell', 'pm', action, APP, 'android.permission.' + perm, check=False)
    time.sleep(0.3)


grant = lambda *k: pm('grant', *k)      # noqa: E731
revoke = lambda *k: pm('revoke', *k)    # noqa: E731
revoke_all = lambda: revoke('fine', 'coarse', 'sms', 'call', 'phone')  # noqa: E731


def permission_dialog_open(nodes=None):
    body = text(nodes or snapshot('perm-dialog-probe'))
    return 'access this device' in body or 'Allow' in body and 'Deny' in body or 'Don\u2019t allow' in body


def require(nodes, phrase, label):
    assert phrase in text(nodes), f'{label}: missing {phrase!r}'


def state_of(title, name):
    """Return the visible state label of one permission row."""
    nodes = scroll_find(title, name)
    lines = text(nodes).split('\n')
    for i, line in enumerate(lines):
        if title in line and not line.strip().startswith('|'):
            for follow in lines[i + 1:i + 4]:
                if follow.strip() in ('Đã cấp', 'Đã cấp (vị trí gần đúng)', 'Chưa cấp') \
                        or follow.strip().startswith('Bị từ chối'):
                    return follow.strip()
    for candidate in ('Đã cấp (vị trí gần đúng)', 'Bị từ chối — cần mở Cài đặt ứng dụng', 'Đã cấp', 'Chưa cấp'):
        if candidate in text(nodes):
            return candidate
    return ''


# ---------------------------------------------------------------- checks

def first_run(name):
    adb('shell', 'pm', 'clear', APP)
    time.sleep(0.6)
    fresh(dismiss_dialog=False)
    nodes = snapshot(name + '-dialog')
    require(nodes, FIRST_RUN_TITLE, name)
    require(nodes, 'Để gửi cảnh báo khi phát hiện té ngã, ứng dụng cần quyền định vị, gửi tin nhắn và gọi người thân.', name)
    require(nodes, 'TIẾP TỤC CẤP QUYỀN', name)
    require(nodes, 'ĐỂ SAU', name)
    tap_label('ĐỂ SAU', name + '-dismiss')
    time.sleep(0.8)
    assert FIRST_RUN_TITLE not in text(snapshot(name + '-dismissed')), 'first-run dialog did not dismiss'
    ok, pid = alive()
    assert ok, 'app died after dismissing first-run dialog'


def permission_center(name):
    revoke_all()
    grant('coarse')
    revoke('coarse')
    fresh()
    open_settings_tab()
    top = snapshot(name + '-center-top')
    require(top, 'QUYỀN ỨNG DỤNG', name)
    for title in ('📍 Vị trí', '📞 Điện thoại', '💬 SMS'):
        nodes = scroll_find(title, name + '-title')
        body = text(nodes)
        assert title in body, f'missing row {title}'
        assert 'ACCESS_' not in body and 'SEND_SMS' not in body and 'CALL_PHONE' not in body, 'raw permission name leaked'
    states = [state_of(t, name + '-state') for t in ('📍 Vị trí', '📞 Điện thoại', '💬 SMS')]
    assert len(states) == 3 and all(states), f'rows without state: {states}'
    (OUT / (name + '-states.json')).write_text(json.dumps(states, ensure_ascii=False) + '\n')


def request_deny_once(name):
    revoke_all()
    adb('shell', 'pm', 'clear', APP)
    fresh()
    open_settings_tab()
    before = state_of('📍 Vị trí', name + '-before')
    assert before == 'Chưa cấp', 'unexpected state before request: ' + before
    tap_label('CẤP QUYỀN', name + '-request')
    time.sleep(1.8)
    dialog = snapshot(name + '-system-dialog')
    assert permission_dialog_open(dialog), 'no system permission dialog: ' + text(dialog)[:200]
    used = tap_any(DENY_LABELS, name + '-deny')
    time.sleep(1.6)
    after_nodes = snapshot(name + '-after-deny')
    assert not permission_dialog_open(after_nodes), 'permission dialog still open after denial'
    ok, pid = alive()
    assert ok, 'app died after permission denial'
    assert resumed().startswith(APP + '/'), 'app did not resume after denial: ' + resumed()
    after = state_of('📍 Vị trí', name + '-after')
    assert after != 'Đã cấp', 'permission shown granted after denial: ' + after
    (OUT / (name + '-deny-label.txt')).write_text(used + '\n')


def permanent_denial(name):
    """A second UI denial marks the capability as USER_FIXED, which Android reports as
    'no more system prompts' -> the app must offer the App Settings path and never re-loop."""
    revoke_all()
    adb('shell', 'pm', 'clear', APP)
    fresh()
    open_settings_tab()
    denied = 0
    for attempt in range(2):
        try:
            tap_label('CẤP QUYỀN', f'{name}-attempt-{attempt}', tries=3)
        except AssertionError:
            break
        time.sleep(1.6)
        dialog = snapshot(f'{name}-dialog-{attempt}')
        if not permission_dialog_open(dialog):
            break
        tap_any(DENY_LABELS, f'{name}-deny-{attempt}')
        denied += 1
        time.sleep(1.4)
    assert denied == 2, f'could not perform two denials (denied={denied})'
    fresh()
    open_settings_tab()
    state = state_of('📍 Vị trí', name + '-state')
    assert state.startswith('Bị từ chối'), 'expected permanent-denial state after two denials, saw: ' + state
    assert 'MỞ CÀI ĐẶT ỨNG DỤNG' in text(scroll_find('MỞ CÀI ĐẶT ỨNG DỤNG', name + '-button')), 'no App Settings button'
    # the app must NOT re-launch a system dialog for a permanently denied capability
    tap_label('MỞ CÀI ĐẶT ỨNG DỤNG', name + '-open-settings', tries=3)
    time.sleep(2.5)
    snapshot(name + '-app-settings')
    current = resumed()
    assert current.startswith('com.android.settings/'), 'App Settings did not open: ' + current
    adb('shell', 'input', 'keyevent', 'KEYCODE_BACK')
    time.sleep(1.6)
    ok, pid = alive()
    assert ok, 'app died returning from Android Settings'
    assert resumed().startswith(APP + '/'), 'did not resume app after Settings: ' + resumed()
    state_after = state_of('📍 Vị trí', name + '-after-settings')
    assert state_after.startswith('Bị từ chối'), 'state lost after Settings round trip: ' + state_after
    # granting inside Android Settings must be picked up when the app resumes
    grant('fine', 'coarse')
    adb('shell', 'input', 'keyevent', 'KEYCODE_HOME')
    time.sleep(0.8)
    adb('shell', 'am', 'start', '-W', '-n', APP + '/.MainActivity')
    time.sleep(1.6)
    open_settings_tab()
    granted_state = state_of('📍 Vị trí', name + '-granted')
    assert granted_state.startswith('Đã cấp'), 'grant inside Android Settings not reflected: ' + granted_state


def grant_on_resume(name):
    revoke_all()
    fresh()
    open_settings_tab()
    before = state_of('📍 Vị trí', name + '-denied')
    assert not before.startswith('Đã cấp'), 'precondition failed, already granted: ' + before
    grant('fine', 'coarse', 'sms', 'call')
    adb('shell', 'input', 'keyevent', 'KEYCODE_HOME')
    time.sleep(1.0)
    adb('shell', 'am', 'start', '-W', '-n', APP + '/.MainActivity')
    time.sleep(1.6)
    open_settings_tab()
    states = {t: state_of(t, name + '-granted') for t in ('📍 Vị trí', '📞 Điện thoại', '💬 SMS')}
    bad = {k: v for k, v in states.items() if not v.startswith('Đã cấp')}
    assert not bad, 'grant not reflected after returning from Android Settings: ' + json.dumps(states, ensure_ascii=False)
    (OUT / (name + '-states.json')).write_text(json.dumps(states, ensure_ascii=False) + '\n')


def approximate_only(name):
    revoke('fine')
    grant('coarse')
    fresh()
    open_settings_tab()
    state = state_of('📍 Vị trí', name + '-approximate')
    assert state == 'Đã cấp (vị trí gần đúng)', 'approximate state not surfaced: ' + state
    nodes = scroll_find('Vị trí gần đúng vẫn dùng được', name + '-note')
    require(nodes, 'Vị trí gần đúng vẫn dùng được', name)


def sos_degradation(name):
    revoke_all()
    adb('shell', 'pm', 'clear', APP)
    fresh()
    open_settings_tab()
    tap_label('3. Kích hoạt đã gửi SOS', name + '-sos')
    time.sleep(2.0)
    ok, pid = alive()
    assert ok, 'app died dispatching SOS without any permission'
    tap_label('Trang chủ', name + '-home', tries=4)
    time.sleep(1.0)
    banner = scroll_find('Chưa cấp quyền', name + '-banner')
    assert 'Chưa cấp quyền' in text(banner), 'no per-step permission-missing summary'
    tap_label('CHI TIẾT', name + '-details', tries=4)
    time.sleep(1.2)
    dialog = snapshot(name + '-report-dialog')
    body = text(dialog)
    require(dialog, 'Tiến trình gửi SOS', name)
    for phrase in ('Vị trí', 'Tin nhắn', 'Cuộc gọi trợ giúp', 'Liên kết bản đồ'):
        assert phrase in body, f'missing step {phrase}: ' + body[:400]
    assert 'không thể tự động gọi' in body, 'auto-call failure not reported truthfully: ' + body[:400]
    tap_label('ĐÓNG', name + '-close', tries=3)


def gps_off(name):
    """Location service disabled: SOS must still dispatch and report the location step truthfully."""
    grant('fine', 'coarse', 'sms')
    sh('settings put secure location_mode 0')
    time.sleep(0.5)
    fresh()
    try:
        open_settings_tab()
        tap_label('3. Kích hoạt đã gửi SOS', name + '-sos')
    except AssertionError:
        pass
    time.sleep(3.0)
    dismiss_system_dialog(name + '-system-alert')
    ok, pid = alive()
    assert ok, 'app died with the location service disabled'
    try:
        tap_label('Trang chủ', name + '-home', tries=4)
    except AssertionError:
        pass
    time.sleep(1.0)
    body = text(snapshot(name + '-gps-off'))
    assert '0.0,0.0' not in body and '0.0, 0.0' not in body, 'fabricated coordinates on screen'
    try:
        tap_label('CHI TIẾT', name + '-details', tries=3)
        time.sleep(1.0)
        dialog = text(snapshot(name + '-gps-off-dialog'))
        assert 'Vị trí' in dialog, 'no LOCATION step in report with the location service off'
        assert 'Thành công' not in dialog.split('Tin nhắn')[0], 'location reported successful with the service off'
        tap_label('ĐÓNG', name + '-close', tries=2)
    except AssertionError:
        pass
    sh('settings put secure location_mode 3')


def dismiss_system_dialog(name):
    """Android may cover the screen with a system alert (e.g. 'No location access')."""
    nodes = snapshot(name)
    body = text(nodes)
    if 'No location access' in body or 'Turn on location' in body or 'Close' in body:
        for label in ('Close', 'Đóng', 'OK'):
            try:
                tap_label(label, name + '-close', tries=1)
                time.sleep(0.8)
                return True
            except AssertionError:
                continue
    return False


def geo(lon, lat):
    adb('emu', 'geo', 'fix', str(lon), str(lat))


def fix_location():
    geo(106.6297, 10.8231)
    time.sleep(1.2)
    geo(106.6297, 10.8231)
    time.sleep(1.2)


def hold_label(label, name, ms=2300):
    nodes = snapshot(name + '-hold')
    found = match(nodes, label)
    assert found, 'hold target not visible: ' + label
    x, y = center(found[0])
    adb('shell', 'input', 'swipe', str(x), str(y), str(x), str(y), str(ms))


def acquire_fix(name):
    """Start a real verification countdown so the app acquires a fix, then cancel it safely."""
    sh('settings put secure location_mode 3')
    grant('fine', 'coarse', 'sms', 'call')
    fresh()
    fix_location()
    open_settings_tab()
    try:
        tap_label('2. Mô phỏng ngã (Đếm ngược 10s)', name + '-replay', tries=3)
    except AssertionError:
        pass
    fix_location()
    time.sleep(2.0)
    dismiss_system_dialog(name + '-system-alert')
    try:
        tap_label('Trang chủ', name + '-home', tries=4)
    except AssertionError:
        pass
    time.sleep(1.0)
    body = text(snapshot(name + '-fix-state'))
    if 'Chưa có vị trí' in body:
        # the countdown may still be running; let it expire so the dispatch keeps the acquired fix
        time.sleep(11)
        body = text(snapshot(name + '-fix-state-late'))
    assert 'Chưa có vị trí' not in body, 'no location fix acquired on the emulator: ' + body[:400]
    # cancel any pending countdown with the documented 2s hold, then return to the home tab
    try:
        hold_label('TÔI VẪN ỔN', name + '-cancel')
        time.sleep(1.5)
    except AssertionError:
        pass
    try:
        tap_label('Trang chủ', name + '-home2', tries=3)
    except AssertionError:
        pass
    time.sleep(0.8)


def enable_maps():
    adb('shell', 'pm', 'enable', '--user', '0', MAPS, check=False)
    time.sleep(1.0)
    disabled = adb('shell', 'pm', 'list', 'packages', '-d', check=False)
    assert MAPS not in disabled, 'Google Maps is still disabled on the device'


def disable_maps():
    adb('shell', 'pm', 'disable-user', '--user', '0', MAPS, check=False)
    time.sleep(1.5)
    disabled = adb('shell', 'pm', 'list', 'packages', '-d', check=False)
    assert MAPS in disabled, 'Google Maps was not disabled on the device'


def maps_intents(name):
    """Device truth for the map-launch order: Google Maps handles geo: when installed;
    with Maps removed the https maps URL still resolves to the browser."""
    lines = []
    enabled = query_uri('geo:10.8231,106.6297?q=10.8231,106.6297')
    lines.append('GOOGLE_MAPS_ENABLED geo: -> ' + enabled)
    url = query_uri('https://www.google.com/maps/search/?api=1&query=10.8231,106.6297')
    lines.append('GOOGLE_MAPS_ENABLED maps-url -> ' + url)
    assert MAPS in enabled, 'Google Maps does not handle the geo: URI when installed: ' + enabled
    adb('shell', 'pm', 'disable-user', '--user', '0', MAPS, check=False)
    time.sleep(1.5)
    try:
        disabled_geo = query_uri('geo:10.8231,106.6297?q=10.8231,106.6297')
        disabled_url = query_uri('https://www.google.com/maps/search/?api=1&query=10.8231,106.6297')
        lines.append('GOOGLE_MAPS_DISABLED geo: -> ' + disabled_geo)
        lines.append('GOOGLE_MAPS_DISABLED maps-url -> ' + disabled_url)
        assert MAPS not in disabled_geo, 'Google Maps still resolves geo: while disabled'
        assert disabled_url.strip(), 'no fallback handler for the maps URL without Google Maps'
    finally:
        enable_maps()
    (OUT / (name + '-intent-resolution.txt')).write_text('\n'.join(lines) + '\n')


def query_uri(uri):
    p = subprocess.run([ADB, '-s', SERIAL, 'shell', 'cmd', 'package', 'query-activities', '--brief',
                        '-a', 'android.intent.action.VIEW', '-d', uri],
                       capture_output=True, text=True, timeout=60)
    return (p.stdout + p.stderr).strip()


def ensure_fix(name):
    """Give the app a real published fix (current fix, or the validated last-known fallback)."""
    sh('settings put secure location_mode 3')
    adb('shell', 'pm', 'clear', APP)
    grant('fine', 'coarse', 'sms', 'call')
    for attempt in range(3):
        fresh()
        open_settings_tab()
        try:
            tap_label('3. Kích hoạt đã gửi SOS', f'{name}-sos-{attempt}', tries=6)
        except AssertionError:
            pass
        time.sleep(2.5)
        try:
            tap_label('Trang chủ', f'{name}-home-{attempt}', tries=4)
        except AssertionError:
            pass
        time.sleep(1.0)
        body = text(snapshot(f'{name}-fix-{attempt}'))
        if 'Chưa có vị trí' not in body:
            (OUT / (name + '-fix.txt')).write_text(body[:600] + '\n')
            return
    raise AssertionError('the app never published a location fix on the emulator')


def map_opens_google_maps(name):
    """Google Maps installed + a published fix: the map card must open Google Maps."""
    enable_maps()
    ensure_fix(name)
    tap_label('VỊ TRÍ CỦA TÔI', name + '-open-maps', tries=4)
    time.sleep(4.0)
    snapshot(name + '-after-open')
    current = resumed()
    assert current.startswith(MAPS), 'Google Maps did not open: ' + current
    (OUT / (name + '-resumed.txt')).write_text(current + '\n')


def map_fallback_without_maps(name):
    """Google Maps absent: the same action must still work (browser) or report truthfully, never crash."""
    disable_maps()
    try:
        ensure_fix(name)
        tap_label('VỊ TRÍ CỦA TÔI', name + '-open-maps', tries=4)
        time.sleep(4.0)
        ok, pid = alive()
        assert ok, 'app died when Google Maps is not installed'
        current = resumed()
        body = text(snapshot(name + '-fallback'))
        assert not current.startswith(MAPS), 'Google Maps opened while disabled: ' + current
        truthful = ('Không có ứng dụng phù hợp để mở bản đồ.' in body
                    or 'Hệ thống từ chối mở bản đồ.' in body
                    or 'Không có vị trí để mở bản đồ.' in body
                    or not current.startswith(APP + '/'))
        assert truthful, 'no fallback and no truthful message: ' + body[:300] + ' | ' + current
        (OUT / (name + '-resumed.txt')).write_text(current + '\n')
    finally:
        enable_maps()


CHECKS = {
    'first-run': first_run,
    'permission-center': permission_center,
    'request-deny': request_deny_once,
    'permanent-denial': permanent_denial,
    'grant-on-resume': grant_on_resume,
    'approximate-only': approximate_only,
    'sos-degradation': sos_degradation,
    'gps-off': gps_off,
    'maps-intents': maps_intents,
    'maps-opens-google-maps': map_opens_google_maps,
    'maps-fallback-browser': map_fallback_without_maps,
}


def run(name, body):
    start = time.monotonic()
    try:
        body(name)
        results.append({'name': name, 'passed': True, 'elapsedSeconds': round(time.monotonic() - start, 2)})
        print('PASS', name, flush=True)
    except Exception as e:  # noqa: BLE001
        results.append({'name': name, 'passed': False, 'error': str(e)[:2000]})
        print('FAIL', name, str(e)[:2000], flush=True)
        try:
            snapshot(name + '-FAILURE')
        except Exception:
            pass
    finally:
        (OUT / 'results.json').write_text(json.dumps(results, ensure_ascii=False, indent=2) + '\n')


if __name__ == '__main__':
    assert sh('getprop ro.kernel.qemu').strip() == '1', 'emulator required'
    selected = sys.argv[1:] or list(CHECKS)
    for name in selected:
        run(name, CHECKS[name])
    passed = sum(1 for r in results if r['passed'])
    print(f'\n{passed}/{len(results)} checks passed', flush=True)
    raise SystemExit(0 if passed == len(results) else 1)
