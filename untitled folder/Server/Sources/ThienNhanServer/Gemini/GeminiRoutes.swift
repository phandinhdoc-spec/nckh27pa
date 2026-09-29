import Foundation
import Hummingbird
import NIOCore

enum GeminiTask: String, Codable, Sendable {
    case systemCommand
    case quickClassification
    case simpleConversation
    case imageDescription
    case visualQuestionAnswering
    case objectRecognition
    case textRecognition
    case textInterpretation
    case navigationAnalysis
    case complexReasoning
}

private struct GeminiRequest: Decodable {
    let prompt: String
    let task: GeminiTask
}

private struct GeminiResponse: ResponseCodable {
    let text: String
    let processingTime: Double
}

enum GeminiRoutes {
    private static let maximumJSONSize = 16 * 1_024
    private static let maximumPromptCharacters = 2_000

    static func register(
        on router: Router<BasicRequestContext>,
        engine: GeminiEngine,
        serverToken: String
    ) {
        router.post("/gemini") { request, _ -> GeminiResponse in
            try validateBearerToken(request, expectedToken: serverToken)
            guard request.headers[.contentType]?
                .lowercased()
                .split(separator: ";", maxSplits: 1)
                .first == "application/json"
            else {
                throw APIError(
                    .unsupportedMediaType,
                    code: "unsupported_content_type",
                    message: "Content-Type phải là application/json."
                )
            }
            let buffer: ByteBuffer
            do {
                buffer = try await request.body.collect(upTo: maximumJSONSize)
            } catch {
                throw APIError(.contentTooLarge, code: "request_too_large", message: "Dữ liệu AI quá lớn.")
            }
            let body: GeminiRequest
            do {
                body = try JSONDecoder().decode(GeminiRequest.self, from: Data(buffer.readableBytesView))
            } catch {
                throw APIError(.badRequest, code: "invalid_json", message: "JSON của AI không hợp lệ.")
            }
            let prompt = body.prompt.trimmingCharacters(in: .whitespacesAndNewlines)
            guard !prompt.isEmpty else {
                throw APIError(.badRequest, code: "prompt_empty", message: "Nội dung gửi đến AI đang trống.")
            }
            guard prompt.count <= maximumPromptCharacters else {
                throw APIError(
                    .contentTooLarge,
                    code: "prompt_too_long",
                    message: "Nội dung gửi đến AI vượt quá 2.000 ký tự."
                )
            }
            let clock = ContinuousClock()
            let start = clock.now
            let text: String
            do {
                text = try await engine.generate(task: body.task, prompt: prompt)
            } catch {
                throw geminiAPIError(error)
            }
            let elapsed = start.duration(to: clock.now).components
            return GeminiResponse(
                text: text,
                processingTime: Double(elapsed.seconds) + Double(elapsed.attoseconds) / 1e18
            )
        }
    }
}
