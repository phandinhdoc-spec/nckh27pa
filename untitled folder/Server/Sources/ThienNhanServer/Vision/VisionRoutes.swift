import Foundation
import Hummingbird
import ImageIO
import NIOCore
import Vision

struct PixelRect: Codable, Sendable {
    let x: Int
    let y: Int
    let width: Int
    let height: Int
}

struct OCRTextBlock: Codable, Sendable {
    let text: String
    let confidence: Float
    let boundingBox: PixelRect
}

struct VisionOCRResponse: ResponseCodable, Sendable {
    let text: String
    let imageWidth: Int
    let imageHeight: Int
    let blocks: [OCRTextBlock]
    let processingTime: Double
}

enum VisionRoutes {
    static let maximumImageSize = 10 * 1_024 * 1_024
    private static let contentTypes: Set<String> = [
        "image/jpeg", "image/jpg", "image/png", "image/heic", "image/heif", "image/webp",
    ]

    static func register(on router: Router<BasicRequestContext>, serverToken: String) {
        router.post("/vision/ocr") { request, _ -> VisionOCRResponse in
            try validateBearerToken(request, expectedToken: serverToken)
            guard let contentType = request.headers[.contentType]?
                .lowercased()
                .split(separator: ";", maxSplits: 1)
                .first
                .map(String.init),
                  supports(contentType)
            else {
                throw APIError(
                    .unsupportedMediaType,
                    code: "unsupported_image_type",
                    message: "Định dạng ảnh không được hỗ trợ."
                )
            }

            let buffer: ByteBuffer
            do {
                buffer = try await request.body.collect(upTo: maximumImageSize)
            } catch {
                throw APIError(.contentTooLarge, code: "image_too_large", message: "Ảnh vượt quá 10 MB.")
            }
            guard buffer.readableBytes > 0 else {
                throw APIError(.badRequest, code: "image_empty", message: "Dữ liệu ảnh đang trống.")
            }

            do {
                return try await recognizeText(in: Data(buffer.readableBytesView))
            } catch VisionOCRError.invalidImage {
                throw APIError(
                    .badRequest,
                    code: "invalid_image",
                    message: "Ảnh không hợp lệ hoặc không thể đọc được."
                )
            }
        }
    }

    static func supports(_ contentType: String) -> Bool {
        contentTypes.contains(contentType.lowercased())
    }

    static func recognizeText(in data: Data) async throws -> VisionOCRResponse {
        let image = try imageInfo(from: data)
        var request = RecognizeTextRequest()
        request.recognitionLevel = .accurate
        request.usesLanguageCorrection = true
        request.automaticallyDetectsLanguage = true
        request.recognitionLanguages = ["vi-VN", "en-US"]
            .map(Locale.Language.init(identifier:))
            .filter(request.supportedRecognitionLanguages.contains)

        let clock = ContinuousClock()
        let start = clock.now
        let observations = try await request.perform(on: data, orientation: image.orientation)
        let blocks = observations.map { observation in
            let rect = observation.boundingBox
                .toImageCoordinates(image.size, origin: .upperLeft)
                .integral
            return OCRTextBlock(
                text: observation.transcript,
                confidence: observation.confidence,
                boundingBox: PixelRect(
                    x: max(0, Int(rect.minX)),
                    y: max(0, Int(rect.minY)),
                    width: max(0, Int(rect.width)),
                    height: max(0, Int(rect.height))
                )
            )
        }
        let elapsed = start.duration(to: clock.now).components

        return VisionOCRResponse(
            text: blocks.map(\.text).joined(separator: "\n"),
            imageWidth: Int(image.size.width),
            imageHeight: Int(image.size.height),
            blocks: blocks,
            processingTime: Double(elapsed.seconds) + Double(elapsed.attoseconds) / 1e18
        )
    }

    private static func imageInfo(from data: Data) throws -> (size: CGSize, orientation: CGImagePropertyOrientation) {
        guard let source = CGImageSourceCreateWithData(data as CFData, nil),
              let properties = CGImageSourceCopyPropertiesAtIndex(source, 0, nil) as? [CFString: Any],
              let width = (properties[kCGImagePropertyPixelWidth] as? NSNumber)?.intValue,
              let height = (properties[kCGImagePropertyPixelHeight] as? NSNumber)?.intValue,
              width > 0,
              height > 0
        else {
            throw VisionOCRError.invalidImage
        }

        let rawOrientation = (properties[kCGImagePropertyOrientation] as? NSNumber)?.uint32Value ?? 1
        let orientation = CGImagePropertyOrientation(rawValue: rawOrientation) ?? .up
        let rotated = [.leftMirrored, .right, .rightMirrored, .left].contains(orientation)
        return (CGSize(width: rotated ? height : width, height: rotated ? width : height), orientation)
    }
}

private enum VisionOCRError: Error {
    case invalidImage
}
