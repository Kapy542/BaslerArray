#!/bin/bash

set -e

# Go to the directory containing this script
cd "$(dirname "$0")"

# Create Python environment
if [ ! -d ".venv" ]; then
	python3 -m venv .venv
	source .venv/bin/activate
	pip install numpy opencv-python ouster-sdk
fi

# Create build directory if it doesn't exist
cd BaslerArray
mkdir -p build
cd build

# Configure CMake
cmake ..

# Build
make -j$(nproc)

cp -r ../PTP_Recorder/configs ./PTP_Recorder/configs