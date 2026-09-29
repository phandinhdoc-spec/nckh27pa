# Dự án Thiên Nhãn

## Mục tiêu
Thiên Nhãn là thiết bị hỗ trợ người khiếm thị, dùng Raspberry Pi Zero 2 W làm thiết bị đầu cuối và MacBook làm máy chủ xử lý trung tâm.

## Nền tảng
- Raspberry Pi Zero 2 W.
- DietPi.
- Tailscale.
- MacBook Apple Silicon.
- Groq Whisper Large V3 Turbo.
- Apple Vision.
- Gemini API.
- TTS trên Mac.
- Camera và tai nghe Bluetooth.

## Nguyên tắc
- Pi chỉ xử lý phần cứng, thu thập dữ liệu và giao tiếp.
- Mac xử lý STT, Vision, AI, điều phối và TTS.
- Ưu tiên framework Apple; chỉ gọi Gemini khi cần.
- Không dùng Dashboard, Groq, hotspot, hostapd, dnsmasq hoặc Raspberry Pi OS.
