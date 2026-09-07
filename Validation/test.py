import cv2
import numpy as np
from pathlib import Path

from utils.basler_reader import BaslerReader
from utils.ouster_reader import OusterReader
from utils.pcd_utils import write_ply
from utils.stats import ts2utc


rec_dir = "I:\JammerTestTestData"
ouster_dir = "I:\JammerTestTestData"
take_name = "2026-09-05--17-56-44"

rec_dir = Path(rec_dir)
ouster_dir = Path(ouster_dir)

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