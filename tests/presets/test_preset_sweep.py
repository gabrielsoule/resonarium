from __future__ import annotations

import unittest

from tests.presets.preset_sweep import _state_signature


class PresetSweepTests(unittest.TestCase):
    def test_state_signature_counts_serialized_state(self) -> None:
        xml = """
            <preset>
              <state>
                <MODITEM/><MODITEM/>
                <MSEG1/><MSEG2/>
                <samplePath/><group><samplePath/></group>
              </state>
            </preset>
        """
        self.assertEqual(
            {"mod_items": 2, "mseg_nodes": 2, "sample_paths": 2},
            _state_signature(xml),
        )

    def test_state_signature_handles_missing_state(self) -> None:
        self.assertEqual(
            {"mod_items": 0, "mseg_nodes": 0, "sample_paths": 0},
            _state_signature("<preset/>"),
        )


if __name__ == "__main__":
    unittest.main()
