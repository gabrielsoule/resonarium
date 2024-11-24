# Resonarium

![Image](https://github.com/user-attachments/assets/8117c58e-8248-4ac3-95e8-453d381b88af)

Resonarium is a MPE-compatible multipurpose waveguide synthesizer.

As of writing it's still in a half-baked developmental state, so I'll update this when it's ready.

# Installation and Usage

> [!NOTE]
> This software has only been tested on Apple Silicon machines running MacOS. It should be able to compile on Windows machines that have been configured with the appropriate development environment (CMake). I don't have a Windows computer, so this has not been verified. The instructions below only apply to MacOS devices.
>

## Precompiled Binaries
Pre-compiled MacOS binaries are uploaded irregularly to GitHub as releases. If you don't have CMake installed, these should suffice. Simply drag and drop the VST3 file into your computer's VST3 directory. However, compiling from source (below) is strongly recommended. MacOS makes running unsigned software difficult. This is particularly frustrating when DAWs try (and fail) to initialize unsigned plugins. 

## Building From Source

Building from source is easy! This project is entirely self-contained, and bundles all its dependencies, including JUCE, as submodules. Resonarium uses the JUCE CMake API, so the Projucer is not needed: all you need is CMake. 

You'll first want to clone the repository to a location of your choice. Once this is done, you'll want to populate the submodules:
```
cd resonarium
git submodule update --init --rescursive
```
This will download and configure all the project dependencies, including JUCE, into your local repository. Once this done, configure the CMake project and create your build directory:
```
cmake -B build -DCMAKE_BUILD_TYPE=Release
```
Then, you can build a target, e.g. the VST3 target, like so:
```
cmake --build build --target Resonarium_VST3
```
This should install the VST3 plugin directly into your system's plugin folder, where host software should be able to scan and load it. If this doesn't work, you may have to manually install the plugin from the build directory into your system's VST3 directory.

If you'd rather cut right to the chase and explore Resonarium without all the hassle of spinning up a DAW, the standalone configuration may be more suitable:
```
cmake --build build --target Resonarium_Standalone
```
When the standalone application is first run, you may need to configure your audio settings in the `Options` menu before any sound can be produced. You'll also need a MIDI input source, since the plugin doesn't include a virtual on-screen keyboard. 

Enjoy!

> [!NOTE]
> Resonarium may take some time to download its submodules and compile from scratch for the first time. Since the repository is self-contained, all the dependencies (including JUCE) must be built from source. I recommend instructing CMake to use parallel processes when building, e.g. `cmake --build build --target <target> -j <numProcesses>`. Once the plugin approaches a release state, I'll take some time to optimize compilation efficiency. Util then, I appreciate your patience.

