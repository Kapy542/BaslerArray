#!/bin/bash

set -e

# ==============================
# Configuration
# ==============================

PTP_CONFIG="/path/to/config/ptp4l.conf"

OUSTER_IFACE="enp1s0"
CAM1_IFACE="enp2s0"
CAM2_IFACE="enp3s0"
CAM3_IFACE="enp4s0"
CAM4_IFACE="enp5s0"

OUSTER_SCRIPT="/path/to/ouster/ouster_capture.py"
PTP_RECORDER="/path/to/BaslerArray/PTP_Recorder"

RECORD_DIR="/path/to/recordings"

# ==============================
# Cleanup
# ==============================

cleanup()
{
    echo ""
    echo "Stopping capture..."

    if [[ -n "$OUSTER_PID" ]]; then
        kill "$OUSTER_PID" 2>/dev/null || true
    fi

    if [[ -n "$RECORDER_PID" ]]; then
        kill "$RECORDER_PID" 2>/dev/null || true
    fi

    if [[ -n "$PHC2SYS_PID" ]]; then
        kill "$PHC2SYS_PID" 2>/dev/null || true
    fi

    if [[ -n "$PTP4L_PID" ]]; then
        kill "$PTP4L_PID" 2>/dev/null || true
    fi

    echo "Capture stopped."
}

trap cleanup SIGINT SIGTERM EXIT


# ==============================
# Start LinuxPTP
# ==============================

echo "Starting ptp4l..."

sudo ptp4l \
    -f "$PTP_CONFIG" \
    -i "$OUSTER_IFACE" \
    -i "$CAM1_IFACE" \
    -i "$CAM2_IFACE" \
    -i "$CAM3_IFACE" \
    -i "$CAM4_IFACE" \
    -m &

PTP4L_PID=$!

sleep 2


echo "Starting phc2sys..."

sudo phc2sys -a -r -m &

PHC2SYS_PID=$!

sleep 2


# ==============================
# TODO:
# Wait for PTP synchronization
# ==============================

echo "Waiting for PTP synchronization..."

sleep 5


# ==============================
# Start Ouster
# ==============================

echo "Starting Ouster capture..."

python3 "$OUSTER_SCRIPT" "$RECORD_DIR" &

OUSTER_PID=$!


# ==============================
# Start Basler recorder
# ==============================

echo "Starting Basler recorder..."

"$PTP_RECORDER" &

RECORDER_PID=$!


echo ""
echo "================================="
echo " Capture started"
echo "================================="
echo ""
echo "Press Ctrl+C to stop."
echo ""

wait

# ==============================
# Process first and last frame to check functionality
# ==============================
