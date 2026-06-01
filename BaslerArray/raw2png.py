import os
import cv2
import numpy as np
from pathlib import Path
import argparse


# -----------------------------
# CONFIG
# -----------------------------
WIDTH = 1920
HEIGHT = 1200
BAYER_CONVERSION = cv2.COLOR_BayerRG2RGB


# -----------------------------
# Convert single file
# -----------------------------
def convert_raw_to_png(raw_path, out_path):
    with open(raw_path, "rb") as f:
        data = f.read()

    img = np.frombuffer(data, dtype=np.uint8)

    expected_size = WIDTH * HEIGHT

    if img.size != expected_size:
        print(f"[SKIP] Wrong size: {raw_path} ({img.size} bytes)")
        return

    img = img.reshape((HEIGHT, WIDTH))

    color = cv2.cvtColor(img, BAYER_CONVERSION)

    cv2.imwrite(str(out_path), color)


# -----------------------------
# Process dataset
# -----------------------------
def process_folder(input_dir, output_dir):
    input_path = Path(input_dir)
    output_path = Path(output_dir)

    output_path.mkdir(exist_ok=True)

    for cam_folder in input_path.iterdir():
        if not cam_folder.is_dir():
            continue

        cam_id = cam_folder.name
        cam_out = output_path / cam_id
        cam_out.mkdir(exist_ok=True)

        for file in sorted(cam_folder.glob("*.raw")):
            out_file = cam_out / (file.stem + ".png")
            convert_raw_to_png(file, out_file)

        print(f"[DONE] Camera {cam_id}")


# -----------------------------
# MAIN
# -----------------------------
def main():
    parser = argparse.ArgumentParser(
        description="Convert RAW Bayer images to PNG"
    )

    parser.add_argument(
        "take_name",
        type=str,
        help="Name of the take inside the input folder",
    )

    parser.add_argument(
        "--input-folder",
        type=str,
        default="./SynchronizedSnapshots/build/recordings",
        help="Base recordings folder (default: ./recordings)",
    )

    args = parser.parse_args()

    input_path = Path(args.input_folder) / args.take_name

    if not input_path.exists():
        print(f"Input path does not exist: {input_path}")
        return

    output_path = str(input_path) + "_png"

    print(f"[INFO] Input : {input_path}")
    print(f"[INFO] Output: {output_path}")

    process_folder(input_path, output_path)


if __name__ == "__main__":
    main()
