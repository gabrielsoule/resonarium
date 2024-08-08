# Resonarium
Resonarium is a MPE-compatible multipurpose waveguide synthesizer.

As of writing it's still in a half-baked developmental state, so I'll update this when it's ready.

# Installation and Usage

> [!NOTE]
> As of writing, this software has only been tested on Apple Silicon machines. I have not been able to test it on a Windows device. If anyone is able to successfully compile on Windows, or runs into any trouble while trying to do so, please let me know.

At present no pre-compiled binaries are available, so you'll have to build Resonarium from source. Fortunately, this is easy: the project is entirely self-contained, and bundles all its dependencies as submodules. Resonarium uses the JUCE CMake API, so the Projucer is not needed. 

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
This should install the VST3 plugin directly into your system's plugin folder, where host software should be able to scan and load it. If this doesn't work, you may have to manually install the plugin to your system's VST3 directory.

If you'd rather cut right to the chase and explore Resonarium without all the hassle of spinning up a DAW, the standalone configuration may be more suitable:
```
cmake --build build --target Resonarium_Standalone
```
When the standalone application is first run, you may have to configure your audio settings in the `Options` menu before any sound can be produced.

Enjoy!

> [!NOTE]
> Resonarium may take some time to download its submodules and compile from scratch for the first time. Since the repository is self-contained, all the dependencies (including JUCE) must be built from source. I recommend instructing CMake to use parallel processes when building, e.g. `cmake --build build --target <target> -j <numProcesses>`. Once the plugin approaches a release state, I'll take some time to optimize compilation efficiency. Util then, I appreciate your patience.

