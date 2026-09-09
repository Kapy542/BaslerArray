#!/bin/bash

OUSTER_IP="169.254.242.16"
INTERVAL=1
LOG="./logs/ouster_ptp_$(date +%Y%m%d_%H%M%S).csv"

echo "host_time,state,offset_ns,offset_ms,gm_present,gm_identity,clock_class,steps_removed,mean_path_delay_ns,sync_ok" > "$LOG"

echo "Logging Ouster PTP status to:"
echo "  $LOG"
echo "Press Ctrl+C to stop."
echo

while true; do
    host_time=$(date -u +"%Y-%m-%dT%H:%M:%S.%3NZ")

    json=$(curl -s --max-time 1 \
        "http://${OUSTER_IP}/api/v1/time/ptp")

    if [ -z "$json" ]; then
        echo "$host_time ERROR: Ouster unreachable"
        sleep "$INTERVAL"
        continue
    fi

    state=$(echo "$json" | jq -r '.port_data_set.port_state // "N/A"')
    offset=$(echo "$json" | jq -r '.current_data_set.offset_from_master // "N/A"')
    gm_present=$(echo "$json" | jq -r '.parent_data_set.gm_present // "N/A"')
    gm_identity=$(echo "$json" | jq -r '.parent_data_set.grandmaster_identity // "N/A"')
    clock_class=$(echo "$json" | jq -r '.parent_data_set.gm_clock_class // "N/A"')
    steps=$(echo "$json" | jq -r '.current_data_set.steps_removed // "N/A"')
    path_delay=$(echo "$json" | jq -r '.current_data_set.mean_path_delay // "N/A"')

    if [[ "$offset" =~ ^-?[0-9]+(\.[0-9]+)?$ ]]; then
        offset_ms=$(awk "BEGIN {printf \"%.3f\", $offset / 1000000}")
    else
        offset_ms="N/A"
    fi
    
    if awk "BEGIN { exit !($offset >= -1000000 && $offset <= 1000000) }"; then
        sync_ok="OK"
    else
        sync_ok="WAIT..."
    fi

    echo "$host_time,$state,$offset,$offset_ms,$gm_present,$gm_identity,$clock_class,$steps,$path_delay,$sync_ok" >> "$LOG"

    printf "%s | %-8s | offset %12s ns (%9s ms) | GM %s | delay %s ns | Sync %s\n" \
        "$host_time" \
        "$state" \
        "$offset" \
        "$offset_ms" \
        "$gm_identity" \
        "$path_delay" \
        "$sync_ok"

    sleep "$INTERVAL"
done
