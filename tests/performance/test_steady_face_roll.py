from __future__ import annotations

import unittest

from tests.performance.steady_face_roll import (
    TimingSummary,
    evaluate_gate,
    normalized_linear_slope,
    percentile,
    summarize,
)


class SteadyFaceRollModelTests(unittest.TestCase):
    def test_percentile_and_summary_are_deterministic(self) -> None:
        values = [40, 10, 30, 20, 50]
        self.assertEqual(percentile(values, 0.0), 10.0)
        self.assertEqual(percentile(values, 0.95), 48.0)
        self.assertEqual(
            summarize(values),
            TimingSummary(count=5, minimum_ns=10, median_ns=30.0, p95_ns=48.0, maximum_ns=50),
        )

    def test_normalized_trend_detects_growth(self) -> None:
        self.assertAlmostEqual(normalized_linear_slope([100.0, 101.0, 102.0], 100.0), 0.01)
        self.assertAlmostEqual(normalized_linear_slope([102.0, 101.0, 100.0], 100.0), -0.01)

    def test_gate_enforces_all_three_thresholds(self) -> None:
        baseline = TimingSummary(100, 80, 100.0, 120.0, 150)
        post = TimingSummary(100, 90, 116.0, 151.0, 170)
        metrics, failures = evaluate_gate(
            baseline,
            post,
            [100.0, 102.0, 105.0],
            max_median_ratio=1.15,
            max_p95_ratio=1.25,
            max_trend_ratio_per_epoch=0.01,
        )
        self.assertAlmostEqual(metrics["post_pre_median_ratio"], 1.16)
        self.assertAlmostEqual(metrics["post_pre_p95_ratio"], 151.0 / 120.0)
        self.assertEqual(len(failures), 3)

    def test_gate_accepts_flat_post_churn_cost(self) -> None:
        baseline = TimingSummary(100, 80, 100.0, 120.0, 150)
        post = TimingSummary(100, 81, 104.0, 124.0, 154)
        _, failures = evaluate_gate(
            baseline,
            post,
            [102.0, 101.0, 103.0, 102.0],
            max_median_ratio=1.15,
            max_p95_ratio=1.25,
            max_trend_ratio_per_epoch=0.01,
        )
        self.assertEqual(failures, [])


if __name__ == "__main__":
    unittest.main()
