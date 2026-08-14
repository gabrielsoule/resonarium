# Repository Guidelines

## Project Structure & Module Organization

Resonarium is a JUCE/CMake C++20 audio plugin. Project-owned code lives in `plugin/Source/`, with UI code in `plugin/Source/ui/`, shared helpers in `plugin/Source/util/`, DSP code in `plugin/Source/dsp/`, and Airwindows-derived processors in `plugin/Source/dsp/airwindows/`. Runtime assets are baked from `plugin/Resources/`, including presets, fonts, SVGs, logos, and `tooltips.json`. Vendored dependencies are under `modules/` and should be treated as third-party code unless a change explicitly targets a forked dependency. Python binding examples live in `python-examples/`; binding notes are in `python-bindings.md`.

## Build, Test, and Development Commands

- `git submodule update --init --recursive`: fetch JUCE, gin, chowdsp_utils, melatonin, and pybind11 dependencies.
- `cmake -B build -DCMAKE_BUILD_TYPE=Release`: configure a release build.
- `cmake --build build --target Resonarium_Instrument_Standalone`: build the standalone synth for quick local checks.
- `cmake --build build --target Resonarium_Instrument_VST3`: build the instrument plugin.
- `cmake --build build --target Resonarium_Effect_VST3`: build the audio-effect variant.
- `cmake --build build --target Resonarium_Python`: build `build/python_modules/resonarium.so` for Python rendering scripts.

## Coding Style & Naming Conventions

Use C++20 and the repository `.clang-format` file (`BasedOnStyle: Google`). Keep class and file naming consistent with existing JUCE code: PascalCase types such as `ResonatorVoice`, matching `.h`/`.cpp` pairs, and lower-case subdirectories. Host-visible parameter IDs are centralized in `plugin/Source/Parameters.cpp`; update that file rather than duplicating literals. Shared constants belong in `plugin/Source/defines.h`.

## Testing Guidelines

There is no first-party automated test suite at present. For code changes, build the narrowest affected target plus at least one plugin or standalone target. For DSP or parameter changes, smoke-test with the standalone app, a DAW/JUCE AudioPluginHost, or the Python target and examples. Document any host, OS, or plugin format used for verification in the PR.

## Commit & Pull Request Guidelines

Recent history uses short imperative or descriptive subjects, for example `Add Alt+scroll support for knobs in scrollable viewports` or `Fixed a bug where...`. Keep commits focused and mention user-visible behavior when relevant. Pull requests should describe the change, list tested targets and hosts, link related issues, and include screenshots or short clips for UI changes.

## Security & Configuration Tips

Do not commit signing certificates, notarization credentials, local build folders, or generated plugin bundles. CI release signing is handled through GitHub Actions secrets in `.github/workflows/build-sign-release.yml`.
