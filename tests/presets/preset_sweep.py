#!/usr/bin/env python3
"""Load, round-trip, and smoke-render every bundled factory preset."""

from __future__ import annotations

import argparse
import gc
import hashlib
import json
import math
import os
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
from pathlib import Path
from types import ModuleType
from typing import Any, Sequence

import numpy as np

if not __package__:
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tests.module_loader import HarnessError, load_resonarium


SAMPLE_RATE = 48_000
BLOCK_SIZE = 128
RENDER_SAMPLES = 24_000
NOTE_OFF_SAMPLE = 12_000
MPE_MEMBER_CHANNEL = 1
PEAK_LIMIT = 100.0
DEFAULT_PRESET_DIR = Path(__file__).resolve().parents[2] / "plugin/Resources/Presets"


def _state_signature(xml: str) -> dict[str, int]:
    root = ET.fromstring(xml)
    state = root.find("state")
    if state is None:
        return {"mod_items": 0, "mseg_nodes": 0, "sample_paths": 0}
    return {
        "mod_items": sum(1 for _ in state.iter("MODITEM")),
        "mseg_nodes": sum(1 for element in state if element.tag.startswith("MSEG")),
        "sample_paths": sum(1 for _ in state.iter("samplePath")),
    }


def _render_probe(synth: Any) -> tuple[float, float, float]:
    block_count = math.ceil(RENDER_SAMPLES / BLOCK_SIZE)
    output = synth.create_multi_block(block_count)
    for block_index in range(block_count):
        first_sample = block_index * BLOCK_SIZE
        if block_index == 0:
            synth.play_note(MPE_MEMBER_CHANNEL, 60, 100, 0)
        if first_sample <= NOTE_OFF_SAMPLE < first_sample + BLOCK_SIZE:
            synth.release_note(
                MPE_MEMBER_CHANNEL, 60, 0, NOTE_OFF_SAMPLE - first_sample
            )
        synth.process_multi_block(output, block_index, 1)

    audio = np.asarray(output, dtype=np.float32)[:, :RENDER_SAMPLES]
    if audio.shape != (2, RENDER_SAMPLES):
        raise HarnessError(f"native adapter returned audio with shape {audio.shape}")
    if not np.all(np.isfinite(audio)):
        return math.inf, math.inf, math.inf
    peak = float(np.max(np.abs(audio), initial=0.0))
    values = np.asarray(audio, dtype=np.float64)
    rms = float(np.sqrt(np.mean(values * values))) if values.size else 0.0
    duration = RENDER_SAMPLES / SAMPLE_RATE
    return peak, rms, duration


def _check_one_preset(module: ModuleType, path: Path) -> dict[str, Any]:
    failures: list[str] = []
    warnings: list[str] = []
    xml = path.read_text(encoding="utf-8")
    try:
        root = ET.fromstring(xml)
    except ET.ParseError as error:
        return {
            "preset": path.stem,
            "status": "fail",
            "failures": [str(error)],
            "warnings": [],
        }

    if root.get("name") != path.stem:
        failures.append(f"embedded name {root.get('name')!r} differs from file name")

    synth = module.Resonarium(float(SAMPLE_RATE), BLOCK_SIZE)
    required = ("set_current_program", "get_state_xml", "get_program_names")
    missing = [name for name in required if not hasattr(synth, name)]
    if missing:
        raise HarnessError(f"native adapter lacks preset/state APIs: {', '.join(missing)}")

    parameters = {parameter.id: parameter for parameter in synth.get_all_params()}
    expected: dict[str, float] = {}
    for element in root.findall("param"):
        parameter_id = element.get("uid", "")
        value = float(element.get("val", "nan"))
        expected[parameter_id] = value
        if parameter_id not in parameters:
            failures.append(f"unknown parameter {parameter_id!r}")
        elif (
            value < float(parameters[parameter_id].min) - 1.0e-4
            or value > float(parameters[parameter_id].max) + 1.0e-4
        ):
            warnings.append(f"{parameter_id!r} is marginally outside its declared range")

    if path.stem not in set(synth.get_program_names()):
        failures.append("preset is missing from the processor's bundled program list")
    else:
        synth.set_current_program(path.stem)

    loaded = {parameter.id: float(parameter.value) for parameter in synth.get_all_params()}
    for parameter_id, expected_value in expected.items():
        if parameter_id in loaded and not math.isclose(
            loaded[parameter_id], expected_value, rel_tol=2.0e-5, abs_tol=2.0e-5
        ):
            failures.append(
                f"{parameter_id!r} loaded as {loaded[parameter_id]:.8g}, "
                f"expected {expected_value:.8g}"
            )

    if _state_signature(xml) != _state_signature(synth.get_state_xml()):
        failures.append("modulation, MSEG, or sample-path state changed during round-trip")

    peak, signal_rms, audio_seconds = _render_probe(synth)
    if not math.isfinite(peak) or not math.isfinite(signal_rms):
        failures.append("render contains NaN or Inf")
    elif peak > PEAK_LIMIT:
        failures.append(f"render peak {peak:.6g} exceeds safety limit {PEAK_LIMIT:g}")
    if signal_rms < 1.0e-7:
        warnings.append("render was silent for the short C4 probe")

    del synth
    gc.collect()
    return {
        "preset": path.stem,
        "status": "fail" if failures else "pass",
        "failures": failures,
        "warnings": warnings,
        "parameter_count": len(expected),
        "peak": peak,
        "rms": signal_rms,
        "audio_seconds": audio_seconds,
    }


def _write_json(path: Path, payload: dict[str, Any]) -> None:
    path = path.expanduser().resolve()
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(f".{path.name}.{os.getpid()}.tmp")
    try:
        temporary.write_text(
            json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )
        os.replace(temporary, path)
    finally:
        if temporary.exists():
            temporary.unlink()


def _output_path(path: Path | None) -> Path | None:
    if path is None:
        return None
    path = path.expanduser().resolve()
    return path / "preset-sweep.json" if path.suffix.lower() != ".json" else path


def _worker(module_path: Path, preset: Path, result_path: Path) -> int:
    module, _ = load_resonarium(module_path)
    result = _check_one_preset(module, preset.expanduser().resolve())
    _write_json(result_path, result)
    return 1 if result["status"] == "fail" else 0


def run(module_path: Path, preset_dir: Path) -> dict[str, Any]:
    _, resolved_module = load_resonarium(module_path)
    preset_dir = preset_dir.expanduser().resolve()
    presets = sorted(preset_dir.glob("*.xml"))
    if not presets:
        raise HarnessError(f"No XML presets found in {preset_dir}")

    results: list[dict[str, Any]] = []
    with tempfile.TemporaryDirectory(prefix="resonarium-preset-workers-") as directory:
        worker_directory = Path(directory)
        for index, preset in enumerate(presets):
            result_path = worker_directory / f"{index}.json"
            completed = subprocess.run(
                [
                    sys.executable,
                    str(Path(__file__).resolve()),
                    "--worker",
                    "--module",
                    str(resolved_module),
                    "--preset",
                    str(preset),
                    "--result",
                    str(result_path),
                ],
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
            )
            if result_path.is_file():
                result = json.loads(result_path.read_text(encoding="utf-8"))
            else:
                result = {
                    "preset": preset.stem,
                    "status": "fail",
                    "failures": [],
                    "warnings": [],
                    "parameter_count": 0,
                    "peak": None,
                    "rms": None,
                    "audio_seconds": None,
                }
            if completed.returncode != 0:
                reason = (
                    f"worker terminated by signal {-completed.returncode}"
                    if completed.returncode < 0
                    else f"worker exited with code {completed.returncode}"
                )
                diagnostics = (completed.stderr or completed.stdout).strip().splitlines()
                if diagnostics:
                    reason += f": {diagnostics[-1]}"
                if reason not in result["failures"]:
                    result["failures"].append(reason)
                result["status"] = "fail"

            print(
                f"{result['status'].upper()} {preset.stem:<36} "
                f"params={result.get('parameter_count', 0):3d} "
                f"peak={result.get('peak')!s}"
            )
            for failure in result["failures"]:
                print(f"     - {failure}")
            for warning in result["warnings"]:
                print(f"     - warning: {warning}")
            results.append(result)

    passed = sum(result["status"] == "pass" for result in results)
    return {
        "schema_version": 1,
        "contract": "factory-preset-sweep",
        "status": "pass" if passed == len(presets) else "fail",
        "module": {
            "path": str(resolved_module),
            "sha256": hashlib.sha256(resolved_module.read_bytes()).hexdigest(),
        },
        "preset_directory": str(preset_dir),
        "preset_count": len(presets),
        "passed_count": passed,
        "results": results,
    }


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--module", type=Path, required=True)
    parser.add_argument("--preset-dir", type=Path, default=DEFAULT_PRESET_DIR)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--worker", action="store_true", help=argparse.SUPPRESS)
    parser.add_argument("--preset", type=Path, help=argparse.SUPPRESS)
    parser.add_argument("--result", type=Path, help=argparse.SUPPRESS)
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    arguments = build_parser().parse_args(argv)
    try:
        if arguments.worker:
            if arguments.preset is None or arguments.result is None:
                raise HarnessError("worker requires --preset and --result")
            return _worker(arguments.module, arguments.preset, arguments.result)
        report = run(arguments.module, arguments.preset_dir)
        output = _output_path(arguments.output)
        if output is not None:
            _write_json(output, report)
            print(f"Report: {output}")
        print(json.dumps(report, sort_keys=True))
        return 0 if report["status"] == "pass" else 1
    except (HarnessError, RuntimeError, ValueError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
