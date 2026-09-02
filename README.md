# BaslerRecorder

## Data Storage

Image data is saved as a raw binary stream in a single `frames.bin` file per camera.

Four Basler cameras running at 20 FPS and full resolution produce approximately:

- Resolution: `1920 × 1200`
- Pixel format: `BayerRG8`
- Data per frame: `2.304 MB`
- Data rate: `46.08 MB/s/camera`
- Data per minute: `≈ 2.76 GB/camera`
- Data per hour: `≈ 166 GB/camera`
- **Four cameras: `≈ 664 GB/hour`**

---

# BaslerRecorder / PTP_Recorder

## Build for Ubuntu

From the project root (`/BaslerArray/PTP_Recorder`), run:

```bash
mkdir build
cd build
cmake ..
make
cp -r ../configs ./configs
```

The executable will be created in the `build` directory.

> **Note:** There is currently no x86 configuration. The recorder is built for the target Ubuntu platform.

---

## Configure the Recorder

Before recording, modify the configuration files as needed.

### `configs/recorder_config.json`

Contains general recorder settings.

- Disable `preview` or increase `previewEveryNth` to reduce the preview processing overhead and improve recording performance.

### `configs/camera_mapping.json`

Defines the logical name and serial number of each camera.

The camera name is used to:

- Open the cameras in the configured order.
- Identify cameras in the recorder.
- Create matching camera folders in the recorded dataset.

Example:

```json
{
    "CAM_01": "21965600",
    "CAM_04": "21965597"
}
```

### `configs/camera_config.json`

Contains the default configuration parameters applied to all cameras.

### `configs/cameras/<cam_name>.json`

Contains camera-specific configuration overrides.

Use this file when an individual camera requires settings different from the default configuration, for example:

- `reverseX`
- `reverseY`
- Exposure
- Gain
- White balance
- FPS

The filename must match the camera name defined in `camera_mapping.json`.

For example:

```text
camera_mapping.json
    CAM_01
    CAM_04

cameras/
    CAM_01.json
    CAM_04.json
```

---

# Running the Recorder

## 1. Start the recorder

From the `build` directory, run:

```bash
./PTP_Recorder
```

The recorder waits for keyboard input:

```text
Press w to write a single preview image  >> DISABLED
Press r to TOGGLE recording              >> Starts/stops recording
Press ESC or q to exit                   >> Exits the recorder
```

Pressing `r` starts a new recording and creates a new take directory using the current timestamp. The take is stored under the directory specified by `outputDirectory` in `recorder_config.json`.

---

## 2. Recorded Data

After stopping the recording, each camera's data is stored in its own directory:

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

Contains the timestamp corresponding to each frame in `frames.bin`.

The timestamps and frames are stored in matching order, so timestamp `N` corresponds to frame `N`.

### `metadata.json`

Contains the camera configuration and information required to interpret the binary data, including:

- Image width and height
- Pixel format
- FPS
- Exposure
- Gain
- Auto-exposure/gain settings
- White balance
- Image orientation (`reverseX`, `reverseY`)
- Packet size
- Timestamp format and unit

---

## 3. Verify and Export the Recording

From the `/BaslerArray` directory, run:

```bash
python ./bin2png.py "<take_name>" (--export_every 10)
```

The script:

1. Finds all camera folders inside the specified take.
2. Reads the `frames.bin` and `timestamps.bin` files.
3. Verifies that the number of frames and timestamps match.
4. Converts the raw Bayer images to PNG.
5. Saves the exported images into an `exported` directory inside each camera folder.

For example:

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