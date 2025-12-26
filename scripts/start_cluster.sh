#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/build"
CONFIG_DIR="$PROJECT_DIR/configs"

if [ ! -f "$BUILD_DIR/kvstore-server" ]; then
    echo "Error: kvstore-server not found. Run cmake --build build first."
    exit 1
fi

mkdir -p "$PROJECT_DIR/data/node1" "$PROJECT_DIR/data/node2" "$PROJECT_DIR/data/node3"

echo "Starting 3-node cluster..."

$BUILD_DIR/kvstore-server --config $CONFIG_DIR/node1.yaml &
PID1=$!

$BUILD_DIR/kvstore-server --config $CONFIG_DIR/node2.yaml &
PID2=$!

$BUILD_DIR/kvstore-server --config $CONFIG_DIR/node3.yaml &
PID3=$!

echo "Cluster started:"
echo "  Primary (node1): PID=$PID1, port=7001"
echo "  Replica (node2): PID=$PID2, port=7002"
echo "  Replica (node3): PID=$PID3, port=7003"
echo "$PID1 $PID2 $PID3" > /tmp/kvstore-cluster.pids
echo "PIDs saved to /tmp/kvstore-cluster.pids"

wait
