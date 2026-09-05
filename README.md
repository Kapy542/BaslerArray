# BaslerRecorder

## Data Storage

Image data is saved as a raw binary stream in a single `frames.bin` file per camera.

Four Basler cameras running at 20 FPS and full resolution produce approximately:

* Resolution: `1920 × 1200`
* Pixel format: `BayerRG8`
* Data per frame: `2.304 MB`
* Data rate: `46.08 MB/s/camera`
* Data per minute: `≈ 2.76 GB/camera`
* Data per hour: `≈ 166 GB/camera`
* **Four cameras: `≈ 664 GB/hour`**

---

# Setup Linux PTP

```bash
sudo apt install linuxptp
```

---

# Ubuntu Setup

These instructions describe how to prepare a fresh Ubuntu installation for building and running the BaslerRecorder / PTP_Recorder.

> **Note:** There is currently no x86 configuration for this project. Install the pylon package matching the architecture of the Ubuntu system.

## 1. Update Ubuntu

```bash
sudo apt update
sudo apt upgrade
```

Install Git if the project is stored in a Git repository:

```bash
sudo apt install git
```

---

## 2. Install C++ Build Tools

Install the compiler and basic build tools:

```bash
sudo apt install build-essential
```

Verify the compiler:

```bash
g++ --version
```

---

## 3. Install CMake

```bash
sudo apt install cmake
```

Verify:

```bash
cmake --version
```

---

## 4. Install OpenCV

The recorder uses OpenCV for image processing and preview functionality.

Install the Ubuntu development package:

```bash
sudo apt install libopencv-dev
```

Verify the installation:

```bash
pkg-config --modversion opencv4
```

There is no need to build OpenCV manually from source.

---

## 5. Install Basler pylon Software Suite

The Basler pylon Software Suite provides the pylon SDK required to build the recorder. It also includes **pylon Viewer**, which can be used to test and configure the cameras independently of the recorder.

Download the appropriate Linux version of the pylon Software Suite from Basler:

[Basler pylon Software Suite Downloads](https://www.baslerweb.com/en/downloads/software/pylon-software-suite/?utm_source=chatgpt.com)

For Ubuntu, Basler provides Debian packages (`.deb`) as well as archive packages. Use the package matching the architecture of the Ubuntu system.

After downloading the package, go to the download directory:

```bash
cd ~/Downloads
```
Extract and Install the downloaded package:

```bash
tar -xzf pylon-26.08.1_linux-x86_64_debs.tar.gz
```

```bash
sudo apt install ./pylon_*.deb
```

If the package has a different filename, use the exact filename instead.

The default installation location is:

```text
/opt/pylon
```

---

## 6. Verify the pylon Installation

Check that pylon was installed:

```bash
ls /opt/pylon
```

The installation should contain directories such as:

```text
bin/
include/
lib/
share/
```

Check the pylon configuration utility:

```bash
/opt/pylon/bin/pylon-config --help
```

---

## 7. Test the Cameras with pylon Viewer

Before building the recorder, verify that the cameras work correctly with pylon Viewer.

Start pylon Viewer:

```bash
/opt/pylon/bin/pylonviewer
```

Check that all Basler cameras are visible.

For each camera, verify:

* Camera is detected
* Correct serial number is shown
* Camera can be opened
* Live image can be displayed
* Camera parameters can be read and changed

**Do this before troubleshooting the recorder.**

If pylon Viewer cannot connect to a camera, the problem is not with the recorder.

For GigE cameras, also verify that the network interface is configured correctly.

---

## 8. Install Python

The `bin2png.py` script is used to verify recordings and convert raw image data to PNG.

Install Python:

```bash
sudo apt install python3 python3-pip python3-venv
```

Verify:

```bash
python3 --version
```

---

## 9. Install Python Dependencies

The export script requires NumPy and OpenCV.

It is recommended to use a Python virtual environment:

```bash
python3 -m venv .venv
source .venv/bin/activate
```

Install the required packages:

```bash
pip install numpy opencv-python
```

If using Ouster:

```bash
pip install ouster-sdk
```

The virtual environment can be activated again later with:

```bash
source .venv/bin/activate
```

---

# Building the Project

## 10. Get the Project

Clone the repository:

```bash
git clone <repository-url>
```

Or copy the project to the Ubuntu system.

The project should contain the `SynchronizedSnapshots` directory.

Example:

```text
BaslerArray/
└── BaslerArray/
    ├── CMakeLists.txt
    ├── BaslerArray.sln
    ├── configs/
    ├── Common/
    ├── Dependencies/
    ├── PTP_Recorder/
    └── ...
```

---

## 11. Build PTP_Recorder

Go to the project directory:

```bash
cd /BaslerArray/BaslerArray
```

Create the build directory:

```bash
mkdir build
cd build
```

Run CMake:

```bash
cmake ..
```

Build the project:

```bash
make -j$(nproc)
```

The executable should now be available as:

```text
PTP_Recorder
```

---

## 12. Copy the Configuration Files

The recorder expects the configuration files inside the `build` directory.

From:

```text
BaslerArray/BaslerArray/build/
```

run:

```bash
cp -r ../PTP_Recorder/configs ./PTP_Recorder/configs
```

The directory should now look similar to:

```text
build/
├── PTP_Recorder
    └── configs/
        ├── recorder_config.json
        ├── camera_mapping.json
        ├── camera_config.json
        └── cameras/
```

---

# Configuration

## `configs/recorder_config.json`

Contains general recorder settings.

Example:

```json
{
    "outputDirectory": "recordings",
    "preview": true,
    "previewEveryNth": 2
}
```

For better recording performance, disable the preview:

```json
{
    "outputDirectory": "recordings",
    "preview": false,
    "previewEveryNth": 2
}
```

Alternatively, increase `previewEveryNth` if preview is required but does not need to update every frame.

---

## `configs/camera_mapping.json`

Maps logical camera names to physical camera serial numbers.

Example:

```json
{
    "CAM_01": "21965600",
    "CAM_04": "21965597"
}
```

The camera name is used to:

* Identify the camera in the recorder
* Match the camera configuration
* Create the corresponding recording directory

---

## `configs/camera_config.json`

Contains the default configuration applied to all cameras.

Example:

```json
{
    "width": 1920,
    "height": 1200,
    "pixelFormat": "BayerRG8",
    "reverseX": false,
    "reverseY": false,
    "exposureUs": 10000.0,
    "gain": 100,
    "exposureAuto": "Off",
    "gainAuto": "Off",
    "whiteBalance": {
        "mode": "Custom",
        "lightSource": "Custom",
        "red": 105,
        "green": 64,
        "blue": 159
    },
    "fps": 20.0,
    "packetSize": 1500
}
```

---

## `configs/cameras/<cam_name>.json`

Camera-specific configuration overrides can be placed in this directory.

For example:

```text
configs/cameras/CAM_04.json
```

Use this when an individual camera requires different settings from the default configuration.

Possible overrides include:

* `reverseX`
* `reverseY`
* Exposure
* Gain
* White balance
* FPS
* Packet size

The filename must match the camera name defined in `camera_mapping.json`.

---

# Running the Recorder

## 13. Start PTP_Recorder

From the `build` directory:

```bash
./PTP_Recorder
```

The recorder initializes the configured cameras and synchronizes them using PTP.

Keyboard controls:

```text
r      Toggle recording
q      Exit
ESC    Exit
```

Press `r` to start recording.

A new take directory is created using the current timestamp.

Press `r` again to stop recording.

---

# Recorded Data

After recording, the data is stored in the following structure:

```text
recordings/
└── <take_name>/
    ├── CAM_01/
    │   ├── frames.bin
    │   ├── timestamps.bin
    │   └── metadata.json
    ├── CAM_04/
    │   ├── frames.bin
    │   ├── timestamps.bin
    │   └── metadata.json
    └── ...
```

### `frames.bin`

Contains the camera images as a continuous raw binary stream.

The image dimensions and pixel format are specified in `metadata.json`.

### `timestamps.bin`

Contains the timestamp corresponding to each frame.

Frame `N` corresponds to timestamp `N`.

### `metadata.json`

Contains the camera configuration and information required to interpret the binary data, including:

* Image width and height
* Pixel format
* FPS
* Exposure
* Gain
* Auto-exposure/gain settings
* White balance
* Image orientation (`reverseX`, `reverseY`)
* Packet size
* Timestamp format and unit

---

# Verify and Export a Recording

## 14. Run `bin2png.py`

From the `/BaslerArray` directory:

```bash
source .venv/bin/activate
```

```bash
python3 ./bin2png.py "<take_name>"
```

For example:

```bash
python3 ./bin2png.py "2026-09-02--15-40-32"
```

The script automatically finds all camera folders inside the specified take.

It checks that:

* `frames.bin` exists
* `timestamps.bin` exists
* The number of frames matches the number of timestamps

It then converts the raw Bayer images to PNG.

The exported images are placed in an `exported` directory inside each camera folder:

```text
recordings/
└── <take_name>/
    ├── CAM_01/
    │   ├── frames.bin
    │   ├── timestamps.bin
    │   ├── metadata.json
    │   └── exported/
    │       ├── 000000_<timestamp>.png
    │       ├── 000001_<timestamp>.png
    │       └── ...
    └── CAM_04/
        └── ...
```

---

# Troubleshooting

## CMake cannot find pylon

Check that pylon is installed:

```bash
ls /opt/pylon
```

If pylon was installed in a non-default location, set `PYLON_ROOT` before running CMake:

```bash
export PYLON_ROOT=/path/to/pylon
```

Then run:

```bash
cmake ..
```

Basler's CMake integration uses the pylon installation and supports `PYLON_ROOT` for specifying a custom installation location.

---

## CMake cannot find OpenCV

Check:

```bash
pkg-config --modversion opencv4
```

If this fails, install:

```bash
sudo apt install libopencv-dev
```

Then reconfigure the build:

```bash
rm -rf build
mkdir build
cd build
cmake ..
```

---

## pylon Viewer cannot see the cameras

First verify:

1. The cameras are powered.
2. Network cables are connected.
3. The correct network interface is being used.
4. The cameras appear in pylon Viewer.

For GigE cameras, check the network interfaces:

```bash
ip addr
```

Do not troubleshoot the recorder until the cameras can be successfully accessed through pylon Viewer.

---

## The recorder cannot find the configuration files

Make sure the recorder is launched from the `build` directory:

```bash
cd /BaslerArray/BaslerArray/build
./PTP_Recorder/PTP_Recorder
```

Check that the configuration directory exists:

```bash
ls configs
```

It should contain:

```text
recorder_config.json
camera_mapping.json
camera_config.json
cameras/
```

---

# Quick Installation Summary

For a fresh Ubuntu installation, install the required Ubuntu packages:

```bash
sudo apt update
sudo apt upgrade

sudo apt install \
    build-essential \
    cmake \
    git \
    libopencv-dev \
    python3 \
    python3-pip \
    python3-venv
```

Then:

1. Install the appropriate **Basler pylon Software Suite**.
2. Verify the cameras with **pylon Viewer**.
3. Get the `BaslerArray` project.
4. Configure the camera mapping.
5. Configure the camera parameters.
6. Configure the recorder.
7. Build the project with CMake.
8. Copy the configuration files into `build/PTP_Recorder/configs`.
9. Run `./PTP_Recorder/PTP_Recorder`.
10. Make a short test recording.
11. Run `bin2png.py` to verify and export the recording.

## Required Software

| Software                    | Purpose                       |
| --------------------------- | ----------------------------- |
| Ubuntu                      | Operating system              |
| GCC / G++                   | C++ compiler                  |
| CMake                       | Build system                  |
| Git                         | Source code management        |
| OpenCV                      | Image processing and preview  |
| Basler pylon Software Suite | Camera SDK and pylon Viewer   |
| Python 3                    | Recording verification/export |
| NumPy                       | Binary data processing        |
| OpenCV-Python               | PNG export                    |

`nlohmann/json` does **not** need to be installed separately because it is included in the project.
