#!/bin/bash

set -eu

BASE_DIR="$( cd "$(dirname "$0")" ; pwd -P )"
GODOT_DIR="$BASE_DIR/godot"
GODOT_CPP_DIR="$BASE_DIR/godot-cpp"

FOXUS_DIR="$BASE_DIR/foxus"
ANDROID_BUILD_DIR="$FOXUS_DIR/android/build"

build_mode_godot="release_debug"
build_mode_godot_cpp="release"

trap "exit 1" 10
PROC="$$"

function fatal() {
    echo "$@" >&2
    kill -10 $PROC
}

ndk_version_prefix='23\.2\.'

host_arch="$(uname -m)"

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
        host_platform="x11"
        host_platform_godot_cpp="linux"
        cpus="$(nproc)"
    ;;
    Darwin)
        host_platform="osx"
        host_platform_godot_cpp="osx"
        cpus="$(sysctl -n hw.logicalcpu)"
    ;;
    *)
        echo "System $host_system is unsupported"
        exit 1
    ;;
esac

profiler_option=""
sync_opengl_traces=""

while [ "${1:-}" != "" ]
do
    case "$1" in
    --debug)
        build_mode_godot="debug"
        build_mode_godot_cpp="debug"
        ;;
    --profiler)
        profiler_option="perfetto=true"
        ;;
    --sync-opengl-traces)
        sync_opengl_traces="perfetto_sync_opengl=true"
        ;;
    *)
        echo "Usage: $0 [--debug] [--profiler] [--sync-opengl-traces]"
        exit 1
        ;;
    esac
    shift
done

function find_godot() {
    for f in "godot.${host_platform}.tools.x86_64" "godot.${host_platform}.opt.tools.x86_64"
    do
        exec="$GODOT_DIR/bin/$f"
        if [ -x "$exec" ]
        then
            echo "$exec"
        fi
    done
}

cd $GODOT_DIR
# Build Godot Engine Editor
scons -j$cpus platform=${host_platform} target=${build_mode_godot} arch=${host_arch} $profiler_option $sync_opengl_traces

# Build Godot Engine for Android
scons -j$cpus platform=android android_arch=arm64v8 target=${build_mode_godot} $profiler_option $sync_opengl_traces
cd $GODOT_DIR/platform/android/java
./gradlew generateGodotTemplates -Prun_from_godot_editor=true

GODOT_EXECUTABLE="$(find_godot)"

# Generate api.json
$GODOT_EXECUTABLE --no-window --gdnative-generate-json-api $GODOT_DIR/api.json

# Godot-CPP
cd $GODOT_CPP_DIR
scons platform=${host_platform_godot_cpp} -j$cpus target=${build_mode_godot_cpp} arch=${host_arch} $profiler_option $sync_opengl_traces \
      headers_dir=$GODOT_DIR/modules/gdnative/include \
      generate_bindings=yes \
      custom_api_file=$GODOT_DIR/api.json

scons platform=android -j$cpus android_arch=arm64v8 target=${build_mode_godot_cpp} $profiler_option $sync_opengl_traces \
      headers_dir=$GODOT_DIR/modules/gdnative/include \
      generate_bindings=yes \
      custom_api_file=$GODOT_DIR/api.json

# Build Android template
cd $GODOT_DIR
scons platform=android target=release android_arch=arm64v8 $profiler_option $sync_opengl_traces
cd $GODOT_DIR/platform/android/java
./gradlew generateGodotTemplates

cd $GODOT_DIR
scons platform=android target=release_debug android_arch=arm64v8 $profiler_option $sync_opengl_traces
cd $GODOT_DIR/platform/android/java
./gradlew generateGodotTemplates

# Install Android template for Foxus
rm -rf $ANDROID_BUILD_DIR
unzip $GODOT_DIR/bin/android_source.zip -d $ANDROID_BUILD_DIR
touch $ANDROID_BUILD_DIR/.gdignore
