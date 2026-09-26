#!/usr/bin/env python3
"""Post-implementation UI regression on a named emulator only; never physical devices."""
import json
import os
from pathlib import Path
import re
import subprocess
import time
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'docs/evidence/android-env/smoke'
OUT.mkdir(parents=True, exist_ok=True)
ADB = os.environ.get('ADB', '/home/pdd/Android/Sdk/platform-tools/adb')
SERIAL = os.environ.get('ANDROID_SERIAL', 'emulator-5554')
if not SERIAL.startswith('emulator-'):
    raise SystemExit('This runner refuses physical devices')
APP = 'vn.nckh27pa.fallsafe'
results = []

def adb(*args):
    p = subprocess.run([ADB, '-s', SERIAL, *args], capture_output=True, text=True, timeout=30)
    if p.returncode:
        raise RuntimeError(p.stdout + p.stderr)
    return p.stdout

def snapshot(name):
    adb('shell', 'uiautomator', 'dump', '/sdcard/nckh-window.xml')
    target = OUT / (name + '.xml')
    adb('pull', '/sdcard/nckh-window.xml', str(target))
    return list(ET.parse(target).iter('node'))

def text(nodes):
    return '\n'.join(n.get('text', '') + ' ' + n.get('content-desc', '') for n in nodes)

def center(node):
    x1,y1,x2,y2 = map(int, re.findall(r'\d+', node.get('bounds')))
    return (x1+x2)//2, (y1+y2)//2

def tap_label(label, name):
    for attempt in range(8):
        nodes = snapshot(name + '-locate-' + str(attempt))
        matches = [n for n in nodes if n.get('text') == label or n.get('content-desc','').startswith(label) or (label.endswith(':') and n.get('text','').startswith(label))]
        if matches:
            matches.sort(key=lambda n: (n.get('clickable') != 'true', n.get('content-desc') != label))
            x,y = center(matches[0]); adb('shell','input','tap',str(x),str(y)); return (x,y)
        # Coordinates derive from actual screen bounds, excluding bottom nav/status bars.
        width = max(int(re.findall(r'\d+', n.get('bounds', ''))[2]) for n in nodes)
        height = max(int(re.findall(r'\d+', n.get('bounds', ''))[3]) for n in nodes)
        adb('shell','input','swipe',str(width//2),str(height*3//4),str(width//2),str(height//2),'1000')
    raise AssertionError('Not visible: ' + label)

def fresh():
    adb('shell','am','force-stop',APP)
    adb('shell','am','start','-W','-n',APP+'/.MainActivity')

def require(nodes, phrase):
    assert phrase in text(nodes), (phrase, text(nodes))

def run(name, body):
    start=time.monotonic()
    try:
        body()
        results.append({'name':name,'passed':True,'elapsedSeconds':round(time.monotonic()-start,2)})
        print('PASS',name,flush=True)
    except Exception as e:
        results.append({'name':name,'passed':False,'error':str(e)})
        print('FAIL',name,str(e),flush=True)
    finally:
        (OUT/'results.json').write_text(json.dumps(results,ensure_ascii=False,indent=2)+'\n')

def no_response():
    fresh();tap_label('Chạy dữ liệu mô phỏng','timeout')
    time.sleep(1.6)
    require(snapshot('timeout-verifying'),'Bạn có ổn không?')
    time.sleep(11)
    require(snapshot('timeout-sent'),'Đã gửi THỬ NGHIỆM')

def safe():
    fresh();tap_label('Chạy dữ liệu mô phỏng','safe');time.sleep(1.6)
    tap_label('TÔI VẪN ỔN','safe-action');time.sleep(10.5)
    nodes=snapshot('safe-cancelled');require(nodes,'Theo dõi thử nghiệm')
    assert 'Đã gửi THỬ NGHIỆM' not in text(nodes)
    tap_label('Người thân','safe-sink');require(snapshot('safe-sink-empty'),'Sink đã nhận 0 cảnh báo giả')

def sos():
    fresh()
    nodes=snapshot('sos-home')
    button=next(n for n in nodes if n.get('content-desc','').startswith('SOS giữ'))
    x,y=center(button)
    adb('shell','input','tap',str(x),str(y));time.sleep(2.3)
    require(snapshot('sos-short-ignored'),'Theo dõi thử nghiệm')
    adb('shell','input','swipe',str(x),str(y),str(x),str(y),'2300')
    require(snapshot('sos-long-sent'),'Đã gửi THỬ NGHIỆM')

def failure():
    fresh();tap_label('Cài đặt','fail-settings');tap_label('Giả lập lỗi gửi','fail-toggle')
    tap_label('Trang chủ','fail-home');tap_label('Chạy dữ liệu mô phỏng','fail-replay');time.sleep(13)
    require(snapshot('fail-state'),'Gửi GIẢ thất bại')
    tap_label('Người thân','fail-sink');require(snapshot('fail-sink-empty'),'Sink đã nhận 0 cảnh báo giả')

def pause_replay():
    fresh();tap_label('Chạy dữ liệu mô phỏng','pause-replay');time.sleep(.45)
    adb('shell','input','keyevent','KEYCODE_HOME');time.sleep(13)
    adb('shell','am','start','-W','-n',APP+'/.MainActivity')
    require(snapshot('pause-replay-sent'),'Đã gửi THỬ NGHIỆM')

def tabs_and_sensors():
    fresh()
    require(snapshot('phone-only'),'Có mẫu gia tốc')
    tap_label('Sự kiện','events');require(snapshot('events-empty'),'Chưa có sự kiện')
    tap_label('Người thân','contacts');require(snapshot('contacts-fake'),'Người thân mô phỏng')
    tap_label('Cài đặt','settings');tap_label('Gia tốc m/s²:','sensor-readout');require(snapshot('settings-sensors'),'Gia tốc m/s²:')

if __name__=='__main__':
    assert adb('shell','getprop','ro.kernel.qemu').strip() == '1'
    for name,body in [('no-response',no_response),('safe',safe),('sos-short-long',sos),('sink-failure',failure),('replay-survives-pause',pause_replay),('tabs-and-sensors',tabs_and_sensors)]:
        run(name,body)
    raise SystemExit(0 if all(r['passed'] for r in results) else 1)
