#!/bin/bash
# Build script for llama-box + agent.cpp combined project

set -e

echo "=========================================="
echo "Building llama-box-agent combined project"
echo "=========================================="

cd ~/Desktop/llama-box-agentw

# Create build directory
mkdir -p build
cd build

# Check for -sbot flag (robot + camera)
SBOT=OFF
if [ "$1" == "-sbot" ]; then
    echo "Building with robot + camera (-sbot)..."
    SBOT=ON
    shift
fi

# Configure with CMake
echo "Configuring with CMake..."

CMAKE_OPTS="-DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=17 -DWITH_WHISPER=ON"

if [ "$SBOT" == "ON" ]; then
    CMAKE_OPTS="$CMAKE_OPTS -DWITH_ROBOT=ON -DWITH_IPCAM=ON"
fi

# Check for TERMUX build flag
if [ "$1" == "--termux" ]; then
    echo "Building for Termux..."
    CMAKE_OPTS="$CMAKE_OPTS -DAUDIO_TERMUX=ON"
    shift
elif [ "$1" == "--linux" ]; then
    echo "Building for Linux..."
    shift
fi

cmake .. $CMAKE_OPTS -G "Unix Makefiles"

# Build
echo "Building..."
cmake --build . --config Release -j $(nproc)

echo "=========================================="
echo "Build complete!"
echo "Binary location: build/llama-box-agent"
echo ""
echo "Usage:"
echo "  ./llama-box-agent [options]"
echo ""
echo "Robot options (when built with -sbot):"
echo "  -sbot            Enable robot + camera mode"
echo "  -sbot-port <p>   Serial port (default: /dev/ttyUSB0)"
echo "  -sbot-baud <n>   Baud rate (default: 115200)"
echo "  -sbot-cam <ip>   IP Webcam address"
echo "  -sbot-cam-port <n> IP Webcam port (default: 8080)"
echo ""
echo "Voice options:"
echo "  -whspr             Enable voice input"
echo "  -wake <phrase>     Set wake word (e.g., 'hey llama')"
echo "  -whspr-model <p>   Set custom model path"
echo "  -whspr-tmp <dir>   Set temp directory (default: /tmp)"
echo "=========================================="
