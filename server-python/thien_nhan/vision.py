from io import BytesIO
from time import perf_counter

from PIL import Image, ImageOps, UnidentifiedImageError
from pillow_heif import register_heif_opener
import pytesseract
from pytesseract import Output

from .config import Config
from .errors import APIError


MAX_IMAGE_SIZE = 10 * 1024 * 1024
IMAGE_TYPES = {"image/jpeg", "image/jpg", "image/png", "image/heic", "image/heif", "image/webp"}
register_heif_opener()


def normalized_type(value: str | None) -> str:
    return (value or "").split(";", 1)[0].lower()


def decode_image(data: bytes, mime: str) -> Image.Image:
    if mime not in IMAGE_TYPES:
        raise APIError(415, "unsupported_image_type", "Định dạng ảnh không được hỗ trợ.")
    if not data:
        raise APIError(400, "image_empty", "Dữ liệu ảnh đang trống.")
    if len(data) > MAX_IMAGE_SIZE:
        raise APIError(413, "image_too_large", "Ảnh vượt quá 10 MB.")
    try:
        image = Image.open(BytesIO(data))
        image.load()
        return ImageOps.exif_transpose(image).convert("RGB")
    except (UnidentifiedImageError, OSError, ValueError) as error:
        raise APIError(400, "invalid_image", "Ảnh không hợp lệ hoặc không thể đọc được.") from error


def recognize(data: bytes, mime: str, config: Config) -> dict:
    image = decode_image(data, mime)
    if config.tesseract_cmd:
        pytesseract.pytesseract.tesseract_cmd = config.tesseract_cmd
    start = perf_counter()
    try:
        result = pytesseract.image_to_data(image, lang="vie+eng", output_type=Output.DICT, timeout=config.timeout)
    except (pytesseract.TesseractError, pytesseract.TesseractNotFoundError, RuntimeError) as error:
        raise APIError(503, "ocr_unavailable", "Bộ nhận dạng văn bản chưa sẵn sàng.") from error
    groups: dict[tuple[int, int, int, int], list[int]] = {}
    for index, word in enumerate(result["text"]):
        if word.strip():
            key = tuple(result[field][index] for field in ("page_num", "block_num", "par_num", "line_num"))
            groups.setdefault(key, []).append(index)
    blocks = []
    for indices in groups.values():
        left = min(result["left"][i] for i in indices)
        top = min(result["top"][i] for i in indices)
        right = max(result["left"][i] + result["width"][i] for i in indices)
        bottom = max(result["top"][i] + result["height"][i] for i in indices)
        confidence = [max(0, float(result["conf"][i])) / 100 for i in indices]
        blocks.append({
            "text": " ".join(result["text"][i].strip() for i in indices),
            "confidence": sum(confidence) / len(confidence),
            "boundingBox": {"x": left, "y": top, "width": right - left, "height": bottom - top},
        })
    return {
        "text": "\n".join(block["text"] for block in blocks),
        "imageWidth": image.width, "imageHeight": image.height,
        "blocks": blocks, "processingTime": perf_counter() - start,
    }
