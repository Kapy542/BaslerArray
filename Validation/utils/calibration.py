from pathlib import Path
import json


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

