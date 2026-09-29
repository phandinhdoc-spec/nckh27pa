import base64
import httpx
from urllib.parse import quote

from .config import Config
from .errors import APIError


TASKS = {
    "systemCommand", "quickClassification", "simpleConversation", "imageDescription",
    "visualQuestionAnswering", "objectRecognition", "textRecognition",
    "textInterpretation", "navigationAnalysis", "complexReasoning",
}
FAST_TASKS = {"systemCommand", "quickClassification", "simpleConversation"}


class Gemini:
    def __init__(self, config: Config):
        self.config = config

    def model_for(self, task: str) -> str:
        if task in FAST_TASKS:
            return self.config.fast_model
        if task == "complexReasoning":
            return self.config.reasoning_model
        return self.config.vision_model

    async def generate(self, task: str, prompt: str, image: tuple[bytes, str] | None = None) -> str:
        if not prompt.strip():
            raise APIError(400, "prompt_empty", "Nội dung gửi đến AI đang trống.")
        primary = self.model_for(task)
        try:
            return await self._request(primary, prompt.strip(), image)
        except APIError as error:
            # Retry only when the named model is explicitly unavailable. A
            # timeout/rate limit may already have consumed the first request.
            if error.code != "gemini_model_unavailable" or primary == self.config.fallback_model:
                raise
            return await self._request(self.config.fallback_model, prompt.strip(), image)

    async def _request(self, model: str, prompt: str, image: tuple[bytes, str] | None) -> str:
        parts = [{"text": prompt}]
        if image:
            data, mime = image
            parts.append({"inline_data": {"mime_type": mime, "data": base64.b64encode(data).decode("ascii")}})
        url = f"https://generativelanguage.googleapis.com/v1beta/models/{quote(model, safe='')}:generateContent"
        body = {"contents": [{"role": "user", "parts": parts}]}
        if model == "gemini-3.8-flash":
            body["generationConfig"] = {"thinkingConfig": {"thinkingLevel": "low"}}
        try:
            async with httpx.AsyncClient(timeout=self.config.timeout) as client:
                response = await client.post(
                    url, params={"key": self.config.gemini_key},
                    json=body,
                )
        except httpx.TimeoutException as error:
            raise APIError(504, "gemini_timeout", "Dịch vụ AI phản hồi quá chậm.") from error
        except httpx.RequestError as error:
            raise APIError(502, "gemini_unavailable", "Không thể kết nối dịch vụ AI.") from error
        if response.status_code == 404:
            raise APIError(502, "gemini_model_unavailable", "Mô hình AI chưa sẵn sàng.")
        if not 200 <= response.status_code < 300:
            raise APIError(502, "gemini_request_failed", "Dịch vụ AI xử lý thất bại.")
        try:
            payload = response.json()
            if payload.get("promptFeedback", {}).get("blockReason"):
                raise APIError(422, "gemini_blocked", "Dịch vụ AI từ chối xử lý nội dung này.")
            text = "\n".join(
                part["text"] for candidate in payload.get("candidates", [])
                for part in candidate.get("content", {}).get("parts", []) if "text" in part
            ).strip()
        except (ValueError, KeyError, TypeError, AttributeError) as error:
            raise APIError(500, "gemini_configuration_error", "Cấu hình dịch vụ AI không hợp lệ.") from error
        if not text:
            raise APIError(502, "gemini_empty_response", "Dịch vụ AI không trả về nội dung.")
        return text
