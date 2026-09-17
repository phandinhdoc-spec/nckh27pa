# Xử lý review FGS
AGY review đầu FAIL. Findings:
1. “resetDetection hủy countdown”: không đúng source. DemoSession.resetDetection chỉ reset DemoDetector, không chạm AlertCore; test pause deadline đang pass. Không sửa countdown thành reset.
2. Start FGS fail bất đồng bộ mất collector và mất message: đúng. Giữ Activity collector tới khi service startForeground thành công, callback handoff; failed onCreate không cleanup/reset/message chung. OwnershipTests RED trước lớp mới, đưa lớp vào actual Activity.
3. postDelayed100ms stop race: đúng, thay bằng onMonitoringChanged sau onDestroy cleanup. “Stop disabled during active” là yêu cầu có chủ đích; không coi là bug, user vẫn stop bằng Android task manager.
4. Wake clamp11s vs countdown>10s: ngoài phạm vi hiện tại DemoSession hardcodes core mặc định10s; khi cho cấu hình dài phải sửa/test. Không claim hỗ trợ countdown dài. bounded wakelock không acquire lại mỗi poll.
Runtime riêng: request notification result lúc paused bị bỏ qua start. UI+dumpsys tái hiện RED, bổ sung pending start đến onResume; không startFGS trái hạn chế background. Callback signature Array<String> theo compile SDK/ComponentActivity actual, build lỗi ban đầu đã sửa. Giữ tất cả log.
