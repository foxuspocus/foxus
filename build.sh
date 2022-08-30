#!/bin/bash

set -eu

BASE_DIR="$( cd "$(dirname "$0")" ; pwd -P )"
EIFFELCAM_DIR="$BASE_DIR/gd_eiffelcam"
EIFFELCAMERA_PLUGIN_DIR="$BASE_DIR/foxus/android/plugins/EiffelCamera"

build_mode="release"

trap "exit 1" 10
PROC="$$"

function fatal() {
    echo "$@" >&2
    kill -10 $PROC
}

ndk_version_prefix='23\.2\.'

# Find the Android SDK
function find_sdk() {
    for f in "$HOME/Android/Sdk" "$HOME/Library/Android/sdk"
    do
        if [ -d "$f" ]
        then
            echo "$f"
            return
        fi
    done
    fatal "Unable to find Android SDK, please install it"
}

if [ "${ANDROID_SDK_ROOT:-}" = "" ]
then
    export ANDROID_SDK_ROOT="$(find_sdk)"
fi

# Find the latest NDK installed
function find_ndk() {
    ndk_version=$(ls -1 "$ANDROID_SDK_ROOT/ndk" | grep "^$ndk_version_prefix" | sort | tail -n 1)
    ndk_path="$ANDROID_SDK_ROOT/ndk/$ndk_version"
    if [ ! -d "$ndk_path" ]
    then
        fatal "Unable to find Android NDK, please install it"
    fi
    echo "$ANDROID_SDK_ROOT/ndk/$ndk_version"
}

if [ "${ANDROID_NDK_ROOT:-}" = "" ]
then
    export ANDROID_NDK_ROOT="$(find_ndk)"
fi

echo "Android SDK: $ANDROID_SDK_ROOT"
echo "Android NDK: $ANDROID_NDK_ROOT"

host_system="$(uname -s)"

case "$host_system" in
    Linux)
        host_platform="linux"
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

host_arch="$(uname -m)"

profiler_option=""

while [ "${1:-}" != "" ]
do
    case "$1" in
    --debug)
        build_mode="debug"
        ;;
    --profiler)
        profiler_option="perfetto=true"
        ;;
    *)
        echo "Usage: $0 [--debug]"
        exit 1
        ;;
    esac
    shift
done


cd $EIFFELCAM_DIR
# Build EiffelCamera
scons -j$cpus platform=android target=${build_mode} $profiler_option

scons -j$cpus platform=${host_platform} target=${build_mode} arch=${host_arch} $profiler_option

cd $EIFFELCAMERA_PLUGIN_DIR

./gradlew assembleDebug
./gradlew assembleRelease
