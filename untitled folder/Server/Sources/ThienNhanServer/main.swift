import Foundation

@main
struct Main {
    static func main() async {
        do {
            try await Server.run(config: AppConfig.load())
        } catch {
            FileHandle.standardError.write(Data("Lỗi: \(error.localizedDescription)\n".utf8))
            Foundation.exit(EXIT_FAILURE)
        }
    }
}
