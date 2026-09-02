# -*- coding: utf-8 -*-
"""
Created on Wed Sep  2 15:49:10 2026

@author: CIVIT
"""

from pathlib import Path
import numpy as np
import cv2
import argparse


# ============================================================
# Configuration
# ============================================================

# take_name = "2026-09-02--16-35-26"
# cam_name = "CAM_01"

WIDTH = 1920
HEIGHT = 1200

# RECORDING_DIR  = Path("./PTP_Recorder/recordings")
# RECORDING_DIR = RECORDING_DIR / take_name / cam_name
# FRAMES_FILE = RECORDING_DIR / "frames.bin"
# TIMESTAMPS_FILE = RECORDING_DIR / "timestamps.bin"

# OUTPUT_DIR = RECORDING_DIR / "exported"


# ============================================================
# Run for a single camera stream
# ============================================================

def export_stream(input_folder, EXPORT_EVERY=1):
    RECORDING_DIR  = Path(input_folder)
    FRAMES_FILE = RECORDING_DIR / "frames.bin"
    TIMESTAMPS_FILE = RECORDING_DIR / "timestamps.bin"
    OUTPUT_DIR = RECORDING_DIR / "exported"
    
    frame_size = WIDTH * HEIGHT

    print(f"Frames file:      {FRAMES_FILE}")
    print(f"Timestamps file:  {TIMESTAMPS_FILE}")

    # --------------------------------------------------------
    # Check file sizes
    # --------------------------------------------------------

    frame_file_size = FRAMES_FILE.stat().st_size
    timestamp_file_size = TIMESTAMPS_FILE.stat().st_size

    print(f"\nframes.bin size:      {frame_file_size:,} bytes")
    print(f"timestamps.bin size:  {timestamp_file_size:,} bytes")

    if frame_file_size % frame_size != 0:
        raise RuntimeError(
            f"frames.bin size ({frame_file_size}) is not "
            f"a multiple of frame size ({frame_size})."
        )

    if timestamp_file_size % 8 != 0:
        raise RuntimeError(
            "timestamps.bin size is not a multiple of 8 bytes."
        )

    num_frames = frame_file_size // frame_size
    num_timestamps = timestamp_file_size // 8

    print(f"\nNumber of frames:      {num_frames}")
    print(f"Number of timestamps:  {num_timestamps}")

    if num_frames != num_timestamps:
        raise RuntimeError(
            f"Frame/timestamp count mismatch! "
            f"{num_frames} frames vs {num_timestamps} timestamps."
        )

    print("Frame/timestamp count matches.")

    # --------------------------------------------------------
    # Create output directory
    # --------------------------------------------------------

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

    print(f"\nExporting images to: {OUTPUT_DIR}")

    # --------------------------------------------------------
    # Open files
    # --------------------------------------------------------
    
    timestamps_ns = []
    with open(FRAMES_FILE, "rb") as frame_file, \
         open(TIMESTAMPS_FILE, "rb") as timestamp_file:

        for index in range(num_frames):
            
            # Read timestamp
            timestamp_bytes = timestamp_file.read(8)

            if len(timestamp_bytes) != 8:
                raise RuntimeError(
                    f"Could not read timestamp for frame {index}."
                )

            timestamp = int.from_bytes(
                timestamp_bytes,
                byteorder="little",
                signed=False
            )
            timestamps_ns.append(timestamp)
            
            
            # Don't write every frame
            if index % EXPORT_EVERY == 0:
            
                # Read raw Bayer frame
                raw = frame_file.read(frame_size)
    
                if len(raw) != frame_size:
                    raise RuntimeError(
                        f"Could not read complete frame {index}."
                    )           
    
                # Convert raw data to image
                bayer = np.frombuffer(
                    raw,
                    dtype=np.uint8
                ).reshape((HEIGHT, WIDTH))
    
                # BayerRG8 -> BGR
                image = cv2.cvtColor(
                    bayer,
                    cv2.COLOR_BAYER_RG2BGR
                )
    
                # ------------------------------------------------
                # Filename
                # ------------------------------------------------
    
                filename = (
                    f"{index:06d}_{timestamp}.png"
                )
    
                output_path = OUTPUT_DIR / filename
    
                # Save PNG
                success = cv2.imwrite(
                    str(output_path),
                    image
                )
    
                if not success:
                    raise RuntimeError(
                        f"Failed to write {output_path}"
                    )
    
                print(
                    f"[{index:6d}/{num_frames}] "
                    f"timestamp={timestamp} "
                    f"-> {filename}"
                )

    print("\nDone.")
    timestamps_ns = np.array(timestamps_ns)
    timestamps_ms = timestamps_ns / 1000000
    timestamp_diffs_ms = np.diff(timestamps_ms)
    mean_period = np.mean(timestamp_diffs_ms)
    mean_fps = 1000/mean_period
    print(f"\nAverage period: {mean_period} ms")
    print(f"\nAverage FPS: {mean_fps} Hz")



# -----------------------------
# MAIN
# -----------------------------
def main():
    parser = argparse.ArgumentParser(
        description="Convert RAW Bayer stream to PNG"
    )

    parser.add_argument(
        "take_name",
        type=str,
        help="Name of the take inside the input folder",
    )

    parser.add_argument(
        "--input_folder",
        type=str,
        default="./PTP_Recorder/recordings",
        help="Base recordings folder (default: ./PTP_Recorder/recordings)",
    )
    
    parser.add_argument(
        "--export_every",
        type=int,
        default=1,
        help="Export every Nth image",
    )
    # parser.add_argument(
    #     "--input-folder",
    #     type=str,
    #     default="./PTP_Recorder/build/recordings",
    #     help="Base recordings folder (default: ./recordings)",
    # )

    args = parser.parse_args()

    input_path = Path(args.input_folder) / args.take_name
    
    if not input_path.exists():
        print(f"Input path does not exist: {input_path}")
        return
    
    camera_folders = [
        path for path in input_path.iterdir()
        if path.is_dir()
    ]
    
    for camera_folder in camera_folders:
        print("Found: ", camera_folder)
        export_stream(camera_folder, args.export_every)
    
if __name__ == "__main__":
    main()