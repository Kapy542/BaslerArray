#!/bin/bash

cd "$(dirname "$0")"

# source .venv/bin/activate

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

#./PTP_Recorder &
#RECORDER_PID=$!

gnome-terminal --title="Basler Recorder" -- bash -c \
    "./PTP_Recorder"
 
echo "PTP_Recorder started"

# Monitor recording
while true; do
    echo
    echo "[$(date '+%H:%M:%S')] Disk space:"
    df -h "$OUTPUT_DIR"

    sleep 60
done

echo
echo "PTP_Recorder stopped."
