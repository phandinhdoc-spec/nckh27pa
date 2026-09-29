import Foundation

enum GeminiEngineError: LocalizedError {
    case emptyPrompt
    case unsupportedImageType(String)
    case invalidEndpoint
    case invalidResponse
    case requestFailed(statusCode: Int, message: String)
    case emptyResponse
    case blocked(String?)
    case timeout
    case networkFailure(String)

    var errorDescription: String? {
        switch self {
        case .emptyPrompt:
            return "Prompt gửi đến Gemini đang trống."

        case .unsupportedImageType(let fileExtension):
            return "Định dạng ảnh không được hỗ trợ: \(fileExtension)"

        case .invalidEndpoint:
            return "Không thể tạo endpoint Gemini hợp lệ."

        case .invalidResponse:
            return "Gemini trả về phản hồi không hợp lệ."

        case .requestFailed(let statusCode, let message):
            return "Gemini API lỗi HTTP \(statusCode): \(message)"

        case .emptyResponse:
            return "Gemini không trả về nội dung."

        case .blocked(let reason):
            if let reason, !reason.isEmpty {
                return "Yêu cầu bị Gemini chặn: \(reason)"
            }

            return "Yêu cầu bị Gemini chặn."

        case .timeout:
            return "Gemini không phản hồi trong thời gian cho phép."

        case .networkFailure(let message):
            return "Không thể kết nối Gemini API: \(message)"
        }
    }
}

struct GeminiImage: Sendable {
    let data: Data
    let mimeType: String
}

struct GeminiEngine: Sendable {
    private let apiKey: String
    private let fastModel: String
    private let visionModel: String
    private let reasoningModel: String
    private let fallbackModel: String
    private let timeoutSeconds: Int

    init(
        config: AppConfig
    ) {
        self.apiKey = config.geminiAPIKey
        self.fastModel = config.geminiModelFast
        self.visionModel = config.geminiModelVision
        self.reasoningModel = config.geminiModelReasoning
        self.fallbackModel = config.geminiModelFallback
        self.timeoutSeconds = config.requestTimeoutSeconds
    }

    func generate(
        task: GeminiTask,
        prompt: String,
        image: GeminiImage? = nil
    ) async throws -> String {
        let normalizedPrompt = prompt.trimmingCharacters(
            in: .whitespacesAndNewlines
        )

        guard !normalizedPrompt.isEmpty else {
            throw GeminiEngineError.emptyPrompt
        }

        let primaryModel = model(for: task)

        do {
            return try await performRequest(
                model: primaryModel,
                prompt: normalizedPrompt,
                image: image
            )
        } catch {
            guard fallbackModel != primaryModel else {
                throw error
            }

            return try await performRequest(
                model: fallbackModel,
                prompt: normalizedPrompt,
                image: image
            )
        }
    }

    private func model(for task: GeminiTask) -> String {
        switch task {
        case .systemCommand, .quickClassification, .simpleConversation:
            fastModel
        case .imageDescription, .visualQuestionAnswering, .objectRecognition,
             .textRecognition, .textInterpretation, .navigationAnalysis:
            visionModel
        case .complexReasoning:
            reasoningModel
        }
    }

    private func performRequest(
        model: String,
        prompt: String,
        image: GeminiImage?
    ) async throws -> String {
        let request = try makeRequest(
            model: model,
            prompt: prompt,
            image: image
        )

        let sessionConfiguration =
            URLSessionConfiguration.ephemeral

        sessionConfiguration.timeoutIntervalForRequest =
            TimeInterval(timeoutSeconds)

        sessionConfiguration.timeoutIntervalForResource =
            TimeInterval(timeoutSeconds)

        let session = URLSession(
            configuration: sessionConfiguration
        )

        let data: Data
        let response: URLResponse

        do {
            (data, response) = try await session.data(
                for: request
            )
        } catch {
            if (error as? URLError)?.code == .timedOut {
                throw GeminiEngineError.timeout
            }
            throw GeminiEngineError.networkFailure(
                error.localizedDescription
            )
        }

        guard let httpResponse =
            response as? HTTPURLResponse
        else {
            throw GeminiEngineError.invalidResponse
        }

        guard (200...299).contains(
            httpResponse.statusCode
        ) else {
            let apiError = try? JSONDecoder().decode(
                GeminiErrorEnvelope.self,
                from: data
            )

            let message =
                apiError?.error.message
                ?? String(
                    data: data,
                    encoding: .utf8
                )
                ?? "Lỗi không xác định."

            throw GeminiEngineError.requestFailed(
                statusCode: httpResponse.statusCode,
                message: message
            )
        }

        let decoded: GeminiGenerateResponse

        do {
            decoded = try JSONDecoder().decode(
                GeminiGenerateResponse.self,
                from: data
            )
        } catch {
            throw GeminiEngineError.invalidResponse
        }

        if let blockReason =
            decoded.promptFeedback?.blockReason
        {
            throw GeminiEngineError.blocked(
                blockReason
            )
        }

        let text = decoded.candidates?
            .flatMap { candidate in
                candidate.content?.parts ?? []
            }
            .compactMap(\.text)
            .joined(separator: "\n")
            .trimmingCharacters(
                in: .whitespacesAndNewlines
            )

        guard let text, !text.isEmpty else {
            throw GeminiEngineError.emptyResponse
        }

        return text
    }

    private func makeRequest(
        model: String,
        prompt: String,
        image: GeminiImage?
    ) throws -> URLRequest {
        var components = URLComponents()

        components.scheme = "https"
        components.host =
            "generativelanguage.googleapis.com"

        components.path =
            "/v1beta/models/\(model):generateContent"

        components.queryItems = [
            URLQueryItem(
                name: "key",
                value: apiKey
            )
        ]

        guard let url = components.url else {
            throw GeminiEngineError.invalidEndpoint
        }

        var parts: [GeminiRequestPart] = [
            GeminiRequestPart(
                text: prompt,
                inlineData: nil
            )
        ]

        if let image {
            parts.append(
                try makeImagePart(
                    image
                )
            )
        }

        let payload = GeminiGenerateRequest(
            contents: [
                GeminiRequestContent(
                    role: "user",
                    parts: parts
                )
            ]
        )

        var request = URLRequest(url: url)

        request.httpMethod = "POST"
        request.timeoutInterval =
            TimeInterval(timeoutSeconds)

        request.setValue(
            "application/json",
            forHTTPHeaderField: "Content-Type"
        )

        request.httpBody = try JSONEncoder().encode(
            payload
        )

        return request
    }

    private func makeImagePart(_ image: GeminiImage) throws -> GeminiRequestPart {
        guard VisionRoutes.supports(image.mimeType) else {
            throw GeminiEngineError.unsupportedImageType(image.mimeType)
        }
        return GeminiRequestPart(
            text: nil,
            inlineData: GeminiInlineData(
                mimeType: image.mimeType,
                data: image.data.base64EncodedString()
            )
        )
    }
}

func geminiAPIError(_ error: Error) -> APIError {
    switch error {
    case GeminiEngineError.emptyPrompt:
        APIError(.badRequest, code: "prompt_empty", message: "Nội dung gửi đến AI đang trống.")
    case GeminiEngineError.unsupportedImageType:
        APIError(
            .unsupportedMediaType,
            code: "unsupported_image_type",
            message: "Định dạng ảnh không được hỗ trợ."
        )
    case GeminiEngineError.timeout:
        APIError(.gatewayTimeout, code: "gemini_timeout", message: "Dịch vụ AI phản hồi quá chậm.")
    case GeminiEngineError.blocked:
        APIError(
            .unprocessableContent,
            code: "gemini_blocked",
            message: "Dịch vụ AI từ chối xử lý nội dung này."
        )
    case GeminiEngineError.emptyResponse:
        APIError(
            .badGateway,
            code: "gemini_empty_response",
            message: "Dịch vụ AI không trả về nội dung."
        )
    case GeminiEngineError.networkFailure:
        APIError(.badGateway, code: "gemini_unavailable", message: "Không thể kết nối dịch vụ AI.")
    case GeminiEngineError.requestFailed:
        APIError(.badGateway, code: "gemini_request_failed", message: "Dịch vụ AI xử lý thất bại.")
    case GeminiEngineError.invalidEndpoint, GeminiEngineError.invalidResponse:
        APIError(
            .internalServerError,
            code: "gemini_configuration_error",
            message: "Cấu hình dịch vụ AI không hợp lệ."
        )
    default:
        APIError(.badGateway, code: "gemini_failed", message: "Không thể xử lý yêu cầu bằng AI.")
    }
}

private struct GeminiGenerateRequest: Encodable {
    let contents: [GeminiRequestContent]
}

private struct GeminiRequestContent: Encodable {
    let role: String
    let parts: [GeminiRequestPart]
}

private struct GeminiRequestPart: Encodable {
    let text: String?
    let inlineData: GeminiInlineData?

    enum CodingKeys: String, CodingKey {
        case text
        case inlineData = "inline_data"
    }
}

private struct GeminiInlineData: Encodable {
    let mimeType: String
    let data: String

    enum CodingKeys: String, CodingKey {
        case mimeType = "mime_type"
        case data
    }
}

private struct GeminiGenerateResponse: Decodable {
    let candidates: [GeminiCandidate]?
    let promptFeedback: GeminiPromptFeedback?
}

private struct GeminiCandidate: Decodable {
    let content: GeminiResponseContent?
}

private struct GeminiResponseContent: Decodable {
    let parts: [GeminiResponsePart]?
}

private struct GeminiResponsePart: Decodable {
    let text: String?
}

private struct GeminiPromptFeedback: Decodable {
    let blockReason: String?
}

private struct GeminiErrorEnvelope: Decodable {
    let error: GeminiAPIError
}

private struct GeminiAPIError: Decodable {
    let message: String
}
