"""Experimental literal tt.md geometry; not calibrated for navigation or SOS."""
import math


class GeometryInputError(ValueError):
    pass


FIXED_ALPHA_DEG = 45.0
FIXED_BETA_DEG = 35.0
FIXED_PITCH_DEG = 90.0 - FIXED_ALPHA_DEG


def compute_geometry(a: float, b: float, L: float) -> dict:
    """Image a is horizontal, b vertical; L and output distances share a unit."""
    values = (a, b, L)
    if any(isinstance(v, bool) or not isinstance(v, (int, float)) or
           not math.isfinite(v) for v in values):
        raise GeometryInputError("Tọa độ và cảm biến phải là số hữu hạn.")
    if not (0 <= a < 640 and 0 <= b < 640 and L > 0):
        raise GeometryInputError("Tọa độ, khoảng cách hoặc góc ngoài phạm vi hỗ trợ.")
    alpha_deg = FIXED_ALPHA_DEG
    beta_deg = FIXED_BETA_DEG
    alpha, beta = math.radians(alpha_deg), math.radians(beta_deg)
    ca, cb = math.cos(alpha), math.cos(beta)
    denominator = 320 * ca * cb
    if abs(denominator) < 1e-9:
        return _invalid(alpha_deg, beta_deg, "singular_tan_phi")
    # tt.md: only 640-a is inside an absolute value; preserve other signs.
    tan_phi = abs(640 - a) * (ca * math.sin(beta) - 320 * math.cos(alpha - beta)) / denominator
    phi = math.atan(tan_phi)
    sum_angle = alpha + phi
    if abs(math.cos(sum_angle)) < 1e-9:
        return _invalid(alpha_deg, beta_deg, "singular_alpha_plus_phi", tan_phi, math.degrees(phi))
    dx = L * ca * math.tan(sum_angle)
    dy = (320 - b) * L * ca * math.sin(beta) * math.cos(phi) / (320 * math.cos(sum_angle) * cb)
    if not all(map(math.isfinite, (tan_phi, phi, dx, dy))):
        return _invalid(alpha_deg, beta_deg, "nonfinite_result")
    # Image-side metadata uses horizontal pixel a independently of signed DY.
    side = "left" if a < 320 else "right" if a > 320 else "center"
    return {"valid": dx >= 0, "reason": None if dx >= 0 else "negative_dx",
            "alphaDeg": alpha_deg, "betaDeg": beta_deg, "phiDeg": math.degrees(phi),
            "tanPhi": tan_phi, "DX": dx, "DY": dy, "lateralSide": side}


def _invalid(alpha, beta, reason, tan_phi=None, phi_deg=None):
    return {"valid": False, "reason": reason, "alphaDeg": alpha, "betaDeg": beta,
            "phiDeg": phi_deg, "tanPhi": tan_phi, "DX": None, "DY": None,
            "lateralSide": "unknown"}
