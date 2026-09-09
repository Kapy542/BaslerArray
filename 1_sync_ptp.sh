#!/bin/bash

cd "$(dirname "$0")"

sudo

# Enable PTP for the cameras
cd ./BaslerArray/build/PTP_Recorder
gnome-terminal --title="BaslerInitPTP" -- bash -c '
    ./PTP_Recorder &
    PID=$!
    sleep 4
    kill -INT "$PID"
    wait "$PID"
'

#gnome-terminal --title="BaslerInitPTP" -- bash -c './PTP_Recorder'

#sudo ptp4l -f /etc/linuxptp/my-ptp4l.conf -m &
gnome-terminal --title="PTP4L" -- bash -c \
    "sudo ptp4l -f /etc/linuxptp/my-ptp4l.conf -m" &
wait

gnome-terminal --title="PHC2SYS" -- bash -c \
    "sudo phc2sys -s /dev/ptp0 -c /dev/ptp1 -c /dev/ptp4 -c /dev/ptp5 -c /dev/ptp6 -c /dev/ptp7 -c CLOCK_REALTIME -O 0 -m" &
wait

cd ../../../
./poll_ouster.sh
