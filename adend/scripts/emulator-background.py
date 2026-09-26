#!/usr/bin/env python3
"""Emulator-only runtime checks for explicit foreground monitoring and notification denial."""
from pathlib import Path
import json
import runpy
import time

ROOT = Path(__file__).resolve().parents[2]
m = runpy.run_path(str(ROOT / 'android/scripts/emulator-smoke.py'))
g = m['fresh'].__globals__
OUT = ROOT / 'docs/evidence/android-env/background-final'
OUT.mkdir(parents=True, exist_ok=True)
g['OUT'] = OUT
adb=m['adb']; tap=m['tap_label']; snapshot=m['snapshot']; require=m['require']
assert adb('shell','getprop','ro.kernel.qemu').strip() == '1'
APP='vn.nckh27pa.fallsafe'

def service(name):
    data=adb('shell','dumpsys','activity','services',APP)
    (OUT/(name+'.txt')).write_text(data)
    return data

def notification_reset():
    adb('shell','pm','revoke',APP,'android.permission.POST_NOTIFICATIONS')
    adb('shell','pm','clear-permission-flags',APP,'android.permission.POST_NOTIFICATIONS','user-set','user-fixed')

def start_with_permission(label, expected_active=True):
    notification_reset();m['fresh']()
    tap('Cài đặt','settings');tap('Bật giám sát nền thử','start')
    tap(label,'permission');time.sleep(1)
    snapshot('after-permission')
    assert ('isForeground=true' in service('foreground-'+label.replace(' ','-'))) == expected_active, 'unexpected service state after permission result' 

def background_flow():
    start_with_permission('Allow')
    adb('shell','input','keyevent','KEYCODE_HOME')
    time.sleep(1)
    assert 'isForeground=true' in service('home-service')
    sensors=adb('shell','dumpsys','sensorservice');(OUT/'sensors-background.txt').write_text(sensors)
    assert APP in sensors
    notifications=adb('shell','dumpsys','notification','--noredact')
    (OUT/'notification.txt').write_text(notifications)
    assert APP in notifications and 'GIÁM SÁT THỬ NGHIỆM' in notifications
    adb('shell','am','start','-W','-n',APP+'/.MainActivity')
    tap('Trang chủ','home');tap('Chạy dữ liệu mô phỏng','replay');time.sleep(1.6)
    require(snapshot('verifying'),'Bạn có ổn không?')
    adb('shell','input','keyevent','KEYCODE_SLEEP')
    power=adb('shell','dumpsys','power');(OUT/'power-during-verifying.txt').write_text(power)
    assert 'nckh27pa:verification' in power
    time.sleep(12)
    power=adb('shell','dumpsys','power');(OUT/'power-after-deadline.txt').write_text(power)
    # Active wake lock line must be absent; historic counters may remain elsewhere.
    assert not any('PARTIAL_WAKE_LOCK' in line and 'nckh27pa:verification' in line for line in power.splitlines())
    adb('shell','input','keyevent','KEYCODE_WAKEUP');adb('shell','wm','dismiss-keyguard')
    adb('shell','am','start','-W','-n',APP+'/.MainActivity')
    require(snapshot('screen-off-no-response'),'Đã gửi THỬ NGHIỆM')
    tap('Hoàn tất sự kiện để thử lại','complete');tap('Cài đặt','stop-settings')
    tap('Dừng giám sát nền thử','stop');time.sleep(1)
    assert 'isForeground=true' not in service('stopped')

def denied_notifications():
    start_with_permission('Don’t allow', expected_active=False)
    require(snapshot('denied-visible'),'Chưa bật giám sát nền vì quyền thông báo bị từ chối')
    assert 'isForeground=true' not in service('denied-stopped')
    tap('Trang chủ','local-return');require(snapshot('local-restored'),'Có mẫu gia tốc')

if __name__=='__main__':
    m['run']('foreground-home-screen-off-stop',background_flow)
    m['run']('notification-denied-local-restored',denied_notifications)
    raise SystemExit(0 if all(x['passed'] for x in g['results']) else 1)
