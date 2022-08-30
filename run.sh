#!/bin/bash

set -eu

BASE_DIR="$( cd "$(dirname "$0")" ; pwd -P )"
FOXUS_DIR="$BASE_DIR/foxus"
GODOT_DIR="$BASE_DIR/godot"

host_system="$(uname -s)"

case "$host_system" in
    Linux)
        host_platform="x11"
        cpus="$(nproc)"
    ;;
    Darwin)
        host_platform="osx"
        cpus="$(sysctl -n hw.logicalcpu)"
    ;;
    *)
        echo "System $host_system is unsupported"
        exit 1
    ;;
esac

function find_godot() {
    for f in "godot.${host_platform}.tools.x86_64" "godot.${host_platform}.opt.tools.x86_64"
    do
        exec="$GODOT_DIR/bin/$f"
        if [ -x "$exec" ]
        then
            echo "$exec"
            break
        fi
    done
}

GODOT_EXECUTABLE="$(find_godot)"
echo "Running: $GODOT_EXECUTABLE --path $FOXUS_DIR $@"
exec $GODOT_EXECUTABLE --path $FOXUS_DIR $@
