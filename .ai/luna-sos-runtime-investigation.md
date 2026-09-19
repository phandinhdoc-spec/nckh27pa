# Luna — SOS runtime investigation

Ngày điều tra: 2026-09-19

Phạm vi: trace runtime Android từ nút SOS đến location, Google Maps URL, SMS và cuộc gọi SIM. Không sửa production code trong bước này.

Lưu ý về workspace: các file production liên quan đang có thay đổi chưa commit từ worker trước. Báo cáo này mô tả code đang có trong working tree và chỉ ra root cause của đường chạy cũ đã được thay thế; Luna không tạo thay đổi production nào.

## 1. Runtime pipeline hiện tại

```text
HomeScreen.kt:1645-1652
  giữ nút SOS đủ 3 giây
    -> DemoController.sos() / help() (DemoApplication.kt:449-450)
      -> DemoController.refresh() (DemoApplication.kt:415-420)
        -> SyncCoordinator.onSyncStateChanged (SyncCoordinator.kt:62-81)
          -> locationController.onVerifyingStarted() (SyncCoordinator.kt:68-69)
          -> emergency.beginVerifying(remoteId) (SyncCoordinator.kt:70-71)
          -> khi ALERTING/AWAITING_HELP: emergency.timeout(..., locationState.fix)
             (SyncCoordinator.kt:73)
            -> EmergencyCoordinator.dispatch (EmergencyCore.kt:248-285)
               -> LOCATION report
               -> dispatchSms -> AndroidSmsManagerGateway.send (EmergencyCore.kt:314-335;
                  AndroidSmsManagerGateway.kt:44-70)
               -> backend voice request
               -> dispatchSimCall -> AndroidSimCallGateway.call (EmergencyCore.kt:288-310;
                  AndroidSimCallGateway.kt:17-76)
               -> MAP_LINK report
```

Location chạy bất đồng bộ. `AndroidEmergencyLocationController.onVerifyingStarted()` (lines 29-37) chạy `BestAvailableLocationRepository.getBestAvailableLocation(8_000 ms)` trên coroutine. Repository (LocationRepository.kt:36-80) kiểm tra quyền, provider, cache mới nhất, rồi lần lượt FUSED/GPS/NETWORK. Android adapter (AndroidPlatformLocationSource.kt:25-127) dùng `FusedLocationProviderClient.getCurrentLocation` cho FUSED/NETWORK và `LocationManager` cho GPS/network fallback.

Nếu location có trước lúc dispatch, `LocationFix.mapsUrl` (EmergencyCore.kt:26-45) được đưa vào `EmergencyMessageFormatter.emergency` (EmergencyCore.kt:126-149), rồi vào SMS. Nếu dispatch xảy ra trước location, SMS đầu tiên dùng nội dung fallback không có tọa độ; khi fix đến muộn, callback DemoApplication.kt:85-88 gọi `EmergencyCoordinator.updateLocation` (EmergencyCore.kt:337-359), gửi SMS bổ sung có Maps URL.

SMS là gửi thật, không phải mock: `AndroidSmsManagerGateway.send` kiểm tra feature và `SEND_SMS`, chọn subscription mặc định, chia multipart nếu cần, rồi gọi `SmsManager.sendMultipartTextMessage` tại line 66.

Cuộc gọi SIM là đường thật, độc lập với location: `EmergencyCoordinator.dispatchSimCall` chọn contact đầu tiên có `receiveSos` và số điện thoại, sau đó `AndroidSimCallGateway.call` kiểm tra `CALL_PHONE` và gọi `Intent.ACTION_CALL` với URI `tel:` tại line 28. Đường `ManualSimCallFallback` và UI gọi thủ công là một đường khác, được gọi từ HomeScreen.kt:1060-1063.

Evidence runtime hiện có (`docs/evidence/sos-runtime/hermes-degraded-location-off.logcat.txt`) xác nhận khi `location_mode=0`: LOCATION=UNAVAILABLE, SMS=SENDING/SUCCESS, SIM_CALL=SUCCESS. SMS sent evidence cũng cho thấy body có `https://maps.google.com/?q=10.8231,106.6296983` khi fix có sẵn.

## 2. Root causes

### LOCATION

**Root cause đã xác định trong implementation cũ — GPS-only, one-shot**

- file: `app/src/main/java/vn/nckh27pa/fallsafe/location/AndroidEmergencyLocationController.kt` (implementation cũ; đường hiện tại đã được thay bằng controller lines 23-48)
- class/function: `AndroidEmergencyLocationController.onVerifyingStarted`
- hiện trạng: đường cũ chọn một provider qua `LocationProviderSelector`, ưu tiên GPS khi có FINE, gọi một lần `LocationManager.getCurrentLocation`, rồi chờ timeout. Không có Fused Location Provider và cache không được ưu tiên trước current lookup.
- vì sao sai: trong nhà hoặc khi GNSS chưa có fix, cả SOS location và UI dễ rơi vào thông báo “Di chuyển ra nơi thoáng và thử lại”; GPS không đồng nghĩa location tổng thể không khả dụng.
- cần sửa: dùng `BestAvailableLocationRepository` với thứ tự cache mới → FUSED → GPS → NETWORK, bounded timeout, không đặt ngưỡng accuracy làm điều kiện thành công. Workspace hiện tại đã có cấu trúc này tại `LocationRepository.kt:36-80` và `AndroidPlatformLocationSource.kt:60-97`.

**Điểm runtime cần giữ/kiểm tra**

- `AndroidPlatformLocationSource.kt:44-52`: FUSED chỉ được coi là enabled khi system location bật và Play Services hoặc platform fused tồn tại; NETWORK vẫn là fallback.
- `AndroidEmergencyLocationController.kt:29-48`: location lookup không được throw và publish lỗi phải giữ trạng thái “cảnh báo vẫn tiếp tục”.
- `AndroidEmergencyLocationController.kt:50-60`: “Ra nơi thoáng hơn...” chỉ là hint; không được dùng làm lý do hủy dispatch.
- `EmergencyCore.kt:257-260`: null location chỉ tạo LOCATION=UNAVAILABLE, sau đó SMS/CALL vẫn phải chạy.

### SMS

**Root cause đã xác định trong implementation cũ — location được xem là điều kiện trước khi gửi**

- file: `app/src/main/java/vn/nckh27pa/fallsafe/emergency/EmergencyCore.kt:248-335`
- class/function: `EmergencyCoordinator.dispatch`, `dispatchSms`
- hiện trạng: đường cũ phát SMS gắn với kết quả location/đường gửi location; khi không có fix, pipeline có thể kết thúc trước khi gọi gateway.
- vì sao sai: permission PASS chỉ chứng minh quyền đã cấp, không chứng minh `SmsManager` được gọi; location failure không được phép loại bỏ kênh cầu cứu còn lại.
- cần sửa: luôn gọi `dispatchSms` sau khi tạo record, với `EmergencyMessageFormatter.emergency(..., fix=null, ...)` nếu chưa có fix; message fallback phải nói chưa xác định vị trí và không bịa tọa độ. Workspace hiện tại thực hiện tại `EmergencyCore.kt:260` và `314-335`.

**Implementation thật và điều kiện có thể làm SMS dừng hiện tại**

- `AndroidSmsManagerGateway.kt:49`: thiết bị không có telephony messaging → FAILED.
- `AndroidSmsManagerGateway.kt:50`: `SEND_SMS` chưa được cấp → FAILED.
- `AndroidSmsManagerGateway.kt:52-58`: lỗi chọn subscription → fallback về default `SmsManager`; không còn chặn cứng dual-SIM.
- `AndroidSmsManagerGateway.kt:59-70`: body rỗng, `SecurityException`, hoặc runtime failure → FAILED.
- `EmergencyCore.kt:316`: không có contact `receiveSos=true` → UNAVAILABLE; đây là điều kiện dữ liệu, không phải lỗi location.
- `EmergencyCore.kt:321`: phone rỗng vẫn được đưa vào gateway; Astra nên xác thực số trước khi gửi hoặc ghi FAILED theo từng contact nếu acceptance yêu cầu contact lỗi được báo chính xác.

### CALL

**Root cause đã xác định trong implementation cũ — SOS chỉ gọi backend, không gọi handset**

- file: `app/src/main/java/vn/nckh27pa/fallsafe/emergency/EmergencyCore.kt:261-277`
- class/function: `EmergencyCoordinator.dispatch`
- hiện trạng: đường cũ chỉ gọi `backend.startVoice(eventId)`. `ManualSimCallFallback` tồn tại nhưng chỉ reachable từ nút gọi thủ công trong UI.
- vì sao sai: backend voice request không phải bằng chứng handset đã thực hiện cuộc gọi; trên điện thoại thật cần có đường `ACTION_CALL` độc lập.
- cần sửa: inject `EmergencyCallGateway`, chọn contact ưu tiên, kiểm tra capability, gọi đúng một lần, map `CallStatus` thành `SIM_CALL`. Workspace hiện tại đã nối tại `EmergencyCore.kt:288-310`, `DemoApplication.kt:77-83`, và `AndroidSimCallGateway.kt:17-76`.

**Điểm runtime có thể làm CALL không chạy hiện tại**

- `EmergencyCore.kt:300`: thiếu `CALL_PHONE` → PERMISSION_MISSING.
- `EmergencyCore.kt:301`: không có contact eligible hoặc số trống → UNAVAILABLE.
- `EmergencyCore.kt:297-299`: nếu backend trả VOICE_CALL=SUCCESS thì SIM_CALL bị SKIPPED theo policy hiện tại; cần giữ policy này hoặc đổi theo yêu cầu owner, nhưng phải ghi rõ vì nó không phải lỗi location.
- `AndroidSimCallGateway.kt:21-28`: thiếu permission, không có telephony calling, số trống, hoặc malformed/unsupported intent → trạng thái lỗi tương ứng.
- `AndroidSimCallGateway.kt:29-49`: nếu gọi từ thread khác main, kết quả thật của `startActivity` được chờ qua main handler; Astra phải bảo đảm không báo STARTED chỉ vì `Handler.post` thành công.

### PIPELINE/ORCHESTRATION

- file: `app/src/main/java/vn/nckh27pa/fallsafe/api/SyncCoordinator.kt:68-74`
- class/function: `SyncCoordinator` state callback
- hiện trạng: location được request khi event vào VERIFYING/ALERTING/AWAITING_HELP; dispatch xảy ra khi state vào ALERTING/AWAITING_HELP. Hai việc không đồng bộ.
- vì sao dễ gây hiểu nhầm: dispatch có thể tạo SMS fallback trước khi current location hoàn tất. Nếu code chỉ nhìn SMS đầu tiên sẽ tưởng Maps link không được tạo; code hiện tại gửi SMS bổ sung khi `updateLocation` nhận fix.
- cần sửa: giữ orchestration non-blocking nhưng phải bảo đảm `updateLocation` được gọi cho event đúng identity, gửi bổ sung tối đa một lần/contact, và report MAP_LINK được cập nhật. Không chờ location vô hạn vì sẽ làm mất SMS/CALL trong tình huống khẩn cấp.

- file: `app/src/main/java/vn/nckh27pa/fallsafe/DemoApplication.kt:85-88`
- class/function: `DemoApplication.onCreate` location callback
- hiện trạng: late fix chỉ update event nếu `controller.snapshot.eventId > 0` và identity lookup thành công.
- vì sao cần kiểm tra: nếu event identity bị reset hoặc event cũ đã bị cancel, late fix sẽ không được gắn vào đúng SOS.
- cần sửa: không đổi UI; Astra chỉ cần verify event identity lifecycle và test late-fix path.

### PERMISSION

- file: `app/src/main/AndroidManifest.xml:4-8`; `app/src/main/java/vn/nckh27pa/fallsafe/MainActivity.kt:97-105`; `permissions/AndroidCapabilityPlatform.kt:32-71`
- hiện trạng: FINE/COARSE, SEND_SMS, CALL_PHONE đã khai báo và được request runtime; capability snapshot kiểm tra permission thật.
- kết luận: permission dialog PASS không chứng minh provider bật, telephony feature tồn tại, SMS được queue, hay ACTION_CALL được launch. Không thấy root cause cần sửa ở permission UI trong trace hiện tại.
- `READ_PHONE_STATE` có trong manifest nhưng không được request runtime. Code SMS hiện tại không còn phụ thuộc quyền này để gửi default subscription; không thêm permission request nếu không có requirement chọn SIM thủ công.

## 3. Files Astra được phép sửa

Chỉ các file runtime sau nếu cần hoàn thiện hoặc sửa lỗi còn tái hiện:

1. `app/src/main/java/vn/nckh27pa/fallsafe/location/AndroidPlatformLocationSource.kt` — provider availability, cached/current lookup, Android API calls.
2. `app/src/main/java/vn/nckh27pa/fallsafe/location/LocationRepository.kt` — fallback order và timeout budget.
3. `app/src/main/java/vn/nckh27pa/fallsafe/location/AndroidEmergencyLocationController.kt` — request generation, publish state, late fix, Maps launching.
4. `app/src/main/java/vn/nckh27pa/fallsafe/emergency/EmergencyCore.kt` — SMS/CALL independence, fallback message, report và late location supplement.
5. `app/src/main/java/vn/nckh27pa/fallsafe/emergency/AndroidSmsManagerGateway.kt` — SEND_SMS check, subscription, multipart dispatch và result logging.
6. `app/src/main/java/vn/nckh27pa/fallsafe/emergency/AndroidSimCallGateway.kt` — CALL_PHONE check và ACTION_CALL result mapping.
7. `app/src/main/java/vn/nckh27pa/fallsafe/DemoApplication.kt` — wiring gateway/repository/callback בלבד.

`MainActivity.kt`, `AndroidManifest.xml`, và `app/build.gradle.kts` chỉ cần sửa nếu kiểm tra runtime chứng minh thiếu quyền/dependency. Trong working tree hiện tại manifest đã có quyền cần thiết và Gradle đã có `play-services-location:21.4.0`; không có lý do từ trace này để sửa chúng. Không sửa `HomeScreen.kt` cho mục tiêu runtime này.

## 4. Functions/classes Astra cần sửa hoặc xác minh

- `DemoController.sos`, `DemoController.help`, `DemoController.refresh` — event trigger và callback ordering.
- `SyncCoordinator` state callback lines 62-74 — location request, event identity, dispatch trigger.
- `AndroidEmergencyLocationController.onVerifyingStarted`, `publish`, `update`.
- `BestAvailableLocationRepository.getBestAvailableLocation`.
- `AndroidPlatformLocationSource.permission`, `enabledProviders`, `lastKnown`, `current`.
- `LocationFix.mapsUrl` và `EmergencyMessageFormatter.emergency/locationSupplement`.
- `EmergencyCoordinator.dispatch`, `dispatchSms`, `dispatchSimCall`, `updateLocation`.
- `AndroidSmsManagerGateway.send`.
- `AndroidSimCallGateway.call` và `launch`.
- `DemoApplication.onCreate` gateway wiring.

## 5. Expected pipeline sau sửa

```text
SOS triggered
  -> bắt đầu location lookup bounded, không chặn toàn bộ SOS
  -> nếu có fix hợp lệ: tạo maps.google.com/?q=latitude,longitude
  -> gửi SMS tới các contact eligible, body có tọa độ + Maps URL
  -> thực hiện ACTION_CALL tới contact ưu tiên
  -> nếu location fail/timeout/provider off: gửi SMS fallback không có tọa độ và vẫn CALL
  -> nếu fix đến muộn: gửi SMS bổ sung có Maps URL và cập nhật MAP_LINK report
```

Location failure không được làm mất SMS hoặc CALL. Không được dùng chuỗi “ra chỗ thoáng...” như điều kiện return trước dispatch; chuỗi này chỉ là remediation hint.

## 6. Test plan tối thiểu cho Astra

- Permission granted: capability snapshot báo đúng; fused/network location trả fix; SMS body có latitude/longitude và exact Maps URL; CALL gateway nhận đúng số.
- Permission denied: location trả `PERMISSION_DENIED`; SMS/CALL vẫn được thử độc lập và report đúng `PERMISSION_MISSING` cho kênh tương ứng.
- GPS/location thành công: fix hợp lệ có accuracy bất kỳ hữu hạn; source và Maps URL đúng.
- Location timeout/failure/provider off: LOCATION không khả dụng; SMS fallback vẫn gửi; SIM_CALL vẫn chạy; không có tọa độ bịa.
- SMS: feature thiếu, SEND_SMS thiếu, default subscription, multipart body, `SecurityException`/runtime failure; report và log không chứa full phone/message.
- CALL: `ACTION_CALL` với URI `tel:`, permission thiếu, feature thiếu, ActivityNotFoundException, gọi từ main/background; không báo SUCCESS nếu startActivity thật thất bại.
- Không có contact: SMS và SIM_CALL trả UNAVAILABLE rõ ràng, không crash.
- SOS end-to-end: một event duy nhất có thứ tự LOCATION → SMS → VOICE_CALL → SIM_CALL → MAP_LINK; test cả fix trước dispatch và fix đến sau dispatch.
- Device test: permission PASS riêng; bật/tắt system location; inject/mock location; kiểm tra `adb logcat` theo event id; kiểm tra SMS sent provider; xác nhận cuộc gọi thật trên thiết bị có SIM. Emulator không chứng minh được SMS delivery hoặc voice call qua nhà mạng.

## Kết luận điều tra

Trong working tree hiện tại, location/SMS/CALL đã có implementation thật và evidence emulator đã chứng minh degraded location không chặn SMS/SIM_CALL. Root cause của triệu chứng ban đầu là đường location cũ GPS-only/one-shot và orchestration cũ không có handset call độc lập. Các file nêu trên là đúng vùng Astra cần kiểm tra/chỉnh nếu nhánh bước 2 bắt đầu từ baseline cũ; không cần đọc lại toàn repository.
