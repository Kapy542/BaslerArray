import cv2
import numpy as np
from pathlib import Path
import matplotlib.pyplot as plt

from utils.basler_reader import BaslerReader
from utils.ouster_reader import OusterReader
from utils.pcd_utils import write_ply
from utils.stats import ts2utc
from utils.calibration import read_calibration, build_lidar_to_camera_transforms


rec_dir = "I:/JammerTestTestData/recordings"
take_name = "2026-09-10--10-36-44"
# cal_path = "D:/A_WorkWork/A_projects/BaslerArray/calibration/calibration.json"
cal_path = "F:/Jani/BaslerArray/calibration/calibration.json"

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
transforms = build_lidar_to_camera_transforms(calib)
print(transforms)

