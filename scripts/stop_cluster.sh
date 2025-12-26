#!/bin/bash

PID_FILE="/tmp/kvstore-cluster.pids"

if [ ! -f "$PID_FILE" ]; then
    echo "No cluster PID file found. Trying pkill..."
    pkill -f kvstore-server 2>/dev/null || true
    exit 0
fi

PIDS=$(cat "$PID_FILE")
echo "Stopping cluster (PIDs: $PIDS)..."

for pid in $PIDS; do
    kill "$pid" 2>/dev/null || true
done

sleep 1

for pid in $PIDS; do
    kill -0 "$pid" 2>/dev/null && kill -9 "$pid" 2>/dev/null || true
done

rm -f "$PID_FILE"
echo "Cluster stopped."
