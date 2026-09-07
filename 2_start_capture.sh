#!/bin/bash

cd "$(dirname "$0")"

source .venv/bin/activate

cd ./BaslerArray/build/PTP_Recorder

# Read recording directory
CONFIG="./configs/recorder_config.json"
OUTPUT_DIR=$(jq -r '.outputDirectory' "$CONFIG")

echo "Recording directory:"
echo "  $OUTPUT_DIR"

# Check directory
if [ ! -d "$OUTPUT_DIR" ]; then
    mkdir -p "$OUTPUT_DIR"
fi

./BaslerArray/build/PTP_Recorder/PTP_Recorder &
RECORDER_PID=$!

echo "PTP_Recorder started (PID $RECORDER_PID)"

# Monitor recording
while kill -0 "$RECORDER_PID" 2>/dev/null; do
    echo
    echo "[$(date '+%H:%M:%S')] Disk space:"
    df -h "$OUTPUT_DIR"

    sleep 60
done

echo
echo "PTP_Recorder stopped."
