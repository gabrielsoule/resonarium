#!/usr/bin/env python3
"""Audio integration gates for private voices and the retained global effects.

These checks deliberately exercise the public Python processor adapter rather
than a model of the voice manager. They require a built ``resonarium`` native
module.
"""

from __future__ import annotations

import argparse
import json
import math
import sys
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any, Mapping, Sequence

import numpy as np

if not __package__:
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tests.module_loader import HarnessError, load_resonarium


SAMPLE_RATE = 48_000
BLOCK_SIZE = 128
ADDITIVITY_TOLERANCE = 1.0e-5
NON_SILENT_PEAK = 1.0e-4
STALE_HISTORY_SILENCE_PEAK = 1.0e-7


@dataclass(frozen=True)
class GateResult:
    name: str
    status: str
    metrics: dict[str, float | int]
    failures: tuple[str, ...]


def _set_parameters(synth: Any, values: Mapping[str, float]) -> None:
    known = {parameter.id for parameter in synth.get_all_params()}
    missing = sorted(set(values) - known)
    if missing:
        raise HarnessError(f"native adapter is missing parameters: {', '.join(missing)}")
    for parameter_id, value in values.items():
        synth.set_param(parameter_id, float(value))


def _queue_note(synth: Any, *, on: bool, channel: int, note: int, offset: int = 0) -> None:
    if not 2 <= channel <= 16:
        raise ValueError("note events must use lower-zone MPE member channels 2-16")
    status = (0x90 if on else 0x80) | (channel - 1)
    velocity = 100 if on else 0
    synth.queue_midi(bytes((status, note, velocity)), offset)


def _process_one(synth: Any, block: np.ndarray) -> np.ndarray:
    synth.process_multi_block(block, 0, 1)
    audio = np.asarray(block, dtype=np.float32).copy(order="C")
    if audio.shape != (2, BLOCK_SIZE):
        raise HarnessError(f"native adapter returned audio with shape {audio.shape}")
    return audio


def _new_synth(module: Any, parameters: Mapping[str, float]) -> tuple[Any, np.ndarray]:
    synth = module.Resonarium(float(SAMPLE_RATE), BLOCK_SIZE)
    _set_parameters(synth, parameters)
    block = synth.create_multi_block(1)

    # Consume the prepare/reset command and let block-snapshotted parameters
    # reach the audio engine before the measured note events.
    silence = _process_one(synth, block)
    if float(np.max(np.abs(silence), initial=0.0)) != 0.0:
        raise HarnessError("warm-up block was not silent")
    return synth, block


def _additivity_parameters() -> dict[str, float]:
    return {
        "numVoices": 4.0,
        "polyEffectChain": 0.0,
        "uiBypassResonators": 1.0,
        "masterGain": -12.0,
        "impExciter0 enabled": 1.0,
        "impExciter0 thickness": 2.0,
        "impExciter0 level": 0.8,
    }


def _render_impulse_notes(module: Any, notes: Sequence[tuple[int, int]]) -> np.ndarray:
    synth, block = _new_synth(module, _additivity_parameters())
    chunks: list[np.ndarray] = []
    for channel, note in notes:
        _queue_note(synth, on=True, channel=channel, note=note)
    for _ in range(4):
        chunks.append(_process_one(synth, block))
    return np.concatenate(chunks, axis=1)


def run_additivity_gate(module: Any) -> GateResult:
    isolated_a = _render_impulse_notes(module, ((2, 60),))
    isolated_b = _render_impulse_notes(module, ((3, 67),))
    simultaneous = _render_impulse_notes(module, ((2, 60), (3, 67)))
    expected = isolated_a + isolated_b
    difference = np.asarray(simultaneous, dtype=np.float64) - np.asarray(expected, dtype=np.float64)

    max_error = float(np.max(np.abs(difference), initial=0.0))
    rms_error = float(np.sqrt(np.mean(difference * difference)))
    expected_peak = float(np.max(np.abs(expected), initial=0.0))
    failures: list[str] = []
    if not np.isfinite(simultaneous).all():
        failures.append("simultaneous render contains non-finite samples")
    if expected_peak < NON_SILENT_PEAK:
        failures.append(f"test signal peak {expected_peak:.9g} is too small")
    if max_error > ADDITIVITY_TOLERANCE:
        failures.append(
            f"max |simultaneous - (isolated A + isolated B)| {max_error:.9g} "
            f"> {ADDITIVITY_TOLERANCE:.9g}"
        )

    return GateResult(
        name="gain-before-mix-additivity",
        status="fail" if failures else "pass",
        metrics={
            "sample_count": int(simultaneous.shape[1]),
            "expected_peak": expected_peak,
            "max_absolute_error": max_error,
            "rms_error": rms_error,
            "tolerance": ADDITIVITY_TOLERANCE,
        },
        failures=tuple(failures),
    )


def _tail_parameters() -> dict[str, float]:
    return {
        "numVoices": 2.0,
        "polyEffectChain": 0.0,
        "uiBypassResonators": 1.0,
        "masterGain": 0.0,
        "noiseExciter0enabled": 1.0,
        "noiseExciter0level": 1.0,
        "noiseExciterEnv0attack": 0.0,
        "noiseExciterEnv0decay": 0.0,
        "noiseExciterEnv0sustain": 100.0,
        "noiseExciterEnv0release": 0.0,
    }


def run_private_tail_gate(module: Any) -> GateResult:
    synth, block = _new_synth(module, _tail_parameters())

    # The held/loud voice gets physical slot zero and renders first. The quiet
    # released voice gets slot one. A tail detector that inspects the shared bus
    # would therefore see slot zero's noise while evaluating slot one.
    _queue_note(synth, on=True, channel=2, note=67, offset=0)
    _queue_note(synth, on=True, channel=3, note=60, offset=1)
    started_audio = _process_one(synth, block)
    _queue_note(synth, on=False, channel=3, note=60, offset=0)

    before = synth.get_voice_telemetry()
    initial_tail_retirements = int(before["tail_retirements"])
    retirement_peak = 0.0
    retirement_rms = 0.0
    retirement_block = -1
    retirement_telemetry: Mapping[str, int] | None = None

    # The private-signal quiet qualification is 500 ms. Give it 750 ms plus a
    # block for envelope/filter settling while requiring retirement only once.
    maximum_blocks = math.ceil(0.75 * SAMPLE_RATE / BLOCK_SIZE) + 1
    for block_index in range(maximum_blocks):
        audio = _process_one(synth, block)
        telemetry = synth.get_voice_telemetry()
        if int(telemetry["tail_retirements"]) > initial_tail_retirements:
            retirement_block = block_index
            retirement_peak = float(np.max(np.abs(audio), initial=0.0))
            samples = np.asarray(audio, dtype=np.float64)
            retirement_rms = float(np.sqrt(np.mean(samples * samples)))
            retirement_telemetry = telemetry
            break

    failures: list[str] = []
    started_peak = float(np.max(np.abs(started_audio), initial=0.0))
    if retirement_telemetry is None:
        failures.append("released quiet voice did not tail-retire within 750 ms")
        retirement_telemetry = synth.get_voice_telemetry()
    else:
        if int(retirement_telemetry["tail_retirements"]) != initial_tail_retirements + 1:
            failures.append("tail retirement counter did not advance exactly once")
        if int(retirement_telemetry["musical_voices"]) != 1:
            failures.append(
                "held voice was not the sole remaining musical voice at quiet-voice retirement"
            )
        if retirement_peak < NON_SILENT_PEAK:
            failures.append(
                f"shared output peak {retirement_peak:.9g} was not loud when quiet voice retired"
            )

        # Exactly-once retirement must remain true on the following host block.
        _process_one(synth, block)
        after = synth.get_voice_telemetry()
        if int(after["tail_retirements"]) != initial_tail_retirements + 1:
            failures.append("tail retirement counter advanced more than once")

    if started_peak < NON_SILENT_PEAK:
        failures.append(f"held-noise setup peak {started_peak:.9g} is too small")

    elapsed_samples = (retirement_block + 1) * BLOCK_SIZE if retirement_block >= 0 else -1
    return GateResult(
        name="private-signal-tail-retirement",
        status="fail" if failures else "pass",
        metrics={
            "retirement_block_after_release": retirement_block,
            "retirement_samples_after_release": elapsed_samples,
            "retirement_seconds_after_release": (
                elapsed_samples / SAMPLE_RATE if elapsed_samples >= 0 else -1.0
            ),
            "shared_peak_at_retirement": retirement_peak,
            "shared_rms_at_retirement": retirement_rms,
            "musical_voices_at_retirement": int(retirement_telemetry["musical_voices"]),
            "tail_retirements_at_retirement": int(retirement_telemetry["tail_retirements"]),
        },
        failures=tuple(failures),
    )


def _global_delay_parameters() -> dict[str, float]:
    parameters = _additivity_parameters()
    parameters.update(
        {
            "masterGain": 0.0,
            "delayEnable": 1.0,
            "delayTimeL": 0.05,
            "delayTimeR": 0.05,
            "delayLock": 1.0,
            "delayFeedback": 0.0,
            "delayMix": 1.0,
        }
    )
    return parameters


def _render_delay_echo(module: Any, disable_and_reenable: bool) -> float:
    synth, block = _new_synth(module, _global_delay_parameters())

    # Parameter modulation smoothers intentionally interpolate user changes.
    # Settle them before writing the one impulse whose delayed copy is measured.
    for _ in range(32):
        _process_one(synth, block)

    _queue_note(synth, on=True, channel=2, note=60)
    _process_one(synth, block)  # Fill the owning delay without reaching its 50 ms tap.

    if disable_and_reenable:
        synth.set_param("delayEnable", 0.0)
        for _ in range(5):
            _process_one(synth, block)
        synth.set_param("delayEnable", 1.0)

    peak = 0.0
    for _ in range(24):
        audio = _process_one(synth, block)
        peak = max(peak, float(np.max(np.abs(audio), initial=0.0)))
    return peak


def run_global_effect_reenable_gate(module: Any) -> GateResult:
    control_peak = _render_delay_echo(module, False)
    resumed_peak = _render_delay_echo(module, True)

    failures: list[str] = []
    if control_peak < NON_SILENT_PEAK:
        failures.append("global-delay control did not produce a measurable echo")
    if resumed_peak > STALE_HISTORY_SILENCE_PEAK:
        failures.append(
            f"global delay resumed stale history after disable/re-enable: {resumed_peak:.9g}"
        )

    return GateResult(
        name="global-effect-disable-reenable-reset",
        status="fail" if failures else "pass",
        metrics={
            "control_echo_peak": control_peak,
            "peak_after_disable_reenable": resumed_peak,
            "silence_tolerance": STALE_HISTORY_SILENCE_PEAK,
        },
        failures=tuple(failures),
    )


def run_cc120_clears_global_tail_gate(module: Any) -> GateResult:
    parameters = _tail_parameters()
    parameters.update(
        {
            "numVoices": 1.0,
            "delayEnable": 1.0,
            "delayTimeL": 0.01,
            "delayTimeR": 0.01,
            "delayLock": 1.0,
            "delayFeedback": 0.95,
            "delayMix": 1.0,
        }
    )
    synth, block = _new_synth(module, parameters)
    for _ in range(32):
        _process_one(synth, block)

    # Fill the delay with a dense private source, then release it so the
    # signal at the CC120 boundary is global delay history rather than a new
    # voice input. The voice remains in its 500 ms private-tail window.
    _queue_note(synth, on=True, channel=2, note=60)
    fill_peak = 0.0
    for _ in range(16):
        audio = _process_one(synth, block)
        fill_peak = max(fill_peak, float(np.max(np.abs(audio), initial=0.0)))
    _queue_note(synth, on=False, channel=2, note=60)
    tail_peak_before_cc = 0.0
    for _ in range(4):
        audio = _process_one(synth, block)
        tail_peak_before_cc = max(
            tail_peak_before_cc,
            float(np.max(np.abs(audio), initial=0.0)),
        )

    if not hasattr(synth, "clear_global_effect_test_call_counters"):
        raise HarnessError("native adapter lacks test-only global-effect counter reset")
    synth.clear_global_effect_test_call_counters()

    event_offset = 37
    synth.queue_midi(bytes((0xB0, 120, 0)), event_offset)
    event_audio = _process_one(synth, block)
    pre_event_peak = float(
        np.max(np.abs(event_audio[:, :event_offset]), initial=0.0)
    )
    post_event_peak = float(
        np.max(np.abs(event_audio[:, event_offset:]), initial=0.0)
    )
    event_telemetry = synth.get_voice_telemetry()
    counters = _read_global_effect_test_counters(synth)

    later_peak = 0.0
    settled_telemetry = event_telemetry
    for _ in range(8):
        audio = _process_one(synth, block)
        later_peak = max(later_peak, float(np.max(np.abs(audio), initial=0.0)))
        settled_telemetry = synth.get_voice_telemetry()

    failures: list[str] = []
    if fill_peak < NON_SILENT_PEAK:
        failures.append(f"high-feedback delay fill peak {fill_peak:.9g} is too small")
    if tail_peak_before_cc < NON_SILENT_PEAK:
        failures.append(f"delay tail before CC120 peak {tail_peak_before_cc:.9g} is too small")
    if pre_event_peak < NON_SILENT_PEAK:
        failures.append(f"CC120 block was not audible before sample {event_offset}")
    if post_event_peak > STALE_HISTORY_SILENCE_PEAK:
        failures.append(
            f"CC120 left {post_event_peak:.9g} peak at/after event sample {event_offset}"
        )
    if later_peak > STALE_HISTORY_SILENCE_PEAK:
        failures.append(f"CC120 global tail resumed in later blocks: {later_peak:.9g}")
    # rendered_voices counts any voice rendered during the whole host block, so
    # the event block legitimately reports the voice rendered before sample 37.
    # Live musical/fade ownership must be gone at that boundary, and the next
    # complete blocks must render no voices at all.
    for name in ("musical_voices", "fast_fade_voices"):
        if int(event_telemetry[name]) != 0:
            failures.append(f"CC120 left {name}={int(event_telemetry[name])}")
    for name in ("musical_voices", "fast_fade_voices", "rendered_voices"):
        if int(settled_telemetry[name]) != 0:
            failures.append(f"post-CC120 block left {name}={int(settled_telemetry[name])}")
    if counters["resets"] != 1:
        failures.append(f"CC120 reset the global chain {counters['resets']} times, expected 1")

    return GateResult(
        name="cc120-clears-global-effect-tail-at-event-boundary",
        status="fail" if failures else "pass",
        metrics={
            "event_sample_offset": event_offset,
            "delay_fill_peak": fill_peak,
            "tail_peak_before_cc_block": tail_peak_before_cc,
            "pre_event_peak": pre_event_peak,
            "post_event_peak": post_event_peak,
            "later_block_peak": later_peak,
            "global_effect_resets_in_event_block": counters["resets"],
            "musical_voices_after_cc120": int(event_telemetry["musical_voices"]),
            "fast_fades_after_cc120": int(event_telemetry["fast_fade_voices"]),
            "voices_rendered_before_event_in_cc120_block": int(
                event_telemetry["rendered_voices"]
            ),
            "rendered_voices_in_later_blocks": int(settled_telemetry["rendered_voices"]),
        },
        failures=tuple(failures),
    )


def _read_global_effect_test_counters(synth: Any) -> dict[str, int]:
    if not hasattr(synth, "get_global_effect_test_call_counters"):
        raise HarnessError("native adapter lacks test-only global-effect call counters")
    counters = synth.get_global_effect_test_call_counters()
    required = ("resets", "parameter_updates", "process_calls")
    missing = [name for name in required if name not in counters]
    if missing:
        raise HarnessError(f"global-effect counters are missing: {', '.join(missing)}")
    return {name: int(counters[name]) for name in required}


def run_effect_chain_topology_gate(module: Any) -> GateResult:
    synth, _ = _new_synth(module, _additivity_parameters())
    if not hasattr(synth, "get_effect_chain_topology"):
        raise HarnessError("native adapter lacks test-only effect-chain topology")
    topology = synth.get_effect_chain_topology()
    expected = {
        "global_instances": 1,
        "voice_instances": 0,
        "total_instances": 1,
    }
    missing = sorted(set(expected) - set(topology))
    if missing:
        raise HarnessError(f"effect-chain topology is missing: {', '.join(missing)}")

    actual = {name: int(topology[name]) for name in expected}
    failures = [
        f"{name}={actual[name]}, expected {value}"
        for name, value in expected.items()
        if actual[name] != value
    ]
    return GateResult(
        name="one-global-zero-voice-effect-chains",
        status="fail" if failures else "pass",
        metrics=actual,
        failures=tuple(failures),
    )


def _render_poly_tombstone(module: Any, value: float) -> np.ndarray:
    parameters = _global_delay_parameters()
    parameters.update(
        {
            "polyEffectChain": value,
            "delayFeedback": 0.35,
            "delayMix": 0.6,
            "distEnable": 1.0,
            "distType": 2.0,
            "distDrive": 9.0,
            "distMix": 0.75,
        }
    )
    synth, block = _new_synth(module, parameters)
    for _ in range(32):
        _process_one(synth, block)

    chunks: list[np.ndarray] = []
    _queue_note(synth, on=True, channel=2, note=60)
    _queue_note(synth, on=True, channel=3, note=67)
    for block_index in range(32):
        if block_index == 2:
            _queue_note(synth, on=False, channel=2, note=60)
            _queue_note(synth, on=False, channel=3, note=67)
        chunks.append(_process_one(synth, block))
    return np.concatenate(chunks, axis=1)


def run_poly_effect_tombstone_gate(module: Any) -> GateResult:
    zero = _render_poly_tombstone(module, 0.0)
    one = _render_poly_tombstone(module, 1.0)
    difference = np.asarray(one, dtype=np.float64) - np.asarray(zero, dtype=np.float64)
    max_error = float(np.max(np.abs(difference), initial=0.0))
    peak = float(np.max(np.abs(zero), initial=0.0))
    failures: list[str] = []
    if peak < NON_SILENT_PEAK:
        failures.append(f"tombstone control signal peak {peak:.9g} is too small")
    if not np.array_equal(zero, one):
        failures.append(f"polyEffectChain 0 vs 1 differs by {max_error:.9g}")

    return GateResult(
        name="legacy-poly-effect-parameter-is-audio-tombstone",
        status="fail" if failures else "pass",
        metrics={
            "sample_count": int(zero.shape[1]),
            "control_peak": peak,
            "max_absolute_error": max_error,
        },
        failures=tuple(failures),
    )


def _global_effect_counts_for_voice_count(module: Any, voice_count: int) -> dict[str, int]:
    parameters = _global_delay_parameters()
    parameters["numVoices"] = float(voice_count)
    synth, block = _new_synth(module, parameters)
    for _ in range(16):
        _process_one(synth, block)
    if not hasattr(synth, "clear_global_effect_test_call_counters"):
        raise HarnessError("native adapter lacks test-only global-effect counter reset")
    synth.clear_global_effect_test_call_counters()
    for index in range(voice_count):
        _queue_note(
            synth,
            on=True,
            channel=2 + index % 15,
            note=48 + index,
            offset=0,
        )
    for _ in range(4):
        _process_one(synth, block)
    return _read_global_effect_test_counters(synth)


def run_global_effect_voice_count_gate(module: Any) -> GateResult:
    one = _global_effect_counts_for_voice_count(module, 1)
    sixteen = _global_effect_counts_for_voice_count(module, 16)
    failures: list[str] = []
    for name in ("parameter_updates", "process_calls"):
        if one[name] <= 0:
            failures.append(f"one-voice control observed no global {name}")
        if sixteen[name] != one[name]:
            failures.append(
                f"global {name} scales with voice count: one={one[name]}, sixteen={sixteen[name]}"
            )

    return GateResult(
        name="global-effect-work-independent-of-voice-count",
        status="fail" if failures else "pass",
        metrics={
            "one_voice_parameter_updates": one["parameter_updates"],
            "sixteen_voice_parameter_updates": sixteen["parameter_updates"],
            "one_voice_process_calls": one["process_calls"],
            "sixteen_voice_process_calls": sixteen["process_calls"],
        },
        failures=tuple(failures),
    )


def run_global_tail_survives_voice_gate(module: Any) -> GateResult:
    parameters = _global_delay_parameters()
    parameters.update({"numVoices": 1.0, "delayFeedback": 0.93, "delayMix": 1.0})
    synth, block = _new_synth(module, parameters)
    for _ in range(32):
        _process_one(synth, block)

    _queue_note(synth, on=True, channel=2, note=60)
    _process_one(synth, block)
    _queue_note(synth, on=False, channel=2, note=60)
    before = synth.get_voice_telemetry()
    initial_retirements = int(before["tail_retirements"])

    retirement_block = -1
    retirement_telemetry: Mapping[str, int] | None = None
    maximum_blocks = math.ceil(0.75 * SAMPLE_RATE / BLOCK_SIZE) + 1
    for block_index in range(maximum_blocks):
        _process_one(synth, block)
        telemetry = synth.get_voice_telemetry()
        if int(telemetry["tail_retirements"]) > initial_retirements:
            retirement_block = block_index
            retirement_telemetry = telemetry
            break

    post_retirement_peak = 0.0
    if retirement_telemetry is not None:
        for _ in range(math.ceil(0.15 * SAMPLE_RATE / BLOCK_SIZE)):
            audio = _process_one(synth, block)
            post_retirement_peak = max(
                post_retirement_peak,
                float(np.max(np.abs(audio), initial=0.0)),
            )

    failures: list[str] = []
    if retirement_telemetry is None:
        failures.append("released voice did not retire within 750 ms with global delay active")
        retirement_telemetry = synth.get_voice_telemetry()
    else:
        if int(retirement_telemetry["tail_retirements"]) != initial_retirements + 1:
            failures.append("voice tail retirement counter did not advance exactly once")
        if int(retirement_telemetry["musical_voices"]) != 0:
            failures.append("voice remained musical after its private tail retired")
        if post_retirement_peak < NON_SILENT_PEAK:
            failures.append(
                f"global delay tail peak after voice retirement {post_retirement_peak:.9g} is too small"
            )

    return GateResult(
        name="global-effect-tail-survives-voice-retirement",
        status="fail" if failures else "pass",
        metrics={
            "retirement_block_after_release": retirement_block,
            "tail_retirements": int(retirement_telemetry["tail_retirements"]),
            "musical_voices_at_retirement": int(retirement_telemetry["musical_voices"]),
            "post_retirement_tail_peak": post_retirement_peak,
        },
        failures=tuple(failures),
    )


def _write_report(path: Path | None, report: Mapping[str, Any]) -> None:
    if path is None:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    temporary.replace(path)


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--module", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args(argv)

    try:
        module, resolved_module = load_resonarium(args.module)
        results = (
            run_additivity_gate(module),
            run_private_tail_gate(module),
            run_effect_chain_topology_gate(module),
            run_poly_effect_tombstone_gate(module),
            run_global_tail_survives_voice_gate(module),
            run_global_effect_reenable_gate(module),
            run_cc120_clears_global_tail_gate(module),
            run_global_effect_voice_count_gate(module),
        )
    except (HarnessError, RuntimeError, ValueError) as exc:
        print(f"configuration error: {exc}", file=sys.stderr)
        return 2

    for result in results:
        print(f"[{result.status.upper():4}] {result.name}")
        for name, value in result.metrics.items():
            print(f"       {name}: {value}")
        for failure in result.failures:
            print(f"       - {failure}")

    report = {
        "schema_version": 1,
        "status": "fail" if any(result.failures for result in results) else "pass",
        "module": str(resolved_module),
        "results": [asdict(result) for result in results],
    }
    _write_report(args.output, report)
    return 1 if report["status"] == "fail" else 0


if __name__ == "__main__":
    raise SystemExit(main())
