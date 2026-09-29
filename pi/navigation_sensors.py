"""Plan 3 sensor adapter for VL53L1X-compatible GY53 I²C.

Do not use concurrently with the old service: only one process may own the camera.
Installation/orientation must be calibrated before navigation.
"""
from __future__ import annotations

import time

from navigation import NavigationError, SensorSample


class GY53FixedAngle:
    def __init__(self) -> None:
        import board
        import adafruit_vl53l1x

        self.distance = adafruit_vl53l1x.VL53L1X(board.I2C())
        self.distance.distance_mode = 2  # long range; test outdoors before relying on it
        self.distance.timing_budget = 100  # milliseconds; must fit frame interval
        self.distance.start_ranging()

    def close(self) -> None:
        self.distance.stop_ranging()

    def sample(self) -> SensorSample:
        deadline = time.monotonic() + 0.4
        while not self.distance.data_ready and time.monotonic() < deadline:
            time.sleep(0.01)
        if not self.distance.data_ready:
            raise NavigationError("GY53 không có phép đo mới.")
        distance_cm = self.distance.distance
        self.distance.clear_interrupt()
        if distance_cm is None or distance_cm <= 0:
            raise NavigationError("GY53 đo khoảng cách không hợp lệ.")
        return SensorSample(distance_m=distance_cm / 100)
