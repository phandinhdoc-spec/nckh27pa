import Hummingbird

struct HealthResponse: ResponseCodable, Sendable {
    let success: Bool
    let service: String
    let status: String
    let version: String

    static let ok = Self(success: true, service: "ThienNhanServer", status: "ok", version: "0.1.0")
}

struct APIError: HTTPResponseError, Sendable {
    struct Payload: Encodable, Sendable {
        struct Detail: Encodable, Sendable {
            let code: String
            let message: String
        }

        let error: Detail
    }

    let status: HTTPResponse.Status
    let code: String
    let message: String

    init(_ status: HTTPResponse.Status, code: String, message: String) {
        self.status = status
        self.code = code
        self.message = message
    }

    func response(from request: Request, context: some RequestContext) throws -> Response {
        var response = try context.responseEncoder.encode(
            Payload(error: .init(code: code, message: message)),
            from: request,
            context: context
        )
        response.status = status
        return response
    }
}

enum Server {
    static func run(config: AppConfig) async throws {
        let router = Router()
        let geminiEngine = GeminiEngine(config: config)
        router.get("/health") { _, _ in HealthResponse.ok }
        GeminiRoutes.register(on: router, engine: geminiEngine, serverToken: config.serverToken)
        VisionRoutes.register(on: router, serverToken: config.serverToken)
        TTSRoutes.register(on: router, serverToken: config.serverToken)
        CommandRoutes.register(
            on: router,
            geminiEngine: geminiEngine,
            serverToken: config.serverToken
        )

        print("ThienNhanServer: http://\(config.serverHost):\(config.serverPort)")
        try await Application(
            router: router,
            configuration: .init(address: .hostname(config.serverHost, port: config.serverPort))
        ).runService()
    }
}

func validateBearerToken(_ request: Request, expectedToken: String) throws {
    guard let authorization = request.headers[.authorization],
          authorization.hasPrefix("Bearer "),
          authorization.dropFirst(7) == expectedToken
    else {
        throw APIError(
            .unauthorized,
            code: "unauthorized",
            message: "Bearer Token không hợp lệ."
        )
    }
}
