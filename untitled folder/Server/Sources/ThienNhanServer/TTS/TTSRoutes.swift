import Foundation
import Hummingbird
import HTTPTypes
import NIOCore

private struct TTSRequest: Decodable, Sendable {
    let text: String
}

enum TTSRoutes {
    static let maximumCharacters = 2_000
    private static let maximumJSONSize = 16 * 1_024

    static func register(on router: Router<BasicRequestContext>, serverToken: String) {
        router.post("/tts") { request, _ -> Response in
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
                throw APIError(.contentTooLarge, code: "request_too_large", message: "Dữ liệu TTS quá lớn.")
            }
            let body: TTSRequest
            do {
                body = try JSONDecoder().decode(TTSRequest.self, from: Data(buffer.readableBytesView))
            } catch {
                throw APIError(.badRequest, code: "invalid_json", message: "JSON của TTS không hợp lệ.")
            }
            let text = body.text.trimmingCharacters(in: .whitespacesAndNewlines)
            guard !text.isEmpty else {
                throw APIError(.badRequest, code: "text_empty", message: "Nội dung TTS đang trống.")
            }
            guard text.count <= maximumCharacters else {
                throw APIError(
                    .contentTooLarge,
                    code: "text_too_long",
                    message: "Nội dung TTS vượt quá 2.000 ký tự."
                )
            }

            do {
                return mp3Response(try await synthesize(text))
            } catch {
                throw ttsAPIError(error)
            }
        }
    }

    static func mp3Response(_ data: Data, text: String? = nil) -> Response {
        var buffer = ByteBufferAllocator().buffer(capacity: data.count)
        buffer.writeBytes(data)
        var headers: HTTPFields = [.contentType: "audio/mpeg"]
        if let text {
            headers[HTTPField.Name("X-ThienNhan-Text-Base64")!] = Data(text.utf8).base64EncodedString()
        }
        return Response(
            status: .ok,
            headers: headers,
            body: .init(byteBuffer: buffer)
        )
    }

    static func synthesize(_ text: String) async throws -> Data {
        try await Task.detached(priority: .userInitiated) {
            try synthesizeSynchronously(text)
        }.value
    }

    private static func synthesizeSynchronously(_ text: String) throws -> Data {
        let base = FileManager.default.temporaryDirectory
            .appendingPathComponent("thiennhan-tts-\(UUID().uuidString)")
        let aiffURL = base.appendingPathExtension("aiff")
        let mp3URL = base.appendingPathExtension("mp3")
        defer {
            try? FileManager.default.removeItem(at: aiffURL)
            try? FileManager.default.removeItem(at: mp3URL)
        }

        try run(
            "/usr/bin/say",
            arguments: [
                "-v", "Linh",
                "-o", aiffURL.path,
                "--data-format=BEI16@22050",
                text,
            ]
        )
        try run(
            lamePath(),
            arguments: ["--silent", "-b", "64", aiffURL.path, mp3URL.path]
        )

        let data = try Data(contentsOf: mp3URL)
        guard !data.isEmpty else { throw TTSError.emptyOutput }
        return data
    }

    private static func lamePath() throws -> String {
        for path in ["/opt/homebrew/bin/lame", "/usr/local/bin/lame"]
        where FileManager.default.isExecutableFile(atPath: path) {
            return path
        }
        throw TTSError.lameNotInstalled
    }

    private static func run(_ executable: String, arguments: [String]) throws {
        let process = Process()
        let errors = Pipe()
        process.executableURL = URL(fileURLWithPath: executable)
        process.arguments = arguments
        process.standardOutput = FileHandle.nullDevice
        process.standardError = errors
        let finished = DispatchSemaphore(value: 0)
        process.terminationHandler = { _ in finished.signal() }
        try process.run()
        if finished.wait(timeout: .now() + 30) == .timedOut {
            process.terminate()
            process.waitUntilExit()
            throw TTSError.commandTimedOut
        }

        guard process.terminationStatus == 0 else {
            let message = String(
                decoding: errors.fileHandleForReading.readDataToEndOfFile(),
                as: UTF8.self
            ).trimmingCharacters(in: .whitespacesAndNewlines)
            throw TTSError.commandFailed(message)
        }
    }
}

enum TTSError: LocalizedError {
    case lameNotInstalled
    case commandFailed(String)
    case commandTimedOut
    case emptyOutput

    var errorDescription: String? {
        switch self {
        case .lameNotInstalled: "Chưa cài bộ mã hóa MP3 `lame`."
        case .commandFailed(let message): message.isEmpty ? "Không thể tạo MP3." : message
        case .commandTimedOut: "Tiến trình tạo giọng nói phản hồi quá chậm."
        case .emptyOutput: "TTS tạo file MP3 rỗng."
        }
    }
}

func ttsAPIError(_ error: Error) -> APIError {
    switch error {
    case TTSError.lameNotInstalled:
        APIError(
            .serviceUnavailable,
            code: "tts_encoder_unavailable",
            message: "Bộ mã hóa âm thanh chưa sẵn sàng."
        )
    case TTSError.commandTimedOut:
        APIError(.gatewayTimeout, code: "tts_timeout", message: "Tạo giọng nói phản hồi quá chậm.")
    default:
        APIError(.internalServerError, code: "tts_failed", message: "Không thể tạo file âm thanh.")
    }
}
