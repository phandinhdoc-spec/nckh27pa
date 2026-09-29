import unittest

from gestures import GestureClassifier, ModeState


class FakeTimer:
    instances = []

    def __init__(self, interval, callback, args=()):
        self.interval, self.callback, self.args = interval, callback, args
        self.daemon = False
        self.cancelled = False
        self.instances.append(self)

    def start(self):
        pass

    def cancel(self):
        self.cancelled = True

    def fire(self):
        self.callback(*self.args)


class GestureTests(unittest.TestCase):
    def setUp(self):
        FakeTimer.instances.clear()
        self.events = []
        self.classifier = GestureClassifier(self.events.append, timer_factory=FakeTimer)

    def tearDown(self):
        self.classifier.close()

    def test_single_double_triple_and_boundary(self):
        for count in (1, 2, 3):
            start = count * 10.0
            for offset in range(count):
                self.classifier.press(start + offset * 1.5)
            FakeTimer.instances[-1].fire()
        self.assertEqual(self.events, [1, 2, 3])

    def test_debounce_and_stale_timer(self):
        self.classifier.press(0.0)
        stale = FakeTimer.instances[-1]
        self.classifier.press(0.02)
        self.classifier.press(0.1)
        stale.fire()
        self.assertEqual(self.events, [])
        FakeTimer.instances[-1].fire()
        self.assertEqual(self.events, [2])

    def test_quiet_gap_flushes_previous_count(self):
        self.classifier.press(0.0)
        self.classifier.press(1.51)
        self.assertEqual(self.events, [1])
        FakeTimer.instances[-1].fire()
        self.assertEqual(self.events, [1, 1])

    def test_close_discards_pending(self):
        self.classifier.press(0.0)
        timer = FakeTimer.instances[-1]
        self.classifier.close()
        timer.fire()
        self.assertEqual(self.events, [])

    def test_one_press_stops_and_restarts_without_double_side_effect(self):
        state = ModeState()
        self.assertTrue(state.plan1_active)
        self.assertEqual(state.gesture(2), "reserved_plan2")
        self.assertEqual(state.gesture(3), "reserved_plan3")
        self.assertTrue(state.plan1_active)
        self.assertEqual(state.gesture(1), "plan1_stopped")
        self.assertFalse(state.plan1_active)
        self.assertEqual(state.gesture(1), "plan1_started")
        self.assertTrue(state.plan1_active)
        self.assertEqual(state.generation, 2)


if __name__ == "__main__":
    unittest.main()
