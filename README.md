# Foxus

This repository contains application software for the [Foxus](https://foxus.com)
camera module for the Oculus Quest 2.

It has the following functions:
* Camera handling
* Parallelized JPEG decoding
* Stereo camera calibration with OpenCV
* Removal of camera distortion (on GPU, based on OpenCV calibration data)
* LUTS effects

To reach maximum performance, an enhanced version of Godot Engine is used, which
is also included with this software.

## Prepare for Building Foxus

Linux and MacOS on Intel is supported for development. 
**M1 and M2 chips are not supported.**

1. Clone this repository and its submodules:
```
git clone --recursive https://github.com/cryptovoxels/foxus.git
cd foxus
```
2. Install the necessary build dependencies as described on the 
[Godot Engine Documentation](https://docs.godotengine.org/en/stable/development/compiling/).
3. Install the Android SDK and Android NDK (version 23.2.*) as described in the [Oculus Developer Documentation](https://developer.oculus.com/documentation/native/android/mobile-reqs/)
4. Set `ANDROID_NDK_ROOT` and `ANDROID_SDK_ROOT` environment variables to match your setup.
*Note:* The default locations will be searched by the build scripts automatically.

## Building Foxus

1. Build Godot Engine and Godot C++ bindings:
```
./build_godot.sh
```
2. Build the Foxus native module and the Foxus Android Plugin:
```
./build.sh
```
## Open Foxus in the Godot Editor

To open the Foxus project in the Godot Editor, execute:
```
./run_editor.sh
```

## Running Foxus

Prior to running Foxus, the camera module should be connected to the device (PC or Oculus Quest 2).

### Linux

* Just click play in the Godot Editor.

### MacOS

* Due to how the USB cameras are handled in MacOS, the Foxus app has to 
run as root (Administrator). Use the provided run.sh with sudo to run it:

```
sudo ./run.sh
```

### Android (Oculus Quest 2)

The prerequisite is that the Oculus Quest 2 is set up in Development Mode
and connected to the PC with ADB over TCP. This way the Foxus camera module can be plugged in while the device is controlled from the PC.

* If the Quest is connected, just click on the robot icon in the Godot Editor for a debug mode run.
* For the full performance, export the project in release mode from
the Godot Editor and then run:

```
adb install <apk name>
```

Then you can start the app from the Quest menu (look for the Unknown Sources category).

When first started, or when the Foxus camera is reconnected, the Foxus app needs to
request permissions to access the camera. If this does not happen automatically, then:

* Press Button B on the right controller to open the UI
* Select the Status tab.
* Click ``Ask for Permission`` to initiate the permission request.
* Pressing Button B again will hide the UI.

## Profiling

The Foxus app integrates with the [Perfetto](https://perfetto.dev) profiling framework that is available as standard on the Oculus Quest 2.

In order to profile the app, it has to be rebuilt with profiling enabled (including the Godot Engine):
```
./build_godot.sh --profiler
./build.sh --profiler
```

After that a release mode APK has to be exported from the Godot Editor and installed on the Oculus Quest 2.

To run the app with profiling enabled, first start the perfetto.py script, that will automatically start the Perfetto daemons and collectors on the device. In the default configuration (specified in perfetto_config.txt), only 30s of profiling data is collected. At the end of the data collection, the saved Perfetto trace file will be copied to the PC from the device.

More information about profiling with Perfetto on the Quest 2 may be found on the [Oculus Developer Portal](https://developer.oculus.com/blog/how-to-run-a-perfetto-trace-on-oculus-quest-or-quest-2/).

## Support

Foxus is brought to you by the [Voxels Team](https://voxels.com).

If you have any questions, don't hesitate to [Contact Us](https://www.foxus.com/pages/contact).
