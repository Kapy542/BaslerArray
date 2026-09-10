#!/bin/bash

OUSTER_IP="169.254.242.16"
DEST_IP="169.254.64.52"

echo "Setting Ouster UDP destination to $DEST_IP..."

curl -s -X POST \
    "http://${OUSTER_IP}/api/v1/sensor/config" \
    -H "Content-Type: application/json" \
    --data "{\"udp_dest\":\"${DEST_IP}\"}"

echo
echo "Current destination:"
curl -s "http://${OUSTER_IP}/api/v1/sensor/config" | jq -r '.udp_dest'
