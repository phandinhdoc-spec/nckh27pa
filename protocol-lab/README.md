# IF-003 host framing lab — PROPOSAL, CHƯA APPROVED

Thực thi `docs/tasks/IF-003.md` và byte layout của
`docs/interface-framing-draft.md` 0.1. Chỉ chạy host Kotlin/C++, không tích hợp
Android/ESP, BLE/HTTP, không sửa payload API v1, UUID hay checksum.
`frameVersion=1` là version lớp ngoài, không phải thay đổi `protocolVersion`.

## Chạy offline

```sh
./protocol-lab/run.sh
SANITIZE=1 ./protocol-lab/run.sh
```

Runner dùng Python 3 stdlib, g++, kotlinc từ Android Studio và JBR local;
không Gradle/download. Có thể đặt `KOTLINC` (đường dẫn executable) và
`JAVA_HOME` nếu Studio nằm nơi khác. Build chỉ ghi vào `protocol-lab/build/`
(được ignore). `SANITIZE=1` bật C++ AddressSanitizer + UndefinedBehaviorSanitizer;
Mặc định `detect_leaks=0` vì LeakSanitizer không chạy được dưới ptrace của
sandbox này (lỗi được giữ ở `tests/sanitizer-unavailable.log`); chưa kiểm leak.
Có thể override `ASAN_OPTIONS` trên host hỗ trợ leak detection.
`-no-pie` tránh va chạm địa chỉ shadow memory trong môi trường host.

Các file chính:

- `kotlin/Framing.kt`, `cpp/framing.hpp`: API encoder và reassembler thuần host.
- `kotlin/Main.kt`, `cpp/main.cpp`: harness dòng lệnh, không phải parser network.
- `fixtures/golden.json`: literal payload/frame hex chung, tạo độc lập bằng
  byte layout và `struct.pack('<BBIHHHH', ...)`, không lấy output encoder làm oracle.
  Bao gồm MTU23 với JSON v1, MTU185 với UTF-8, MTU23 với 1024 byte/256 fragment.
- `tests/run.py`: cùng input và expected output cho cả hai runtime, thêm encode
  Kotlin → ráp C++ và encode C++ → ráp Kotlin theo thứ tự đảo ngược.
- `tests/red.log`, `tests/green.log`, `tests/sanitizer.log`: bằng chứng thực thi.

TDD: toàn bộ 52 scenario được viết và chạy với hai stub trước implementation:
104 behavior checks đỏ (`UNIMPLEMENTED`); sau đó 104 checks xanh và 56 lượt
cross-language. Các nhóm behavior: golden encoder/decoder, validation header,
characteristic, ID unsigned tối đa, giới hạn encoder, reorder, identical/conflicting
index, count/total mismatch, overlap/gap/order coverage, capacity, timeout,
disconnect/generation, clock regression, opaque bytes và không completion cache.
Test cross-language còn so header/data với oracle Python độc lập, không chỉ roundtrip.

## Semantics và giới hạn

Mỗi instance dành cho **một connection**, truy cập tuần tự, tối đa hai message.
Caller cấp messageId duy nhất trong phiên; encoder không tự cấp ID và không đổi
MTU giữa các fragment. Decoder nhận mỗi characteristic value riêng, không yêu
cầu các mảnh bằng độ dài chunk encoder. Adapter tương lai phải kiểm value không
vượt negotiated ATT_MTU−3; host decoder chỉ kiểm bound frame 17..1040 byte.

`receive(now, generation, characteristicKind, bytes)` kiểm clock, expire slot,
kiểm generation, rồi toàn bộ giới hạn header trước khi nhận slot/copy data.
Frame malformed trả `INVALID`, giữ message còn hiệu lực; frame hợp lệ nhưng
metadata/duplicate/overlap mâu thuẫn trả `CONFLICT` và hủy đúng message đó.
Đầy hai slot trả `CAPACITY`, không loại message khác. `COMPLETE` trả bản sao
payload và giải phóng slot sau khi kiểm đủ fragment, index0 offset0 và coverage
liên tục theo index tới totalLength. Không kiểm UTF-8/JSON ở lớp này.

Clock là millisecond monotonic không âm, không phải wall clock; adapter nên dùng
elapsed time từ một mốc ổn định. Hạn cố định 2000ms từ mảnh đầu, không reset do
mảnh mới hay duplicate. `tick(now)` cho phép expire khi không có traffic; caller
phải schedule tick nếu cần giải phóng đúng hạn lúc im lặng. Mọi receive cũng expire
trước xử lý frame. Clock đi lùi trả `CLOCK` và không thay state. Sau timeout,
mảnh cùng ID có thể bắt đầu ráp mới: draft chưa có tombstone/quarantine.

`disconnect()` xóa cả hai slot và tăng generation, không reset clock.
Callback phải mang generation đã capture lúc đăng ký phiên; không được gắn
callback cũ vào generation hiện hành. Generation cũ trả `STALE`.
Không cache completion: replay trọn message có thể trả COMPLETE lần nữa.
Generation exhaustion fail closed, không wrap; host CLI chỉ dùng giá trị nhỏ
trong phần miền số chung Kotlin/C++. Key thực tế là instance/generation/kind/ID.

### Memory và thời gian xử lý

C++ giữ hai slot inline, mỗi slot chứa data[1024], offsets[256] và lengths[256]
uint16, cùng scalar. `sizeof(lab::Reassembler)` đo trên g++ 15.2.0 x86_64 là
**4192 byte**, bao gồm padding; chạy `printf 'M\n' | protocol-lab/build/cpp-lab`
để đo lại ABI khác. Không heap allocation cho slot hay fragment metadata khi
receive. Result COMPLETE có vector tối đa1024byte; string/result và input thuộc
chi phí API, không nằm trong sizeof state.

Kotlin cấp phát cố định hai Slot ngay lúc tạo instance: mỗi slot một ByteArray
1024, hai IntArray 256, tổng **6144 byte phần tử mảng** cho hai slot. Không tạo
mảng theo count/total từ frame; malformed frame không cấp phát message storage.
Chi phí header object/array, reference, scalar và alignment JVM **chưa đo tổng
retained heap**, do đó 6144 không phải tổng RAM. Result/iterator/lambda tạm và
bản sao COMPLETE tối đa1024byte còn tạo allocation JVM. Đây là giới hạn state
ráp, không hứa bound toàn JVM, caller queue hoặc input buffers.

Encoder giữ tối đa256 frame, tổng header+data tối đa5120 byte chưa kể container.
Receive quét tối đa2 slot và256 metadata, copy/compare tối đa1024 byte;
không có loop retry hoặc state tăng theo số frame nhận. Harness đọc trusted hex
bằng stdin và giữ transcript để test; không phải ingress có memory bound cho
chuỗi input tùy ý. Khi tích hợp cần giới hạn input trước tạo ByteArray/vector.

## Còn thiếu trước approval/tích hợp

- Review độc lập AGY PASS static (docs/evidence/REV-AGY-protocol.txt); Hermes kiểm lại code và chạy suite+sanitizer host với detect_leaks=1 PASS (docs/evidence/IF-003-hermes*.log). Những khẳng định quá mức trong review được hiệu chỉnh tại review-disposition.md. Passing suite không đồng nghĩa transport production approved.
- Đo tổng retained heap Kotlin và peak allocations; RAM ESP, GATT MTU thực,
  scheduling, throughput, pin và disconnect callback trên thiết bị chưa nghiệm thu.
- Payload decoder v1/UTF-8/JSON và validation schema; frame COMPLETE không phải
  bằng chứng payload hợp lệ, không phát ACK_EVENT. Payload phải decode thành công,
  event phải lưu bền vững rồi mới ACK_EVENT; checksum giữ nguyên/null theo hợp đồng.
- Completion cache có bound, event dedupe `(pairedDeviceId,eventId)+fingerprint`,
  sensor dedupe/reorder window500ms, late events, durable queue, reboot và seq
  reservation chưa có. Không dùng max_seen để tự loại event đến trễ.
- COMMAND_ACK 2000ms/2 retries, ACCEPTED khác COMPLETED và deadline5000ms không
  reset; EVENT_ACK 2000ms/2 retries/connection; reconnect backoff
  [1000,2000,4000,8000,16000,30000] reset sau ổn định30s đều thuộc lượt sau.
- Idempotent result cache, authenticated cancellation, liên kết SOS_CANCELLED,
  policy ưu tiên queue và ma trận rủi ro/detection trong draft chưa triển khai.
  Cần review hai phía và quyết định chủ dự án trước bật transport.

Các điểm draft cần chốt: cách báo expiry cho lớp trên, chống late fragment ghép
sau timeout, bound/retention completion cache, adapter kiểm MTU, chính sách lỗi
malformed so với conflict, và measurement budget JVM/ESP. Lab ghi rõ lựa chọn
ở trên để review; không tự nâng proposal thành hợp đồng đã duyệt.
