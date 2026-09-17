#!/usr/bin/env python3
"""Exercise Android SensorManager path with emulator-controlled acceleration, not replay."""
from pathlib import Path
import runpy
import time

ROOT=Path(__file__).resolve().parents[2]
m=runpy.run_path(str(ROOT/'android/scripts/emulator-smoke.py'))
g=m['fresh'].__globals__
g['OUT']=ROOT/'docs/evidence/android-env/sensor-scroll-final'
g['OUT'].mkdir(parents=True,exist_ok=True)
a=m['adb']
assert a('shell','getprop','ro.kernel.qemu').strip()=='1'

def check_flow():
    m['fresh']();m['tap_label']('Cài đặt','settings')
    a('shell','input','swipe','160','470','160','120','300')
    a('shell','input','swipe','160','470','160','120','300')
    a('emu','sensor','set','acceleration','0:0:30');time.sleep(.2)
    a('emu','sensor','set','acceleration','0:0:9.81');time.sleep(1.5)
    nodes=m['snapshot']('verification-visible')
    m['require'](nodes,'Bạn có ổn không?');m['require'](nodes,'TÔI VẪN ỔN')
    time.sleep(11)
    m['tap_label']('Trang chủ','home')
    nodes=m['snapshot']('sensor-path-sent')
    m['require'](nodes,'PHONE_ONLY');m['require'](nodes,'Đã gửi THỬ NGHIỆM')

if __name__=='__main__':
    m['run']('sensor-api-scroll-verification-timeout',check_flow)
    raise SystemExit(0 if all(x['passed'] for x in g['results']) else 1)
