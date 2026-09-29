import unittest
from unittest.mock import Mock

from navigation import NavigationError
from navigation_sensors import GY53FixedAngle


class SensorTests(unittest.TestCase):
    def new_adapter(self, *, cm: int | None = 123, ready: bool = True):
        adapter = GY53FixedAngle.__new__(GY53FixedAngle)
        adapter.distance = Mock(data_ready=ready, distance=cm)
        return adapter

    def test_gy53_centimeters_without_gy25(self):
        adapter = self.new_adapter()
        sample = adapter.sample()
        self.assertEqual(sample.distance_m, 1.23)
        adapter.distance.clear_interrupt.assert_called_once()

    def test_unavailable_distance_fails_closed(self):
        adapter = self.new_adapter(cm=None)
        with self.assertRaises(NavigationError):
            adapter.sample()


if __name__ == "__main__":
    unittest.main()
