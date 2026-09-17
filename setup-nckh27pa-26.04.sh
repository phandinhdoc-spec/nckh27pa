#!/usr/bin/env bash
# Ubuntu/Xubuntu 24.04 and 26.04 x86_64 — Android + Arduino IDE 2 + Arduino CLI/ESP32.
# Run: bash setup-nckh27pa-26.04.sh
# Re-running resumes installed components. Does not modify project plans.
# Running this installer accepts Android SDK licenses on your behalf.
# Sources: developer.android.com, github.com/arduino, espressif.github.io.
set -Eeuo pipefail
trap 'printf "\nLOI tai dong %s. Xem: sudo journalctl -u nckh27pa-setup --no-pager -n 100\n" "$LINENO" >&2' ERR

phase=${1:-launch}
if [[ "$phase" == launch ]]; then
  if (( EUID == 0 )); then
    target=${SUDO_USER:-}
  else
    target=$(id -un)
  fi
  [[ -n "$target" && "$target" != root ]] || { echo 'Hay chay bang tai khoan thuong: bash setup-nckh27pa.sh'; exit 1; }
  echo 'Cai Android Studio/SDK/emulator, Arduino IDE/CLI/ESP32 va cong cu Python.'
  echo 'Lenh nay chap nhan giay phep Android SDK: https://developer.android.com/studio/terms'
  echo 'Can Internet, khoang 25 GB trong; sudo chi dung cho dot cai nay.'
  sudo -v
  if sudo systemctl is-active --quiet nckh27pa-setup.service; then
    echo 'Dang cai. Xem: sudo journalctl -fu nckh27pa-setup'; exit 0
  fi
  sudo install -d -m 755 /var/lib/nckh27pa-setup
  sudo install -m 755 "$(realpath "$0")" /var/lib/nckh27pa-setup/setup.sh
  sudo systemctl reset-failed nckh27pa-setup.service 2>/dev/null || true
  sudo systemd-run --unit=nckh27pa-setup --collect \
    --description='Install nckh27pa development tools' \
    /bin/bash /var/lib/nckh27pa-setup/setup.sh worker "$target"
  echo
  echo 'DA BAT DAU CAI NEN. Co the dong SSH; khong tat may.'
  echo 'Xem tien trinh: sudo journalctl -fu nckh27pa-setup'
  echo 'Ctrl+C chi dong cua so xem nhat ky, khong dung cai dat.'
  exit 0
fi

if [[ "$phase" == worker ]]; then
  (( EUID == 0 )) || exit 1
  target=$2
  target_home=$(getent passwd "$target" | cut -d: -f6)
  [[ -d "$target_home" && "$target" != root ]] || exit 1
  exec 9>/var/lib/nckh27pa-setup/install.lock
  flock -n 9 || { echo 'Another installer is running'; exit 1; }
  . /etc/os-release
  [[ "$ID" == ubuntu && ( "$VERSION_ID" == 24.04 || "$VERSION_ID" == 26.04 ) && "$(uname -m)" == x86_64 ]] || { echo 'Chi ho tro Ubuntu/Xubuntu 24.04 hoac 26.04 x86_64'; exit 1; }
  available=$(df -Pk "$target_home" | awk 'NR==2 {print $4}')
  (( available >= 25*1024*1024 )) || { echo 'Can it nhat 25 GiB trong tren phan vung home'; exit 1; }
  export DEBIAN_FRONTEND=noninteractive NEEDRESTART_MODE=a
  apt-get -o DPkg::Lock::Timeout=300 -o Acquire::Retries=5 update
  apt-get -o DPkg::Lock::Timeout=300 -o Acquire::Retries=5 \
    -o Dpkg::Options::=--force-confdef -o Dpkg::Options::=--force-confold install -y \
    git curl wget unzip zip ca-certificates build-essential cmake ninja-build \
    pkg-config python3 python3-pip python3-venv python3-yaml \
    flex bison gperf ccache libffi-dev libssl-dev dfu-util libusb-1.0-0 \
    ripgrep jq openjdk-21-jdk android-sdk-platform-tools-common \
    qemu-system-x86 cpu-checker bluez libfuse2t64 libgtk-3-0t64 libnss3 \
    libasound2t64 libgbm1 libxss1 libxtst6 libx11-xcb1 libxcb-cursor0
  usermod -aG dialout,plugdev,kvm "$target"
  # USB debug devices: Espressif USB, Silicon Labs, WCH and FTDI.
  cat > /etc/udev/rules.d/70-nckh27pa-serial.rules <<'RULES'
SUBSYSTEM=="usb", ATTR{idVendor}=="303a", MODE="0660", GROUP="dialout", TAG+="uaccess"
SUBSYSTEM=="usb", ATTR{idVendor}=="10c4", MODE="0660", GROUP="dialout", TAG+="uaccess"
SUBSYSTEM=="usb", ATTR{idVendor}=="1a86", MODE="0660", GROUP="dialout", TAG+="uaccess"
SUBSYSTEM=="usb", ATTR{idVendor}=="0403", MODE="0660", GROUP="dialout", TAG+="uaccess"
RULES
  udevadm control --reload-rules
  udevadm trigger --subsystem-match=usb
  runuser -u "$target" -- env HOME="$target_home" USER="$target" \
    /bin/bash /var/lib/nckh27pa-setup/setup.sh user
  # Electron fallback sandbox must be root-owned; do not disable sandbox globally.
  ide_dir="$target_home/.local/opt/nckh27pa/arduino-ide"
  if [[ -f "$ide_dir/chrome-sandbox" && ! -L "$ide_dir/chrome-sandbox" ]]; then
    # Avoid installing a setuid executable in a user-writable directory.
    # Use an AppArmor profile allowing this IDE's unprivileged user namespace.
    if [[ -d /etc/apparmor.d ]]; then
      python3 - "$ide_dir/arduino-ide" > /etc/apparmor.d/nckh27pa-arduino-ide <<'PY'
import sys
p=sys.argv[1]
if any(c in p for c in '"\n\\'): raise SystemExit('Unsupported home path for AppArmor')
print('abi <abi/4.0>,\ninclude <tunables/global>\nprofile nckh27pa-arduino-ide "'+p+'" flags=(unconfined) {\n  userns,\n}')
PY
      if command -v apparmor_parser >/dev/null && systemctl is-active --quiet apparmor; then
        apparmor_parser -r /etc/apparmor.d/nckh27pa-arduino-ide
      fi
    fi
  fi
  echo 'HOAN TAT. Dang nhap lai SSH va khoi dong lai Hermes de nhan PATH/quyen USB.'
  exit 0
fi

[[ "$phase" == user && "$EUID" != 0 ]] || exit 1
BASE="$HOME/.local/opt/nckh27pa"
CACHE="$HOME/.cache/nckh27pa-setup"
BIN="$HOME/.local/bin"
SDK="$HOME/Android/Sdk"
mkdir -p "$BASE" "$CACHE" "$BIN" "$SDK" "$HOME/.config/nckh27pa" "$HOME/.local/share/applications"
exec > >(tee -a "$CACHE/install.log") 2>&1
download() {
  local url=$1 dest=$2
  if [[ ! -s "$dest" ]]; then
    curl --fail --location --retry 5 --retry-delay 3 --connect-timeout 30 \
      "$url" -o "$dest.part"
    mv "$dest.part" "$dest"
  fi
}

echo '=== Arduino CLI ==='
if [[ ! -x "$BIN/arduino-cli" ]]; then
  download https://api.github.com/repos/arduino/arduino-cli/releases/latest "$CACHE/cli-release.json"
  url=$(jq -er '.assets[] | select(.name | test("Linux_64bit.tar.gz$")) | .browser_download_url' "$CACHE/cli-release.json")
  download "$url" "$CACHE/arduino-cli.tar.gz"
  tar -xzf "$CACHE/arduino-cli.tar.gz" -C "$BIN" arduino-cli
fi
export PATH="$BIN:$PATH"
ESP_INDEX=https://espressif.github.io/arduino-esp32/package_esp32_index.json
mkdir -p "$HOME/.arduinoIDE"
# IDE 2 and CLI use the same config and core directory; preserve existing keys.
python3 - "$HOME/.arduinoIDE/arduino-cli.yaml" "$ESP_INDEX" <<'PY'
import sys,yaml,pathlib,shutil,datetime
p=pathlib.Path(sys.argv[1]); data=yaml.safe_load(p.read_text()) if p.exists() else {}
data=data or {}
if p.exists(): shutil.copy2(p,str(p)+'.backup-'+datetime.datetime.now().strftime('%Y%m%d%H%M%S'))
urls=data.setdefault('board_manager',{}).setdefault('additional_urls',[])
if sys.argv[2] not in urls: urls.append(sys.argv[2])
data.setdefault('network',{})['connection_timeout']='600s'
p.write_text(yaml.safe_dump(data,sort_keys=False))
PY
CFG="$HOME/.arduinoIDE/arduino-cli.yaml"
arduino-cli --config-file "$CFG" core update-index
if ! arduino-cli --config-file "$CFG" core list | awk '{print $1}' | rg -qx esp32:esp32; then
  arduino-cli --config-file "$CFG" core install esp32:esp32
fi

echo '=== Arduino IDE 2 ==='
if [[ ! -x "$BASE/arduino-ide/arduino-ide" ]]; then
  download https://api.github.com/repos/arduino/arduino-ide/releases/latest "$CACHE/ide-release.json"
  url=$(jq -er '.assets[] | select(.name | test("Linux_64bit.zip$")) | .browser_download_url' "$CACHE/ide-release.json")
  download "$url" "$CACHE/arduino-ide.zip"
  mkdir -p "$CACHE/ide-unpack"
  unzip -qo "$CACHE/arduino-ide.zip" -d "$CACHE/ide-unpack"
  exe=$(find "$CACHE/ide-unpack" -type f -name arduino-ide -print -quit)
  [[ -n "$exe" ]] || { echo 'Khong tim thay Arduino IDE trong zip'; exit 1; }
  mkdir -p "$BASE/arduino-ide"
  cp -a "$(dirname "$exe")/." "$BASE/arduino-ide/"
  chmod +x "$BASE/arduino-ide/arduino-ide"
fi
ln -sfn "$BASE/arduino-ide/arduino-ide" "$BIN/arduino-ide"

echo '=== Android Studio ==='
if [[ ! -x "$BASE/android-studio/bin/studio.sh" ]]; then
  download https://developer.android.com/studio "$CACHE/studio-page.html"
  url=$(python3 - "$CACHE/studio-page.html" <<'PY'
import re,sys,html
s=html.unescape(open(sys.argv[1]).read())
urls=re.findall(r'https://(?:redirector\.gvt1\.com/edgedl|edgedl\.me\.gvt1\.com|dl\.google\.com)/[^\s"<>]+linux\.tar\.gz',s)
if not urls: raise SystemExit('Khong tim thay Android Studio Linux tren trang chinh thuc; khong dung URL doan.')
print(urls[0])
PY
)
  download "$url" "$CACHE/android-studio.tar.gz"
  tar -xzf "$CACHE/android-studio.tar.gz" -C "$BASE"
fi
ln -sfn "$BASE/android-studio/bin/studio.sh" "$BIN/android-studio"
export JAVA_HOME="$BASE/android-studio/jbr"
[[ -x "$JAVA_HOME/bin/java" ]] || { echo 'Thieu JDK di kem Android Studio'; exit 1; }
export ANDROID_HOME="$SDK" ANDROID_SDK_ROOT="$SDK"
export PATH="$JAVA_HOME/bin:$SDK/platform-tools:$SDK/cmdline-tools/latest/bin:$SDK/emulator:$PATH"

echo '=== Android SDK ==='
if [[ ! -x "$SDK/cmdline-tools/latest/bin/sdkmanager" ]]; then
  download https://dl.google.com/android/repository/repository2-1.xml "$CACHE/android-repository.xml"
  python3 - "$CACHE/android-repository.xml" > "$CACHE/cmdtools-meta" <<'PY'
import xml.etree.ElementTree as E,sys
r=E.parse(sys.argv[1]).getroot()
for el in r.iter(): el.tag=el.tag.split('}')[-1]
p=next(p for p in r.findall('remotePackage') if p.get('path')=='cmdline-tools;latest')
a=next(a for a in p.findall('./archives/archive') if a.findtext('host-os')=='linux')
c=a.find('complete'); url=c.findtext('url')
print('https://dl.google.com/android/repository/'+url)
print(c.findtext('checksum'))
PY
  mapfile -t meta < "$CACHE/cmdtools-meta"
  download "${meta[0]}" "$CACHE/cmdtools.zip"
  printf '%s  %s\n' "${meta[1]}" "$CACHE/cmdtools.zip" | sha1sum -c -
  mkdir -p "$CACHE/cmdtools-unpack" "$SDK/cmdline-tools/latest"
  unzip -qo "$CACHE/cmdtools.zip" -d "$CACHE/cmdtools-unpack"
  cp -a "$CACHE/cmdtools-unpack/cmdline-tools/." "$SDK/cmdline-tools/latest/"
fi
# Capture sdkmanager status, not yes's expected SIGPIPE.
set +o pipefail
yes | sdkmanager --sdk_root="$SDK" --licenses
license_status=$?
set -o pipefail
(( license_status == 0 )) || exit "$license_status"
# Baseline API 36; actual project compileSdk/buildTools may require additions.
sdkmanager --sdk_root="$SDK" 'platform-tools' 'platforms;android-36' \
  'build-tools;36.0.0' 'emulator' 'system-images;android-36;google_apis;x86_64'
if ! avdmanager list avd -c | rg -qx NCKH27PA; then
  printf 'no\n' | avdmanager create avd --name NCKH27PA \
    --package 'system-images;android-36;google_apis;x86_64'
fi

echo '=== Python data tools ==='
[[ -x "$BASE/venv/bin/python" ]] || python3 -m venv "$BASE/venv"
"$BASE/venv/bin/python" -m pip install --disable-pip-version-check \
  pyserial numpy pandas matplotlib bleak

echo '=== Configure environment and launchers ==='
python3 - "$BASE" "$SDK" "$BIN" <<'PY'
import pathlib,sys,shlex
b,s,k=map(pathlib.Path,sys.argv[1:]); h=pathlib.Path.home(); q=shlex.quote
env=h/'.config/nckh27pa/env.sh'
env.write_text('export JAVA_HOME='+q(str(b/'android-studio/jbr'))+'\n'
 +'export ANDROID_HOME='+q(str(s))+'\nexport ANDROID_SDK_ROOT="$ANDROID_HOME"\n'
 +'export PATH="$JAVA_HOME/bin:$ANDROID_HOME/platform-tools:$ANDROID_HOME/cmdline-tools/latest/bin:$ANDROID_HOME/emulator:$HOME/.local/bin:$PATH"\n'
 +'export ARDUINO_CONFIG_FILE="$HOME/.arduinoIDE/arduino-cli.yaml"\n')
line='[ -r "$HOME/.config/nckh27pa/env.sh" ] && . "$HOME/.config/nckh27pa/env.sh"'
for name in ('.profile','.bashrc'):
 p=h/name; old=p.read_text() if p.exists() else ''
 if line not in old: p.write_text(old+'\n# nckh27pa tools\n'+line+'\n')
launchers={
 'nckh-env':'source "$HOME/.config/nckh27pa/env.sh"\nexec "$@"',
 'nckh-arduino':'source "$HOME/.config/nckh27pa/env.sh"\nexec arduino-cli --config-file "$HOME/.arduinoIDE/arduino-cli.yaml" "$@"',
 'nckh-python':'exec '+q(str(b/'venv/bin/python'))+' "$@"',
 'nckh-emulator':'source "$HOME/.config/nckh27pa/env.sh"\nexec emulator -avd NCKH27PA -memory 2048 "$@"',
 'nckh-check':'source "$HOME/.config/nckh27pa/env.sh"\njava -version\nadb version\narduino-cli version\narduino-cli --config-file "$HOME/.arduinoIDE/arduino-cli.yaml" core list\narduino-cli --config-file "$HOME/.arduinoIDE/arduino-cli.yaml" board list\nemulator -accel-check\n'
}
for name,body in launchers.items():
 p=k/name; p.write_text('#!/usr/bin/env bash\nset -e\n'+body+'\n'); p.chmod(0o755)
for name,exe in [('Arduino IDE',b/'arduino-ide/arduino-ide'),('Android Studio',b/'android-studio/bin/studio.sh')]:
 p=h/'.local/share/applications'/('nckh-'+name.lower().replace(' ','-')+'.desktop')
 p.write_text('[Desktop Entry]\nType=Application\nName='+name+'\nExec="'+str(exe)+'" %F\nTerminal=false\nCategories=Development;IDE;\n')
PY
source "$HOME/.config/nckh27pa/env.sh"
echo '=== Verification ==='
java -version
adb version
arduino-cli version
arduino-cli --config-file "$CFG" core list
"$BASE/venv/bin/python" -c 'import serial,numpy,pandas,matplotlib,bleak; print("Python tools OK")'
sdkmanager --list_installed --sdk_root="$SDK"
avdmanager list avd -c
if ! emulator -accel-check; then
  echo 'LUU Y: KVM chua san sang; bat Intel VT-x/AMD-V trong BIOS de dung emulator.'
fi
cat > "$HOME/.config/nckh27pa/setup-result.txt" <<EOF
User-stage completed: $(date -Is)
Arduino IDE: $BASE/arduino-ide
CLI config: $CFG
Android SDK: $SDK
Environment: $HOME/.config/nckh27pa/env.sh
Python: $BASE/venv/bin/python
Emulator: NCKH27PA (API 36, Google APIs, x86_64)
Log: $CACHE/install.log
Reconnect SSH; restart Hermes. Use nckh-arduino, nckh-python, nckh-env.
Hermes must read android/android-plan.md and esp/esp32-plan.md in the real project.
No plan files changed. Sensor libraries/board FQBN must match actual hardware.
Project Gradle/JDK/SDK compatibility still needs project build verification.
EOF
echo 'Cac cong cu da cai. Buoc cuoi he thong se cau hinh Arduino IDE sandbox.'
