from pathlib import Path
import json
import numpy as np


def read_calibration(calibration_file) -> dict:
    """
    Read camera + LiDAR calibration data from a JSON file.

    Parameters
    ----------
    calibration_file : str or Path
        Path to the calibration JSON file.

    Returns
    -------
    dict
        Calibration data containing:
            - metadata
            - intrinsics
            - extrinsics

    Example
    -------
    calibration = read_calibration("calibration.json")

    K_cam01 = calibration["intrinsics"]["CAM_01"]["K"]

    T_cam01_lidar = calibration["extrinsics"]["CAM_01_to_LiDAR"]["T"]
    """
    
    print("Reading calibration from: ", calibration_file)

    calibration_file = Path(calibration_file)

    if not calibration_file.exists():
        raise FileNotFoundError(
            f"Calibration file not found: {calibration_file}"
        )

    if not calibration_file.is_file():
        raise ValueError(
            f"Calibration path is not a file: {calibration_file}"
        )

    with open(calibration_file, "r") as f:
        calibration = json.load(f)

    # Basic validation
    required_sections = [
        "metadata",
        "intrinsics",
        "extrinsics",
    ]

    for section in required_sections:
        if section not in calibration:
            raise RuntimeError(
                f"Calibration file is missing required section: "
                f"'{section}'"
            )

    return calibration


def build_lidar_to_camera_transforms(calib):
    """
    Build LiDAR -> camera transforms from the calibration dictionary.

    Calibration convention:
        p_target = T_source_target @ p_source

    Returns:
        transforms: dict
            {
                "CAM_01": T_lidar_to_cam01,
                "CAM_02": T_lidar_to_cam02,
                "CAM_03": T_lidar_to_cam03,
                "CAM_04": T_lidar_to_cam04,
            }
    """

    def get_T(name):
        return np.asarray(
            calib["extrinsics"][name]["T"],
            dtype=np.float64
        )

    # Direct calibration transform:
    #
    #   p_LiDAR = T_CAM01_LiDAR @ p_CAM01
    #
    # Therefore:
    #
    #   p_CAM01 = inv(T_CAM01_LiDAR) @ p_LiDAR
    #
    T_cam01_to_lidar = get_T("CAM_01_to_LiDAR")
    T_lidar_to_cam01 = np.linalg.inv(T_cam01_to_lidar)

    # CAM_02:
    #
    #   p_CAM02 = T_CAM01_CAM02 @ p_CAM01
    #
    # Therefore:
    #
    #   p_CAM02 =
    #       T_CAM01_CAM02 @ T_LiDAR_CAM01 @ p_LiDAR
    #
    T_cam01_to_cam02 = get_T("CAM_01_to_CAM_02")

    T_lidar_to_cam02 = (
        T_cam01_to_cam02
        @ T_lidar_to_cam01
    )

    # CAM_03:
    #
    # Given:
    #
    #   p_CAM01 = T_CAM03_CAM01 @ p_CAM03
    #
    # Therefore:
    #
    #   p_CAM03 = inv(T_CAM03_CAM01) @ p_CAM01
    #
    T_cam03_to_cam01 = get_T("CAM_03_to_CAM_01")
    T_cam01_to_cam03 = np.linalg.inv(T_cam03_to_cam01)

    T_lidar_to_cam03 = (
        T_cam01_to_cam03
        @ T_lidar_to_cam01
    )

    # CAM_04:
    #
    #   p_CAM02 = T_CAM01_CAM02 @ p_CAM01
    #
    #   p_CAM04 = T_CAM02_CAM04 @ p_CAM02
    #
    # Therefore:
    #
    #   p_CAM04 =
    #       T_CAM02_CAM04
    #       @ T_CAM01_CAM02
    #       @ T_LiDAR_CAM01
    #       @ p_LiDAR
    #
    T_cam02_to_cam04 = get_T("CAM_02_to_CAM_04")

    T_lidar_to_cam04 = (
        T_cam02_to_cam04
        @ T_cam01_to_cam02
        @ T_lidar_to_cam01
    )

    return {
        "CAM_01": T_lidar_to_cam01,
        "CAM_02": T_lidar_to_cam02,
        "CAM_03": T_lidar_to_cam03,
        "CAM_04": T_lidar_to_cam04,
    }