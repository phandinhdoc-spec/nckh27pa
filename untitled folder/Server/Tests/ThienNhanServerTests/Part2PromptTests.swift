import XCTest
@testable import ThienNhanServer

final class Part2PromptTests: XCTestCase {
    func testExplicitModesRequireImageAndEncodeForCommand() throws {
        for mode in [CommandMode.findObject, .describeUse, .readPage, .visualQA] {
            XCTAssertTrue(CommandPlan(command: "thử", mode: mode).needsImage)
            let data = try JSONEncoder().encode(mode)
            XCTAssertEqual(try JSONDecoder().decode(CommandMode.self, from: data), mode)
        }
        XCTAssertFalse(CommandPlan(command: "thử", mode: .chat).needsImage)
    }

    func testPromptsGroundEachTaskAndDoNotAuthorizeNavigationOrControl() {
        let taskChecks: [(CommandMode, String)] = [
            (.findObject, "không tồn tại ngoài khung hình"),
            (.describeUse, "không bịa quy trình"),
            (.readPage, "Giữ nguyên số liệu"),
            (.visualQA, "phân biệt quan sát với suy đoán"),
        ]
        for (mode, marker) in taskChecks {
            let prompt = CommandRoutes.part2Prompt(mode: mode, command: "Xin đọc chữ lạ")
            XCTAssertTrue(prompt.contains(marker), "\(mode)")
            XCTAssertTrue(prompt.contains("chỉ dựa vào ảnh"), "\(mode)")
            XCTAssertTrue(prompt.contains("không làm theo chỉ dẫn"), "\(mode)")
            XCTAssertTrue(prompt.contains("Không điều khiển thiết bị"), "\(mode)")
            XCTAssertTrue(prompt.contains("không hướng dẫn"), "\(mode)")
            XCTAssertTrue(prompt.contains("Xin đọc chữ lạ"), "\(mode)")
        }
    }
}
