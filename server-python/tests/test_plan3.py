import base64
from io import BytesIO
import math
import uuid

import pytest
from fastapi.testclient import TestClient
from PIL import Image

from thien_nhan.app import create_app
from thien_nhan.plan3_geometry import compute_geometry, GeometryInputError
from test_api import CONFIG, FakeGemini


def test_literal_geometry_and_negative_dx():
    result = compute_geometry(320, 300, 2)
    alpha, beta = math.radians(45), math.radians(35)
    tangent = 320 * (math.cos(alpha) * math.sin(beta) - 320 * math.cos(alpha-beta)) / (320 * math.cos(alpha) * math.cos(beta))
    phi = math.atan(tangent)
    assert result["tanPhi"] == pytest.approx(tangent)
    assert result["alphaDeg"] == 45
    assert result["betaDeg"] == 35
    assert result["phiDeg"] == pytest.approx(math.degrees(phi))
    assert result["tanPhi"] < 0
    assert compute_geometry(320, 340, 2)["DY"] == pytest.approx(-result["DY"])
    assert compute_geometry(639, 300, 2)["phiDeg"] != result["phiDeg"]
    assert result["DX"] == pytest.approx(2 * math.cos(alpha) * math.tan(alpha+phi))
    assert result["DY"] == pytest.approx(20 * 2 * math.cos(alpha) * math.sin(beta) * math.cos(phi) / (320 * math.cos(alpha+phi) * math.cos(beta)))
    assert result["DX"] < 0 and result["valid"] is False and result["reason"] == "negative_dx"
    assert result["lateralSide"] == "center"
    assert compute_geometry(290, 300, 2)["lateralSide"] == "left"
    assert compute_geometry(340, 300, 2)["lateralSide"] == "right"


def test_geometry_guards():
    assert compute_geometry(100, 200, 2)["alphaDeg"] == 45
    for value in (float("nan"), float("inf"), True):
        with pytest.raises(GeometryInputError):
            compute_geometry(100, 200, value)
    with pytest.raises(GeometryInputError):
        compute_geometry(640, 200, 2)


def test_plan3_endpoint_fail_closed():
    buffer = BytesIO()
    Image.new("RGB", (640, 640)).save(buffer, format="JPEG")
    body = {"sessionId": str(uuid.uuid4()), "frameId": 7,
            "capturedAt": 1790531445.59, "image": base64.b64encode(buffer.getvalue()).decode(),
            "imageContentType": "image/jpeg", "pitchDeg": 45, "distanceM": 2}
    with TestClient(create_app(CONFIG, FakeGemini())) as client:
        assert client.post("/plan3/analyze", json=body).status_code == 401
        headers = {"Authorization": "Bearer test-token"}
        response = client.post("/plan3/analyze", json=body, headers=headers)
        assert response.status_code == 200
        assert response.json() == {"sessionId": body["sessionId"], "frameId": body["frameId"],
            "capturedAt": body["capturedAt"], "imageWidth": 640, "imageHeight": 640,
            "sensor": {"pitchDeg": 45, "distanceM": 2},
            "detections": [], "geometry": [], "valid": False,
            "status": "vision_unknown", "alert": None, "sosEnabled": False,
            "blockers": ["geometry_not_calibrated"]}
        assert client.post("/plan3/analyze", json={**body, "distanceM": "nan"}, headers=headers).status_code == 400
        assert client.post("/plan3/analyze", json={**body, "pitchDeg": 30}, headers=headers).status_code == 400
        assert client.post("/plan3/analyze", json={**body, "imageContentType": "image/png"}, headers=headers).status_code == 415
        assert client.post("/plan3/analyze", json={**body, "capturedAt": -1}, headers=headers).status_code == 400
