import Foundation
import Hummingbird
import NIOCore

struct STTResult: ResponseCodable, Sendable {
    let success: Bool
    let text: String
    let command: String
    let mode: CommandMode
    let needsImage: Bool
    let locale: String
    let audioDuration: Double
    let processingTime: Double
    let timestamp: Date

    init(output: SpeechEngineOutput, plan: CommandPlan) {
        self.success = true
        self.text = output.text
        self.command = plan.command
        self.mode = plan.mode
        self.needsImage = plan.needsImage
        self.locale = output.localeIdentifier
        self.audioDuration = output.audioDurationSeconds
        self.processingTime = output.processingDurationSeconds
        self.timestamp = Date()
    }
}

enum STTRoutes {
    static let maximumAudioSize = 20 * 1_024 * 1_024

    static func register(
        on router: Router<BasicRequestContext>,
        engine: SpeechEngine,
        geminiEngine: GeminiEngine,
        serverToken: String,
        audioArchive: AudioArchive
    ) {
        router.post("/stt") { request, _ -> STTResult in
            try validateBearerToken(request, expectedToken: serverToken)
            let buffer: ByteBuffer
            do {
                buffer = try await request.body.collect(upTo: maximumAudioSize)
            } catch {
                throw APIError(
                    .contentTooLarge,
                    code: "audio_too_large",
                    message: "Âm thanh vượt quá 20 MB."
                )
            }
            let output = try await transcribe(
                data: Data(buffer.readableBytesView),
                contentType: request.headers[.contentType],
                engine: engine,
                audioArchive: audioArchive
            )
            let plan = await CommandRoutes.plan(for: output.text, using: geminiEngine)
            return STTResult(output: output, plan: plan)
        }
    }

    static func transcribe(
        data: Data,
        contentType: String?,
        engine: SpeechEngine,
        audioArchive: AudioArchive
    ) async throws -> SpeechEngineOutput {
        let audio = try StoredAudioFile(data: data, contentType: contentType, archive: audioArchive)
        do {
            return try await engine.transcribe(fileURL: audio.url)
        } catch {
            throw speechAPIError(error)
        }
    }
}

private struct StoredAudioFile {
    let url: URL

    init(data: Data, contentType: String?, archive: AudioArchive) throws {
        guard !data.isEmpty else {
            throw APIError(.badRequest, code: "audio_empty", message: "Dữ liệu âm thanh đang trống.")
        }
        guard data.count <= STTRoutes.maximumAudioSize else {
            throw APIError(.contentTooLarge, code: "audio_too_large", message: "Âm thanh vượt quá 20 MB.")
        }

        let type = contentType?
            .lowercased()
            .split(separator: ";", maxSplits: 1)
            .first
            .map(String.init)
        let extensions = [
            "audio/wav": "wav", "audio/wave": "wav", "audio/x-wav": "wav",
            "audio/mp4": "m4a", "audio/m4a": "m4a", "audio/x-m4a": "m4a",
            "audio/aiff": "aiff", "audio/x-aiff": "aiff",
            "audio/caf": "caf", "audio/x-caf": "caf",
        ]
        guard let fileExtension = type.flatMap({ extensions[$0] }) else {
            throw APIError(
                .unsupportedMediaType,
                code: "unsupported_audio_type",
                message: "Định dạng âm thanh không được hỗ trợ."
            )
        }

        do {
            url = try archive.store(data, fileExtension: fileExtension)
        } catch {
            throw APIError(
                .internalServerError,
                code: "audio_storage_failed",
                message: "Không thể lưu âm thanh tạm thời."
            )
        }
    }

}

private func speechAPIError(_ error: Error) -> APIError {
    switch error {
    case SpeechEngineError.emptyAudio:
        APIError(.badRequest, code: "audio_empty", message: "Dữ liệu âm thanh đang trống.")
    case SpeechEngineError.emptyTranscript:
        APIError(
            .unprocessableContent,
            code: "speech_not_recognized",
            message: "Không nhận dạng được lời nói."
        )
    case SpeechEngineError.timeout:
        APIError(
            .gatewayTimeout,
            code: "groq_stt_timeout",
            message: "Dịch vụ nhận dạng giọng nói phản hồi quá chậm."
        )
    case SpeechEngineError.networkFailure:
        APIError(
            .badGateway,
            code: "groq_stt_unavailable",
            message: "Không thể kết nối dịch vụ nhận dạng giọng nói."
        )
    case SpeechEngineError.requestFailed:
        APIError(
            .badGateway,
            code: "groq_stt_failed",
            message: "Dịch vụ nhận dạng giọng nói xử lý thất bại."
        )
    case SpeechEngineError.invalidEndpoint, SpeechEngineError.invalidResponse:
        APIError(
            .badGateway,
            code: "groq_stt_invalid_response",
            message: "Dịch vụ nhận dạng giọng nói trả phản hồi không hợp lệ."
        )
    default:
        APIError(
            .unprocessableContent,
            code: "audio_processing_failed",
            message: "Không thể xử lý âm thanh."
        )
    }
}
