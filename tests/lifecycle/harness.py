#!/usr/bin/env python3
"""Run deterministic voice lifecycle and containment scenarios."""

from __future__ import annotations

import argparse
import json
import math
import sys
import time
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any, Mapping, Sequence

import numpy as np

if __package__:
    from .scenarios import RawMidiEvent, VoiceLimitEvent, VoiceScenario, SCENARIOS, make_random_stress, select_scenarios
    from tests.module_loader import HarnessError, load_resonarium
else:
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
    from tests.module_loader import HarnessError, load_resonarium
    from tests.lifecycle.scenarios import (
        RawMidiEvent,
        VoiceLimitEvent,
        VoiceScenario,
        SCENARIOS,
        make_random_stress,
        select_scenarios,
    )


TELEMETRY_FIELDS = (
    "configured_voice_limit",
    "musical_voices",
    "fast_fade_voices",
    "rendered_voices",
    "physical_pool_size",
    "note_starts",
    "steals",
    "forced_fade_retirements",
    "tail_retirements",
    "dropped_note_ons",
)
COUNTER_FIELDS = (
    "note_starts",
    "steals",
    "forced_fade_retirements",
    "tail_retirements",
    "dropped_note_ons",
)


@dataclass
class ScenarioResult:
    id: str
    status: str
    processing_seconds: float
    audio_samples: int
    expected_note_starts: int
    final_telemetry: dict[str, int]
    maxima: dict[str, int]
    failures: list[str]


def normalize_telemetry(value: Mapping[str, Any]) -> dict[str, int]:
    if not isinstance(value, Mapping):
        raise HarnessError("get_voice_telemetry() must return a mapping")
    missing = [field for field in TELEMETRY_FIELDS if field not in value]
    if missing:
        raise HarnessError(f"voice telemetry is missing: {', '.join(missing)}")
    result = {field: int(value[field]) for field in TELEMETRY_FIELDS}
    if any(number < 0 for number in result.values()):
        raise HarnessError("voice telemetry contains a negative value")
    return result


def telemetry_failures(
    telemetry: Mapping[str, int], expected_limit: int, previous: Mapping[str, int] | None = None
) -> list[str]:
    failures: list[str] = []
    if telemetry["configured_voice_limit"] != expected_limit:
        failures.append(
            f"configured limit {telemetry['configured_voice_limit']} != expected {expected_limit}"
        )
    if telemetry["physical_pool_size"] != 18:
        failures.append(f"physical pool {telemetry['physical_pool_size']} != 18")
    if telemetry["musical_voices"] > expected_limit:
        failures.append(f"musical voices {telemetry['musical_voices']} > limit {expected_limit}")
    if telemetry["fast_fade_voices"] > 2:
        failures.append(f"fast fades {telemetry['fast_fade_voices']} > 2")
    if telemetry["rendered_voices"] > expected_limit + 2:
        failures.append(f"rendered voices {telemetry['rendered_voices']} > {expected_limit + 2}")
    if telemetry["dropped_note_ons"] != 0:
        failures.append(f"dropped note-ons is {telemetry['dropped_note_ons']}, expected zero")
    if previous is not None:
        for field in COUNTER_FIELDS:
            if telemetry[field] < previous[field]:
                failures.append(f"counter {field} moved backwards")
    return failures


def _read_telemetry(synth: Any) -> dict[str, int]:
    if not hasattr(synth, "get_voice_telemetry"):
        raise HarnessError("extension lacks get_voice_telemetry(); rebuild Resonarium_Python")
    return normalize_telemetry(synth.get_voice_telemetry())


def run_scenario(module: Any, scenario: VoiceScenario) -> ScenarioResult:
    scenario.validate()
    synth = module.Resonarium(float(scenario.sample_rate), scenario.block_size)
    if not hasattr(synth, "queue_midi"):
        raise HarnessError("extension lacks queue_midi(); rebuild Resonarium_Python")

    synth.set_param("numVoices", float(scenario.voice_limit))
    initial = _read_telemetry(synth)
    initial_starts = initial["note_starts"]
    expected_limit = scenario.voice_limit
    previous: dict[str, int] | None = None
    failures: list[str] = []
    maxima = {field: 0 for field in ("musical_voices", "fast_fade_voices", "rendered_voices")}
    actions_by_block: dict[int, list[RawMidiEvent | VoiceLimitEvent]] = {}
    for action in scenario.actions:
        actions_by_block.setdefault(action.sample // scenario.block_size, []).append(action)

    audio = synth.create_multi_block(1)
    block_count = math.ceil(scenario.duration_samples / scenario.block_size)
    start = time.perf_counter()
    for block_index in range(block_count):
        for action in actions_by_block.get(block_index, ()):
            if isinstance(action, VoiceLimitEvent):
                synth.set_param("numVoices", float(action.limit))
                expected_limit = action.limit
            else:
                synth.queue_midi(action.data, action.sample % scenario.block_size)
        synth.process_multi_block(audio, 0, 1)
        samples = np.asarray(audio)
        if not np.isfinite(samples).all():
            failures.append(f"block {block_index}: non-finite shared output")
        peak = float(np.max(np.abs(samples), initial=0.0))
        if peak > 100.0:
            failures.append(f"block {block_index}: shared-output magnitude {peak:.6g} > 100")

        telemetry = _read_telemetry(synth)
        for field in maxima:
            maxima[field] = max(maxima[field], telemetry[field])
        failures.extend(
            f"block {block_index}: {failure}"
            for failure in telemetry_failures(telemetry, expected_limit, previous)
        )
        previous = telemetry
    processing_seconds = time.perf_counter() - start

    final = previous if previous is not None else _read_telemetry(synth)
    actual_starts = final["note_starts"] - initial_starts
    if actual_starts != scenario.expected_note_starts:
        failures.append(f"note starts {actual_starts} != expected {scenario.expected_note_starts}")
    return ScenarioResult(
        id=scenario.id,
        status="fail" if failures else "pass",
        processing_seconds=processing_seconds,
        audio_samples=scenario.duration_samples,
        expected_note_starts=scenario.expected_note_starts,
        final_telemetry=final,
        maxima=maxima,
        failures=failures,
    )


def _write_report(path: Path | None, report: dict[str, Any]) -> None:
    if path is None:
        return
    destination = path / "voice-lifecycle-report.json" if path.suffix == "" else path
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = destination.with_suffix(destination.suffix + ".tmp")
    temporary.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    temporary.replace(destination)


def _run(module_path: Path, scenarios: Sequence[VoiceScenario], output: Path | None) -> int:
    module, resolved_module = load_resonarium(module_path)
    results: list[ScenarioResult] = []
    for scenario in scenarios:
        result = run_scenario(module, scenario)
        results.append(result)
        print(f"[{result.status.upper():4}] {result.id}")
        for failure in result.failures:
            print(f"       - {failure}")
    report = {
        "schema_version": 1,
        "status": "fail" if any(result.failures for result in results) else "pass",
        "module": str(resolved_module),
        "results": [asdict(result) for result in results],
    }
    _write_report(output, report)
    return 1 if report["status"] == "fail" else 0


def list_command(_: argparse.Namespace) -> int:
    for scenario in SCENARIOS:
        print(f"{scenario.id:<44} [{','.join(scenario.tags)}] {scenario.description}")
    return 0


def check_command(args: argparse.Namespace) -> int:
    return _run(args.module, select_scenarios(args.suite, args.scenario), args.output)


def stress_command(args: argparse.Namespace) -> int:
    return _run(args.module, (make_random_stress(args.events, args.seed),), args.output)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    listing = subparsers.add_parser("list", help="list deterministic lifecycle scenarios")
    listing.set_defaults(handler=list_command)

    check = subparsers.add_parser("check", help="run bounded voice-lifecycle scenarios")
    check.add_argument("--module", type=Path, required=True)
    check.add_argument("--suite", choices=("smoke", "core", "all"), default="smoke")
    check.add_argument("--scenario", action="append", default=[])
    check.add_argument("--output", type=Path)
    check.set_defaults(handler=check_command)

    stress = subparsers.add_parser("stress", help="run the deterministic randomized event stream")
    stress.add_argument("--module", type=Path, required=True)
    stress.add_argument("--events", type=int, default=100_000)
    stress.add_argument("--seed", type=lambda value: int(value, 0), default=0x5EED)
    stress.add_argument("--output", type=Path)
    stress.set_defaults(handler=stress_command)
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    try:
        return int(args.handler(args))
    except (HarnessError, ValueError) as exc:
        print(f"configuration error: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
