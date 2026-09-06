from pathlib import Path

import numpy as np
# import open3d as o3d

from ouster.sdk import open_source, core


class OusterReader:
    """
    Reader for an Ouster PCAP recording.

    Expected directory structure:

        recording/
            ouster.pcap
            ouster.json

    Timestamps are returned in nanoseconds.

    Point clouds are returned as Open3D PointCloud objects.

    Only timestamps are kept in memory. Individual LiDAR scans
    are loaded from the PCAP when requested.
    """

    def __init__(
        self,
        recording_dir,
        take_name,
    ):
        pcap_filename = take_name + ".pcap"
        metadata_filename = take_name + ".json"
        
        self.recording_dir = Path(recording_dir)

        self.pcap_file = self.recording_dir / pcap_filename
        self.metadata_file = self.recording_dir / metadata_filename

        if not self.pcap_file.exists():
            raise FileNotFoundError(
                f"PCAP file not found: {self.pcap_file}"
            )

        if not self.metadata_file.exists():
            raise FileNotFoundError(
                f"Metadata file not found: {self.metadata_file}"
            )

        # Open indexed source.
        #
        # The index allows random access without storing
        # all scans in memory.
        self._source = open_source(
            str(self.pcap_file),
            index=True,
        )

        self._metadata = self._source.sensor_info[0]

        self._xyzlut = core.XYZLut(self._metadata)

        # Store only one timestamp per scan.
        self._timestamps = self._build_timestamp_index()

        self.num_frames = len(self._timestamps)

        if self.num_frames == 0:
            raise RuntimeError(
                "No LiDAR scans found in PCAP."
            )

    @property
    def frame_count(self):
        return self.num_frames

    def timestamps(self):
        return self._timestamps

    def get_timestamp(self, index):
        self._check_index(index)

        return int(self._timestamps[index])

    def first_timestamp(self):
        return int(self._timestamps[0])

    def last_timestamp(self):
        return int(self._timestamps[-1])

    # def read_pointcloud(self, index):
    #     """
    #     Read one LiDAR scan and convert it to an Open3D
    #     point cloud.

    #     Only the requested scan is loaded into memory.
    #     """

    #     self._check_index(index)

    #     scan_set = self._source[index]

    #     if not scan_set:
    #         raise RuntimeError(
    #             f"No scan found at index {index}."
    #         )

    #     scan = scan_set[0]

    #     if scan is None:
    #         raise RuntimeError(
    #             f"Scan at index {index} is None."
    #         )

    #     xyz = self._xyzlut(scan)

    #     points = xyz.reshape((-1, 3))

    #     # Remove invalid points.
    #     valid = np.isfinite(points).all(axis=1)
    #     points = points[valid]

    #     pointcloud = o3d.geometry.PointCloud()

    #     pointcloud.points = o3d.utility.Vector3dVector(
    #         points
    #     )

    #     return pointcloud

    # def read_pointcloud_at_timestamp(self, timestamp):
    #     """
    #     Read the LiDAR scan closest to the requested
    #     timestamp.

    #     Timestamp is in nanoseconds.

    #     Returns:

    #         pointcloud
    #         actual_timestamp
    #         index
    #         difference
    #     """

    #     timestamp = int(timestamp)

    #     index = np.searchsorted(
    #         self._timestamps,
    #         timestamp
    #     )

    #     if index == 0:
    #         closest_index = 0

    #     elif index >= self.num_frames:
    #         closest_index = self.num_frames - 1

    #     else:
    #         previous_index = index - 1
    #         next_index = index

    #         previous_difference = abs(
    #             int(self._timestamps[previous_index])
    #             - timestamp
    #         )

    #         next_difference = abs(
    #             int(self._timestamps[next_index])
    #             - timestamp
    #         )

    #         if previous_difference <= next_difference:
    #             closest_index = previous_index
    #         else:
    #             closest_index = next_index

    #     actual_timestamp = int(
    #         self._timestamps[closest_index]
    #     )

    #     difference = abs(
    #         actual_timestamp - timestamp
    #     )

    #     pointcloud = self.read_pointcloud(
    #         closest_index
    #     )

    #     return (
    #         pointcloud,
    #         actual_timestamp,
    #         closest_index,
    #         difference,
    #     )

    def timestamp_differences(self):
        return np.diff(self._timestamps)

    def close(self):
        if self._source is not None:
            self._source.close()
            self._source = None

    def __enter__(self):
        return self

    def __exit__(
        self,
        exc_type,
        exc_value,
        traceback,
    ):
        self.close()

    def _build_timestamp_index(self):
        """
        Build an array containing one timestamp per LiDAR scan.

        Ouster timestamps are stored in nanoseconds.

        Only the timestamp array is retained. The actual
        scans are NOT stored in memory.
        """

        timestamps = []

        for scan_set in self._source:

            if not scan_set:
                continue

            scan = scan_set[0]

            if scan is None:
                continue

            timestamp = self._get_scan_timestamp(scan)

            timestamps.append(timestamp)

        return np.asarray(
            timestamps,
            dtype=np.int64
        )

    @staticmethod
    def _get_scan_timestamp(scan):
        """
        Get the timestamp of the first valid column.

        Ouster timestamps are in nanoseconds.
        """

        timestamps = scan.timestamp

        valid = timestamps[timestamps > 0]

        if len(valid) == 0:
            raise RuntimeError(
                "LiDAR scan contains no valid timestamps."
            )
    
        return int(valid[0])

    def _check_index(self, index):
        if not isinstance(
            index,
            (int, np.integer)
        ):
            raise TypeError(
                f"Scan index must be an integer, "
                f"got {type(index)}."
            )

        if index < 0 or index >= self.num_frames:
            raise IndexError(
                f"Scan index {index} outside range "
                f"[0, {self.num_frames - 1}]."
            )