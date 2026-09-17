# REV-CORE-002
Mục tiêu: review độc lập regression CORE-002; không triển khai. Nguồn Android §4.4/7/9, CORE-002, chỉ thị user tests post-implementation.
Người: Codex gpt-6-astra phiên riêng (không session người triển khai). Input chỉ android/core/src, tests/CoreTests.kt, README và evidence/core-002-report.md, docs/evidence/CORE-002-hermes.log.
Phạm vi: read-only, output CLI docs/evidence/REV-CORE-002.txt; không source/tests, không build/network/agent con.
Phụ thuộc CORE-002. Bàn giao PASS/FAIL, bugs file:line, test coverage/assertions (không nuốt lỗi), không nhận tests mới là test-first. Nghiệm thu không bug chặn. Hermes đã chạy suite12 nhóm+demo exit0; kiểm source không thay đổi. Giới hạn1 review, tối đa2 fix/review nếu bug.
