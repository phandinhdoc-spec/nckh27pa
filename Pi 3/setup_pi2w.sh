#!/usr/bin/env bash
set -Eeuo pipefail

# Setup toi gian cho Raspberry Pi Zero 2 W chay DietPi Trixie ARM64.
# Chay bang: sudo bash setup_pi2w.sh

HEADSET_MAC="${HEADSET_MAC:-04:60:61:EF:78:22}"
HEADSET_A2DP_DEVICE="bluealsa:DEV=${HEADSET_MAC},PROFILE=a2dp"
WIFI_FALLBACK_SSID="${WIFI_FALLBACK_SSID:-Pdmq}"
WIFI_FALLBACK_PSK="${WIFI_FALLBACK_PSK:-12345678}"
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
INSTALL_DIR="/opt/thiennhan"
SERVICE_NAME="thiennhan.service"
BOOT_CONFIG="/boot/firmware/config.txt"
OVERLAY_README="/boot/firmware/overlays/README"
MAX98357A_OVERLAY="max98357a"
REBOOT_NEEDED=0

CAMERA_STATUS="chua kiem tra"
BT_STATUS="chua kiem tra"
TAILSCALE_STATUS="chua kiem tra"
ALSA_STATUS="chua kiem tra"
SERVICE_STATUS="chua cai"

log() {
    printf '\n==> %s\n' "$*"
}

ok() {
    printf '  [OK] %s\n' "$*"
}

warn() {
    printf '  [CAN LUU Y] %s\n' "$*"
}

fail() {
    printf '  [LOI] %s\n' "$*" >&2
    exit 1
}

need_cmd() {
    command -v "$1" >/dev/null 2>&1
}

require_root() {
    if [[ "${EUID}" -ne 0 ]]; then
        fail "Hay chay bang: sudo bash setup_pi2w.sh"
    fi
}

require_arm64() {
    local machine arch
    machine="$(uname -m)"
    arch="$(dpkg --print-architecture)"

    [[ "${machine}" == "aarch64" ]] || fail "uname -m la '${machine}', khong phai aarch64. Dung DietPi Trixie ARM64."
    [[ "${arch}" == "arm64" ]] || fail "dpkg architecture la '${arch}', khong phai arm64. Dung DietPi Trixie ARM64."
}

apt_install_minimal() {
    log "Cap nhat he thong va cai goi toi thieu"

    export DEBIAN_FRONTEND=noninteractive
    apt update
    apt full-upgrade -y
    if [[ -f /var/run/reboot-required ]]; then
        REBOOT_NEEDED=1
        warn "He thong yeu cau reboot sau khi full-upgrade."
    fi

    local camera_pkg="rpicam-apps"
    apt-cache show "${camera_pkg}" >/dev/null 2>&1 || \
        fail "Khong tim thay rpicam-apps. Runtime bat buoc co lenh rpicam-jpeg; kiem tra repository DietPi/Raspberry Pi."

    apt install -y --no-install-recommends \
        ca-certificates \
        curl \
        "${camera_pkg}" \
        alsa-utils \
        lame \
        mpg123 \
        bluez \
        bluetooth \
        bluez-alsa-utils \
        libasound2-plugin-bluez \
        python3 \
        python3-gpiozero \
        python3-serial \
        python3-smbus2

    ok "Da cai cac goi toi thieu, camera package: ${camera_pkg}"
}

install_enable_tailscale() {
    log "Kiem tra va bat Tailscale"

    if ! need_cmd tailscale; then
        warn "Chua co Tailscale, dang cai bang script chinh thuc cua Tailscale. Khong nhung auth key."
        curl -fsSL https://tailscale.com/install.sh | sh
    fi

    systemctl enable --now tailscaled.service

    if tailscale status >/dev/null 2>&1; then
        TAILSCALE_STATUS="da cai va da dang nhap"
        ok "Tailscale dang hoat dong."
    else
        TAILSCALE_STATUS="da cai, chua dang nhap"
        warn "Tailscale chua dang nhap. Sau khi script xong, chay: sudo tailscale up"
    fi
}

configure_bluealsa() {
    log "Cau hinh BlueALSA: chi giu A2DP playback cho tai nghe (loa headset)"

    need_cmd bluealsa || fail "Da cai package nhung khong tim thay lenh bluealsa."

    local help
    help="$(bluealsa --help 2>&1 || true)"
    grep -q "a2dp-source" <<<"${help}" || fail "BlueALSA khong ho tro profile a2dp-source."

    # Mic khong con thu qua tai nghe (da dung mic USB rieng), nen chi can A2DP
    # de phat MP3 phan hoi ra loa tai nghe. Khong bat HFP/HSP/SCO capture.
    options="-p a2dp-source"

    install -d -o root -g root -m 0755 /etc/systemd/system/bluealsa.service.d
    {
        printf '%s\n' '[Service]' 'ExecStart='
        printf 'ExecStart=/usr/bin/bluealsa -S %s\n' "${options}"
    } >/etc/systemd/system/bluealsa.service.d/override.conf
    chmod 0644 /etc/systemd/system/bluealsa.service.d/override.conf

    systemctl daemon-reload
    systemctl enable --now bluealsa.service
    systemctl restart bluealsa.service
    systemctl is-active --quiet bluealsa.service || fail "bluealsa.service khong active."

    aplay -L 2>/dev/null | grep -qi bluealsa || \
        fail "ALSA khong thay PCM bluealsa; kiem tra libasound2-plugin-bluez."
    ok "BlueALSA da bat chi A2DP-source (loa tai nghe)."
}

bluetooth_info_has() {
    local field="$1"
    LC_ALL=C timeout 8s bluetoothctl info "${HEADSET_MAC}" 2>/dev/null | grep -Fq "${field}: yes"
}

setup_bluetooth_headset() {
    log "Bat Bluetooth va ket noi tai nghe ${HEADSET_MAC}"

    systemctl enable --now bluetooth.service
    sleep 2

    if need_cmd rfkill; then
        rfkill unblock bluetooth || true
    fi

    bluetoothctl power on >/dev/null 2>&1 || true
    warn "Hay dua tai nghe vao pairing mode. Script se scan truoc khi pair va thu toi da 2 lan."

    local connected=0
    local attempt
    for attempt in 1 2; do
        printf '  Thu ket noi Bluetooth lan %s/2...\n' "${attempt}"

        if ! LC_ALL=C bluetoothctl info "${HEADSET_MAC}" 2>/dev/null | grep -Fq "Device ${HEADSET_MAC}"; then
            timeout 18s bluetoothctl --timeout 15 scan on >/dev/null 2>&1 || true
        fi

        if ! bluetooth_info_has "Paired"; then
            timeout 35s bluetoothctl --agent NoInputNoOutput --timeout 30 pair "${HEADSET_MAC}" >/dev/null 2>&1 || true
        fi
        timeout 12s bluetoothctl trust "${HEADSET_MAC}" >/dev/null 2>&1 || true
        timeout 25s bluetoothctl connect "${HEADSET_MAC}" >/dev/null 2>&1 || true
        bluetoothctl scan off >/dev/null 2>&1 || true

        if bluetooth_info_has "Paired" && bluetooth_info_has "Trusted" && bluetooth_info_has "Connected"; then
            connected=1
            break
        fi
        sleep 4
    done

    bluetoothctl scan off >/dev/null 2>&1 || true
    if [[ "${connected}" -ne 1 ]]; then
        BT_STATUS="chua ket noi ${HEADSET_MAC}; se dung loa MAX98357A"
        warn "Khong thay/ket noi duoc tai nghe sau 2 lan; tiep tuc cai dat, runtime se dung loa MAX98357A."
        return 0
    fi

    BT_STATUS="da pair/trust/connect ${HEADSET_MAC}"
    ok "Da pair/trust/connect tai nghe ${HEADSET_MAC}."
}

check_bluetooth_audio_profiles() {
    log "Kiem tra A2DP playback ra tai nghe (loa headset)"

    if ! bluetooth_info_has "Connected"; then
        warn "Tai nghe chua ket noi; bo qua A2DP test. Setup se tiep tuc voi loa MAX98357A."
        return 0
    fi

    local wav="/dev/shm/thiennhan-bluealsa-test.wav"
    python3 - "${wav}" <<'PY'
import struct
import sys
import wave

path = sys.argv[1]
with wave.open(path, "wb") as output:
    output.setnchannels(1)
    output.setsampwidth(2)
    output.setframerate(16000)
    output.writeframes(struct.pack("<h", 0) * 800)
PY

    if ! timeout 12s aplay -q -D "${HEADSET_A2DP_DEVICE}" "${wav}"; then
        rm -f "${wav}"
        fail "Khong mo duoc PCM A2DP ${HEADSET_A2DP_DEVICE}."
    fi
    rm -f "${wav}"

    ok "A2DP playback ra tai nghe mo duoc bang ALSA."
}

overlay_exists() {
    if [[ -f "${OVERLAY_README}" ]] && grep -Eq "^[[:space:]]*Name:[[:space:]]+${MAX98357A_OVERLAY}([[:space:]]|$)" "${OVERLAY_README}"; then
        return 0
    fi

    [[ -f "/boot/firmware/overlays/${MAX98357A_OVERLAY}.dtbo" ]]
}

ensure_config_line() {
    local line="$1"
    local file="$2"

    touch "${file}"
    if grep -Fxq "${line}" "${file}"; then
        ok "Da co san: ${line}"
    else
        printf '%s\n' "${line}" >>"${file}"
        ok "Da them: ${line}"
        REBOOT_NEEDED=1
    fi
}

configure_camera_boot() {
    log "Cau hinh camera boot"

    if [[ ! -d /boot/firmware ]]; then
        CAMERA_STATUS="khong thay /boot/firmware"
        fail "Khong thay /boot/firmware; khong the cau hinh camera cho DietPi Trixie."
    fi
    ensure_config_line "camera_auto_detect=1" "${BOOT_CONFIG}"
    CAMERA_STATUS="da cau hinh boot; cho test sau reboot neu config vua thay doi"
}

configure_max98357a() {
    log "Cau hinh ALSA/I2S cho MAX98357A"

    if [[ ! -d /boot/firmware ]]; then
        ALSA_STATUS="khong thay /boot/firmware"
        fail "Khong thay /boot/firmware; khong the cau hinh MAX98357A."
    fi

    if ! overlay_exists; then
        ALSA_STATUS="khong tim thay overlay ${MAX98357A_OVERLAY}"
        fail "Khong tim thay overlay '${MAX98357A_OVERLAY}' trong ${OVERLAY_README} hoac /boot/firmware/overlays/${MAX98357A_OVERLAY}.dtbo."
    fi

    ensure_config_line "dtparam=audio=off" "${BOOT_CONFIG}"
    ensure_config_line "dtparam=i2s=on" "${BOOT_CONFIG}"
    ensure_config_line "dtoverlay=${MAX98357A_OVERLAY}" "${BOOT_CONFIG}"

    ALSA_STATUS="da cau hinh ${MAX98357A_OVERLAY}: GPIO18 BCLK, GPIO19 LRC/WS, GPIO21 DIN"
    warn "MAX98357A dung I2S mac dinh cua Pi: GPIO18 BCLK, GPIO19 LRC/WS, GPIO21 DIN."
}

configure_sensor_boot() {
    log "Cau hinh UART va I2C cho cam bien GY25/GY63"

    if [[ ! -d /boot/firmware ]]; then
        fail "Khong thay /boot/firmware; khong the cau hinh UART/I2C cho cam bien."
    fi

    ensure_config_line "enable_uart=1" "${BOOT_CONFIG}"
    ensure_config_line "dtparam=i2c_arm=on" "${BOOT_CONFIG}"

    ok "Da bat UART (/dev/serial0 cho GY25) va I2C (bus 1 cho MS5611/GY63)."
}

check_camera() {
    log "Kiem tra camera bang rpicam-jpeg"

    if [[ "${REBOOT_NEEDED}" -eq 1 ]]; then
        CAMERA_STATUS="cho reboot truoc khi test camera"
        warn "Da sua ${BOOT_CONFIG}; bo qua camera test trong lan nay. Chay lai setup sau reboot de test JPEG."
        return 0
    fi

    if ! need_cmd rpicam-jpeg; then
        CAMERA_STATUS="khong co rpicam-jpeg"
        fail "Khong tim thay rpicam-jpeg sau khi cai camera package."
    fi

    if ! timeout 15s rpicam-jpeg --version >/tmp/rpicam-version.out 2>/tmp/rpicam-version.err; then
        CAMERA_STATUS="rpicam-jpeg --version loi"
        fail "rpicam-jpeg --version loi; xem /tmp/rpicam-version.err."
    fi

    local jpg="/dev/shm/ai_green_eye_camera_test.jpg"
    rm -f "${jpg}"

    if ! timeout 20s rpicam-jpeg -n -t 1000 -o "${jpg}" >/tmp/rpicam-capture.out 2>/tmp/rpicam-capture.err; then
        CAMERA_STATUS="chup anh that bai"
        rm -f "${jpg}"
        fail "Camera chup that bai; xem /tmp/rpicam-capture.err."
    fi

    if [[ ! -s "${jpg}" ]]; then
        CAMERA_STATUS="file JPEG rong"
        rm -f "${jpg}"
        fail "Camera tao file JPEG rong."
    fi

    if head -c 2 "${jpg}" | od -An -tx1 | grep -qi "ff d8"; then
        CAMERA_STATUS="OK"
        ok "Camera chup JPEG hop le."
    else
        CAMERA_STATUS="header JPEG khong hop le"
        rm -f "${jpg}"
        fail "File camera khong co header JPEG hop le."
    fi

    rm -f "${jpg}"
}

configure_wifi_fallback() {
    log "Them WiFi du phong SSID '${WIFI_FALLBACK_SSID}' (hotspot dien thoai)"

    local wpa="/etc/wpa_supplicant/wpa_supplicant.conf"
    if [[ ! -f "${wpa}" ]]; then
        warn "Khong thay ${wpa}. DietPi co the dung NetworkManager/systemd-networkd thay wpa_supplicant."
        warn "Chua them WiFi du phong. Cau hinh thu cong qua dietpi-config hoac nmcli."
        return 0
    fi

    if ! systemctl is-active --quiet wpa_supplicant; then
        warn "wpa_supplicant dang khong chay; WiFi co the do NetworkManager quan ly."
        warn "Bo qua them WiFi du phong de tranh pha cau hinh mang chinh."
        return 0
    fi

    if grep -q "ssid=\"${WIFI_FALLBACK_SSID}\"" "${wpa}"; then
        ok "Da co san wifi du phong ${WIFI_FALLBACK_SSID} trong ${wpa}."
        return 0
    fi

    # Chi them 1 network block moi, khong ghi de cac network san co. Priority
    # am de wpa_supplicant uu tien mang chinh (mac dinh 0) khi ca hai cung co.
    printf '%s\n' \
        "" \
        "network={" \
        "    ssid=\"${WIFI_FALLBACK_SSID}\"" \
        "    psk=\"${WIFI_FALLBACK_PSK}\"" \
        "    key_mgmt=WPA-PSK" \
        "    priority=-5" \
        "}" >>"${wpa}"

    chmod 0600 "${wpa}"
    chown root:root "${wpa}"
    systemctl restart wpa_supplicant >/dev/null 2>&1 || wpa_cli -i wlan0 reconfigure >/dev/null 2>&1 || true
    ok "Da them wifi du phong ${WIFI_FALLBACK_SSID} vao ${wpa}. Khong dam bao tu chuyen mang."
}

install_application_service() {
    log "Cai ung dung va systemd service"

    [[ -f "${SCRIPT_DIR}/.env" ]] || fail "Thieu ${SCRIPT_DIR}/.env; khong cai service khi chua co cau hinh."
    [[ -f "${SCRIPT_DIR}/thiennhan.service" ]] || fail "Thieu file thiennhan.service."

    if ! id -u thiennhan >/dev/null 2>&1; then
        useradd --system --user-group --home-dir "${INSTALL_DIR}" --shell /usr/sbin/nologin thiennhan
    fi
    local group
    for group in audio video bluetooth gpio dialout i2c; do
        if getent group "${group}" >/dev/null 2>&1; then
            usermod -a -G "${group}" thiennhan
        fi
    done

    install -d -o thiennhan -g thiennhan -m 0750 "${INSTALL_DIR}" "${INSTALL_DIR}/history"
    install -d -o thiennhan -g thiennhan -m 0755 "${INSTALL_DIR}/sounds"
    install -o thiennhan -g thiennhan -m 0644 \
        "${SCRIPT_DIR}/app.py" \
        "${SCRIPT_DIR}/button_runtime.py" \
        "${SCRIPT_DIR}/config.py" \
        "${SCRIPT_DIR}/devices.py" \
        "${SCRIPT_DIR}/groq_stt.py" \
        "${SCRIPT_DIR}/history.py" \
        "${SCRIPT_DIR}/mac_client.py" \
        "${SCRIPT_DIR}/part2_runtime.py" \
        "${SCRIPT_DIR}/sensors.py" \
        "${INSTALL_DIR}/"
    cp -a "${SCRIPT_DIR}/sounds/." "${INSTALL_DIR}/sounds/"
    chown -R thiennhan:thiennhan "${INSTALL_DIR}/sounds"
    install -o thiennhan -g thiennhan -m 0600 "${SCRIPT_DIR}/.env" "${INSTALL_DIR}/.env"
    if [[ -f "${SCRIPT_DIR}/.api" ]]; then
        install -o thiennhan -g thiennhan -m 0600 "${SCRIPT_DIR}/.api" "${INSTALL_DIR}/.api"
    fi
    install -o root -g root -m 0755 "${SCRIPT_DIR}/play-audio" /usr/local/bin/play-audio
    install -o root -g root -m 0644 "${SCRIPT_DIR}/thiennhan.service" "/etc/systemd/system/${SERVICE_NAME}"

    systemctl daemon-reload
    systemctl enable "${SERVICE_NAME}"
    if [[ "${REBOOT_NEEDED}" -eq 1 ]]; then
        SERVICE_STATUS="da cai va enable; se start sau reboot"
        ok "Da cai ${SERVICE_NAME}; chua start vi boot config can reboot."
    else
        systemctl restart "${SERVICE_NAME}"
        systemctl is-active --quiet "${SERVICE_NAME}" || {
            systemctl status --no-pager "${SERVICE_NAME}" || true
            fail "${SERVICE_NAME} khong active."
        }
        SERVICE_STATUS="active"
        ok "${SERVICE_NAME} da active."
    fi
}

print_report() {
    local machine arch tailscale_hint reboot_hint
    machine="$(uname -m)"
    arch="$(dpkg --print-architecture)"

    tailscale_hint=""
    if [[ "${TAILSCALE_STATUS}" == *"chua dang nhap"* ]]; then
        tailscale_hint=" Can chay: sudo tailscale up"
    fi

    reboot_hint="khong"
    if [[ "${REBOOT_NEEDED}" -eq 1 ]]; then
        reboot_hint="co, de ap dung boot config/kernel moi"
    fi

    cat <<REPORT

================ BAO CAO SETUP PI ZERO 2 W ================
Kien truc:
  uname -m:              ${machine}
  dpkg architecture:     ${arch}

Camera:
  Trang thai:            ${CAMERA_STATUS}

Bluetooth:
  Trang thai:            ${BT_STATUS}
  Tai nghe co dinh:      ${HEADSET_MAC}

Tailscale:
  Trang thai:            ${TAILSCALE_STATUS}${tailscale_hint}

ALSA/MAX98357A:
  Trang thai:            ${ALSA_STATUS}
  Chan dau noi:          GPIO18 BCLK, GPIO19 LRC/WS, GPIO21 DIN

Mic/Wake:
  Mic:                   USB, luong mo lien tuc + VAD cuc bo (khong button/GPIO)
  Tu khoa:               "Thien Nhan" (khong phan biet dau/hoa thuong)
  STT:                   MP3 (lame) -> Groq Whisper (GROQ_API_KEY)

Phat am thanh:
  Loa:                   MAX98357A (moi prompt/tra loi)
  Bluetooth:             chi ket noi khi nguoi dung yeu cau bang giong noi

Ung dung:
  Service:               ${SERVICE_STATUS}

Can reboot:
  ${reboot_hint}

Script khong tu reboot. Neu muc "Can reboot" la co, hay chay:
  sudo reboot
============================================================
REPORT
}

main() {
    require_root
    require_arm64
    systemctl stop "${SERVICE_NAME}" >/dev/null 2>&1 || true
    apt_install_minimal
    install_enable_tailscale
    configure_bluealsa
    setup_bluetooth_headset
    check_bluetooth_audio_profiles
    configure_camera_boot
    configure_max98357a
    configure_sensor_boot
    check_camera
    configure_wifi_fallback
    install_application_service
    print_report
}

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
    main "$@"
fi
