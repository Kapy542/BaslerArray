#!/bin/bash

set -e

# Go to project root
cd "$(dirname "$0")"

# Activate Python environment
source .venv/bin/activate

# Configuration
CONFIG="./BaslerArray/build/PTP_Recorder/configs/recorder_config.json"

# Read recording directory
OUTPUT_DIR=$(jq -r '.outputDirectory' "$CONFIG")

echo "Recording directory:"
echo "  $OUTPUT_DIR"
echo

# Find most recent recording
RECORDING_DIR=$(find "$OUTPUT_DIR" -mindepth 1 -maxdepth 1 -type d \
    -printf '%T@ %p\n' | sort -nr | head -n 1 | cut -d' ' -f2-)

if [ -z "$RECORDING_DIR" ]; then
    echo "ERROR: No recordings found in $OUTPUT_DIR"
    exit 1
fi

# Recording name = directory name
TAKE_NAME=$(basename "$RECORDING_DIR")

echo "Most recent recording:"
echo "  $TAKE_NAME"
echo "  $RECORDING_DIR"
echo

# Run validation
echo "=== Validating recording ==="
python ./Validation/validate_recording.py "$RECORDING_DIR" "$TAKE_NAME"

echo
echo "=== Visualizing frames ==="
python ./Validation/vis_frames.py "$RECORDING_DIR" "$TAKE_NAME"

echo
echo "Validation finished."