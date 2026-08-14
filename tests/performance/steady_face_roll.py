#!/usr/bin/env python3
"""Gate steady 15-voice callback cost before and after repeated face-rolls.

The extension times ``ResonariumProcessor::processBlock`` internally, so these
measurements exclude Python dispatch, NumPy inspection and audio-buffer copies.
Every post-churn window restores the same 15-note lower-zone MPE chord used by
the baseline window.
"""

from __future__ import annotations

import argparse
import json
import statistics
import sys
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any, Sequence

if not __package__:
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tests.module_loader import HarnessError, load_resonarium


@dataclass(frozen=True)
class TimingSummary:
    count: int
    minimum_ns: int
    median_ns: float
    p95_ns: float
    maximum_ns: int


STEADY_VOICE_COUNT = 15
FACE_ROLL_NOTE_ONS = 128
PHYSICAL_VOICE_COUNT = 18

# Exercise the expensive voice DSP, not merely the fixed allocator.  The
# processor's default state has every exciter and resonator disabled, which is
# useful as a silence fixture but is not a meaningful real-audio CPU gate.
WORKLOAD_PARAMETERS: tuple[tuple[str, float], ...] = (
    ("numVoices", 16.0),
    ("masterGain", -24.0),
    ("impExciter0 enabled", 1.0),
    ("impExciter0 thickness", 2.0),
    ("impExciter0 pickPosition", 0.37),
    ("impExciter0 level", 0.35),
    ("outputGain wb0", -12.0),
    *((f"enabled wb0r{index}", 1.0) for index in range(8)),
    *((f"pitch wb0r{index}", pitch) for index, pitch in enumerate(
        (1.0, 1.25, 1.5, 2.0, 2.5, 3.0, 4.0, 5.0)
    )),
    *((f"decayTime wb0r{index}", 1.25) for index in range(8)),
)


def percentile(values: Sequence[int], quantile: float) -> float:
    """Return the linearly interpolated percentile used by the gate."""
    if not values:
        raise ValueError("cannot summarize an empty timing window")
    if not 0.0 <= quantile <= 1.0:
        raise ValueError("quantile must be in [0, 1]")

    ordered = sorted(values)
    position = (len(ordered) - 1) * quantile
    lower = int(position)
    upper = min(lower + 1, len(ordered) - 1)
    fraction = position - lower
    return ordered[lower] * (1.0 - fraction) + ordered[upper] * fraction


def summarize(values: Sequence[int]) -> TimingSummary:
    if not values or any(value <= 0 for value in values):
        raise ValueError("callback timings must be positive")
    return TimingSummary(
        count=len(values),
        minimum_ns=min(values),
        median_ns=float(statistics.median(values)),
        p95_ns=percentile(values, 0.95),
        maximum_ns=max(values),
    )


def normalized_linear_slope(values: Sequence[float], normalizer: float) -> float:
    """Least-squares slope per epoch, normalized to the pre-churn median."""
    if len(values) < 2:
        raise ValueError("at least two epoch medians are required")
    if normalizer <= 0.0:
        raise ValueError("normalizer must be positive")

    midpoint = (len(values) - 1) * 0.5
    denominator = sum((index - midpoint) ** 2 for index in range(len(values)))
    mean = sum(values) / len(values)
    numerator = sum(
        (index - midpoint) * (value - mean) for index, value in enumerate(values)
    )
    return numerator / denominator / normalizer


def evaluate_gate(
    baseline: TimingSummary,
    post: TimingSummary,
    epoch_medians_ns: Sequence[float],
    max_median_ratio: float,
    max_p95_ratio: float,
    max_trend_ratio_per_epoch: float,
) -> tuple[dict[str, float], list[str]]:
    median_ratio = post.median_ns / baseline.median_ns
    p95_ratio = post.p95_ns / baseline.p95_ns
    trend = normalized_linear_slope(epoch_medians_ns, baseline.median_ns)
    metrics = {
        "post_pre_median_ratio": median_ratio,
        "post_pre_p95_ratio": p95_ratio,
        "normalized_trend_per_epoch": trend,
    }
    failures: list[str] = []
    if median_ratio > max_median_ratio:
        failures.append(f"post/pre median ratio {median_ratio:.4f} > {max_median_ratio:.4f}")
    if p95_ratio > max_p95_ratio:
        failures.append(f"post/pre p95 ratio {p95_ratio:.4f} > {max_p95_ratio:.4f}")
    if trend > max_trend_ratio_per_epoch:
        failures.append(
            f"normalized epoch trend {trend:.4f} > {max_trend_ratio_per_epoch:.4f} per epoch"
        )
    return metrics, failures


def _status(kind: int, channel: int) -> int:
    return kind | (channel - 1)


def _queue_note_on(synth: Any, channel: int, note: int, velocity: int, offset: int = 0) -> None:
    synth.queue_midi(bytes((_status(0x90, channel), note, velocity)), offset)


def _process(synth: Any) -> int:
    duration = int(synth.benchmark_process_block_ns())
    if duration <= 0:
        raise HarnessError("benchmark_process_block_ns() returned a non-positive duration")
    return duration


def _telemetry(synth: Any) -> dict[str, int]:
    if not hasattr(synth, "get_voice_telemetry"):
        raise HarnessError("extension lacks get_voice_telemetry(); rebuild Resonarium_Python")
    return {key: int(value) for key, value in synth.get_voice_telemetry().items()}


def _require_steady_voices(synth: Any, context: str) -> None:
    telemetry = _telemetry(synth)
    expected = {
        "configured_voice_limit": 16,
        "musical_voices": STEADY_VOICE_COUNT,
        "fast_fade_voices": 0,
        "rendered_voices": STEADY_VOICE_COUNT,
        "physical_pool_size": PHYSICAL_VOICE_COUNT,
        "dropped_note_ons": 0,
    }
    mismatches = [
        f"{field}={telemetry.get(field)!r} (expected {value})"
        for field, value in expected.items()
        if telemetry.get(field) != value
    ]
    if mismatches:
        raise HarnessError(f"{context} is not a steady 15-voice render: " + ", ".join(mismatches))


def _require_face_roll(synth: Any, starts_before: int, context: str) -> None:
    telemetry = _telemetry(synth)
    failures: list[str] = []
    if telemetry.get("note_starts", 0) - starts_before != FACE_ROLL_NOTE_ONS:
        failures.append(
            f"note-start delta={telemetry.get('note_starts', 0) - starts_before} "
            f"(expected {FACE_ROLL_NOTE_ONS})"
        )
    if telemetry.get("configured_voice_limit") != 16:
        failures.append(f"configured_voice_limit={telemetry.get('configured_voice_limit')} != 16")
    if telemetry.get("musical_voices") != 16:
        failures.append(f"musical_voices={telemetry.get('musical_voices')} != 16")
    if telemetry.get("fast_fade_voices", 0) > 2:
        failures.append(f"fast_fade_voices={telemetry.get('fast_fade_voices')} > 2")
    rendered = telemetry.get("rendered_voices", 0)
    if not 16 <= rendered <= PHYSICAL_VOICE_COUNT:
        failures.append(
            f"rendered_voices={rendered} outside [16, {PHYSICAL_VOICE_COUNT}]"
        )
    if telemetry.get("physical_pool_size") != PHYSICAL_VOICE_COUNT:
        failures.append(
            f"physical_pool_size={telemetry.get('physical_pool_size')} != {PHYSICAL_VOICE_COUNT}"
        )
    if telemetry.get("dropped_note_ons", 0) != 0:
        failures.append(f"dropped_note_ons={telemetry.get('dropped_note_ons')}")
    if failures:
        raise HarnessError(f"{context} did not complete a bounded face-roll: " + ", ".join(failures))


def _panic(synth: Any) -> None:
    synth.queue_midi(bytes((_status(0xB0, 1), 120, 0)), 0)
    _process(synth)


def _start_steady_chord(synth: Any) -> None:
    # Lower-zone member channels 2-16, one stable note per member.
    for member in range(15):
        _queue_note_on(synth, member + 2, 48 + member, 96)
    _process(synth)


def _face_roll(synth: Any, epoch: int, block_size: int) -> None:
    # Queue exactly 128 deterministic note-ons into one callback.  Vary the
    # permutation by epoch while preserving identical work and event count.
    for index in range(FACE_ROLL_NOTE_ONS):
        channel = 2 + (index + epoch * 7) % 15
        note = 24 + (index * 29 + epoch * 11) % 96
        velocity = 70 + (index + epoch * 13) % 58
        _queue_note_on(synth, channel, note, velocity, index % block_size)
    _process(synth)


def _settle(synth: Any, blocks: int) -> None:
    for _ in range(blocks):
        _process(synth)


def _measure(synth: Any, blocks: int) -> list[int]:
    return [_process(synth) for _ in range(blocks)]


def _write_report(path: Path | None, report: dict[str, Any]) -> None:
    if path is None:
        return
    destination = path / "steady-face-roll-report.json" if path.suffix == "" else path
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = destination.with_suffix(destination.suffix + ".tmp")
    temporary.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    temporary.replace(destination)


def run(args: argparse.Namespace) -> int:
    module, resolved_module = load_resonarium(args.module)
    synth = module.Resonarium(float(args.sample_rate), args.block_size)
    if not hasattr(synth, "benchmark_process_block_ns"):
        raise HarnessError(
            "extension lacks benchmark_process_block_ns(); rebuild Resonarium_Python"
        )

    for parameter_id, value in WORKLOAD_PARAMETERS:
        synth.set_param(parameter_id, value)
    _settle(synth, args.initial_warmup_blocks)
    _start_steady_chord(synth)
    _settle(synth, args.settle_blocks)
    _require_steady_voices(synth, "baseline window")
    baseline_values = _measure(synth, args.measure_blocks)
    _require_steady_voices(synth, "completed baseline window")
    baseline = summarize(baseline_values)

    epoch_summaries: list[TimingSummary] = []
    for epoch in range(args.epochs):
        _panic(synth)
        starts_before = _telemetry(synth)["note_starts"]
        _face_roll(synth, epoch, args.block_size)
        _require_face_roll(synth, starts_before, f"epoch {epoch + 1}")
        _panic(synth)
        _start_steady_chord(synth)
        _settle(synth, args.settle_blocks)
        _require_steady_voices(synth, f"epoch {epoch + 1} window")
        epoch_values = _measure(synth, args.measure_blocks)
        _require_steady_voices(synth, f"completed epoch {epoch + 1} window")
        epoch_summaries.append(summarize(epoch_values))

    post = epoch_summaries[-1]
    epoch_medians = [summary.median_ns for summary in epoch_summaries]
    metrics, failures = evaluate_gate(
        baseline,
        post,
        epoch_medians,
        args.max_median_ratio,
        args.max_p95_ratio,
        args.max_trend_ratio_per_epoch,
    )
    report = {
        "schema_version": 1,
        "status": "fail" if failures else "pass",
        "module": str(resolved_module),
        "configuration": {
            "sample_rate": args.sample_rate,
            "block_size": args.block_size,
            "steady_voice_count": STEADY_VOICE_COUNT,
            "enabled_resonators_per_voice": 8,
            "face_roll_note_ons_per_epoch": FACE_ROLL_NOTE_ONS,
            "epochs": args.epochs,
            "initial_warmup_blocks": args.initial_warmup_blocks,
            "settle_blocks": args.settle_blocks,
            "measure_blocks": args.measure_blocks,
        },
        "thresholds": {
            "max_median_ratio": args.max_median_ratio,
            "max_p95_ratio": args.max_p95_ratio,
            "max_trend_ratio_per_epoch": args.max_trend_ratio_per_epoch,
        },
        "baseline": asdict(baseline),
        "post": asdict(post),
        "epochs": [asdict(summary) for summary in epoch_summaries],
        "metrics": metrics,
        "failures": failures,
    }
    _write_report(args.output, report)

    label = "FAIL" if failures else "PASS"
    print(
        f"[{label}] steady face-roll: median={metrics['post_pre_median_ratio']:.4f} "
        f"p95={metrics['post_pre_p95_ratio']:.4f} "
        f"trend/epoch={metrics['normalized_trend_per_epoch']:.4f}"
    )
    for failure in failures:
        print(f"       - {failure}")
    return 1 if failures else 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--module", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--sample-rate", type=int, default=48_000)
    parser.add_argument("--block-size", type=int, default=128)
    parser.add_argument("--epochs", type=int, default=6)
    parser.add_argument("--initial-warmup-blocks", type=int, default=32)
    parser.add_argument("--settle-blocks", type=int, default=16)
    # At 128 callbacks the interpolated p95 is determined by only the seventh
    # slowest sample and is too sensitive to a short scheduler interruption.
    # 512 still keeps this gate quick while giving the tail statistic enough
    # observations for a credible regression comparison.
    parser.add_argument("--measure-blocks", type=int, default=512)
    parser.add_argument("--max-median-ratio", type=float, default=1.15)
    parser.add_argument("--max-p95-ratio", type=float, default=1.25)
    # A positive least-squares slope below 1% of the baseline median per epoch
    # is treated as scheduler/timer jitter, not a material upward trend.
    parser.add_argument("--max-trend-ratio-per-epoch", type=float, default=0.01)
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    try:
        if args.sample_rate <= 0 or args.block_size <= 0:
            raise ValueError("sample rate and block size must be positive")
        if args.epochs < 2 or args.measure_blocks < 2:
            raise ValueError("at least two epochs and two measured blocks are required")
        if args.initial_warmup_blocks < 0 or args.settle_blocks < 0:
            raise ValueError("warm-up and settle block counts cannot be negative")
        if args.max_median_ratio <= 0.0 or args.max_p95_ratio <= 0.0:
            raise ValueError("median and p95 ratio thresholds must be positive")
        if args.max_trend_ratio_per_epoch < 0.0:
            raise ValueError("trend tolerance cannot be negative")
        return run(args)
    except (HarnessError, ValueError) as exc:
        print(f"configuration error: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
