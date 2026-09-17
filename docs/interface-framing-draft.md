# IF-002 — Frame phòng thí nghiệm, bản đề xuất 0.1
TRẠNG THÁI: ĐỀ XUẤT KỸ THUẬT của Hermes, chưa phê duyệt giao thức triển khai. Không thay protocolVersion=1 của payload, UUID hoặc tên API. Chỉ được kiểm thử host; chưa bật BLE/HTTP thật. Cần review hai phía trước tích hợp, và quyết định chủ dự án nếu đổi hợp đồng đã duyệt.
Nguồn: Android §6.1,6.9,8; ESP §8.6–8.7. Phạm vi: chia JSON UTF-8 thành frame, không phát hiện ngã/không xác thực. ESP còn cần GATT MTU đo thật.

## Byte layout
Mỗi frame là một GATT characteristic value; ATT_MTU-3 là giới hạn value. Không vượt MTU, không dùng MTU như độ dài cố định. Không gộp nhiều frame trong một value.
Little-endian unsigned cho số nguyên. Header16byte:
|Offset|Bytes|Trường ngoài payload|Quy tắc|
|---|---|---|---|
|0|2|magic|0x46 0x53 (FS)|
|2|1|frameVersion|1 của draft, không phải protocolVersion payload|
|3|1|kind|1 sensor,2 event,3 status,4 command,5 commandAck; phải khớp characteristic|
|4|4|messageId|1..4294967295, unique trong phiên transport; không phải sequenceNumber/eventId|
|8|2|fragmentIndex|0-based|
|10|2|fragmentCount|1..256|
|12|2|totalLength|1..1024 byte UTF-8|
|14|2|fragmentOffset|offset byte trong toàn JSON|
|16|N|data|N>=1; offset+N<=totalLength|
Tải MTU tối thiểu23→value20→4byte data. Encoder dùng chunk=MTU-3-16, chia mảnh theo thứ tự index/offset tăng, count=ceil(total/chunk), last chunk có thể ngắn. Decoder không giả mọi message dùng MTU giống nhau; kiểm contiguous coverage khi hoàn tất. Không thay MTU giữa một message; nếu cần gửi lại toàn message.

## Giới hạn và kết thúc
- Hai message đang ráp tối đa mỗi connection; key=(connection generation,kind,messageId). Nếu đầy, từ chối message mới với lỗi cục bộ CAPACITY, không âm thầm bỏ sự kiện đang ráp. Transport event queue vẫn cần chính sách ưu tiên riêng.
- Mỗi message giữ tối đa1024byte data và metadata256fragment; gói vượt giới hạn bị reject trước cấp phát. Tổng memory bound cần đo theo implementation, không hứa RAM=2048 vì còn metadata.
- index<count<=min(256,totalLength). Mọi frame cùng key phải khớp count/totalLength. Mảnh cùng index chỉ được lặp khi offset/length/data y hệt; mâu thuẫn hủy message. Mảnh khác index không được chồng byte. Completion phải có đủ count, coverage đúng toàn bộ, index0 offset0 và contiguous offsets theo index; UTF-8/JSON được kiểm ở lớp decoder payload sau ráp.
- Hạn ráp2000ms từ mảnh đầu, monotonic; mảnh lặp không gia hạn. Tại now-start>=2000 bỏ message trước nhận mảnh tiếp theo. disconnect xóa phần đang ráp, tăng generation. Metadata completion cache có bound cần lớp trên; frame layer không thay event dedupe.
- Frame đã ráp không chứng minh JSON hợp lệ, chưa ACK_EVENT. Decode v1 thành công, lưu sự kiện bền vững rồi mới ACK_EVENT. checksum=null đến khi thống nhất.

## Gói và sự kiện
Giữ mẫu sensor/events, types/units theo interface-contract.md. Status/ACK không có sequenceNumber (C02 đề xuất theo schema cụ thể). Không loại gói có số thấp chỉ vì max_seen lớn. Dedupe sự kiện theo (pairedDeviceId,eventId) + payload fingerprint; trùng ID nhưng khác payload là lỗi, không phát lại cảnh báo. Sensor dedupe theo (pairedDeviceId,sequenceNumber) với cửa sổ hữu hạn; sự kiện đến trễ vẫn xử lý riêng. Bền vững reboot/seq reservation chưa được host demo này nghiệm thu.

## Timeout/retry đề xuất, không đã duyệt
COMMAND_ACK_TIMEOUT_MS=2000, COMMAND_MAX_RETRIES=2 (tối đa3 lần gửi tổng). Giữ commandId/payload khi retry. ACCEPTED không phải COMPLETED; completion deadline riêng5000ms, không reset vì ACCEPTED lặp. REBOOT/đổi cấu hình cần cache result để idempotent, không tự gửi lại với ID mới.
EVENT_ACK_TIMEOUT_MS=2000, EVENT_MAX_RETRIES_PER_CONNECTION=2; hết lượt giữ queue bền vững giới hạn và hiển thị chưa chuyển, chờ reconnect; ACK không tắt local alert.
RECONNECT_BACKOFF_MS thử [1000,2000,4000,8000,16000,30000], reset chỉ sau kết nối ổn định30s; không retry busy loop. Sensor gaps chỉ thống kê sau reorder window500ms, không coi mất sensor sample là mất event đã lưu. Những giá trị này cần đo BLE/pin trước chốt.

## Ma trận rủi ro đề xuất
SOS_PRESSED → khẩn ngay. IMPACT/FREE_FALL/POSTURE/INSTABILITY/INACTIVITY → detection đa dấu hiệu hoặc xác minh, CRITICAL do luật wearable đã hiệu chỉnh có thể vào đường khẩn; trước hiệu chỉnh chỉ dữ liệu test. LOW_BATTERY/SENSOR_ERROR dù severity CRITICAL là lỗi kỹ thuật, không tự gọi là ngã. SOS_CANCELLED thiếu liên kết, không replay hủy qua disconnect; không tự tắt sự kiện khác. CANCEL_ALERT phải đúng eventId và authenticated caller. Những chính sách cuối phải thống nhất Android/ESP và reviewer trước live transport.

## Nghiệm thu IF-003 trước tích hợp
Hai encoder/decoder Kotlin/C++ phải dùng chung vectors: MTU23/185, out-of-order, duplicate identical/conflicting, index/count/offset/length sai, phiên bản sai, oversize, overlap/gap, missing timeout, disconnect, capacity. Test roundtrip không đủ: cần golden hex độc lập. ACK/retry/device seq thuộc lượt sau, không tuyên bố done bằng frame test.
