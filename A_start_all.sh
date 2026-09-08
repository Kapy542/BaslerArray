#!/bin/bash

cd "$(dirname "$0")"

source .venv/bin/activate

sudo

# Enable PTP for the cameras
cd ./BaslerArray/build/PTP_Recorder
gnome-terminal --title="BaslerRecorder" -- bash -c '
    source ../../../.venv/bin/activate
    ./PTP_Recorder &
    PID=$!
    sleep 5
    kill -INT "$PID"
    wait "$PID"
'


sudo ptp4l -f /etc/linuxptp/my-ptp4l.conf -m &

gnome-terminal --title="PHC2SYS" -- bash -c \
    "sudo phc2sys -s /dev/ptp6 -c /dev/ptp2 -c /dev/ptp4 -c /dev/ptp5 -c /dev/ptp7 -c CLOCK_REALTIME -O 0 -m" &

wait
