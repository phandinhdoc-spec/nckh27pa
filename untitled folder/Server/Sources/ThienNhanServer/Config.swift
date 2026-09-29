import Foundation

enum AppConfigError: LocalizedError {
    case missing(String)
    case invalidPort(String)
    case invalidPositiveInteger(key: String, value: String)

    var errorDescription: String? {
        switch self {
        case .missing(let key): "Thiếu biến cấu hình bắt buộc: \(key)"
        case .invalidPort(let value): "SERVER_PORT không hợp lệ: \(value)"
        case .invalidPositiveInteger(let key, let value): "\(key) phải là số nguyên dương: \(value)"
        }
    }
}

struct AppConfig: Sendable {
    let serverHost: String
    let serverPort: Int
    let serverToken: String
    let groqAPIKey: String
    let groqSTTModel: String
    let geminiAPIKey: String
    let geminiModelFast: String
    let geminiModelVision: String
    let geminiModelReasoning: String
    let geminiModelFallback: String
    let requestTimeoutSeconds: Int

    static func load(environmentFileURL: URL? = nil) throws -> Self {
        var values = try readEnvironmentFile(
            at: environmentFileURL ?? URL(fileURLWithPath: FileManager.default.currentDirectoryPath)
                .appendingPathComponent(".env")
        )
        values.merge(ProcessInfo.processInfo.environment) { _, processValue in processValue }

        let portText = value("SERVER_PORT", in: values) ?? "8765"
        guard let port = Int(portText), (1...65_535).contains(port) else {
            throw AppConfigError.invalidPort(portText)
        }

        let timeoutText = value("REQUEST_TIMEOUT_SECONDS", in: values) ?? "30"
        guard let timeout = Int(timeoutText), timeout > 0 else {
            throw AppConfigError.invalidPositiveInteger(
                key: "REQUEST_TIMEOUT_SECONDS",
                value: timeoutText
            )
        }

        func required(_ key: String) throws -> String {
            guard let value = value(key, in: values) else { throw AppConfigError.missing(key) }
            return value
        }

        return try Self(
            serverHost: value("SERVER_HOST", in: values) ?? "0.0.0.0",
            serverPort: port,
            serverToken: required("SERVER_TOKEN"),
            groqAPIKey: value("GROQ_API_KEY", in: values) ?? "",
            groqSTTModel: value("GROQ_STT_MODEL", in: values)
                ?? "whisper-large-v3-turbo",
            geminiAPIKey: required("GEMINI_API_KEY"),
            geminiModelFast: required("GEMINI_MODEL_FAST"),
            geminiModelVision: required("GEMINI_MODEL_VISION"),
            geminiModelReasoning: required("GEMINI_MODEL_REASONING"),
            geminiModelFallback: required("GEMINI_MODEL_FALLBACK"),
            requestTimeoutSeconds: timeout
        )
    }

    private static func value(_ key: String, in values: [String: String]) -> String? {
        values[key]?.trimmingCharacters(in: .whitespacesAndNewlines).nilIfEmpty
    }

    private static func readEnvironmentFile(at url: URL) throws -> [String: String] {
        guard FileManager.default.fileExists(atPath: url.path) else { return [:] }

        return try String(contentsOf: url, encoding: .utf8)
            .split(whereSeparator: \.isNewline)
            .reduce(into: [:]) { values, rawLine in
                let line = rawLine.trimmingCharacters(in: .whitespaces)
                guard !line.isEmpty, !line.hasPrefix("#") else { return }
                let assignment = line.hasPrefix("export ") ? line.dropFirst(7) : line[...]
                guard let separator = assignment.firstIndex(of: "=") else { return }
                let key = assignment[..<separator].trimmingCharacters(in: .whitespaces)
                var value = assignment[assignment.index(after: separator)...]
                    .trimmingCharacters(in: .whitespaces)
                if value.count >= 2, ["\"", "'"].contains(String(value.first!)), value.first == value.last {
                    value.removeFirst()
                    value.removeLast()
                }
                guard !key.isEmpty else { return }
                values[key] = value
            }
    }
}

private extension String {
    var nilIfEmpty: String? { isEmpty ? nil : self }
}
