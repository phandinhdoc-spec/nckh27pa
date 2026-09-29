import Foundation

enum SpeechEngineError: LocalizedError {
    case emptyAudio
    case emptyTranscript
    case invalidEndpoint
    case invalidResponse
    case timeout
    case networkFailure(String)
    case requestFailed(statusCode: Int, message: String)

    var errorDescription: String? {
        switch self {
        case .emptyAudio:
            "File âm thanh không chứa dữ liệu."
        case .emptyTranscript:
            "Groq không nhận dạng được nội dung giọng nói."
        case .invalidEndpoint:
            "Endpoint Groq STT không hợp lệ."
        case .invalidResponse:
            "Groq trả về phản hồi STT không hợp lệ."
        case .timeout:
            "Groq STT phản hồi quá chậm."
        case .networkFailure(let message):
            "Không thể kết nối Groq STT: \(message)"
        case .requestFailed(let statusCode, let message):
            "Groq STT lỗi HTTP \(statusCode): \(message)"
        }
    }
}

struct SpeechEngineOutput {
    let text: String
    let localeIdentifier: String
    let audioDurationSeconds: Double
    let processingDurationSeconds: Double
}

actor SpeechEngine {
    private let apiKey: String
    private let model: String
    private let timeoutSeconds: Int

    init(config: AppConfig) {
        apiKey = config.groqAPIKey
        model = config.groqSTTModel
        timeoutSeconds = config.requestTimeoutSeconds
    }

    func transcribe(fileURL: URL) async throws -> SpeechEngineOutput {
        let audio = try Data(contentsOf: fileURL)
        guard !audio.isEmpty else {
            throw SpeechEngineError.emptyAudio
        }

        let clock = ContinuousClock()
        let start = clock.now
        let transcription = try await requestTranscription(audio: audio)
        let text = transcription.text
            .trimmingCharacters(in: .whitespacesAndNewlines)

        guard !text.isEmpty, transcription.containsSpeech else {
            throw SpeechEngineError.emptyTranscript
        }

        print(
            "[GROQ STT] \(String(format: "%.2f", transcription.duration ?? wavDuration(audio))) giây"
                + " → \(text)"
        )

        return SpeechEngineOutput(
            text: text,
            localeIdentifier: "vi",
            audioDurationSeconds: wavDuration(audio),
            processingDurationSeconds: durationInSeconds(start.duration(to: clock.now))
        )
    }

    private func requestTranscription(audio: Data) async throws -> GroqTranscription {
        guard let url = URL(
            string: "https://api.groq.com/openai/v1/audio/transcriptions"
        ) else {
            throw SpeechEngineError.invalidEndpoint
        }

        let boundary = "ThienNhan-\(UUID().uuidString)"
        var body = Data()
        body.appendMultipartField(name: "model", value: model, boundary: boundary)
        body.appendMultipartField(name: "language", value: "vi", boundary: boundary)
        body.appendMultipartField(name: "response_format", value: "verbose_json", boundary: boundary)
        body.appendMultipartField(name: "temperature", value: "0", boundary: boundary)
        body.appendMultipartField(
            name: "prompt",
            value: "Tiếng Việt. Trợ lý tên Thiên Nhãn. Chỉ chép lại lời nói rõ ràng.",
            boundary: boundary
        )
        body.appendMultipartFile(
            name: "file",
            filename: "recording.wav",
            contentType: "audio/wav",
            data: audio,
            boundary: boundary
        )
        body.append("--\(boundary)--\r\n".data(using: .utf8)!)

        var request = URLRequest(url: url)
        request.httpMethod = "POST"
        request.httpBody = body
        request.setValue("Bearer \(apiKey)", forHTTPHeaderField: "Authorization")
        request.setValue(
            "multipart/form-data; boundary=\(boundary)",
            forHTTPHeaderField: "Content-Type"
        )

        let configuration = URLSessionConfiguration.ephemeral
        configuration.timeoutIntervalForRequest = TimeInterval(timeoutSeconds)
        configuration.timeoutIntervalForResource = TimeInterval(timeoutSeconds)

        let data: Data
        let response: URLResponse
        do {
            (data, response) = try await URLSession(configuration: configuration)
                .data(for: request)
        } catch {
            if (error as? URLError)?.code == .timedOut {
                throw SpeechEngineError.timeout
            }
            throw SpeechEngineError.networkFailure(error.localizedDescription)
        }

        guard let httpResponse = response as? HTTPURLResponse else {
            throw SpeechEngineError.invalidResponse
        }
        guard (200...299).contains(httpResponse.statusCode) else {
            let message = (try? JSONDecoder().decode(GroqErrorEnvelope.self, from: data))?
                .error.message
                ?? String(data: data, encoding: .utf8)
                ?? "Lỗi không xác định."
            throw SpeechEngineError.requestFailed(
                statusCode: httpResponse.statusCode,
                message: message
            )
        }

        guard let decoded = try? JSONDecoder().decode(GroqTranscription.self, from: data) else {
            throw SpeechEngineError.invalidResponse
        }
        return decoded
    }
}

private struct GroqTranscription: Decodable {
    let text: String
    let duration: Double?
    let segments: [Segment]?

    struct Segment: Decodable {
        let avgLogprob: Double?
        let noSpeechProb: Double?

        enum CodingKeys: String, CodingKey {
            case avgLogprob = "avg_logprob"
            case noSpeechProb = "no_speech_prob"
        }
    }

    var containsSpeech: Bool {
        guard let segments, !segments.isEmpty else {
            return !text.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty
        }

        let probabilities = segments.compactMap(\.noSpeechProb)
        if !probabilities.isEmpty, probabilities.allSatisfy({ $0 >= 0.6 }) {
            return false
        }

        let averageNoSpeech = probabilities.isEmpty
            ? 0
            : probabilities.reduce(0, +) / Double(probabilities.count)
        let logProbabilities = segments.compactMap(\.avgLogprob)
        let averageLogProbability = logProbabilities.isEmpty
            ? 0
            : logProbabilities.reduce(0, +) / Double(logProbabilities.count)

        if averageNoSpeech >= 0.5, averageLogProbability < -0.8 {
            return false
        }

        return !Self.commonHallucinations.contains(foldedText)
    }

    private var foldedText: String {
        text.folding(
            options: [.caseInsensitive, .diacriticInsensitive],
            locale: Locale(identifier: "vi_VN")
        )
        .lowercased()
        .trimmingCharacters(in: .whitespacesAndNewlines.union(.punctuationCharacters))
    }

    private static let commonHallucinations: Set<String> = [
        "cam on cac ban da theo doi",
        "cam on cac ban da theo doi va hen gap lai",
        "hen gap lai",
        "thank you for watching",
    ]
}

private struct GroqErrorEnvelope: Decodable {
    struct Detail: Decodable {
        let message: String
    }

    let error: Detail
}

private extension Data {
    mutating func appendMultipartField(name: String, value: String, boundary: String) {
        append("--\(boundary)\r\n".data(using: .utf8)!)
        append("Content-Disposition: form-data; name=\"\(name)\"\r\n\r\n".data(using: .utf8)!)
        append("\(value)\r\n".data(using: .utf8)!)
    }

    mutating func appendMultipartFile(
        name: String,
        filename: String,
        contentType: String,
        data: Data,
        boundary: String
    ) {
        append("--\(boundary)\r\n".data(using: .utf8)!)
        append(
            "Content-Disposition: form-data; name=\"\(name)\"; filename=\"\(filename)\"\r\n"
                .data(using: .utf8)!
        )
        append("Content-Type: \(contentType)\r\n\r\n".data(using: .utf8)!)
        append(data)
        append("\r\n".data(using: .utf8)!)
    }
}

private func wavDuration(_ data: Data) -> Double {
    guard data.count >= 44,
          data.prefix(4) == Data("RIFF".utf8),
          data[8..<12] == Data("WAVE".utf8)
    else {
        return 0
    }

    var offset = 12
    var bytesPerSecond: UInt32?
    var audioBytes: UInt32?

    while offset + 8 <= data.count {
        let id = String(data: data[offset..<(offset + 4)], encoding: .ascii)
        let size = data.uint32LE(at: offset + 4)
        let content = offset + 8
        guard content + Int(size) <= data.count else { break }

        if id == "fmt ", size >= 12 {
            bytesPerSecond = data.uint32LE(at: content + 8)
        } else if id == "data" {
            audioBytes = size
        }
        offset = content + Int(size) + Int(size % 2)
    }

    guard let bytesPerSecond, bytesPerSecond > 0, let audioBytes else {
        return 0
    }
    return Double(audioBytes) / Double(bytesPerSecond)
}

private extension Data {
    func uint32LE(at offset: Int) -> UInt32 {
        UInt32(self[offset])
            | UInt32(self[offset + 1]) << 8
            | UInt32(self[offset + 2]) << 16
            | UInt32(self[offset + 3]) << 24
    }
}

private func durationInSeconds(_ duration: Duration) -> Double {
    let components = duration.components
    return Double(components.seconds) + Double(components.attoseconds) / 1e18
}
