#!/usr/bin/env python3
"""Deterministic audio contract for the production exciter rack.

The checks use mathematical and behavioral oracles for seeded voice streams,
block-partition independence, sample snapshots, and shared external input.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import sys
import tempfile
from pathlib import Path
from types import ModuleType
from typing import Any

import numpy as np

if not __package__:
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tests.module_loader import load_resonarium, resolve_module_path


TOTAL_SAMPLES = 8192
BLOCK_SIZES = (32, 128, 512)


class ContractError(RuntimeError):
    pass


def load_module(path: Path) -> ModuleType:
    return load_resonarium(path)[0]


def set_param(synth: Any, uid: str, value: float) -> None:
    synth.set_param(uid, float(value))


def configure_noise(synth: Any) -> None:
    set_param(synth, "uiBypassResonators", 1.0)
    set_param(synth, "noiseExciter0enabled", 1.0)
    set_param(synth, "noiseExciter0level", 1.0)
    set_param(synth, "noiseExciter0filterType", 0.0)
    set_param(synth, "noiseExciterEnv0attack", 0.0)
    set_param(synth, "noiseExciterEnv0decay", 0.0)
    set_param(synth, "noiseExciterEnv0sustain", 100.0)
    set_param(synth, "noiseExciterEnv0release", 0.0)


def render_noise(module: ModuleType, block_size: int, voices: int) -> np.ndarray:
    if TOTAL_SAMPLES % block_size:
        raise ContractError("test length must be divisible by block size")
    synth = module.Resonarium(48_000.0, block_size)
    configure_noise(synth)
    for voice in range(voices):
        synth.play_note(1 + voice, 48 + voice, 100, 0)
    audio = synth.create_multi_block(TOTAL_SAMPLES // block_size)
    synth.process_multi_block(audio)
    result = np.asarray(audio, dtype=np.float32).copy()
    if result.shape != (2, TOTAL_SAMPLES):
        raise ContractError(f"unexpected render shape {result.shape}")
    if not np.isfinite(result).all():
        raise ContractError("noise render contains a non-finite sample")
    return result


def process_blocks(synth: Any, blocks: int = 1) -> np.ndarray:
    audio = synth.create_multi_block(blocks)
    synth.process_multi_block(audio)
    return np.asarray(audio, dtype=np.float32).copy()


def configure_sample(synth: Any, mix: float = 1.0) -> None:
    set_param(synth, "uiBypassResonators", 1.0)
    set_param(synth, "sampleEnable", 1.0)
    set_param(synth, "sampleGain", 1.0)
    set_param(synth, "sampleMix", mix)
    set_param(synth, "sampleLoop", 1.0)
    set_param(synth, "sampleStart", 0.0)
    set_param(synth, "sampleEnd", 100.0)
    set_param(synth, "SamplefilterType", 0.0)
    set_param(synth, "Sampleattack", 0.0)
    set_param(synth, "Sampledecay", 0.0)
    set_param(synth, "Samplesustain", 100.0)
    set_param(synth, "Samplerelease", 0.0)


def install_and_commit(synth: Any, sample: np.ndarray) -> int:
    generation = int(synth.install_sample_for_testing(np.asarray(sample, dtype=np.float32), 48_000.0))
    process_blocks(synth)
    telemetry = dict(synth.get_sample_telemetry())
    if int(telemetry["active_generation"]) != generation:
        raise ContractError(
            f"snapshot generation {generation} was not committed at the next host boundary: {telemetry}"
        )
    return generation


def voice_buses(synth: Any, voice_index: int) -> tuple[np.ndarray, np.ndarray]:
    excitation, direct = synth.get_voice_exciter_buses(voice_index)
    return (np.asarray(excitation, dtype=np.float32).copy(),
            np.asarray(direct, dtype=np.float32).copy())


def sample_snapshot_checks(module: ModuleType) -> tuple[dict[str, Any], list[str]]:
    required = (
        "install_sample_for_testing",
        "clear_sample_for_testing",
        "get_sample_telemetry",
        "get_voice_exciter_buses",
    )
    probe = module.Resonarium(48_000.0, 128)
    missing = [name for name in required if not hasattr(probe, name)]
    if missing:
        raise ContractError(f"extension lacks sample test APIs: {', '.join(missing)}")

    failures: list[str] = []

    mono = module.Resonarium(48_000.0, 128)
    configure_sample(mono, 1.0)
    install_and_commit(mono, np.full((1, 5), 0.25, dtype=np.float32))
    mono.play_note(1, 60, 100, 0)
    process_blocks(mono)
    mono_excitation, _ = voice_buses(mono, 0)
    mono_error = max_error(mono_excitation[0], mono_excitation[1])
    if mono_error != 0.0:
        failures.append(f"mono snapshot did not become exact dual-mono (max error {mono_error:.9g})")

    stereo = module.Resonarium(48_000.0, 128)
    configure_sample(stereo, 0.25)
    stereo_source = np.vstack(
        (np.arange(1, 8, dtype=np.float32) * 0.05,
         np.arange(1, 8, dtype=np.float32) * -0.15)
    )
    install_and_commit(stereo, stereo_source)
    stereo.play_note(1, 60, 100, 0)
    process_blocks(stereo)
    excitation, direct = voice_buses(stereo, 0)
    audible = np.abs(excitation[0]) > 1.0e-8
    if not np.any(audible):
        failures.append("stereo sample excitation is silent")
        stereo_ratio_error = math.inf
        direct_mix_error = math.inf
    else:
        stereo_ratio_error = float(np.max(np.abs(excitation[1, audible] + 3.0 * excitation[0, audible])))
        direct_mix_error = float(np.max(np.abs(direct[:, audible] - 3.0 * excitation[:, audible])))
        if stereo_ratio_error > 1.0e-6:
            failures.append(f"sample right channel routing error {stereo_ratio_error:.9g}")
        if direct_mix_error > 1.0e-6:
            failures.append(f"sample excitation/direct mix complement error {direct_mix_error:.9g}")
    loop_period_error = max_error(excitation[0, :-7], excitation[0, 7:])
    if loop_period_error > 1.0e-7:
        failures.append(f"non-power-of-two sample loop is not periodic (max error {loop_period_error:.9g})")

    latched = module.Resonarium(48_000.0, 128)
    configure_sample(latched, 1.0)
    old_generation = install_and_commit(
        latched,
        np.vstack((np.full(5, 0.125, dtype=np.float32), np.full(5, 0.25, dtype=np.float32))),
    )
    latched.play_note(1, 60, 100, 0)
    process_blocks(latched)
    new_generation = install_and_commit(
        latched,
        np.vstack((np.full(5, 0.5, dtype=np.float32), np.full(5, -0.25, dtype=np.float32))),
    )
    old_voice_after_replace, _ = voice_buses(latched, 0)
    latched.play_note(2, 64, 100, 0)
    process_blocks(latched)
    old_voice, _ = voice_buses(latched, 0)
    new_voice, _ = voice_buses(latched, 1)
    old_mean = float(np.mean(old_voice[0]))
    new_mean = float(np.mean(new_voice[0]))
    latch_ratio = math.inf if abs(old_mean) < 1.0e-9 else new_mean / old_mean
    if abs(latch_ratio - 4.0) > 1.0e-4:
        failures.append(
            f"active assignment did not retain old snapshot across {old_generation}->{new_generation} "
            f"(new/old={latch_ratio:.9g})"
        )
    if float(np.max(np.abs(old_voice_after_replace))) <= 1.0e-8:
        failures.append("old snapshot stopped when replacement committed")

    clear_generation = int(latched.clear_sample_for_testing())
    process_blocks(latched)
    latched.play_note(3, 67, 100, 0)
    process_blocks(latched)
    cleared_voice, _ = voice_buses(latched, 2)
    clear_silence = float(np.max(np.abs(cleared_voice)))
    if clear_silence != 0.0:
        failures.append(f"new assignment after clear is not silent (peak {clear_silence:.9g})")

    latched.queue_midi(bytes((0xB0, 120, 0)), 0)
    process_blocks(latched)
    latched.clear_sample_for_testing()  # wakes worker and drains retired voice snapshots
    process_blocks(latched)
    latched.clear_sample_for_testing()  # drains the snapshot retired by the preceding commit
    telemetry = dict(latched.get_sample_telemetry())
    if int(telemetry["reclaim_overflows"]) != 0:
        failures.append(f"snapshot reclaim queue overflowed: {telemetry}")
    if int(telemetry["destroyed_off_audio_thread"]) < 2:
        failures.append(f"retired snapshots were not reclaimed off the audio thread: {telemetry}")

    return ({
        "mono_dual_mono_max_error": mono_error,
        "stereo_channel_oracle_max_error": stereo_ratio_error,
        "direct_mix_oracle_max_error": direct_mix_error,
        "non_power_of_two_loop_max_error": loop_period_error,
        "latched_new_to_old_ratio": latch_ratio,
        "clear_generation": clear_generation,
        "clear_new_voice_peak": clear_silence,
        "telemetry": telemetry,
    }, failures)


def external_input_checks(module: ModuleType) -> tuple[list[dict[str, Any]], list[str]]:
    failures: list[str] = []
    results: list[dict[str, Any]] = []
    note_offset = 37
    for voices in (1, 8, 16):
        synth = module.Resonarium(48_000.0, 128)
        set_param(synth, "uiBypassResonators", 1.0)
        set_param(synth, "extInEnable", 1.0)
        set_param(synth, "extInMix", 1.0)
        set_param(synth, "extInGain", 0.0)
        set_param(synth, "extInattack", 0.0)
        set_param(synth, "extIndecay", 0.0)
        set_param(synth, "extInsustain", 100.0)
        synth.clear_external_input_telemetry()
        synth.clear_exciter_telemetry()
        for voice in range(voices):
            synth.play_note(1 + voice % 15, 40 + voice, 100, note_offset)
        host_input = np.vstack(
            (np.linspace(0.1, 0.3, 128, dtype=np.float32),
             np.linspace(-0.05, -0.15, 128, dtype=np.float32))
        )
        output = np.asarray(synth.process_input_block(host_input), dtype=np.float32)
        capture = dict(synth.get_external_input_telemetry())
        rack = dict(synth.get_exciter_telemetry())
        pre_peak = float(np.max(np.abs(output[:, :note_offset]), initial=0.0))
        post_peak = float(np.max(np.abs(output[:, note_offset:]), initial=0.0))
        stereo_error = float(np.max(np.abs(output[1, note_offset:] + 0.5 * output[0, note_offset:])))
        reads = int(rack["external_voice_reads"])
        generated = int(rack["source_samples_generated"][4])
        if capture != {"host_blocks": 1, "valid_blocks": 1, "source_samples_copied": 256}:
            failures.append(f"{voices}-voice external capture was not exactly once: {capture}")
        # cos(pi / 2) is only approximately zero in the processor's float dry
        # law, so tolerate its sub-ULP residue before the note event.
        if pre_peak > 1.0e-7 or post_peak <= 1.0e-8:
            failures.append(f"{voices}-voice external event offset failed (pre={pre_peak}, post={post_peak})")
        if stereo_error > 1.0e-6:
            failures.append(f"{voices}-voice external stereo routing error {stereo_error:.9g}")
        if reads != voices or generated != voices * (128 - note_offset):
            failures.append(
                f"{voices}-voice external per-voice work mismatch reads={reads}, generated={generated}"
            )
        results.append({
            "voices": voices,
            "capture": capture,
            "external_voice_reads": reads,
            "generated_samples": generated,
            "pre_event_peak": pre_peak,
            "post_event_peak": post_peak,
            "stereo_oracle_max_error": stereo_error,
        })
    return results, failures


def max_error(left: np.ndarray, right: np.ndarray) -> float:
    return float(np.max(np.abs(left.astype(np.float64) - right.astype(np.float64))))


def normalized_correlation(left: np.ndarray, right: np.ndarray) -> float:
    left = left.astype(np.float64) - float(np.mean(left))
    right = right.astype(np.float64) - float(np.mean(right))
    denominator = math.sqrt(float(np.dot(left, left)) * float(np.dot(right, right)))
    return 0.0 if denominator == 0.0 else float(np.dot(left, right) / denominator)


def _splitmix64(state: int) -> tuple[int, int]:
    mask = (1 << 64) - 1
    state = (state + 0x9E3779B97F4A7C15) & mask
    value = state
    value = ((value ^ (value >> 30)) * 0xBF58476D1CE4E5B9) & mask
    value = ((value ^ (value >> 27)) * 0x94D049BB133111EB) & mask
    return state, (value ^ (value >> 31)) & mask


def _rotl32(value: int, count: int) -> int:
    return ((value << count) | (value >> (32 - count))) & 0xFFFFFFFF


def prng_oracle(voice: int, generation: int, kind: int,
                substream: int, count: int) -> np.ndarray:
    """Return the specified xoshiro128++ stream with the production float map."""
    mask64 = (1 << 64) - 1
    state = 0xD1B54A32D192ED03
    state ^= (voice & 0xFFFFFFFF) * 0x9E3779B97F4A7C15
    state ^= generation * 0xD2B74407B1CE6E93
    state ^= (kind + 1) * 0xCA5A826395121157
    state ^= (substream + 1) * 0xA24BAED4963EE407
    state &= mask64

    words: list[int] = []
    for _ in range(4):
        state, value = _splitmix64(state)
        words.append(value & 0xFFFFFFFF)
    if not any(words):
        words[0] = 1

    output = np.empty(count, dtype=np.float32)
    for index in range(count):
        result = (_rotl32((words[0] + words[3]) & 0xFFFFFFFF, 7) + words[0]) & 0xFFFFFFFF
        temporary = (words[1] << 9) & 0xFFFFFFFF
        words[2] ^= words[0]
        words[3] ^= words[1]
        words[1] ^= words[2]
        words[0] ^= words[3]
        words[2] ^= temporary
        words[3] = _rotl32(words[3], 11)
        words[:] = [word & 0xFFFFFFFF for word in words]
        output[index] = np.float32(result >> 8) * np.float32(2.0 ** -24)
    return output


def run(module: ModuleType) -> dict[str, Any]:
    probe = module.Resonarium(48_000.0, 128)
    fresh_a = render_noise(module, 128, 1)
    fresh_b = render_noise(module, 128, 1)
    deterministic_error = max_error(fresh_a, fresh_b)

    partition_renders = {size: render_noise(module, size, 1) for size in BLOCK_SIZES}
    partition_errors = {
        f"128_vs_{size}": max_error(partition_renders[128], audio)
        for size, audio in partition_renders.items()
        if size != 128
    }

    two_voices = render_noise(module, 128, 2)
    first_stream = fresh_a[0]
    second_stream = two_voices[0] - first_stream
    correlation = normalized_correlation(first_stream[256:], second_stream[256:])
    first_rms = float(np.sqrt(np.mean(np.square(first_stream.astype(np.float64)))))
    second_rms = float(np.sqrt(np.mean(np.square(second_stream.astype(np.float64)))))

    failures: list[str] = []
    if deterministic_error != 0.0:
        failures.append(f"fresh-process determinism max error {deterministic_error:.9g} != 0")
    for label, error in partition_errors.items():
        if error != 0.0:
            failures.append(f"partition {label} max error {error:.9g} != 0")
    if first_rms <= 1.0e-8 or second_rms <= 1.0e-8:
        failures.append(f"voice stream is silent (rms={first_rms:.9g}, {second_rms:.9g})")
    if abs(correlation) >= 0.1:
        failures.append(f"cross-voice correlation {correlation:.6g} exceeds 0.1")

    isolated_a = np.asarray(probe.render_exciter_prng_for_testing(0, 7, 1, 0, 8192), dtype=np.float32)
    isolated_repeat = np.asarray(probe.render_exciter_prng_for_testing(0, 7, 1, 0, 8192), dtype=np.float32)
    isolated_voice = np.asarray(probe.render_exciter_prng_for_testing(1, 7, 1, 0, 8192), dtype=np.float32)
    isolated_generation = np.asarray(probe.render_exciter_prng_for_testing(0, 8, 1, 0, 8192), dtype=np.float32)
    isolated_substream = np.asarray(probe.render_exciter_prng_for_testing(0, 7, 1, 1, 8192), dtype=np.float32)
    isolated_oracle = prng_oracle(0, 7, 1, 0, 8192)
    isolated_metrics = {
        "oracle_max_error": max_error(isolated_a, isolated_oracle),
        "repeat_max_error": max_error(isolated_a, isolated_repeat),
        "minimum": float(np.min(isolated_a)),
        "maximum": float(np.max(isolated_a)),
        "voice_correlation": normalized_correlation(isolated_a, isolated_voice),
        "generation_correlation": normalized_correlation(isolated_a, isolated_generation),
        "substream_correlation": normalized_correlation(isolated_a, isolated_substream),
    }
    if isolated_metrics["oracle_max_error"] != 0.0:
        failures.append(f"isolated PRNG oracle error {isolated_metrics['oracle_max_error']:.9g}")
    if isolated_metrics["repeat_max_error"] != 0.0:
        failures.append(f"isolated PRNG repeat error {isolated_metrics['repeat_max_error']:.9g}")
    if isolated_metrics["minimum"] < 0.0 or isolated_metrics["maximum"] >= 1.0:
        failures.append("isolated PRNG float mapping escaped [0, 1)")
    for label in ("voice_correlation", "generation_correlation", "substream_correlation"):
        if abs(isolated_metrics[label]) >= 0.1:
            failures.append(f"isolated PRNG {label} {isolated_metrics[label]:.6g} exceeds 0.1")

    sample_checks, sample_failures = sample_snapshot_checks(module)
    failures.extend(sample_failures)
    external_checks, external_failures = external_input_checks(module)
    failures.extend(external_failures)

    return {
        "schema_version": 1,
        "contract": "exciter-audio",
        "status": "pass" if not failures else "fail",
        "deterministic_max_error": deterministic_error,
        "partition_max_errors": partition_errors,
        "cross_voice_correlation": correlation,
        "isolated_prng": isolated_metrics,
        "voice_rms": [first_rms, second_rms],
        "sample_snapshots": sample_checks,
        "external_input": external_checks,
        "failures": failures,
    }


def atomic_json(path: Path, payload: dict[str, Any]) -> None:
    path = path.expanduser().resolve()
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary_name = tempfile.mkstemp(prefix=path.name, suffix=".tmp", dir=path.parent)
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as stream:
            json.dump(payload, stream, indent=2, sort_keys=True)
            stream.write("\n")
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary_name, path)
    finally:
        if os.path.exists(temporary_name):
            os.unlink(temporary_name)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--module", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    try:
        module_path = resolve_module_path(args.module)
        report = run(load_module(module_path))
        report["environment"] = {
            "module_path": str(module_path),
            "module_sha256": hashlib.sha256(module_path.read_bytes()).hexdigest(),
        }
    except (ContractError, RuntimeError, ValueError) as exc:
        print(f"ERROR: {exc}")
        return 2
    if args.output is not None:
        atomic_json(args.output, report)
        print(f"Report: {args.output.expanduser().resolve()}")
    print(json.dumps(report, sort_keys=True))
    return 0 if report["status"] == "pass" else 1


if __name__ == "__main__":
    raise SystemExit(main())
