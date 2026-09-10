import cv2
import numpy as np
from pathlib import Path

import argparse

from utils.basler_reader import BaslerReader
from utils.ouster_reader import OusterReader
from utils.pcd_utils import write_ply
from utils.stats import ts2utc


def export_all(
    rec_dir,
    take_name,
    export_lidar=False,
    output_dir=None,
):
    """
    Export all Basler images from a recording.

    Optionally exports every LiDAR scan as a separate PLY file.

    Parameters
    ----------
    rec_dir : str or Path
        Root recordings directory.

    take_name : str
        Recording name, e.g. "2026-09-05--17-56-44".

    export_lidar : bool, optional
        If True, export all Ouster scans as individual PLY files.

    output_dir : str or Path, optional
        Destination directory. If None:
            <rec_dir>/<take_name>_export
    """

    rec_dir = Path(rec_dir)
    recording_dir = rec_dir / take_name

    if output_dir is None:
        output_dir = rec_dir / f"{take_name}_export"
    else:
        output_dir = Path(output_dir)

    output_dir.mkdir(parents=True, exist_ok=True)

    # ---------------------------------------------------------
    # Basler cameras
    # ---------------------------------------------------------

    camera_dirs = sorted(
        p for p in recording_dir.iterdir()
        if p.is_dir() and p.name.startswith("CAM_")
    )

    if not camera_dirs:
        raise RuntimeError(f"No camera folders found in {recording_dir}")

    print(f"Found {len(camera_dirs)} cameras")

    for camera_dir in camera_dirs:

        camera_name = camera_dir.name
        camera_output = output_dir / camera_name
        camera_output.mkdir(parents=True, exist_ok=True)

        print(f"\nExporting {camera_name}...")

        reader = BaslerReader(camera_dir)

        n_frames = reader.frame_count

        for i in range(n_frames):

            image = reader.read_image(i)

            image_path = camera_output / f"{i:06d}.png"

            if not cv2.imwrite(str(image_path), image):
                raise RuntimeError(
                    f"Failed to write image: {image_path}"
                )

            if i % 100 == 0 or i == n_frames - 1:
                print(
                    f"\r  {i + 1}/{n_frames}",
                    end="",
                    flush=True,
                )

        print()

    # ---------------------------------------------------------
    # LiDAR
    # ---------------------------------------------------------

    if export_lidar:

        print("\nExporting LiDAR...")

        lidar_output = output_dir / "LIDAR"
        lidar_output.mkdir(parents=True, exist_ok=True)

        lidar_reader = OusterReader(rec_dir, take_name)

        n_scans = lidar_reader.frame_count

        for i in range(n_scans):

            # Assuming your OusterReader has something like this.
            points = lidar_reader.read_pointcloud(i)

            ply_path = lidar_output / f"{i:06d}.ply"

            write_ply(points, ply_path)

            if i % 100 == 0 or i == n_scans - 1:
                print(
                    f"\r  {i + 1}/{n_scans}",
                    end="",
                    flush=True,
                )

        print()

    print(f"\nExport complete: {output_dir}")
        
if __name__ == "__main__":
    
    parser = argparse.ArgumentParser(
        description="Export all frames"
    )

    parser.add_argument(
        "rec_dir",
        type=str,
        help="Recordings folder for Baslers. (e.g. contains folder '2026-09-05--17-56-44' which contains 'CAM_01' etc )",
    )
    
    parser.add_argument(
        "take_name",
        type=str,
        help="Name of the recording (e.g. 2026-09-05--17-56-44)",
    )
        
    parser.add_argument(
        "--export_lidar",
        type=bool,
        default=False,
        help="Export LiDar?",
    )
    
    args = parser.parse_args()
    
    rec_dir = args.rec_dir
    take_name = args.take_name
    export_lidar = args.export_lidar
    
    # rec_dir = "I:\JammerTestTestData"
    # take_name = "1970-01-01--04-14-34"
    # frames = [0, -1]
    
    export_all(rec_dir, take_name, export_lidar)