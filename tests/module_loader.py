"""Shared loader for Resonarium's native Python test modules."""

from __future__ import annotations

import importlib.util
import os
import sys
from pathlib import Path
from types import ModuleType


_DLL_HANDLES: list[object] = []


class HarnessError(RuntimeError):
    """A native test-module or adapter error."""


def resolve_module_path(path: Path) -> Path:
    path = path.expanduser().resolve()
    if path.is_file():
        return path
    if not path.is_dir():
        raise HarnessError(f"Python module does not exist: {path}")

    candidates: list[Path] = []
    for pattern in ("resonarium*.so", "resonarium*.pyd", "resonarium*.dylib"):
        candidates.extend(path.glob(pattern))
    candidates = sorted(set(candidates))
    if len(candidates) != 1:
        raise HarnessError(
            f"Expected exactly one Resonarium extension in {path}, found {len(candidates)}"
        )
    return candidates[0].resolve()


def load_resonarium(path: Path) -> tuple[ModuleType, Path]:
    module_path = resolve_module_path(path)
    if os.name == "nt" and hasattr(os, "add_dll_directory"):
        _DLL_HANDLES.append(os.add_dll_directory(str(module_path.parent)))

    module_name = module_path.name.split(".", 1)[0]
    sys.modules.pop(module_name, None)
    spec = importlib.util.spec_from_file_location(module_name, module_path)
    if spec is None or spec.loader is None:
        raise HarnessError(f"Cannot load extension module: {module_path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[module_name] = module
    spec.loader.exec_module(module)
    if not hasattr(module, "Resonarium"):
        raise HarnessError(f"Not a Resonarium Python extension: {module_path}")
    return module, module_path
