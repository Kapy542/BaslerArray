#!/bin/bash

CONFIG="./BaslerArray/build/PTP_Recorder/configs/recorder_config.json"

# Read recording directory
OUTPUT_DIR=$(jq -r '.outputDirectory' "$CONFIG")

echo "Recording directory:"
echo "  $OUTPUT_DIR"

# Check directory
if [ ! -d "$OUTPUT_DIR" ]; then
    echo "ERROR: Recording directory does not exist!"
    exit 1
fi

# Show available space
df -h "$OUTPUT_DIR"

# Create session directory
SESSION=$(date +"%Y-%m-%d_%H-%M-%S")
RECORDING_DIR="$OUTPUT_DIR/$SESSION"

mkdir -p "$RECORDING_DIR"

echo
echo "Recording to:"
echo "  $RECORDING_DIR"
echo

# Start Ouster
source .venv/bin/activate
ouster-cli source 169.254.242.16 save "$RECORDING_DIR/lidar.pcap" &
OUSTER_PID=$!

# Start PTP Recorder
#cd ./BaslerArray/build/PTP_Recorder
#./PTP_Recorder &
#RECORDER_PID=$!

gnome-terminal \
    --title="BaslerRecorder" \
    --working-directory="/home/civit/Desktop/BaslerArray/BaslerArray/build/PTP_Recorder" \
    -- bash -c './PTP_Recorder; exec bash' &
RECORDER_PID=$!
    
echo "Recording started."
echo "Ouster PID:       $OUSTER_PID"
echo "PTP_Recorder PID: $RECORDER_PID"
echo
echo "Disk space will be shown every minute."
echo

# Monitor recording
while kill -0 "$RECORDER_PID" 2>/dev/null; do
    echo
    echo "[$(date '+%H:%M:%S')] Disk space:"
    df -h "$OUTPUT_DIR"

    sleep 60
done

echo
echo "PTP_Recorder stopped."

# Stop Ouster
kill -INT "$OUSTER_PID" 2>/dev/null
wait "$OUSTER_PID" 2>/dev/null

echo "Ouster recorder stopped."
echo "Recording finished."
