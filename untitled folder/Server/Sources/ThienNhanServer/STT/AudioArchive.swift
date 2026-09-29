import Foundation

struct AudioArchive {
    static let retention: TimeInterval = 30 * 24 * 60 * 60

    let directory: URL

    init(directory: URL = URL(fileURLWithPath: FileManager.default.currentDirectoryPath)
        .appendingPathComponent("wavpi", isDirectory: true)) throws {
        self.directory = directory
        try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
        try prune()
    }

    func store(_ data: Data, fileExtension: String) throws -> URL {
        try prune()
        let url = directory
            .appendingPathComponent("pi-\(UUID().uuidString)")
            .appendingPathExtension(fileExtension)
        try data.write(to: url, options: .atomic)
        return url
    }

    func prune(now: Date = .now) throws {
        let cutoff = now.addingTimeInterval(-Self.retention)
        for url in try FileManager.default.contentsOfDirectory(
            at: directory,
            includingPropertiesForKeys: [.contentModificationDateKey, .isRegularFileKey]
        ) {
            let values = try url.resourceValues(forKeys: [.contentModificationDateKey, .isRegularFileKey])
            if values.isRegularFile == true, values.contentModificationDate ?? now < cutoff {
                try FileManager.default.removeItem(at: url)
            }
        }
    }
}
