#!/usr/bin/env python3
"""Lifecycle and state-machine contract for the production exciter rack.

The checks cover every enabled mask, cached controls, source additivity,
MIDI-delimited one-sample renders, lazy enable after release, and source
freeze/resume through the instrumented Python adapter.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import sys
from pathlib import Path
from types import ModuleType
from typing import Any, Iterable

import numpy as np

if not __package__:
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tests.module_loader import load_resonarium, resolve_module_path


SOURCE_COUNT = 5
ALL_SOURCES_MASK = (1 << SOURCE_COUNT) - 1
ENABLE_PARAMETERS = (
    "impExciter0 enabled",
    "noiseExciter0enabled",
    "impTrainExciter0enabled",
    "sampleEnable",
    "extInEnable",
)


class ContractError(RuntimeError):
    pass


def load_module(path: Path) -> ModuleType:
    return load_resonarium(path)[0]


def set_params(synth: Any, values: dict[str, float]) -> None:
    for parameter, value in values.items():
        synth.set_param(parameter, float(value))


def process_silence(synth: Any, blocks: int = 1) -> np.ndarray:
    audio = synth.create_multi_block(blocks)
    synth.process_multi_block(audio)
    return np.asarray(audio, dtype=np.float32).copy()


def process_input(synth: Any, source: np.ndarray) -> np.ndarray:
    return np.asarray(synth.process_input_block(np.asarray(source, dtype=np.float32)),
                      dtype=np.float32).copy()


def voice_buses(synth: Any, voice: int = 0) -> tuple[np.ndarray, np.ndarray]:
    excitation, direct = synth.get_voice_exciter_buses(voice)
    return (np.asarray(excitation, dtype=np.float32).copy(),
            np.asarray(direct, dtype=np.float32).copy())


def configure_common_sources(synth: Any) -> None:
    set_params(synth, {
        "uiBypassResonators": 1.0,
        "impExciter0 thickness": 3.0,
        "impExciter0 level": 0.35,
        "impExciter0filterType": 0.0,
        "noiseExciter0level": 0.4,
        "noiseExciter0filterType": 0.0,
        "noiseExciterEnv0attack": 0.0,
        "noiseExciterEnv0decay": 0.0,
        "noiseExciterEnv0sustain": 100.0,
        "noiseExciterEnv0release": 0.0,
        "impTrainExciter0mode": 2.0,
        "impTrainExciter0rate": 1000.0,
        "impTrainExciter0staticDensity": 1.0,
        "impTrainExciter0impulseLength": 3.0,
        "impTrainExciter0level": 0.2,
        "impTrainExciter0filterType": 0.0,
        "impTrainExciterEnv0attack": 0.0,
        "impTrainExciterEnv0decay": 0.0,
        "impTrainExciterEnv0sustain": 100.0,
        "impTrainExciterEnv0release": 0.0,
        "sampleGain": 0.3,
        "sampleMix": 0.6,
        "sampleLoop": 1.0,
        "sampleStart": 0.0,
        "sampleEnd": 100.0,
        "SamplefilterType": 0.0,
        "Sampleattack": 0.0,
        "Sampledecay": 0.0,
        "Samplesustain": 100.0,
        "Samplerelease": 0.0,
        "extInMix": 1.0,
        "extInGain": -6.0,
        "extInattack": 0.0,
        "extIndecay": 0.0,
        "extInsustain": 100.0,
        "extInrelease": 0.0,
    })


def set_enabled_mask(synth: Any, mask: int) -> None:
    for kind, parameter in enumerate(ENABLE_PARAMETERS):
        synth.set_param(parameter, float(bool(mask & (1 << kind))))


def check_enabled_masks(module: ModuleType) -> dict[str, Any]:
    block_size = 32
    synth = module.Resonarium(48_000.0, block_size)
    configure_common_sources(synth)
    host = np.vstack((np.linspace(0.1, 0.2, block_size, dtype=np.float32),
                      np.linspace(-0.3, -0.1, block_size, dtype=np.float32)))

    checked: list[int] = []
    for mask in range(ALL_SOURCES_MASK + 1):
        set_enabled_mask(synth, mask)
        synth.clear_exciter_telemetry()
        synth.play_note(1, 36 + mask, 100, 0)
        process_input(synth, host)
        telemetry = dict(synth.get_exciter_telemetry())
        expected_pending = ALL_SOURCES_MASK & ~mask
        matching = [dict(voice) for voice in telemetry["voices"]
                    if int(voice["enabled_mask"]) == mask
                    and int(voice["initialized_mask"]) == mask
                    and int(voice["pending_reset_mask"]) == expected_pending]
        if not matching:
            raise ContractError(
                f"enabled mask {mask:#04x} did not publish initialized={mask:#04x}, "
                f"pending={expected_pending:#04x}: {telemetry['voices']}"
            )

        generated = [int(value) for value in telemetry["source_samples_generated"]]
        expected = [block_size if mask & (1 << kind) else 0 for kind in range(SOURCE_COUNT)]
        # A sample source with no installed snapshot initializes but has no data
        # to advance. This distinguishes control lifecycle from data availability.
        expected[3] = 0
        if generated != expected:
            raise ContractError(f"enabled mask {mask:#04x} generated {generated}, expected {expected}")
        checked.append(mask)

        # CC120 immediately retires the assignment so one physical voice can be
        # reused for all 32 patterns without growing the working set.
        synth.queue_midi(bytes((0xB1, 120, 0)), 0)
        process_silence(synth)

    return {"patterns_checked": len(checked), "first": checked[0], "last": checked[-1]}


def configure_noise(synth: Any, *, release: float = 0.0) -> None:
    configure_common_sources(synth)
    set_enabled_mask(synth, 1 << 1)
    synth.set_param("noiseExciterEnv0release", release)


def configure_exact_noise(synth: Any, *, attack: float = 0.0,
                          release: float = 0.0, voice_limit: int = 16) -> None:
    """Configure a filter-free noise source whose PRNG can be checked exactly."""
    configure_noise(synth, release=release)
    set_params(synth, {
        "numVoices": float(voice_limit),
        "noiseExciter0level": 1.0,
        "noiseExciterEnv0attack": attack,
        "noiseExciterEnv0decay": 0.0,
        "noiseExciterEnv0sustain": 100.0,
        "noiseExciter0filterType": 0.0,
    })


def noise_prng(module: ModuleType, synth: Any, voice_id: int, generation: int,
               start: int, count: int) -> np.ndarray:
    unit = np.asarray(
        synth.render_exciter_prng_for_testing(
            voice_id, generation, 1, 0, start + count
        ),
        dtype=np.float32,
    )
    return (unit[start:] * np.float32(2.0) - np.float32(1.0)) * np.float32(0.1)


def noise_voice_block(synth: Any, voice_id: int) -> np.ndarray:
    excitation, _ = voice_buses(synth, voice_id)
    return excitation[0].copy()


def require_noise_oracle(module: ModuleType, synth: Any, voice_id: int,
                         generation: int, start: int, actual: np.ndarray,
                         context: str, tolerance: float = 2.0e-7) -> float:
    expected = noise_prng(module, synth, voice_id, generation, start, actual.size)
    error = float(np.max(np.abs(actual.astype(np.float64)
                                - expected.astype(np.float64))))
    if error > tolerance:
        raise ContractError(f"{context} PRNG oracle error {error:.9g} > {tolerance:.9g}")
    return error


def normalized_envelope(actual: np.ndarray, carrier: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    usable = np.abs(carrier) > 5.0e-3
    if int(np.count_nonzero(usable)) < actual.size // 2:
        raise ContractError("PRNG carrier has too few usable samples for an envelope oracle")
    return actual[usable] / carrier[usable], usable


def matching_noise_voice(module: ModuleType, synth: Any, generation: int,
                         start: int = 0, tolerance: float = 2.0e-7) -> tuple[int, float]:
    matches: list[tuple[int, float]] = []
    for voice_id in range(18):
        actual = noise_voice_block(synth, voice_id)
        expected = noise_prng(module, synth, voice_id, generation, start, actual.size)
        error = float(np.max(np.abs(actual.astype(np.float64)
                                    - expected.astype(np.float64))))
        if error <= tolerance:
            matches.append((voice_id, error))
    if len(matches) != 1:
        raise ContractError(
            f"assignment generation {generation} matched {len(matches)} physical voices: {matches}"
        )
    return matches[0]


def check_filter_cache(module: ModuleType) -> dict[str, Any]:
    modes: list[dict[str, Any]] = []
    for mode in range(6):
        synth = module.Resonarium(48_000.0, 64)
        configure_noise(synth)
        set_params(synth, {
            "noiseExciter0filterType": float(mode),
            "noiseExciter0frequency": 1200.0,
            "noiseExciter0_resonance": 0.70710678,
        })
        synth.clear_exciter_telemetry()
        synth.play_note(1, 60, 100, 0)
        process_silence(synth)
        first, _ = voice_buses(synth)
        first_count = int(synth.get_exciter_telemetry()["filter_coefficient_rebuilds"])
        process_silence(synth)
        unchanged_count = int(synth.get_exciter_telemetry()["filter_coefficient_rebuilds"])
        if unchanged_count != first_count:
            raise ContractError(
                f"filter mode {mode} rebuilt unchanged coefficients: {first_count}->{unchanged_count}"
            )
        expected_first = 0 if mode == 0 else 1
        if first_count != expected_first:
            raise ContractError(f"filter mode {mode} initial rebuilds={first_count}, expected {expected_first}")

        synth.set_param("noiseExciter0frequency", 2400.0)
        process_silence(synth)
        changed_count = int(synth.get_exciter_telemetry()["filter_coefficient_rebuilds"])
        expected_changed = expected_first if mode == 0 else expected_first + 1
        if changed_count != expected_changed:
            raise ContractError(
                f"filter mode {mode} dependency change rebuilds={changed_count}, "
                f"expected {expected_changed}"
            )
        if not np.isfinite(first).all() or float(np.max(np.abs(first))) <= 1.0e-8:
            raise ContractError(f"filter mode {mode} produced invalid or silent noise")
        modes.append({"mode": mode, "initial_rebuilds": first_count,
                      "unchanged_rebuilds": unchanged_count,
                      "changed_rebuilds": changed_count})
    return {"modes": modes}


def check_nyquist_filter_safety(module: ModuleType) -> dict[str, Any]:
    sample_rate = 32_000.0
    block_size = 64
    requested_frequency = 20_000.0
    clamped_equivalent_frequency = 19_000.0
    in_range_frequency = 12_000.0
    peak_limit = 1.0
    modes: list[dict[str, Any]] = []

    for mode in range(1, 6):
        synth = module.Resonarium(sample_rate, block_size)
        configure_noise(synth)
        set_params(synth, {
            "noiseExciter0level": 1.0,
            "noiseExciter0filterType": float(mode),
            "noiseExciter0frequency": requested_frequency,
            "noiseExciter0_resonance": 0.70710678,
        })
        synth.clear_exciter_telemetry()
        synth.play_note(1, 60, 100, 0)
        process_silence(synth)
        first, _ = voice_buses(synth)
        first_count = int(synth.get_exciter_telemetry()["filter_coefficient_rebuilds"])

        process_silence(synth)
        unchanged, _ = voice_buses(synth)
        unchanged_count = int(
            synth.get_exciter_telemetry()["filter_coefficient_rebuilds"]
        )

        # Both requested cutoffs are above 32 kHz Nyquist. They resolve to the
        # same effective cutoff and therefore must share the cached tuple.
        synth.set_param("noiseExciter0frequency", clamped_equivalent_frequency)
        process_silence(synth)
        clamped_equivalent, _ = voice_buses(synth)
        clamped_equivalent_count = int(
            synth.get_exciter_telemetry()["filter_coefficient_rebuilds"]
        )

        synth.set_param("noiseExciter0frequency", in_range_frequency)
        process_silence(synth)
        in_range, _ = voice_buses(synth)
        in_range_count = int(
            synth.get_exciter_telemetry()["filter_coefficient_rebuilds"]
        )

        renders = (first, unchanged, clamped_equivalent, in_range)
        peaks = [float(np.max(np.abs(render))) for render in renders]
        if not all(np.isfinite(render).all() for render in renders):
            raise ContractError(f"32 kHz filter mode {mode} produced non-finite samples")
        if any(peak <= 1.0e-10 for peak in peaks):
            raise ContractError(f"32 kHz filter mode {mode} was silent: peaks={peaks}")
        if any(peak > peak_limit for peak in peaks):
            raise ContractError(
                f"32 kHz filter mode {mode} exceeded {peak_limit}: peaks={peaks}"
            )
        if (first_count != 1 or unchanged_count != 1
                or clamped_equivalent_count != 1 or in_range_count != 2):
            raise ContractError(
                f"32 kHz filter mode {mode} cache counts were "
                f"{first_count}/{unchanged_count}/{clamped_equivalent_count}/"
                f"{in_range_count}, expected 1/1/1/2"
            )

        modes.append({
            "mode": mode,
            "peaks": peaks,
            "initial_rebuilds": first_count,
            "unchanged_rebuilds": unchanged_count,
            "clamped_equivalent_rebuilds": clamped_equivalent_count,
            "in_range_rebuilds": in_range_count,
        })

    return {
        "sample_rate": sample_rate,
        "requested_frequency": requested_frequency,
        "effective_frequency": sample_rate * 0.5 - 1.0,
        "peak_limit": peak_limit,
        "modes": modes,
    }


def render_sequence(module: ModuleType, mode: int, block_size: int,
                    total_samples: int = 1024) -> np.ndarray:
    synth = module.Resonarium(48_000.0, block_size)
    configure_common_sources(synth)
    set_enabled_mask(synth, 1 << 2)
    set_params(synth, {
        "impTrainExciter0mode": float(mode),
        "impTrainExciter0rate": 1000.0,
        "impTrainExciter0staticDensity": 1.0,
        "impTrainExciter0impulseLength": 3.0,
    })
    synth.play_note(1, 60, 100, 0)
    audio = synth.create_multi_block(total_samples // block_size)
    synth.process_multi_block(audio)
    return np.asarray(audio, dtype=np.float32).copy()


def check_sequence_modes(module: ModuleType) -> dict[str, Any]:
    results: list[dict[str, Any]] = []
    renders: list[np.ndarray] = []
    for mode in range(3):
        canonical = render_sequence(module, mode, 128)
        renders.append(canonical)
        errors: dict[str, float] = {}
        for block_size in (32, 512):
            candidate = render_sequence(module, mode, block_size)
            error = float(np.max(np.abs(canonical.astype(np.float64)
                                        - candidate.astype(np.float64))))
            errors[f"128_vs_{block_size}"] = error
            if error > 1.0e-7:
                raise ContractError(f"sequence mode {mode} partition error {block_size}: {error:.9g}")
        peak = float(np.max(np.abs(canonical)))
        if not np.isfinite(canonical).all() or peak <= 1.0e-8:
            raise ContractError(f"sequence mode {mode} is invalid or silent")
        results.append({"mode": mode, "peak": peak, "partition_errors": errors})

    for left in range(3):
        for right in range(left + 1, 3):
            if np.array_equal(renders[left], renders[right]):
                raise ContractError(f"sequence modes {left} and {right} produced identical streams")
    return {"modes": results}


def render_live_sequence_schedule(module: ModuleType, sample_rate: int, block_size: int,
                                  schedule: tuple[tuple[int, float], ...]) -> np.ndarray:
    segment_samples = 1024
    if segment_samples % block_size != 0:
        raise ContractError(f"live sequence segment is not divisible by block size {block_size}")

    synth = module.Resonarium(float(sample_rate), block_size)
    configure_common_sources(synth)
    set_enabled_mask(synth, 1 << 2)
    set_params(synth, {
        "impTrainExciter0level": 1.0,
        "impTrainExciter0filterType": 0.0,
        "impTrainExciter0staticDensity": 0.73,
        "impTrainExciter0impulseLength": 4.0,
    })
    synth.play_note(1, 60, 100, 0)

    chunks: list[np.ndarray] = []
    for mode, rate in schedule:
        set_params(synth, {
            "impTrainExciter0mode": float(mode),
            "impTrainExciter0rate": rate,
        })
        audio = synth.create_multi_block(segment_samples // block_size)
        synth.process_multi_block(audio)
        chunks.append(np.asarray(audio, dtype=np.float32).copy())
    return np.concatenate(chunks, axis=1)


def check_live_sequence_retargets(module: ModuleType) -> dict[str, Any]:
    schedule = ((0, 137.0), (1, 431.0), (2, 73.0), (2, 997.0))
    no_final_rate_retarget = schedule[:-1] + ((2, 73.0),)
    rates: list[dict[str, Any]] = []
    segment_samples = 1024

    for sample_rate in (44_100, 48_000, 96_000):
        canonical = render_live_sequence_schedule(module, sample_rate, 128, schedule)
        errors: dict[str, float] = {}
        for block_size in (32, 512):
            candidate = render_live_sequence_schedule(module, sample_rate, block_size, schedule)
            error = float(np.max(np.abs(canonical.astype(np.float64)
                                        - candidate.astype(np.float64))))
            errors[f"128_vs_{block_size}"] = error
            if error > 1.0e-7:
                raise ContractError(
                    f"live sequence retarget partition error at {sample_rate} Hz/{block_size}: "
                    f"{error:.9g}"
                )

        unchanged_rate = render_live_sequence_schedule(
            module, sample_rate, 128, no_final_rate_retarget
        )
        final_slice = slice(3 * segment_samples, 4 * segment_samples)
        rate_retarget_delta = float(np.max(np.abs(
            canonical[:, final_slice].astype(np.float64)
            - unchanged_rate[:, final_slice].astype(np.float64)
        )))
        peak = float(np.max(np.abs(canonical)))
        if not np.isfinite(canonical).all() or peak <= 1.0e-8:
            raise ContractError(f"live sequence retarget at {sample_rate} Hz was invalid or silent")
        if rate_retarget_delta <= 1.0e-5:
            raise ContractError(
                f"live sequence rate retarget at {sample_rate} Hz had no material effect "
                f"({rate_retarget_delta:.9g})"
            )
        rates.append({
            "sample_rate": sample_rate,
            "partition_errors": errors,
            "rate_retarget_delta": rate_retarget_delta,
            "peak": peak,
        })
    return {"schedule": [[mode, rate] for mode, rate in schedule], "sample_rates": rates}


def check_impulse_sequence_freeze(module: ModuleType) -> dict[str, Any]:
    def render_impulse(freeze: bool) -> tuple[np.ndarray, np.ndarray, dict[str, Any]]:
        synth = module.Resonarium(48_000.0, 4)
        configure_common_sources(synth)
        set_enabled_mask(synth, 1 << 0)
        set_params(synth, {
            "impExciter0 thickness": 10.0,
            "impExciter0 level": 1.0,
            "impExciter0filterType": 0.0,
        })
        synth.clear_exciter_telemetry()
        synth.play_note(1, 60, 100, 0)
        process_silence(synth)
        if freeze:
            synth.set_param("impExciter0 enabled", 0.0)
            before = int(synth.get_exciter_telemetry()["source_samples_generated"][0])
            disabled_a = process_silence(synth)
            disabled_b = process_silence(synth)
            after = int(synth.get_exciter_telemetry()["source_samples_generated"][0])
            if (float(np.max(np.abs(disabled_a))) != 0.0
                    or float(np.max(np.abs(disabled_b))) != 0.0 or after != before):
                raise ContractError(
                    f"disabled impulse rendered or advanced ({before}->{after})"
                )
            synth.set_param("impExciter0 enabled", 1.0)
        second = process_silence(synth)
        third = process_silence(synth)
        return second, third, dict(synth.get_exciter_telemetry())

    impulse_continuous = render_impulse(False)
    impulse_frozen = render_impulse(True)
    impulse_errors = [
        float(np.max(np.abs(impulse_continuous[index].astype(np.float64)
                            - impulse_frozen[index].astype(np.float64))))
        for index in (0, 1)
    ]
    if max(impulse_errors) != 0.0:
        raise ContractError(f"impulse disable/re-enable reset its remaining count: {impulse_errors}")
    impulse_generated = int(impulse_frozen[2]["source_samples_generated"][0])
    if impulse_generated != 12:
        raise ContractError(f"frozen impulse generated {impulse_generated} samples, expected 12")

    def render_sequence_freeze(freeze: bool) -> tuple[np.ndarray, np.ndarray, dict[str, Any]]:
        synth = module.Resonarium(48_000.0, 64)
        configure_common_sources(synth)
        set_enabled_mask(synth, 1 << 2)
        set_params(synth, {
            "impTrainExciter0mode": 2.0,
            "impTrainExciter0rate": 137.0,
            "impTrainExciter0level": 1.0,
            "impTrainExciter0filterType": 0.0,
        })
        synth.clear_exciter_telemetry()
        synth.play_note(1, 60, 100, 0)
        process_silence(synth)
        if freeze:
            synth.set_param("impTrainExciter0enabled", 0.0)
            before = int(synth.get_exciter_telemetry()["source_samples_generated"][2])
            disabled_a = process_silence(synth)
            disabled_b = process_silence(synth)
            after = int(synth.get_exciter_telemetry()["source_samples_generated"][2])
            if (float(np.max(np.abs(disabled_a))) != 0.0
                    or float(np.max(np.abs(disabled_b))) != 0.0 or after != before):
                raise ContractError(
                    f"disabled sequence rendered or advanced ({before}->{after})"
                )
            synth.set_param("impTrainExciter0enabled", 1.0)
        second = process_silence(synth)
        third = process_silence(synth)
        return second, third, dict(synth.get_exciter_telemetry())

    sequence_continuous = render_sequence_freeze(False)
    sequence_frozen = render_sequence_freeze(True)
    sequence_errors = [
        float(np.max(np.abs(sequence_continuous[index].astype(np.float64)
                            - sequence_frozen[index].astype(np.float64))))
        for index in (0, 1)
    ]
    if max(sequence_errors) != 0.0:
        raise ContractError(
            f"sequence disable/re-enable reset phase or PRNG state: {sequence_errors}"
        )
    sequence_generated = int(sequence_frozen[2]["source_samples_generated"][2])
    if sequence_generated != 192:
        raise ContractError(
            f"frozen sequence generated {sequence_generated} samples, expected 192"
        )

    return {
        "impulse_resume_errors": impulse_errors,
        "impulse_generated_samples": impulse_generated,
        "sequence_resume_errors": sequence_errors,
        "sequence_generated_samples": sequence_generated,
    }


def install_sample(synth: Any, source: np.ndarray) -> int:
    generation = int(synth.install_sample_for_testing(np.asarray(source, dtype=np.float32), 48_000.0))
    process_silence(synth)
    active = int(synth.get_sample_telemetry()["active_generation"])
    if active != generation:
        raise ContractError(f"sample generation {generation} did not commit (active={active})")
    return generation


def render_source_set(module: ModuleType, enabled: Iterable[int]) -> tuple[np.ndarray, np.ndarray]:
    block_size = 64
    synth = module.Resonarium(48_000.0, block_size)
    configure_common_sources(synth)
    sample = np.vstack((np.linspace(-0.4, 0.5, 19, dtype=np.float32),
                        np.linspace(0.25, -0.6, 19, dtype=np.float32)))
    install_sample(synth, sample)
    mask = sum(1 << kind for kind in enabled)
    set_enabled_mask(synth, mask)
    synth.play_note(1, 60, 100, 0)
    host = np.vstack((np.linspace(-0.2, 0.35, block_size, dtype=np.float32),
                      np.linspace(0.4, -0.1, block_size, dtype=np.float32)))
    process_input(synth, host)
    return voice_buses(synth)


def check_ordered_additivity(module: ModuleType) -> dict[str, Any]:
    isolated = [render_source_set(module, (kind,)) for kind in range(SOURCE_COUNT)]
    combined_excitation, combined_direct = render_source_set(module, range(SOURCE_COUNT))
    summed_excitation = np.sum([item[0] for item in isolated], axis=0, dtype=np.float64)
    summed_direct = np.sum([item[1] for item in isolated], axis=0, dtype=np.float64)
    excitation_error = float(np.max(np.abs(combined_excitation.astype(np.float64)
                                           - summed_excitation)))
    direct_error = float(np.max(np.abs(combined_direct.astype(np.float64) - summed_direct)))
    if excitation_error > 1.0e-5 or direct_error > 1.0e-5:
        raise ContractError(
            f"mixed rack != ordered isolated sum (excitation={excitation_error:.9g}, "
            f"direct={direct_error:.9g})"
        )
    return {"excitation_max_error": excitation_error, "direct_max_error": direct_error}


def check_freeze_and_lazy_enable(module: ModuleType) -> dict[str, Any]:
    block_size = 64

    continuous = module.Resonarium(48_000.0, block_size)
    configure_noise(continuous)
    continuous.play_note(1, 60, 100, 0)
    process_silence(continuous)
    process_silence(continuous)
    expected_resume, _ = voice_buses(continuous)

    frozen = module.Resonarium(48_000.0, block_size)
    configure_noise(frozen)
    frozen.play_note(1, 60, 100, 0)
    process_silence(frozen)
    before_disable = [int(value) for value in
                      frozen.get_exciter_telemetry()["source_samples_generated"]]
    frozen.set_param("noiseExciter0enabled", 0.0)
    process_silence(frozen)
    disabled, _ = voice_buses(frozen)
    after_disable = [int(value) for value in
                     frozen.get_exciter_telemetry()["source_samples_generated"]]
    frozen.set_param("noiseExciter0enabled", 1.0)
    process_silence(frozen)
    resumed, _ = voice_buses(frozen)
    resume_error = float(np.max(np.abs(expected_resume.astype(np.float64)
                                       - resumed.astype(np.float64))))
    if resume_error != 0.0:
        raise ContractError(f"disabled noise did not resume frozen state (error={resume_error:.9g})")
    if float(np.max(np.abs(disabled))) != 0.0 or after_disable != before_disable:
        raise ContractError(
            f"disabled noise performed work (peak={np.max(np.abs(disabled))}, "
            f"counts={before_disable}->{after_disable})"
        )

    held = module.Resonarium(48_000.0, block_size)
    configure_common_sources(held)
    set_enabled_mask(held, 0)
    held.play_note(1, 60, 100, 0)
    process_silence(held)
    held.set_param("noiseExciter0enabled", 1.0)
    process_silence(held)
    held_lazy, _ = voice_buses(held)

    released = module.Resonarium(48_000.0, block_size)
    configure_common_sources(released)
    set_enabled_mask(released, 0)
    released.play_note(1, 60, 100, 0)
    process_silence(released)
    released.release_note(1, 60, 0, 0)
    process_silence(released)
    released.set_param("noiseExciter0enabled", 1.0)
    process_silence(released)
    released_lazy, _ = voice_buses(released)
    released_voice = dict(released.get_exciter_telemetry()["voices"][0])
    held_peak = float(np.max(np.abs(held_lazy)))
    released_peak = float(np.max(np.abs(released_lazy)))
    if held_peak <= 1.0e-8 or released_peak != 0.0:
        raise ContractError(
            f"lazy enable ignored held/released gate (held={held_peak:.9g}, released={released_peak:.9g})"
        )
    if (int(released_voice["initialized_mask"]) & (1 << 1)) == 0:
        raise ContractError(f"released lazy source was not initialized: {released_voice}")

    # Note-off does not cancel an impulse first enabled later in the assignment.
    released_impulse = module.Resonarium(48_000.0, block_size)
    configure_common_sources(released_impulse)
    set_enabled_mask(released_impulse, 0)
    released_impulse.play_note(1, 60, 100, 0)
    process_silence(released_impulse)
    released_impulse.release_note(1, 60, 0, 0)
    process_silence(released_impulse)
    released_impulse.set_param("impExciter0 enabled", 1.0)
    process_silence(released_impulse)
    impulse_after_release, _ = voice_buses(released_impulse)
    impulse_peak = float(np.max(np.abs(impulse_after_release)))
    if impulse_peak <= 1.0e-8:
        raise ContractError("lazy impulse was incorrectly cancelled by an earlier note-off")

    return {"noise_resume_max_error": resume_error, "disabled_noise_peak": 0.0,
            "held_lazy_peak": held_peak, "released_lazy_peak": released_peak,
            "released_lazy_impulse_peak": impulse_peak}


def check_prng_assignment_lifecycle(module: ModuleType) -> dict[str, Any]:
    block_size = 32

    # A true Gin mono-legato note retarget reuses the physical voice and calls
    # noteRetriggered without beginning a new exciter assignment. Its generator
    # must therefore continue the original assignment's stream.
    continuity = module.Resonarium(48_000.0, block_size)
    configure_exact_noise(continuity, voice_limit=1)
    continuity.set_play_mode_for_testing("mono_legato")
    continuity.clear_exciter_telemetry()
    continuity.play_note(2, 60, 100, 0)
    process_silence(continuity)
    first = noise_voice_block(continuity, 0)
    first_error = require_noise_oracle(
        module, continuity, 0, 1, 0, first, "initial assignment"
    )
    continuity.play_note(2, 64, 100, 0)
    process_silence(continuity)
    after_retarget = noise_voice_block(continuity, 0)
    retarget_error = require_noise_oracle(
        module, continuity, 0, 1, block_size, after_retarget,
        "held-note pitch retarget"
    )
    continuity_rack = dict(continuity.get_exciter_telemetry())
    continuity_voices = dict(continuity.get_voice_telemetry())
    if (int(continuity_rack["assignments"]) != 1
            or int(continuity_voices["note_starts"]) != 2):
        raise ContractError(
            f"mono-legato retarget created an exciter assignment: rack={continuity_rack}, "
            f"voices={continuity_voices}"
        )

    # With one musical slot, the next note must use a bounded stealing slot and
    # receive a new (physical voice, assignment generation) PRNG identity.
    stealing = module.Resonarium(48_000.0, block_size)
    configure_exact_noise(stealing, voice_limit=1)
    stealing.clear_exciter_telemetry()
    stealing.play_note(2, 60, 100, 0)
    process_silence(stealing)
    stealing.play_note(3, 64, 100, 0)
    process_silence(stealing)
    stolen_voice, stolen_error = matching_noise_voice(module, stealing, generation=2)
    steal_rack = dict(stealing.get_exciter_telemetry())
    steal_voices = dict(stealing.get_voice_telemetry())
    if (int(steal_rack["assignments"]) != 2
            or int(steal_voices["steals"]) != 1
            or int(steal_voices["fast_fade_voices"]) != 1):
        raise ContractError(
            f"steal did not create one reseeded bounded-overlap assignment: "
            f"rack={steal_rack}, voices={steal_voices}"
        )

    # A repeated same-note note-on is a full assignment, so its deterministic
    # stream changes and its envelope restarts at the same attack curve.
    retrigger = module.Resonarium(48_000.0, block_size)
    configure_exact_noise(retrigger, attack=0.01, release=0.02, voice_limit=4)
    retrigger.clear_exciter_telemetry()
    retrigger.play_note(1, 60, 100, 0)
    process_silence(retrigger)
    first_attack = noise_voice_block(retrigger, 0)
    retrigger.play_note(1, 60, 90, 0)
    process_silence(retrigger)
    rendered = [
        (voice_id, noise_voice_block(retrigger, voice_id))
        for voice_id in range(18)
    ]
    first_carrier = noise_prng(module, retrigger, 0, 1, 0, block_size)
    attack_matches: list[tuple[int, float]] = []
    for voice_id, block in rendered:
        second_carrier = noise_prng(module, retrigger, voice_id, 2, 0, block_size)
        common = ((np.abs(first_carrier) > 5.0e-3)
                  & (np.abs(second_carrier) > 5.0e-3))
        if int(np.count_nonzero(common)) < block_size // 3:
            continue
        error = float(np.max(np.abs(
            first_attack[common] / first_carrier[common]
            - block[common] / second_carrier[common]
        )))
        if error <= 2.0e-6:
            attack_matches.append((voice_id, error))
    if len(attack_matches) != 1:
        raise ContractError(
            f"same-note retrigger matched {len(attack_matches)} restarted envelopes: "
            f"{attack_matches}"
        )
    retrigger_voice, attack_error = attack_matches[0]
    retrigger_rack = dict(retrigger.get_exciter_telemetry())
    retrigger_voices = dict(retrigger.get_voice_telemetry())
    if attack_error > 2.0e-6:
        raise ContractError(f"retrigger did not restart its envelope (error={attack_error:.9g})")
    if (int(retrigger_rack["assignments"]) != 2
            or int(retrigger_voices["note_starts"]) != 2):
        raise ContractError(
            f"same-note retrigger assignment counts are wrong: "
            f"rack={retrigger_rack}, voices={retrigger_voices}"
        )

    # Sustain holds both the envelope and generator continuity after key-up.
    sustained = module.Resonarium(48_000.0, block_size)
    configure_exact_noise(sustained, release=0.02, voice_limit=4)
    sustained.clear_exciter_telemetry()
    sustained.play_note(1, 60, 100, 0)
    process_silence(sustained)
    sustained.queue_midi(bytes((0xB0, 64, 127)), 0)
    sustained.release_note(1, 60, 0, 0)
    process_silence(sustained)
    pedal_held = noise_voice_block(sustained, 0)
    pedal_held_error = require_noise_oracle(
        module, sustained, 0, 1, block_size, pedal_held, "sustain-held assignment"
    )
    sustained_rack = dict(sustained.get_exciter_telemetry())
    sustained_voices = dict(sustained.get_voice_telemetry())
    if (int(sustained_rack["assignments"]) != 1
            or int(sustained_voices["musical_voices"]) != 1):
        raise ContractError(
            f"sustain did not retain one assignment: rack={sustained_rack}, "
            f"voices={sustained_voices}"
        )
    sustained.queue_midi(bytes((0xB0, 64, 0)), 0)
    process_silence(sustained)
    pedal_release = noise_voice_block(sustained, 0)
    pedal_carrier = noise_prng(module, sustained, 0, 1, 2 * block_size, block_size)
    pedal_envelope, _ = normalized_envelope(pedal_release, pedal_carrier)
    if (float(np.min(pedal_envelope)) < -1.0e-5
            or float(np.max(pedal_envelope)) >= 1.0
            or float(np.max(np.diff(pedal_envelope), initial=-1.0)) > 1.0e-5):
        raise ContractError(f"sustain release envelope is invalid: {pedal_envelope.tolist()}")

    # CC123 releases normally: the same stream advances under a monotonic
    # release envelope, rather than resetting or reseeding the generator.
    all_notes = module.Resonarium(48_000.0, block_size)
    configure_exact_noise(all_notes, release=0.02, voice_limit=4)
    all_notes.clear_exciter_telemetry()
    all_notes.play_note(1, 60, 100, 0)
    process_silence(all_notes)
    all_notes.all_notes_off(0)
    process_silence(all_notes)
    cc123_release = noise_voice_block(all_notes, 0)
    cc123_carrier = noise_prng(module, all_notes, 0, 1, block_size, block_size)
    cc123_envelope, _ = normalized_envelope(cc123_release, cc123_carrier)
    if (float(np.min(cc123_envelope)) < -1.0e-5
            or float(np.max(cc123_envelope)) >= 1.0
            or float(np.max(np.diff(cc123_envelope), initial=-1.0)) > 1.0e-5
            or int(all_notes.get_exciter_telemetry()["assignments"]) != 1):
        raise ContractError(f"CC123 release reset source state: {cc123_envelope.tolist()}")

    def check_hard_restart(kind: str) -> dict[str, Any]:
        synth = module.Resonarium(48_000.0, block_size)
        configure_exact_noise(synth, attack=0.01, release=0.02, voice_limit=4)
        synth.clear_exciter_telemetry()
        synth.play_note(1, 60, 100, 0)
        process_silence(synth)
        initial = noise_voice_block(synth, 0)
        initial_carrier = noise_prng(module, synth, 0, 1, 0, block_size)

        if kind == "cc120":
            synth.queue_midi(bytes((0xB0, 120, 0)), 0)
        elif kind == "reset":
            synth.reset()
        else:
            raise AssertionError(kind)
        process_silence(synth)

        retired_voices = dict(synth.get_voice_telemetry())
        retired_rack = dict(synth.get_exciter_telemetry())
        if (int(retired_voices["musical_voices"]) != 0
                or int(retired_voices["fast_fade_voices"]) != 0):
            raise ContractError(f"{kind} did not hard-retire voices: {retired_voices}")
        masks = [
            (int(voice["enabled_mask"]), int(voice["initialized_mask"]),
             int(voice["pending_reset_mask"]))
            for voice in retired_rack["voices"]
        ]
        if any(mask != (0, 0, ALL_SOURCES_MASK) for mask in masks):
            raise ContractError(f"{kind} left initialized rack state: {masks}")

        synth.play_note(1, 62, 100, 0)
        process_silence(synth)
        rendered = [
            (voice_id, noise_voice_block(synth, voice_id))
            for voice_id in range(18)
        ]
        active = [(voice_id, block) for voice_id, block in rendered
                  if float(np.max(np.abs(block))) > 1.0e-8]
        if len(active) != 1:
            raise ContractError(f"{kind} restart rendered {len(active)} private noise buses")
        restarted_voice, restarted = active[0]
        restarted_carrier = noise_prng(
            module, synth, restarted_voice, 2, 0, block_size
        )
        common = ((np.abs(initial_carrier) > 5.0e-3)
                  & (np.abs(restarted_carrier) > 5.0e-3))
        envelope_error = float(np.max(np.abs(
            initial[common] / initial_carrier[common]
            - restarted[common] / restarted_carrier[common]
        )))
        final_rack = dict(synth.get_exciter_telemetry())
        if envelope_error > 2.0e-6 or int(final_rack["assignments"]) != 2:
            raise ContractError(
                f"{kind} restart did not reseed/reset its assignment "
                f"(envelope={envelope_error:.9g}, rack={final_rack})"
            )
        return {
            "restarted_voice": restarted_voice,
            "generation": 2,
            "attack_restart_max_error": envelope_error,
            "retired_masks": len(masks),
        }

    return {
        "held_retarget": {
            "initial_oracle_max_error": first_error,
            "continuation_oracle_max_error": retarget_error,
            "assignments": int(continuity_rack["assignments"]),
        },
        "steal": {
            "new_voice": stolen_voice,
            "generation": 2,
            "oracle_max_error": stolen_error,
        },
        "same_note_retrigger": {
            "new_voice": retrigger_voice,
            "generation": 2,
            "attack_restart_max_error": attack_error,
        },
        "sustain": {
            "held_oracle_max_error": pedal_held_error,
            "release_envelope_first": float(pedal_envelope[0]),
            "release_envelope_last": float(pedal_envelope[-1]),
        },
        "cc123": {
            "release_envelope_first": float(cc123_envelope[0]),
            "release_envelope_last": float(cc123_envelope[-1]),
        },
        "panic_cc120": check_hard_restart("cc120"),
        "processor_reset": check_hard_restart("reset"),
    }


def check_one_sample_quanta(module: ModuleType) -> dict[str, Any]:
    block_size = 32
    synth = module.Resonarium(48_000.0, block_size)
    configure_common_sources(synth)
    set_enabled_mask(synth, 1 << 0)
    synth.clear_exciter_telemetry()
    synth.play_note(1, 60, 100, block_size - 2)
    synth.release_note(1, 60, 0, block_size - 1)
    process_silence(synth)
    excitation, _ = voice_buses(synth)
    generated = int(synth.get_exciter_telemetry()["source_samples_generated"][0])
    pre_peak = float(np.max(np.abs(excitation[:, :block_size - 2])))
    first = float(np.max(np.abs(excitation[:, block_size - 2])))
    second = float(np.max(np.abs(excitation[:, block_size - 1])))
    if generated != 2 or pre_peak != 0.0 or first <= 1.0e-8 or second <= 1.0e-8:
        raise ContractError(
            f"one-sample MIDI quanta failed: generated={generated}, pre={pre_peak:.9g}, "
            f"samples=({first:.9g},{second:.9g})"
        )
    return {"generated_samples": generated, "pre_event_peak": pre_peak,
            "note_on_sample_peak": first, "note_off_sample_peak": second}


def configure_sample(synth: Any, *, loop: bool, start: float, end: float) -> None:
    configure_common_sources(synth)
    set_enabled_mask(synth, 1 << 3)
    set_params(synth, {"sampleGain": 1.0, "sampleMix": 1.0,
                       "sampleLoop": float(loop), "sampleStart": start, "sampleEnd": end})


def check_sample_ranges_and_freeze(module: ModuleType) -> dict[str, Any]:
    source = np.vstack((np.arange(1, 9, dtype=np.float32),
                        -np.arange(1, 9, dtype=np.float32)))

    bounded = module.Resonarium(48_000.0, 32)
    configure_sample(bounded, loop=False, start=25.0, end=75.0)
    install_sample(bounded, source)
    bounded.play_note(1, 60, 100, 0)
    process_silence(bounded)
    bounded_excitation, _ = voice_buses(bounded)
    expected_bounded = np.zeros((2, 32), dtype=np.float32)
    # Playback starts at frame zero and
    # sampleStart is consulted only after a loop reaches sampleEnd.
    expected_bounded[0, :6] = (1.0, 2.0, 3.0, 4.0, 5.0, 6.0)
    expected_bounded[1, :6] = (-1.0, -2.0, -3.0, -4.0, -5.0, -6.0)
    bounded_error = float(np.max(np.abs(bounded_excitation - expected_bounded)))
    if bounded_error > 2.0e-6:
        raise ContractError(f"sample start/end/non-loop oracle error {bounded_error:.9g}")

    wrapping = module.Resonarium(48_000.0, 32)
    configure_sample(wrapping, loop=True, start=25.0, end=62.5)
    install_sample(wrapping, source)
    wrapping.play_note(1, 60, 100, 0)
    process_silence(wrapping)
    wrapped, _ = voice_buses(wrapping)
    expected_cycle = np.asarray((3.0, 4.0, 5.0), dtype=np.float32)
    expected_left = np.concatenate(
        (np.asarray((1.0, 2.0, 3.0, 4.0, 5.0), dtype=np.float32),
         np.resize(expected_cycle, 27))
    )
    wrapping_error = max(float(np.max(np.abs(wrapped[0] - expected_left))),
                         float(np.max(np.abs(wrapped[1] + expected_left))))
    if wrapping_error > 2.0e-6:
        raise ContractError(f"sample multi-wrap oracle error {wrapping_error:.9g}")

    invalid_range = module.Resonarium(48_000.0, 32)
    configure_sample(invalid_range, loop=True, start=75.0, end=25.0)
    install_sample(invalid_range, source)
    invalid_range.play_note(1, 60, 100, 0)
    process_silence(invalid_range)
    invalid_audio, _ = voice_buses(invalid_range)
    invalid_generated = int(invalid_range.get_exciter_telemetry()["source_samples_generated"][3])
    if float(np.max(np.abs(invalid_audio))) != 0.0 or invalid_generated != 0:
        raise ContractError("empty/reversed sample range performed render work")

    frozen = module.Resonarium(48_000.0, 8)
    configure_sample(frozen, loop=True, start=0.0, end=100.0)
    install_sample(frozen, np.vstack((np.arange(1, 17, dtype=np.float32),
                                      -np.arange(1, 17, dtype=np.float32))))
    frozen.play_note(1, 60, 100, 0)
    process_silence(frozen)
    first, _ = voice_buses(frozen)
    frozen.set_param("sampleEnable", 0.0)
    process_silence(frozen)
    disabled, _ = voice_buses(frozen)
    frozen.set_param("sampleEnable", 1.0)
    process_silence(frozen)
    resumed, _ = voice_buses(frozen)
    freeze_error = max(float(np.max(np.abs(first[0] - np.arange(1, 9, dtype=np.float32)))),
                       float(np.max(np.abs(resumed[0] - np.arange(9, 17, dtype=np.float32)))))
    if freeze_error > 3.0e-6 or float(np.max(np.abs(disabled))) != 0.0:
        raise ContractError(
            f"disabled sample did not freeze/resume playback (error={freeze_error:.9g}, "
            f"disabled={np.max(np.abs(disabled)):.9g})"
        )

    return {"start_end_max_error": bounded_error, "multi_wrap_max_error": wrapping_error,
            "reversed_range_generated": invalid_generated, "freeze_resume_max_error": freeze_error}


def check_independent_stereo_sample_mix(module: ModuleType) -> dict[str, Any]:
    """Exercise the actual poly-modulated L/R mix values, not equal base values.

    A stereo mono-LFO source gives getValue(sampleMix, 0/1) two different values
    through the real modulation matrix, proving independent excitation and direct
    routing without adding a bespoke production hook.
    """
    synth = module.Resonarium(48_000.0, 32)
    configure_sample(synth, loop=True, start=0.0, end=100.0)
    set_params(synth, {
        "sampleMix": 0.5,
        "lfo0enable": 1.0,
        "lfo0sync": 0.0,
        "lfo0retrig": 1.0,
        "lfo0wave": 1.0,
        "lfo0rate": 1.0,
        "lfo0depth": 1.0,
        "lfo0phase": 0.0,
        "lfo0offset": 0.0,
        "lfo0fade": 0.0,
        "lfo0delay": 0.0,
        "lfo0stereo": 1.0,
    })
    synth.connect_modulation("LFO 1 (Mono)", "sampleMix", 0.5)
    install_sample(synth, np.vstack((np.ones(5, dtype=np.float32),
                                     np.full(5, 2.0, dtype=np.float32))))
    synth.play_note(1, 60, 100, 0)
    process_silence(synth)
    excitation, direct = voice_buses(synth)

    total = excitation + direct
    left_fraction = float(excitation[0, 0] / total[0, 0])
    right_fraction = float(excitation[1, 0] / total[1, 0])
    total_error = max(float(np.max(np.abs(total[0] - 1.0))),
                      float(np.max(np.abs(total[1] - 2.0))))
    direct_error = max(float(np.max(np.abs(direct[0] - (1.0 - excitation[0])))),
                       float(np.max(np.abs(direct[1] - (2.0 - excitation[1])))))
    separation = abs(right_fraction - left_fraction)
    if separation < 1.0e-3:
        raise ContractError(
            f"sample mix modulation collapsed L/R to one value ({left_fraction:.9g}, "
            f"{right_fraction:.9g})"
        )
    if total_error > 2.0e-6 or direct_error > 2.0e-6:
        raise ContractError(
            f"sample excitation/direct stereo complements failed (total={total_error:.9g}, "
            f"direct={direct_error:.9g})"
        )
    return {"left_mix": left_fraction, "right_mix": right_fraction,
            "mix_separation": separation, "total_max_error": total_error,
            "direct_complement_max_error": direct_error}


def configure_external(synth: Any, mix: float = 1.0, gain_db: float = 0.0) -> None:
    configure_common_sources(synth)
    set_enabled_mask(synth, 1 << 4)
    set_params(synth, {"extInMix": mix, "extInGain": gain_db,
                       "extInattack": 0.0, "extIndecay": 0.0,
                       "extInsustain": 100.0, "extInrelease": 1.0})


def check_external_offsets_and_stale_data(module: ModuleType) -> dict[str, Any]:
    block_size = 32
    offsets: list[dict[str, Any]] = []
    for offset in (0, 1, block_size - 1):
        synth = module.Resonarium(48_000.0, block_size)
        configure_external(synth)
        host = np.vstack((np.linspace(0.1, 0.4, block_size, dtype=np.float32),
                          np.linspace(-0.2, -0.8, block_size, dtype=np.float32)))
        synth.clear_exciter_telemetry()
        synth.clear_external_input_telemetry()
        synth.play_note(1, 60, 100, offset)
        process_input(synth, host)
        excitation, _ = voice_buses(synth)
        pre_peak = float(np.max(np.abs(excitation[:, :offset]), initial=0.0))
        post_peak = float(np.max(np.abs(excitation[:, offset:])))
        expected = host[:, offset:] * 0.05
        oracle_error = float(np.max(np.abs(excitation[:, offset:] - expected)))
        generated = int(synth.get_exciter_telemetry()["source_samples_generated"][4])
        frame = dict(synth.get_external_input_telemetry())
        if pre_peak != 0.0 or post_peak <= 1.0e-8 or oracle_error > 1.0e-7:
            raise ContractError(
                f"external offset {offset} failed pre={pre_peak:.9g}, post={post_peak:.9g}, "
                f"oracle={oracle_error:.9g}"
            )
        if generated != block_size - offset:
            raise ContractError(f"external offset {offset} generated {generated} samples")
        if frame != {"host_blocks": 1, "valid_blocks": 1,
                     "source_samples_copied": block_size * 2}:
            raise ContractError(f"external offset {offset} capture telemetry mismatch: {frame}")
        offsets.append({"offset": offset, "oracle_max_error": oracle_error,
                        "generated_samples": generated})

    synth = module.Resonarium(48_000.0, block_size)
    configure_external(synth)
    synth.clear_exciter_telemetry()
    synth.clear_external_input_telemetry()
    synth.play_note(1, 60, 100, 0)
    process_input(synth, np.ones((2, block_size), dtype=np.float32))
    before_disable = int(synth.get_exciter_telemetry()["source_samples_generated"][4])
    synth.set_param("extInEnable", 0.0)
    process_input(synth, np.full((2, block_size), 0.75, dtype=np.float32))
    disabled, _ = voice_buses(synth)
    after_disable = int(synth.get_exciter_telemetry()["source_samples_generated"][4])
    synth.set_param("extInEnable", 1.0)
    process_input(synth, np.zeros((2, block_size), dtype=np.float32))
    resumed_zero, _ = voice_buses(synth)
    final_count = int(synth.get_exciter_telemetry()["source_samples_generated"][4])
    frame = dict(synth.get_external_input_telemetry())
    if float(np.max(np.abs(disabled))) != 0.0 or after_disable != before_disable:
        raise ContractError("disabled external source rendered or advanced")
    if float(np.max(np.abs(resumed_zero))) != 0.0:
        raise ContractError("re-enabled external source replayed stale captured samples")
    if final_count != before_disable + block_size:
        raise ContractError(f"re-enabled external source advanced {final_count - before_disable} samples")
    if frame != {"host_blocks": 3, "valid_blocks": 2,
                 "source_samples_copied": block_size * 2 * 2}:
        raise ContractError(f"external disable/re-enable capture mismatch: {frame}")

    gain_mix_matrix: list[dict[str, Any]] = []
    matrix_host = np.vstack((np.linspace(0.1, 0.4, block_size, dtype=np.float32),
                             np.linspace(-0.2, -0.8, block_size, dtype=np.float32)))
    for mix in (0.0, 0.5, 1.0):
        for gain_db in (-100.0, 0.0, 24.0):
            corner = module.Resonarium(48_000.0, block_size)
            configure_external(corner, mix, gain_db)
            corner.play_note(1, 60, 100, 0)
            process_input(corner, matrix_host)
            excitation, _ = voice_buses(corner)
            # JUCE's default decibel conversion treats -100 dB as exact silence.
            linear_gain = 0.0 if gain_db <= -100.0 else math.pow(10.0, gain_db / 20.0)
            wet_gain = math.sin(mix * math.pi / 2.0) * linear_gain
            expected = matrix_host * np.float32(0.05 * wet_gain)
            error = float(np.max(np.abs(excitation.astype(np.float64)
                                        - expected.astype(np.float64))))
            tolerance = max(2.0e-7, float(np.max(np.abs(expected))) * 2.0e-6)
            if error > tolerance:
                raise ContractError(
                    f"external mix/gain ({mix}, {gain_db}) oracle error {error:.9g} "
                    f"> {tolerance:.9g}"
                )
            gain_mix_matrix.append({"mix": mix, "gain_db": gain_db,
                                    "oracle_max_error": error})

    voice_matrix: list[dict[str, Any]] = []
    for voice_count in (1, 8, 16):
        voices = module.Resonarium(48_000.0, block_size)
        configure_external(voices)
        voices.clear_exciter_telemetry()
        for voice in range(voice_count):
            voices.play_note(1 + voice % 15, 36 + voice, 100, 0)
        process_input(voices, matrix_host)
        telemetry = dict(voices.get_exciter_telemetry())
        reads = int(telemetry["external_voice_reads"])
        generated = int(telemetry["source_samples_generated"][4])
        if reads != voice_count or generated != voice_count * block_size:
            raise ContractError(
                f"external {voice_count}-voice work mismatch reads={reads}, generated={generated}"
            )
        voice_matrix.append({"rendered_voices": voice_count, "voice_reads": reads,
                             "generated_samples": generated})

    # Reach all 18 physical voices through the two bounded stealing overlaps:
    # render a full chord first, then steal two already-rendered assignments.
    voices = module.Resonarium(48_000.0, block_size)
    configure_external(voices)
    for voice in range(16):
        voices.play_note(1 + voice % 15, 36 + voice, 100, 0)
    process_input(voices, matrix_host)
    voices.clear_exciter_telemetry()
    voices.play_note(1, 84, 100, 0)
    voices.play_note(2, 85, 100, 0)
    process_input(voices, matrix_host)
    rack = dict(voices.get_exciter_telemetry())
    lifecycle = dict(voices.get_voice_telemetry())
    reads = int(rack["external_voice_reads"])
    generated = int(rack["source_samples_generated"][4])
    if (reads != 18 or generated != 18 * block_size
            or int(lifecycle["rendered_voices"]) != 18
            or int(lifecycle["fast_fade_voices"]) != 2):
        raise ContractError(
            f"external 18-physical-voice overlap mismatch reads={reads}, generated={generated}, "
            f"lifecycle={lifecycle}"
        )
    voice_matrix.append({"rendered_voices": 18, "voice_reads": reads,
                         "generated_samples": generated,
                         "fast_fade_voices": int(lifecycle["fast_fade_voices"])})

    return {"offsets": offsets, "gain_mix_matrix": gain_mix_matrix,
            "voice_matrix": voice_matrix, "disable_reenable_frame": frame,
            "stale_replay_peak": float(np.max(np.abs(resumed_zero)))}


def run(module: ModuleType) -> dict[str, Any]:
    required = (
        "get_exciter_telemetry", "clear_exciter_telemetry", "get_voice_exciter_buses",
        "install_sample_for_testing", "get_external_input_telemetry",
        "clear_external_input_telemetry", "process_input_block",
        "set_play_mode_for_testing",
    )
    probe = module.Resonarium(48_000.0, 32)
    missing = [name for name in required if not hasattr(probe, name)]
    if missing:
        raise ContractError(f"extension lacks rack contract APIs: {', '.join(missing)}")

    return {
        "schema_version": 1,
        "contract": "exciter-rack",
        "status": "pass",
        "enabled_masks": check_enabled_masks(module),
        "filter_cache": check_filter_cache(module),
        "nyquist_filter_safety": check_nyquist_filter_safety(module),
        "sequence_modes": check_sequence_modes(module),
        "live_sequence_retargets": check_live_sequence_retargets(module),
        "ordered_additivity": check_ordered_additivity(module),
        "impulse_sequence_freeze": check_impulse_sequence_freeze(module),
        "freeze_and_lazy_enable": check_freeze_and_lazy_enable(module),
        "prng_assignment_lifecycle": check_prng_assignment_lifecycle(module),
        "one_sample_quanta": check_one_sample_quanta(module),
        "sample_ranges": check_sample_ranges_and_freeze(module),
        "independent_stereo_sample_mix": check_independent_stereo_sample_mix(module),
        "external_input": check_external_offsets_and_stale_data(module),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--module", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    arguments = parser.parse_args()

    try:
        module_path = resolve_module_path(arguments.module)
        report = run(load_module(module_path))
        report["environment"] = {
            "module_path": str(module_path),
            "module_sha256": hashlib.sha256(module_path.read_bytes()).hexdigest(),
        }
    except Exception as error:  # Preserve a machine-readable failure artifact.
        report = {"schema_version": 1, "contract": "exciter-rack",
                  "status": "fail", "error": str(error)}

    if arguments.output is not None:
        arguments.output.parent.mkdir(parents=True, exist_ok=True)
        arguments.output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n",
                                    encoding="utf-8")
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0 if report["status"] == "pass" else 1


if __name__ == "__main__":
    raise SystemExit(main())
