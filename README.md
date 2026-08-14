# Resonarium

![Image](https://ccrma.stanford.edu/~gns/resonarium.png)

Resonarium is a MPE-compatible physical modeling synthesizer, based on coupled string waveguide models. It is designed to encourage abstract sound design, exploration, and open-ended play. 

Many of the presets work best when used in conjunction with a MPE control device, such as a Ableton Push 3 or a Roli Seaboard. Such presets expect MPE control signals such as pressure and timbre, and are therefore musically more interesting when these expressive dimensions are active.

**Resonarium is feature-complete and no longer in active development**. I'm not planning to add any new features; the codebase is pretty clunky at this point and I'd rather spend my time dreaming up my next instrument (coming soon). However, I welcome PRs from the community.

Some audio samples can be found [here](https://ccrma.stanford.edu/~gns/resonarium_samples.html). They sound neat. Check them out!

Resonarium works well in Ableton and Bitwig. There are reported issues with Logic and FL Studio.

Resonarium's DSP kernels are optimized for efficient polyphonic performance.

While there isn't a user manual yet, most of the parameters have helpful pop-ups that manifest on hover. Have fun!

# Installation

> [!NOTE]
> Development and testing of this software is done on Apple Silicon machines running MacOS. However, it should be able to compile and run on Windows machines that have been configured with the appropriate development environment (CMake). There may be unforseen caveats and complications on Windows machines.

> [!CAUTION]
> The sounds produced by waveguide modeling can be unpredictable. Resonarium tries to keep the user safe by proactively terminating voices that produce dangerously loud samples, but the software is still in development and glitches may occur. __The maximum volume of your system _must_ be set to a safe level before using this software.__ We urge particular caution with headphones or large speakers. Be gentle with the gain knobs, and protect your ears!
>
> When Resonarium's overflow protection engages, you will hear a sharp pop followed by silence. If this happens repeatedly, you have either created an unstable preset, or the plugin's internal state is somehow corrupted. Deleting and re-instantiating the plugin instance may resolve this issue. If the problem can be reliably reproduced, please submit an issue to this repository.

## Precompiled Binaries
Pre-compiled binaries can be found in the Releases tab. The MacOS binaries are signed and notarized, and, as such, should work right out of the box. The Windows binaries are neither signed nor notarized; as such, Windows Defender can get a little nervious and falsely flag Resonarium precompiles as malicious. As such, building from source is recommended.

## Building From Source

Building from source is super easy! This project is entirely self-contained, and bundles all its dependencies, including JUCE, as submodules. Resonarium uses the JUCE CMake API, so the Projucer is not needed: all you need is CMake. 

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
> Resonarium is reasonably stable when run in a lightweight testing environment such as Juce's AudioPluginHost; however, smooth operation across most DAWs is not assured at this time. Ableton and Bitwig are known to be compatible; despite this, there are still some rare issues that pop up in specific DAWs. As such, the AudioPluginHost host is recommended for a cleaner demo experience. 
> To compile AudioPluginHost, do the following:
> ```cd
> cd /path/to/JUCE
> cmake . -B cmake-build -DJUCE_BUILD_EXAMPLES=ON -DJUCE_BUILD_EXTRAS=ON
> cmake --build cmake-build --target AudioPluginHost
> ```
> If Resonarium doesn't show up, ensure that the `.vst3` file is copied to your system's plugin directory, and that AudioPluginHost is configured to look for plugins in that directory. You can use the copy of JUCE bundled with this project, in the `modules` subdirectory, or clone a standalone copy of the JUCE repository elsewhere.
> 


> [!NOTE]
> Since this repository is self-contained, all its dependencies (including JUCE) must be downloaded and built from scratch. This may take some time. Many C++ build tools support parallel compilation, the use of which is recommended.
> 

## Licensing

Resonarium is licensed under a GPL license; therefore, any software that depends upon, modifies, or otherwise adapts Resonarium or any part(s) of Resonarium must also be licensed under a GPL license. If you are interested in using Resonarium or parts of Resonarium in a commercial capacity, please email me directly to discuss licensing terms. Unlicensed use of this work within a closed-source commercial application is prohibited.

## Dependencies

This project uses a modified fork of [Gin](https://github.com/FigBug/Gin), which includes several wonderful GUI widgets and backend utilities. Go check it out, it's great!
