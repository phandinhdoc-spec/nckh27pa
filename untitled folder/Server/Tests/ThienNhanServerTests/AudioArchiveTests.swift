import Foundation
import XCTest
@testable import ThienNhanServer

final class AudioArchiveTests: XCTestCase {
    func testKeepsNewFilesAndPrunesOldFiles() throws {
        let directory = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        defer { try? FileManager.default.removeItem(at: directory) }
        let archive = try AudioArchive(directory: directory)
        let old = try archive.store(Data("old".utf8), fileExtension: "wav")
        try FileManager.default.setAttributes(
            [.modificationDate: Date.now.addingTimeInterval(-AudioArchive.retention - 1)],
            ofItemAtPath: old.path
        )
        let fresh = try archive.store(Data("fresh".utf8), fileExtension: "wav")

        try archive.prune()

        XCTAssertFalse(FileManager.default.fileExists(atPath: old.path))
        XCTAssertTrue(FileManager.default.fileExists(atPath: fresh.path))
    }
}
