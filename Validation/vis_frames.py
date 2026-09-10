import cv2
import numpy as np
from pathlib import Path

import argparse

from utils.basler_reader import BaslerReader
from utils.ouster_reader import OusterReader
from utils.pcd_utils import write_ply
from utils.stats import ts2utc


def visualize_frames(
    rec_dir,
    take_name,
    frame_numbers,
    edge_margin=10,
):
    """
    Visualize temporally synchronized camera frames and Ouster scans.

    Parameters
    ----------
    rec_dir : str or Path
        Directory containing the camera recordings.

    ouster_dir : str or Path
        Directory containing the Ouster PCAP/JSON.

    take_name : str
        Recording/take name.

    frame_numbers : list[int]
        CAM_01 frame numbers to visualize.

        0  -> first frame inside the common sensor range
        -1 -> last frame inside the common sensor range

    edge_margin : int
        Number of CAM_01 frames to stay away from the
        beginning/end of the common sensor range.
    """

    rec_dir = Path(rec_dir)
    ouster_dir = Path(rec_dir)

    camera_ids = [
        "CAM_01",
        "CAM_02",
        "CAM_03",
        "CAM_04",
    ]

    # Raw camera recording.
    recording_dir = rec_dir / take_name

    # Export is next to the raw recording.
    export_dir = rec_dir / f"{take_name}_export"

    image_export_dir = export_dir / "images"
    lidar_export_dir = export_dir / "lidar"

    image_export_dir.mkdir(
        parents=True,
        exist_ok=True,
    )

    lidar_export_dir.mkdir(
        parents=True,
        exist_ok=True,
    )

    # ------------------------------------------------------------------
    # Open readers
    # ------------------------------------------------------------------

    cameras = {
        camera_id: BaslerReader(
            recording_dir / camera_id
        )
        for camera_id in camera_ids
    }

    ouster = OusterReader(
        ouster_dir,
        take_name,
    )

    try:
        reference_camera = cameras["CAM_01"]

        # --------------------------------------------------------------
        # Find common timestamp range
        # --------------------------------------------------------------

        first_timestamps = [
            camera.first_timestamp()
            for camera in cameras.values()
        ]

        first_timestamps.append(
            ouster.first_timestamp()
        )

        last_timestamps = [
            camera.last_timestamp()
            for camera in cameras.values()
        ]

        last_timestamps.append(
            ouster.last_timestamp()
        )

        common_start = max(
            first_timestamps
        )

        common_end = min(
            last_timestamps
        )

        # TODO: uncomment
        if common_start >= common_end:
            raise RuntimeError(
                "No common time range exists between "
                "the cameras and Ouster."
            )

        # --------------------------------------------------------------
        # Find corresponding CAM_01 frame range
        # --------------------------------------------------------------

        cam01_timestamps = (
            reference_camera.timestamps()
        )

        common_start_index = int(
            np.searchsorted(
                cam01_timestamps,
                common_start,
                side="left",
            )
        )

        common_end_index = int(
            np.searchsorted(
                cam01_timestamps,
                common_end,
                side="right",
            )
        ) - 1

        # TODO: uncomment
        if common_start_index > common_end_index:
            raise RuntimeError(
                "No CAM_01 frames exist inside "
                "the common sensor time range."
            )

        # Add safety margin.
        safe_start_index = (
            common_start_index + edge_margin
        )

        safe_end_index = (
            common_end_index - edge_margin
        )

        # Handle short recordings.
        if safe_start_index > safe_end_index:
            safe_start_index = common_start_index
            safe_end_index = common_end_index
            
        # TODO: Remove
        # safe_start_index = 10
        # safe_end_index = cameras["CAM_01"].frame_count - 10
    
        print()
        print("Common sensor range:")
        print(
            f"  {ts2utc(common_start)}"
        )
        print(
            f"  {ts2utc(common_end)}"
        )

        print()
        print("Safe CAM_01 frame range:")
        print(
            f"  {safe_start_index}"
            f" -> "
            f"{safe_end_index}"
        )

        # --------------------------------------------------------------
        # Resolve requested frames
        # --------------------------------------------------------------

        resolved_frames = []

        for frame_number in frame_numbers:

            if frame_number == 0:
                frame_index = safe_start_index

            elif frame_number == -1:
                frame_index = safe_end_index

            else:
                frame_index = int(
                    frame_number
                )

                if (
                    frame_index < safe_start_index
                    or frame_index > safe_end_index
                ):
                    raise ValueError(
                        f"CAM_01 frame {frame_index} "
                        f"is outside the safe synchronized "
                        f"range "
                        f"[{safe_start_index}, "
                        f"{safe_end_index}]."
                    )

            resolved_frames.append(
                frame_index
            )

        # --------------------------------------------------------------
        # OpenCV window
        # --------------------------------------------------------------

        window_name = (
            f"{take_name} - synchronized frames"
        )

        cv2.namedWindow(
            window_name,
            cv2.WINDOW_NORMAL,
        )

        # --------------------------------------------------------------
        # Process requested frames
        # --------------------------------------------------------------

        for frame_index in resolved_frames:

            reference_timestamp = (
                reference_camera.get_timestamp(
                    frame_index
                )
            )

            print()
            print("=" * 80)

            print(
                f"CAM_01 frame: {frame_index}"
            )

            print(
                f"Timestamp: "
                f"{ts2utc(reference_timestamp)}"
            )

            panels = []

            # ==========================================================
            # Cameras
            # ==========================================================

            for camera_id in camera_ids:

                camera = cameras[camera_id]

                if camera_id == "CAM_01":

                    image = camera.read_image(
                        frame_index
                    )

                    actual_index = frame_index
                    actual_timestamp = (
                        reference_timestamp
                    )
                    difference = 0

                else:

                    (
                        image,
                        actual_timestamp,
                        actual_index,
                        difference,
                    ) = camera.read_image_at_timestamp(
                        reference_timestamp
                    )

                # BaslerReader returns RGB.
                # OpenCV expects BGR.
                image = cv2.cvtColor(
                    image,
                    cv2.COLOR_RGB2BGR,
                )

                panel = make_image_panel(
                    image,
                    camera_id,
                    actual_index,
                    actual_timestamp,
                    difference,
                )

                panels.append(panel)

                print(
                    f"{camera_id}: "
                    f"frame {actual_index}, "
                    f"dt = "
                    f"{difference / 1_000:.3f} us"
                )

            # ==========================================================
            # Ouster
            # ==========================================================

            (
                points,
                ouster_timestamp,
                ouster_index,
                ouster_difference,
            ) = ouster.read_pointcloud_at_timestamp(
                reference_timestamp
            )

            print(
                f"Ouster: "
                f"scan {ouster_index}, "
                f"dt = "
                f"{ouster_difference / 1_000_000:.3f} ms"
            )

            # ----------------------------------------------------------
            # Write LiDAR PLY
            # ----------------------------------------------------------

            ply_path = (
                lidar_export_dir
                / (
                    f"frame_{frame_index:06d}"
                    f"_ouster_{ouster_index:06d}.ply"
                )
            )

            write_ply(
                points,
                ply_path,
            )

            print(
                f"PLY: {ply_path}"
            )

            # ----------------------------------------------------------
            # Make LiDAR BEV visualization
            # ----------------------------------------------------------

            lidar_image = pointcloud_to_bev(
                points
            )

            lidar_panel = make_image_panel(
                lidar_image,
                "Ouster",
                ouster_index,
                ouster_timestamp,
                ouster_difference,
            )

            panels.append(
                lidar_panel
            )

            # ==========================================================
            # Combine camera + LiDAR panels
            # ==========================================================

            combined = concatenate_panels(
                panels
            )

            # Add overall title.
            combined = add_overall_header(
                combined,
                take_name,
                frame_index,
                reference_timestamp,
            )

            # ----------------------------------------------------------
            # Save visualization
            # ----------------------------------------------------------

            image_path = (
                image_export_dir
                / f"frame_{frame_index:06d}.jpg"
            )

            cv2.imwrite(
                str(image_path),
                combined,
            )

            print(
                f"Image: {image_path}"
            )

            # ----------------------------------------------------------
            # Display
            # ----------------------------------------------------------

            cv2.imshow(
                window_name,
                combined,
            )

            print(
                "Press any key for next frame, "
                "or ESC to exit."
            )

            key = cv2.waitKey(0)

            if key == 27:
                break

        cv2.destroyWindow(
            window_name
        )

    finally:

        for camera in cameras.values():
            camera.close()

        ouster.close()


def make_image_panel(
    image,
    sensor_name,
    frame_index,
    timestamp,
    difference,
):
    """
    Add an information bar above an image.
    """

    image = np.asarray(
        image,
        dtype=np.uint8,
    )

    if image.ndim == 2:
        image = cv2.cvtColor(
            image,
            cv2.COLOR_GRAY2BGR,
        )

    timestamp_text = ts2utc(
        timestamp
    )

    difference_us = (
        difference / 1_000.0
    )

    line1 = (
        f"{sensor_name} | frame {frame_index}"
    )

    line2 = (
        f"{timestamp_text} | "
        f"dt = {difference_us:.3f} us"
    )

    bar_height = 55

    bar = np.zeros(
        (
            bar_height,
            image.shape[1],
            3,
        ),
        dtype=np.uint8,
    )

    cv2.putText(
        bar,
        line1,
        (10, 21),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.55,
        (255, 255, 255),
        1,
        cv2.LINE_AA,
    )

    cv2.putText(
        bar,
        line2,
        (10, 44),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.42,
        (220, 220, 220),
        1,
        cv2.LINE_AA,
    )

    return np.vstack(
        [
            bar,
            image,
        ]
    )


def add_overall_header(
    image,
    take_name,
    frame_index,
    timestamp,
):
    """
    Add a header above the complete visualization.
    """

    height = 35

    header = np.zeros(
        (
            height,
            image.shape[1],
            3,
        ),
        dtype=np.uint8,
    )

    text = (
        f"{take_name} | "
        f"CAM_01 frame {frame_index} | "
        f"{ts2utc(timestamp)}"
    )

    cv2.putText(
        header,
        text,
        (10, 24),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.55,
        (255, 255, 255),
        1,
        cv2.LINE_AA,
    )

    return np.vstack(
        [
            header,
            image,
        ]
    )


def concatenate_panels(
    panels,
):
    """
    Resize panels to the same height and
    concatenate them horizontally.
    """

    target_height = min(
        panel.shape[0]
        for panel in panels
    )

    resized = []

    for panel in panels:

        scale = (
            target_height
            / panel.shape[0]
        )

        width = int(
            panel.shape[1] * scale
        )

        panel = cv2.resize(
            panel,
            (width, target_height),
            interpolation=cv2.INTER_AREA,
        )

        resized.append(
            panel
        )

    return np.hstack(
        resized
    )


def pointcloud_to_bev(
    points,
    resolution=0.05,
):
    """
    Convert XYZ points to a simple bird's-eye-view image.

    Region:
        X: -30 ... +30 m
        Y: -30 ... +30 m

    resolution:
        metres per pixel.
    """

    points = np.asarray(
        points
    )

    if len(points) == 0:
        return np.zeros(
            (500, 500, 3),
            dtype=np.uint8,
        )

    valid = np.isfinite(
        points
    ).all(axis=1)

    points = points[valid]

    if len(points) == 0:
        return np.zeros(
            (500, 500, 3),
            dtype=np.uint8,
        )

    x = points[:, 0]
    y = points[:, 1]

    mask = (
        (x > -30)
        & (x < 30)
        & (y > -30)
        & (y < 30)
    )

    x = x[mask]
    y = y[mask]

    if len(x) == 0:
        return np.zeros(
            (500, 500, 3),
            dtype=np.uint8,
        )

    size = int(
        60 / resolution
    )

    px = (
        x / resolution
        + 30 / resolution
    ).astype(np.int32)

    py = (
        y / resolution
        + 30 / resolution
    ).astype(np.int32)

    image = np.zeros(
        (size, size, 3),
        dtype=np.uint8,
    )

    valid_pixels = (
        (px >= 0)
        & (px < size)
        & (py >= 0)
        & (py < size)
    )

    px = px[valid_pixels]
    py = py[valid_pixels]

    image[
        size - 1 - py,
        px,
    ] = 255

    return image

        
if __name__ == "__main__":
    
    parser = argparse.ArgumentParser(
        description="Validate Basler and Ouster data by exporting frames"
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
        "-i",
        type=int,
        default=None,
        help="Frame index to process",
    )
    
    args = parser.parse_args()
    
    rec_dir = args.rec_dir
    take_name = args.take_name
    idx = args.i
    
    if idx is not None:
        frames = [idx]
    else:
        frames = [0, -1]
    
    # rec_dir = "I:\JammerTestTestData"
    # take_name = "1970-01-01--04-14-34"
    # frames = [0, -1]
    
    visualize_frames(rec_dir, take_name, frames)