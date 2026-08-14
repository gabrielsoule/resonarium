from __future__ import annotations

import unittest

import numpy as np

from tests.exciter.audio_contract import max_error, normalized_correlation, prng_oracle


class ExciterContractMathTests(unittest.TestCase):
    def test_max_error_uses_float64_subtraction(self) -> None:
        left = np.asarray([1.0, -2.0], dtype=np.float32)
        right = np.asarray([1.0, -1.75], dtype=np.float32)
        self.assertEqual(0.25, max_error(left, right))

    def test_normalized_correlation_centres_streams(self) -> None:
        left = np.asarray([1.0, -1.0, 1.0, -1.0], dtype=np.float32)
        orthogonal = np.asarray([1.0, 1.0, -1.0, -1.0], dtype=np.float32)
        self.assertAlmostEqual(0.0, normalized_correlation(left, orthogonal))
        self.assertAlmostEqual(1.0, normalized_correlation(left, left))

    def test_prng_oracle_is_repeatable_and_bounded(self) -> None:
        first = prng_oracle(3, 17, 1, 0, 1024)
        second = prng_oracle(3, 17, 1, 0, 1024)
        self.assertTrue(np.array_equal(first, second))
        self.assertGreaterEqual(float(np.min(first)), 0.0)
        self.assertLess(float(np.max(first)), 1.0)


if __name__ == "__main__":
    unittest.main()
