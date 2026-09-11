# -*- coding: utf-8 -*-
"""
Created on Thu Sep 10 20:55:26 2026

@author: janii
"""

from pathlib import Path

import cv2
import numpy as np

from utils.calibration import read_calibration
from utils.basler_reader import BaslerReader
from utils.ouster_reader import OusterReader


def _matrix_from_calibration(extrinsic):
    """Convert calibration R/t into a 4x4 transformation matrix."""

    if "T" in extrinsic:
        return np.asarray(extrinsic["T"], dtype=np.float64)

    R = np.asarray(extrinsic["R"], dtype=np.float64)
    t = np.asarray(extrinsic["t"], dtype=np.float64)

    T = np.eye(4, dtype=np.float64)
    T[:3, :3] = R
    T[:3, 3] = t

    return T


def _transform_points(points, T):
    """
    Transform Nx3 points using a 4x4 transformation matrix.

    p_target = T @ p_source
    """

    points = np.asarray(points, dtype=np.float64)

    points_h = np.ones(
        (points.shape[0], 4),
        dtype=np.float64,
    )

    points_h[:, :3] = points

    transformed = (T @ points_h.T).T

    return transformed[:, :3]


def _write_colored_ply(filename, points, colors):
    """
    Write XYZ + RGB point cloud as binary little-endian PLY.

    Parameters
    ----------
    filename : str or Path
        Output PLY filename.

    points : np.ndarray
        Nx3 float array.

    colors : np.ndarray
        Nx3 uint8 RGB array.
    """

    points = np.asarray(points, dtype=np.float32)
    colors = np.asarray(colors, dtype=np.uint8)

    if len(points) != len(colors):
        raise ValueError(
            "Number of points and colors must match."
        )

    filename = Path(filename)

    header = (
        "ply\n"
        "format binary_little_endian 1.0\n"
        f"element vertex {len(points)}\n"
        "property float x\n"
        "property float y\n"
        "property float z\n"
        "property uchar red\n"
        "property uchar green\n"
        "property uchar blue\n"
        "end_header\n"
    )

    with open(filename, "wb") as f:

        f.write(header.encode("ascii"))

        # Create binary PLY records.
        data = np.empty(
            len(points),
            dtype=[
                ("x", "<f4"),
                ("y", "<f4"),
                ("z", "<f4"),
                ("red", "u1"),
                ("green", "u1"),
                ("blue", "u1"),
            ],
        )

        data["x"] = points[:, 0]
        data["y"] = points[:, 1]
        data["z"] = points[:, 2]

        data["red"] = colors[:, 0]
        data["green"] = colors[:, 1]
        data["blue"] = colors[:, 2]

        f.write(data.tobytes())


def _build_lidar_to_camera_transforms(calibration):
    """
    Build transformations:

        LiDAR -> CAM_01
        LiDAR -> CAM_02
        LiDAR -> CAM_03
        LiDAR -> CAM_04

    Calibration convention:

        p_target = T_source_target @ p_source
    """

    extrinsics = calibration["extrinsics"]

    T_cam01_lidar = _matrix_from_calibration(
        extrinsics["CAM_01_to_LiDAR"]
    )

    T_cam01_to_lidar = T_cam01_lidar

    # LiDAR -> CAM_01
    T_lidar_cam01 = np.linalg.inv(
        T_cam01_to_lidar
    )

    # CAM_01 -> CAM_02
    T_cam01_cam02 = _matrix_from_calibration(
        extrinsics["CAM_01_to_CAM_02"]
    )

    # CAM_02 -> CAM_04
    T_cam02_cam04 = _matrix_from_calibration(
        extrinsics["CAM_02_to_CAM_04"]
    )

    # CAM_03 -> CAM_01
    T_cam03_cam01 = _matrix_from_calibration(
        extrinsics["CAM_03_to_CAM_01"]
    )

    # ---------------------------------------------------------
    # LiDAR -> CAM_02
    #
    # LiDAR -> CAM_01 -> CAM_02
    # ---------------------------------------------------------

    T_lidar_cam02 = (
        T_cam01_cam02 @
        T_lidar_cam01
    )

    # ---------------------------------------------------------
    # LiDAR -> CAM_04
    #
    # LiDAR -> CAM_01 -> CAM_02 -> CAM_04
    # ---------------------------------------------------------

    T_lidar_cam04 = (
        T_cam02_cam04 @
        T_cam01_cam02 @
        T_lidar_cam01
    )

    # ---------------------------------------------------------
    # LiDAR -> CAM_03
    #
    # LiDAR -> CAM_01
    # and invert CAM_03 -> CAM_01
    # ---------------------------------------------------------

    T_cam01_cam03 = np.linalg.inv(
        T_cam03_cam01
    )

    T_lidar_cam03 = (
        T_cam01_cam03 @
        T_lidar_cam01
    )

    return {
        "CAM_01": T_lidar_cam01,
        "CAM_02": T_lidar_cam02,
        "CAM_03": T_lidar_cam03,
        "CAM_04": T_lidar_cam04,
    }


def _color_pointcloud(
    points_lidar,
    image,
    camera_calibration,
    T_lidar_camera,
):
    """
    Project LiDAR points into a camera image and obtain RGB colors.

    Only points that are in front of the camera and inside the
    image are retained.
    """

    K = np.asarray(
        camera_calibration["K"],
        dtype=np.float64,
    )

    radial = camera_calibration.get(
        "radialDistortion",
        [0.0, 0.0],
    )

    tangential = camera_calibration.get(
        "tangentialDistortion",
        [0.0, 0.0],
    )

    # OpenCV distortion vector:
    #
    # [k1, k2, p1, p2]
    #
    # Your calibration currently has only k1/k2.
    dist_coeffs = np.array(
        [
            radial[0],
            radial[1],
            tangential[0],
            tangential[1],
        ],
        dtype=np.float64,
    )

    # Transform LiDAR -> camera.
    points_camera = _transform_points(
        points_lidar,
        T_lidar_camera,
    )

    # Points behind camera cannot be projected.
    valid_z = points_camera[:, 2] > 0

    if not np.any(valid_z):
        return (
            np.empty((0, 3), dtype=np.float32),
            np.empty((0, 3), dtype=np.uint8),
        )

    valid_indices = np.flatnonzero(valid_z)

    points_camera_valid = points_camera[
        valid_z
    ]

    # Project points.
    #
    # rvec/tvec are zero because points have already
    # been transformed into camera coordinates.
    image_points, _ = cv2.projectPoints(
        points_camera_valid.reshape(-1, 1, 3),
        np.zeros(3),
        np.zeros(3),
        K,
        dist_coeffs,
    )

    image_points = image_points.reshape(-1, 2)

    height, width = image.shape[:2]

    u = np.round(
        image_points[:, 0]
    ).astype(np.int32)

    v = np.round(
        image_points[:, 1]
    ).astype(np.int32)

    inside = (
        (u >= 0) &
        (u < width) &
        (v >= 0) &
        (v < height)
    )

    if not np.any(inside):
        return (
            np.empty((0, 3), dtype=np.float32),
            np.empty((0, 3), dtype=np.uint8),
        )

    # Original LiDAR points corresponding to valid projections.
    selected_indices = valid_indices[inside]

    points_out = points_lidar[
        selected_indices
    ]

    # ---------------------------------------------------------
    # Get image colors.
    #
    # BaslerReader currently uses:
    #
    # cv2.COLOR_BAYER_RG2RGB
    #
    # so the returned image is RGB.
    # ---------------------------------------------------------

    colors = image[
        v[inside],
        u[inside],
        :3,
    ]

    colors = np.asarray(
        colors,
        dtype=np.uint8,
    )

    return (
        points_out.astype(np.float32),
        colors,
    )


def color_lidar_from_frame(
    rec_dir,
    take_name,
    frame_number,
    output_dir=None,
):
    """
    Color the temporally closest LiDAR scan using all four
    Basler images at the specified frame number.

    One colored PLY is generated for each camera.

    Parameters
    ----------
    rec_dir : str or Path
        Root recording directory.

        Expected structure:

            rec_dir/
                take_name.pcap
                take_name_0.json
                take_name/
                    CAM_01/
                    CAM_02/
                    CAM_03/
                    CAM_04/

    take_name : str
        Recording/take name.

    frame_number : int
        Basler frame index to use.

    output_dir : str or Path, optional
        Output directory.

        Default:
            rec_dir / take_name / "colored"

    Returns
    -------
    dict
        Information about the generated files.
    """

    # ---------------------------------------------------------
    # Calibration
    # ---------------------------------------------------------

    calibration_file = (
        Path("./calibration/calibration.json")
    )

    calibration = read_calibration(
        calibration_file
    )

    # ---------------------------------------------------------
    # Recording directories
    # ---------------------------------------------------------

    recording_dir = (
        Path(rec_dir) / take_name
    )

    if not recording_dir.exists():
        raise FileNotFoundError(
            f"Recording directory not found: "
            f"{recording_dir}"
        )

    if output_dir is None:
        output_dir = Path(
            rec_dir + "/" + take_name + "_export"
        )
    else:
        output_dir = Path(output_dir)

    output_dir.mkdir(
        parents=True,
        exist_ok=True,
    )

    # ---------------------------------------------------------
    # Readers
    # ---------------------------------------------------------

    cameras = {}

    for camera_name in (
        "CAM_01",
        "CAM_02",
        "CAM_03",
        "CAM_04",
    ):

        camera_dir = (
            recording_dir / camera_name
        )

        cameras[camera_name] = BaslerReader(
            camera_dir
        )

    lidar = OusterReader(
        rec_dir,
        take_name,
    )

    try:

        # -----------------------------------------------------
        # Build LiDAR -> camera transforms
        # -----------------------------------------------------

        transforms = (
            _build_lidar_to_camera_transforms(
                calibration
            )
        )

        results = {}

        # -----------------------------------------------------
        # Process each camera
        # -----------------------------------------------------

        for camera_name, camera in cameras.items():

            if frame_number < 0:
                raise ValueError(
                    "frame_number must be >= 0."
                )

            if frame_number >= camera.frame_count:
                raise IndexError(
                    f"{camera_name}: frame {frame_number} "
                    f"outside range "
                    f"[0, {camera.frame_count - 1}]"
                )

            print(
                f"\nProcessing {camera_name}, "
                f"frame {frame_number}..."
            )

            # -------------------------------------------------
            # Read image and timestamp
            # -------------------------------------------------

            image = camera.read_image(
                frame_number
            )

            camera_timestamp = camera.get_timestamp(
                frame_number
            )

            print(
                f"  Camera timestamp: "
                f"{camera_timestamp} ns"
            )

            # -------------------------------------------------
            # Find closest LiDAR scan
            # -------------------------------------------------

            points_lidar, lidar_timestamp, lidar_index, difference = (
                lidar.read_pointcloud_at_timestamp(
                    camera_timestamp
                )
            )

            print(
                f"  LiDAR index: {lidar_index}"
            )

            print(
                f"  LiDAR timestamp: "
                f"{lidar_timestamp} ns"
            )

            print(
                f"  Time difference: "
                f"{difference / 1e6:.3f} ms"
            )

            # -------------------------------------------------
            # Color point cloud
            # -------------------------------------------------

            camera_calibration = (
                calibration["intrinsics"][
                    camera_name
                ]
            )

            colored_points, colors = (
                _color_pointcloud(
                    points_lidar,
                    image,
                    camera_calibration,
                    transforms[camera_name],
                )
            )

            print(
                f"  LiDAR points: "
                f"{len(points_lidar)}"
            )

            print(
                f"  Colored points: "
                f"{len(colored_points)}"
            )

            # -------------------------------------------------
            # Write PLY
            # -------------------------------------------------

            output_file = (
                output_dir /
                f"{camera_name}_frame_{frame_number:06d}.ply"
            )

            _write_colored_ply(
                output_file,
                colored_points,
                colors,
            )

            print(
                f"  Saved: {output_file}"
            )

            results[camera_name] = {
                "camera_frame": frame_number,
                "camera_timestamp_ns": camera_timestamp,
                "lidar_index": lidar_index,
                "lidar_timestamp_ns": lidar_timestamp,
                "timestamp_difference_ns": difference,
                "num_lidar_points": len(points_lidar),
                "num_colored_points": len(colored_points),
                "output_file": str(output_file),
            }

        return results

    finally:

        for camera in cameras.values():
            camera.close()

        lidar.close()


rec_dir = "I:/JammerTestTestData/recordings"
take_name = "2026-09-10--10-36-44"
frame_number = 100

color_lidar_from_frame(rec_dir, take_name, frame_number)
