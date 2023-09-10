#!/bin/bash -e

ROOT=$(cd "$(dirname "$0")/.."; pwd)
cd "$ROOT"

export PATH=$PATH:"/c/Program Files/CMake/bin"

if [ "$(uname)" == "Darwin" ]; then
  TOOLCHAIN="xcode"
elif [ "$(expr substr $(uname -s) 1 5)" == "Linux" ]; then
  TOOLCHAIN="ninja-gcc"
elif [ "$(expr substr $(uname -s) 1 10)" == "MINGW64_NT" ]; then
  TOOLCHAIN="vs"
fi

cmake --preset $TOOLCHAIN -D BUILD_EXTRAS=OFF -D JUCE_COPY_PLUGIN_AFTER_BUILD=ON
