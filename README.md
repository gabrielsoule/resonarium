# Resonarium

![Image](https://private-user-images.githubusercontent.com/5464864/453700687-20c6069a-a1b5-4d5f-9105-db8428d7afe2.png?jwt=eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJnaXRodWIuY29tIiwiYXVkIjoicmF3LmdpdGh1YnVzZXJjb250ZW50LmNvbSIsImtleSI6ImtleTUiLCJleHAiOjE3NDk2MDQ5MzQsIm5iZiI6MTc0OTYwNDYzNCwicGF0aCI6Ii81NDY0ODY0LzQ1MzcwMDY4Ny0yMGM2MDY5YS1hMWI1LTRkNWYtOTEwNS1kYjg0MjhkN2FmZTIucG5nP1gtQW16LUFsZ29yaXRobT1BV1M0LUhNQUMtU0hBMjU2JlgtQW16LUNyZWRlbnRpYWw9QUtJQVZDT0RZTFNBNTNQUUs0WkElMkYyMDI1MDYxMSUyRnVzLWVhc3QtMSUyRnMzJTJGYXdzNF9yZXF1ZXN0JlgtQW16LURhdGU9MjAyNTA2MTFUMDExNzE0WiZYLUFtei1FeHBpcmVzPTMwMCZYLUFtei1TaWduYXR1cmU9N2U5MGE3ZDkzMTIxZmFjNmQxZDYwZjg0NTgyODZjOGM3ODkwZWUxYWFhOGVkZWUwYzcwOWIzNTA1YzViYjlmOCZYLUFtei1TaWduZWRIZWFkZXJzPWhvc3QifQ.bGbHaK361AFgPPhhRxdy3aFFIoUHjMpYomV8bCEtIJk)

Resonarium is a MPE-compatible expressive physical modeling synthesizer. It is designed to encourage abstract sound design, exploration, and open-ended play. 

Many of the presets work best when used in conjunction with a MPE control device, such as a Ableton Push 3 or a Roli Seaboard.

**This software is still in development.** The primary focus at present is stability and performance. Use at your own risk, and expect bugs or crashes. You'll probably need a beefy computer to run complex presets in the plugin's current state; performance optimizations such as SIMD processing are in the pipeline, but not ready to deploy just yet.

But hey, it's free and open source, so no whining allowed. I welcome contributions. Want to add/change/improve something? Clone the repo, make some changes, and submit a pull request!

Some audio samples can be found [here](https://ccrma.stanford.edu/~gns/resonarium_samples.html). They sound neat. Check them out!

Resonarium works well in Ableton and Bitwig. There are reported issues with Logic and FL Studio. 

While there isn't a user manual yet, most of the parameters have helpful pop-ups that manifest on hover. Have fun!

# Installation

> [!NOTE]
> Development and testing of this software is done on Apple Silicon machines running MacOS. However, it should be able to compile and run on Windows machines that have been configured with the appropriate development environment (CMake). There may be unforseen caveats and complications on Windows machines.

> [!CAUTION]
> The sounds produced by waveguide modeling can be unpredictable. Resonarium tries to keep the user safe by proactively terminating voices that produce dangerously loud samples, but the software is still in development and glitches may occur. __The maximum volume of your system _must_ be set to a safe level before using this software.__ We urge particular caution with headphones or large speakers. Be gentle with the gain knobs, and protect your ears!
>
> When Resonarium's overflow protection engages, you will hear a sharp pop followed by silence. If this happens repeatedly, you have either created an unstable preset, or the plugin's internal state is somehow corrupted. Deleting and re-instantiating the plugin instance may resolve this issue. If the problem can be reliably reproduced, please submit an issue to this repository.

## Precompiled Binaries
Pre-compiled binaries can be found in the Releases tab. The MacOS binaries are signed and notarized, and, as such, should work right out of the box. The Windows binaries are not signed nor notarized; as such, Windows Defener can get a little jumpy and falsely flag Resonarium precompiles as malicious. As such, building from source is recommended.

## Building From Source

Building from source is easy! This project is entirely self-contained, and bundles all its dependencies, including JUCE, as submodules. Resonarium uses the JUCE CMake API, so the Projucer is not needed: all you need is CMake. 

You'll first want to clone the repository to a location of your choice. Once this is done, you'll want to populate the submodules:
```
cd resonarium
git submodule update --init --recursive
```
This will download and configure all the project dependencies, including JUCE, into your local repository. Once this done, configure the CMake project and create your build directory:
```
cmake -B build -DCMAKE_BUILD_TYPE=Release
```
Then, you can build a target, e.g. the VST3 target, like so:
```
cmake --build build --target Resonarium_Instrument_VST3
```
This should install the VST3 plugin directly into your system's plugin folder, where host software should be able to scan and load it. If this doesn't work, you may have to manually install the plugin from the build directory into your system's VST3 directory.

If you'd rather cut right to the chase and explore Resonarium without all the hassle of spinning up a DAW, the standalone configuration may be more suitable:
```
cmake --build build --target Resonarium_Instrument_Standalone
```
When the standalone application is first run, you may need to configure your audio settings in the `Options` menu before any sound can be produced. You'll also need a MIDI input source, since the plugin doesn't include a virtual on-screen keyboard. 

Resonarium can also be used as an audio effect with live external audio input, or as part of an effect chain in a DAW. Many DAWs require that plugins be _either_ instruments or effects; therefore, to compile the effect version of the software, replace "Instrument" with "Effect" in your build target (e.g `cmake --build build --target Resonarium_Effect_VST3`). The _effect_ version of the plugin is distinguished by a handsome green color scheme. Both versions of the software can be used in parallel, in the same hosted environment.

> [!NOTE]
> Resonarium is reasonably stable when run in a lightweight testing environment such as Juce's AudioPluginHost; however, smooth operation across most DAWs is not assured at this time. Ableton and Bitwig are known to be compatible; despite this, there are still some rare issues that pop up in specific DAWs. The AudioPluginHost host is recommended for a cleaner demo experience. 
> To compile AudioPluginHost, do the following:
> ```cd
> cd /path/to/JUCE
> cmake . -B cmake-build -DJUCE_BUILD_EXAMPLES=ON -DJUCE_BUILD_EXTRAS=ON
> cmake --build cmake-build --target AudioPluginHost
> ```
> If Resonarium doesn't show up, ensure that the `.vst3` file is copied to your system's plugin directory, and that AudioPluginHost is configured to look for plugins in that directory. You can use the copy of JUCE bundled with this project, in the `modules` subdirectory, or clone a standalone copy of the JUCE repository elsewhere.

## Licensing

Resonarium is licensed under a GPL license; therefore, any software that depends upon, modifies, or otherwise adapts Resonarium or any part(s) of Resonarium must also be licensed under a GPL license. If you are interested in using Resonarium or parts of Resonarium in a commercial capacity, please email me directly to discuss licensing terms. Unlicensed use of this work within a closed-source commercial application is prohibited.

## Dependencies

This project uses a modified fork of [Gin](https://github.com/FigBug/Gin), which includes several wonderful GUI widgets and backend utilities. Go check it out, it's great!

This project uses a modified fork of [chowdsp-utils](https://github.com/Chowdhury-DSP/chowdsp_utils); in particular, we adapt and extend the state variable filter implementation. Go check it out, it's (also) great! 

> [!NOTE]
> Since this repository is self-contained, all its dependencies (including JUCE) must be downloaded and built from scratch. This may take some time. Many C++ build tools support parallel compilation, the use of which is recommended.
> 
