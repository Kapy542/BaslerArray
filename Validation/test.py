import cv2
import numpy as np
from pathlib import Path
import matplotlib.pyplot as plt

from utils.basler_reader import BaslerReader
from utils.ouster_reader import OusterReader
from utils.pcd_utils import write_ply
from utils.stats import ts2utc
from utils.calibration import read_calibration

from project import _build_lidar_to_camera_transforms


rec_dir = "I:/JammerTestTestData/recordings"
take_name = "2026-09-10--10-36-44"
cal_path = "D:/A_WorkWork/A_projects/BaslerArray/calibration/calibration.json"

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

calib = read_calibration(cal_path)
transforms = _build_lidar_to_camera_transforms(calib)
print(transforms)




def plot_sensor_poses(
    calibration_file="../calibration/calibration.json",
    axis_length=100.0,
):
    """
    Plot camera and LiDAR poses in 3D using the calibration data.

    Coordinate convention from the calibration file:

        p_target = T_source_target @ p_source

    CAM_01 is used as the reference coordinate system.

    Parameters
    ----------
    calibration_file : str or Path
        Path to calibration.json.

    axis_length : float
        Length of coordinate axes to draw.

        The same units as the calibration translations are used.
        For example, if camera translations are in mm, use something
        like axis_length=100.

    Returns
    -------
    fig : matplotlib.figure.Figure
        Matplotlib figure.
    ax : matplotlib.axes._subplots.Axes3DSubplot
        3D axes.
    """

    # ---------------------------------------------------------
    # Load calibration
    # ---------------------------------------------------------

    calibration_file = Path(calibration_file)


    calibration = read_calibration(calibration_file)

    extrinsics = calibration["extrinsics"]

    # ---------------------------------------------------------
    # Helper
    # ---------------------------------------------------------

    def get_T(name):
        """Get 4x4 transform from calibration."""

        extrinsic = extrinsics[name]

        if "T" in extrinsic:
            return np.asarray(
                extrinsic["T"],
                dtype=np.float64,
            )

        R = np.asarray(
            extrinsic["R"],
            dtype=np.float64,
        )

        t = np.asarray(
            extrinsic["t"],
            dtype=np.float64,
        )

        T = np.eye(4)

        T[:3, :3] = R
        T[:3, 3] = t

        return T

    # ---------------------------------------------------------
    # Camera/reference transformations
    #
    # Everything will be expressed in CAM_01 coordinates.
    # ---------------------------------------------------------

    T_cam01 = np.eye(4)

    T_cam01_cam02 = get_T(
        "CAM_01_to_CAM_02"
    )

    T_cam03_cam01 = get_T(
        "CAM_03_to_CAM_01"
    )

    T_cam02_cam04 = get_T(
        "CAM_02_to_CAM_04"
    )

    T_cam01_lidar = get_T(
        "CAM_01_to_LiDAR"
    )

    # CAM_01 -> CAM_03
    T_cam01_cam03 = np.linalg.inv(
        T_cam03_cam01
    )

    # CAM_01 -> LiDAR
    # Already available.
    T_cam01_lidar = T_cam01_lidar

    poses = {
        "CAM_01": T_cam01,
        "CAM_02": T_cam01_cam02,
        "CAM_03": T_cam01_cam03,
        "CAM_04": (
            T_cam02_cam04 @
            T_cam01_cam02
        ),
        "LiDAR": T_cam01_lidar,
    }

    # ---------------------------------------------------------
    # Create plot
    # ---------------------------------------------------------

    fig = plt.figure(figsize=(11, 9))

    ax = fig.add_subplot(
        111,
        projection="3d",
    )

    # ---------------------------------------------------------
    # Plot sensor coordinate frames
    # ---------------------------------------------------------

    for name, T in poses.items():

        position = T[:3, 3]
        rotation = T[:3, :3]

        # Camera/LiDAR local axes expressed in CAM_01 frame.
        x_axis = rotation[:, 0]
        y_axis = rotation[:, 1]
        z_axis = rotation[:, 2]

        # Origin
        ax.scatter(
            position[0],
            position[1],
            position[2],
            s=70,
        )

        # X axis
        ax.quiver(
            position[0],
            position[1],
            position[2],
            x_axis[0],
            x_axis[1],
            x_axis[2],
            length=axis_length,
            normalize=True,
        )

        # Y axis
        ax.quiver(
            position[0],
            position[1],
            position[2],
            y_axis[0],
            y_axis[1],
            y_axis[2],
            length=axis_length,
            normalize=True,
        )

        # Z axis
        ax.quiver(
            position[0],
            position[1],
            position[2],
            z_axis[0],
            z_axis[1],
            z_axis[2],
            length=axis_length,
            normalize=True,
        )

        # Label
        ax.text(
            position[0],
            position[1],
            position[2],
            f"  {name}",
            fontsize=11,
        )

    # ---------------------------------------------------------
    # Draw reference coordinate system at CAM_01
    # ---------------------------------------------------------

    origin = np.zeros(3)

    ax.quiver(
        origin[0],
        origin[1],
        origin[2],
        1,
        0,
        0,
        length=axis_length,
        normalize=True,
    )

    ax.quiver(
        origin[0],
        origin[1],
        origin[2],
        0,
        1,
        0,
        length=axis_length,
        normalize=True,
    )

    ax.quiver(
        origin[0],
        origin[1],
        origin[2],
        0,
        0,
        1,
        length=axis_length,
        normalize=True,
    )

    # ---------------------------------------------------------
    # Connect sensor positions to show geometry
    # ---------------------------------------------------------

    camera_positions = np.array([
        poses["CAM_03"][:3, 3],
        poses["CAM_01"][:3, 3],
        poses["CAM_02"][:3, 3],
        poses["CAM_04"][:3, 3],
    ])

    ax.plot(
        camera_positions[:, 0],
        camera_positions[:, 1],
        camera_positions[:, 2],
        linestyle="--",
        linewidth=1,
    )

    # ---------------------------------------------------------
    # Labels
    # ---------------------------------------------------------

    ax.set_xlabel("X")
    ax.set_ylabel("Y")
    ax.set_zlabel("Z")

    ax.set_title(
        "Camera + LiDAR Calibration Poses\n"
        "(CAM_01 coordinate system)"
    )

    ax.set_box_aspect(
        [1, 1, 1]
    )

    plt.tight_layout()

    plt.show()

    return fig, ax


plot_sensor_poses(cal_path)