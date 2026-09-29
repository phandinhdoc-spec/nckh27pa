import XCTest
@testable import ThienNhanServer

final class CommandPlanTests: XCTestCase {
    func testVisualIntentRequestsCamera() {
        let phrases = [
            "Đây là cái gì?",
            "Tôi đang cầm cái gì?",
            "Có chướng ngại vật nào trước tôi không?",
            "Bên trái có ai không?",
            "Đường đi có an toàn không?",
        ]

        for phrase in phrases {
            XCTAssertTrue(CommandRoutes.plan(for: phrase).needsImage, phrase)
        }
    }

    func testParsesStructuredGeminiCameraDecision() {
        let response = #"{"camera":"yes","mode":"observe","text":"Đây là vật gì?"}"#
        let plan = CommandRoutes.cameraPlan(from: response, originalSpeech: "Đây là vật gì?")

        XCTAssertEqual(plan?.mode, .observe)
        XCTAssertEqual(plan?.needsImage, true)
        XCTAssertEqual(plan?.command, "Đây là vật gì?")
    }

    func testRejectsInconsistentGeminiCameraDecision() {
        let response = #"{"camera":"no","mode":"observe","text":"Đây là vật gì?"}"#
        XCTAssertNil(CommandRoutes.cameraPlan(from: response, originalSpeech: "Đây là vật gì?"))
    }

    func testCameraRoutingPromptContainsUserSpeech() {
        XCTAssertTrue(
            CommandRoutes.cameraRoutingPrompt(for: "Đây là cái gì?")
                .contains("Yêu cầu người dùng: Đây là cái gì?")
        )
    }

    func testPlanResponseExposesCommandModeAndCameraNeed() throws {
        let response = CommandPlanResponse(
            plan: CommandPlan(command: "phía trước tôi là gì?", mode: .observe)
        )
        let data = try JSONEncoder().encode(response)
        let object = try XCTUnwrap(JSONSerialization.jsonObject(with: data) as? [String: Any])

        XCTAssertEqual(object["command"] as? String, "phía trước tôi là gì?")
        XCTAssertEqual(object["mode"] as? String, "observe")
        XCTAssertEqual(object["needsImage"] as? Bool, true)
    }

    func testVisualPromptPrioritizesObjectHeldInHand() {
        let prompt = CommandRoutes.visualQuestionPrompt(for: "Đây là vật gì?")
        XCTAssertTrue(prompt.contains("vật trong tay"))
        XCTAssertTrue(prompt.contains("bỏ qua các vật ở bên cạnh và hậu cảnh"))
        XCTAssertTrue(prompt.contains("Yêu cầu người dùng: Đây là vật gì?"))
    }

    func testNormalizeVietnameseFoldsAccentsAndD() {
        XCTAssertEqual(CommandRoutes.normalizeVietnamese("Đường"), "duong")
        XCTAssertEqual(CommandRoutes.normalizeVietnamese("đường"), "duong")
        XCTAssertEqual(CommandRoutes.normalizeVietnamese("Đi Đường"), "di duong")
        XCTAssertEqual(CommandRoutes.normalizeVietnamese("đi đường"), "di duong")
    }

    func testRouteGuidanceIntentMatchesAccentedAndPlainVariants() {
        let matches = [
            "có ý hướng dẫn tôi đi đường",
            "Có ý hướng dẫn tôi đi đường",
            "CÓ Ý HƯỚNG DẪN TÔI ĐI ĐƯỜNG",
            "co y huong dan toi di duong",
            "CO Y HUONG DAN TOI DI DUONG",
            "có ý hướng dẫn tôi đi duong",
            "co y huong dan toi di đường",
            "Hãy hướng dẫn tôi đi đường",
            "Hướng dẫn tôi đi đường",
            "hướng dẫn tôi đi đường",
            "HƯỚNG DẪN TÔI ĐI ĐƯỜNG",
            "hay huong dan toi di duong",
            "HAY HUONG DAN TOI DI DUONG",
            "huong dan toi di duong",
            "HUONG DAN TOI DI DUONG",
            "hướng dẫn tôi đi duong",
            "huong dan toi di đường",
            "Thông báo giờ tôi đi đường",
            "thông báo giờ tôi đi đường",
            "THÔNG BÁO GIỜ TÔI ĐI ĐƯỜNG",
            "Thông báo cho tôi giờ tôi đi đường",
            "thong bao gio toi di duong",
            "THONG BAO GIO TOI DI DUONG",
            "thông báo giờ tôi đi duong",
            "thong bao gio toi di đường",
        ]
        for phrase in matches {
            XCTAssertTrue(CommandRoutes.isRouteGuidanceIntent(phrase), phrase)
        }
    }

    func testRouteGuidanceIntentRejectsUnrelatedTimeQuestion() {
        let unrelated = [
            "Bây giờ là mấy giờ?",
            "Mấy giờ rồi?",
            "Xem giờ giúp tôi",
            "Thông báo thời gian cho tôi",
            "Báo giờ cho tôi",
            "Thời gian hiện tại",
        ]
        for phrase in unrelated {
            XCTAssertFalse(CommandRoutes.isRouteGuidanceIntent(phrase), phrase)
        }
    }

    func testRouteGuidanceIntentPlansLocalWithoutImage() {
        let triggers = [
            "có ý hướng dẫn tôi đi đường",
            "Có ý hướng dẫn tôi đi đường",
            "co y huong dan toi di duong",
            "Hãy hướng dẫn tôi đi đường",
            "Hướng dẫn tôi đi đường",
            "hướng dẫn tôi đi đường",
            "huong dan toi di duong",
            "hướng dẫn tôi đi duong",
            "Thông báo giờ tôi đi đường",
            "thông báo giờ tôi đi đường",
            "thong bao gio toi di duong",
            "thông báo giờ tôi đi duong",
            "Thiên Nhãn, có ý hướng dẫn tôi đi đường",
            "Thiên Nhãn, hướng dẫn tôi đi đường",
            "Thiên Nhãn, thông báo giờ tôi đi đường",
        ]
        for phrase in triggers {
            let plan = CommandRoutes.plan(for: phrase)
            XCTAssertEqual(plan.mode, .local, phrase)
            XCTAssertFalse(plan.needsImage, phrase)
        }
    }

    func testOrdinaryVisualCommandStillObservesWithImage() {
        let visualPhrases = [
            "Đây là cái gì?",
            "Tôi đang cầm cái gì?",
            "Đường đi có an toàn không?",
            "Phía trước có chướng ngại vật nào không?",
        ]
        for phrase in visualPhrases {
            let plan = CommandRoutes.plan(for: phrase)
            XCTAssertEqual(plan.mode, .observe, phrase)
            XCTAssertTrue(plan.needsImage, phrase)
        }
    }

    func testRouteGuidanceSafeResponseHasNoDigitsOrDirections() {
        let text = CommandRoutes.routeGuidanceUnavailableText
        XCTAssertTrue(text.contains("chưa được hiệu chuẩn"))
        XCTAssertTrue(text.rangeOfCharacter(from: .decimalDigits) == nil)
        for claim in ["trái", "phải", "rẽ", "quẹo", "bên trái", "bên phải", "thẳng"] {
            XCTAssertFalse(text.lowercased().contains(claim), claim)
        }
    }
}
