import unittest

from control import Controller


class ControllerTests(unittest.TestCase):
    def setUp(self):
        self.now = 10.0
        self.controller = Controller(clock=lambda: self.now)

    def test_double_toggles_and_single_interrupts_plan2(self):
        first = self.controller.gesture(2)
        self.assertEqual(first.action, "mode_changed")
        self.assertTrue(first.cancel_current)
        self.assertEqual(self.controller.mode, "plan2")
        self.assertEqual(self.controller.gesture(1).action, "interrupted")
        self.assertEqual(self.controller.mode, "plan2")
        self.assertEqual(self.controller.gesture(2).action, "mode_changed")
        self.assertEqual(self.controller.mode, "plan1")
        self.assertTrue(self.controller.plan1_active)

    def test_single_stops_and_starts_plan1(self):
        self.assertEqual(self.controller.gesture(1).action, "plan1_stopped")
        self.assertEqual(self.controller.transcript("Mấy giờ rồi?").action, "ignored")
        self.assertEqual(self.controller.gesture(1).action, "plan1_started")
        self.assertEqual(self.controller.transcript("Mấy giờ rồi?").action, "request")

    def test_spoken_controls_override_stopped_plan1(self):
        self.controller.gesture(1)
        self.assertEqual(self.controller.transcript("Bật chế độ tìm đồ vật").action, "mode_changed")
        self.assertEqual(self.controller.mode, "plan2")
        self.assertEqual(self.controller.transcript("Tìm cái cốc").action, "request")
        self.assertEqual(self.controller.transcript("Về chế độ hội thoại").action, "mode_changed")
        self.assertTrue(self.controller.plan1_active)

    def test_triple_asks_but_never_navigates(self):
        self.assertEqual(self.controller.gesture(3).action, "navigation_confirmation")
        self.assertEqual(self.controller.transcript("Có").action, "navigation_unavailable")
        self.assertEqual(self.controller.mode, "plan1")
        self.assertEqual(self.controller.gesture(3).action, "navigation_confirmation")
        self.assertEqual(self.controller.transcript("Không").action, "navigation_declined")

    def test_voice_navigation_and_expired_confirmation(self):
        self.assertEqual(self.controller.transcript("Hướng dẫn đi đường").action, "navigation_confirmation")
        self.now += 16
        self.assertEqual(self.controller.transcript("Có").action, "confirmation_expired")
        self.assertNotEqual(self.controller.transcript("Có").action, "navigation_unavailable")

    def test_unclear_answer_keeps_confirmation_pending(self):
        self.controller.gesture(3)
        self.assertEqual(self.controller.transcript("Hướng dẫn đi đường").action, "navigation_unclear")
        self.assertEqual(self.controller.transcript("Có").action, "navigation_unavailable")

    def test_stop_cancels_previous_generation(self):
        previous = self.controller.generation
        result = self.controller.transcript("Dừng lại")
        self.assertEqual(result.action, "interrupted")
        self.assertTrue(result.cancel_current)
        self.assertGreater(self.controller.generation, previous)

    def test_no_navigation_on_other_multi_press(self):
        self.assertEqual(self.controller.gesture(4).action, "reserved")
        self.assertEqual(self.controller.transcript("Có").action, "request")

    def test_experimental_plan3_can_start_and_stop_without_guidance(self):
        controller = Controller(clock=lambda: self.now, allow_experimental_plan3=True)
        self.assertEqual(controller.gesture(3).action, "navigation_confirmation")
        self.assertEqual(controller.transcript("Có").action, "navigation_started")
        self.assertEqual(controller.mode, "plan3")
        self.assertEqual(controller.transcript("Vật cản ở đâu?").action, "navigation_running")
        self.assertEqual(controller.gesture(1).action, "navigation_stopped")
        self.assertEqual(controller.mode, "plan1")

    def test_button_confirms_pending_plan3_without_microphone(self):
        controller = Controller(clock=lambda: self.now, allow_experimental_plan3=True)
        controller.gesture(3)
        self.assertEqual(controller.gesture(1).action, "navigation_started")
        self.assertEqual(controller.mode, "plan3")
        self.assertEqual(controller.gesture(1).action, "navigation_stopped")

    def test_button_does_not_confirm_expired_request(self):
        controller = Controller(clock=lambda: self.now, allow_experimental_plan3=True)
        controller.gesture(3)
        self.now += 16
        self.assertEqual(controller.gesture(1).action, "confirmation_expired")
        self.assertEqual(controller.mode, "plan1")

    def test_confirmation_window_starts_again_after_prompt_audio(self):
        controller = Controller(clock=lambda: self.now, allow_experimental_plan3=True)
        controller.gesture(3)
        self.now += 20
        controller.confirmation_prompt_finished(controller.generation)
        self.assertEqual(controller.gesture(1).action, "navigation_started")


if __name__ == "__main__":
    unittest.main()
