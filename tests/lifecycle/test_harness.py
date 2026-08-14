from __future__ import annotations

import unittest

from tests.lifecycle.harness import normalize_telemetry, telemetry_failures
from tests.lifecycle.scenarios import (
    RawMidiEvent,
    SCENARIOS,
    VoiceScenario,
    make_random_stress,
    pitch_bend,
    select_scenarios,
)


def telemetry(**changes: int) -> dict[str, int]:
    result = {
        "configured_voice_limit": 8,
        "musical_voices": 8,
        "fast_fade_voices": 2,
        "rendered_voices": 10,
        "physical_pool_size": 18,
        "note_starts": 12,
        "steals": 4,
        "forced_fade_retirements": 2,
        "tail_retirements": 1,
        "dropped_note_ons": 0,
    }
    result.update(changes)
    return result


class ScenarioTests(unittest.TestCase):
    def test_catalog_is_valid_and_unique(self) -> None:
        for scenario in SCENARIOS:
            scenario.validate()
        self.assertEqual(len(SCENARIOS), len({scenario.id for scenario in SCENARIOS}))

    def test_catalog_uses_only_mpe_member_note_channels(self) -> None:
        for scenario in SCENARIOS:
            for action in scenario.actions:
                if isinstance(action, RawMidiEvent) and action.is_note_on:
                    self.assertGreaterEqual((action.data[0] & 0x0F) + 1, 2)

    def test_smoke_and_exact_selection(self) -> None:
        smoke = select_scenarios("smoke")
        self.assertGreaterEqual(len(smoke), 2)
        self.assertEqual((smoke[0],), select_scenarios("all", (smoke[0].id,)))

    def test_pitch_bend_is_little_endian_seven_bit_data(self) -> None:
        self.assertEqual(bytes((0xE1, 0x00, 0x40)), pitch_bend(0, 2, 8192).data)

    def test_stress_stream_is_deterministic_and_exact_length(self) -> None:
        first = make_random_stress(100, 123)
        second = make_random_stress(100, 123)
        self.assertEqual(first.actions, second.actions)
        self.assertEqual(100, len(first.actions))

    def test_invalid_event_order_is_rejected(self) -> None:
        scenario = VoiceScenario(
            "test.invalid",
            "invalid",
            128,
            actions=(RawMidiEvent(2, b"\x91\x3c\x64"), RawMidiEvent(1, b"\x81\x3c\x00")),
        )
        with self.assertRaises(ValueError):
            scenario.validate()


class TelemetryTests(unittest.TestCase):
    def test_valid_boundaries_pass(self) -> None:
        self.assertEqual([], telemetry_failures(telemetry(), 8))

    def test_each_hard_bound_is_enforced(self) -> None:
        self.assertTrue(telemetry_failures(telemetry(musical_voices=9), 8))
        self.assertTrue(telemetry_failures(telemetry(fast_fade_voices=3), 8))
        self.assertTrue(telemetry_failures(telemetry(rendered_voices=11), 8))
        self.assertTrue(telemetry_failures(telemetry(physical_pool_size=64), 8))
        self.assertTrue(telemetry_failures(telemetry(dropped_note_ons=1), 8))

    def test_counters_must_be_monotonic(self) -> None:
        previous = telemetry(note_starts=13)
        self.assertIn("counter note_starts moved backwards", telemetry_failures(telemetry(), 8, previous))

    def test_missing_fields_are_rejected(self) -> None:
        value = telemetry()
        del value["steals"]
        with self.assertRaisesRegex(RuntimeError, "steals"):
            normalize_telemetry(value)


if __name__ == "__main__":
    unittest.main()

