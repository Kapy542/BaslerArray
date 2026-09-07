from pathlib import Path
import json

import cv2
import numpy as np

def read_metadata(camera_path: Path) -> dict:
    metadata_path = camera_path / "metadata.json"

    if not metadata_path.exists():
        raise FileNotFoundError(
            f"Metadata file not found: {metadata_path}"
        )

    with open(metadata_path, "r") as f:
        return json.load(f)

class BaslerReader:
    """
    Reader for a Basler camera recording.

    Expected directory structure:

        recording/
            frames.bin
            timestamps.bin

    frames.bin:
        Consecutive BayerRG8 frames, uint8.

    timestamps.bin:
        Consecutive uint64 timestamps in microseconds.
    """

    def __init__(
        self,
        recording_dir,
    ):
        self.recording_dir = Path(recording_dir)

        self.frames_file = self.recording_dir / "frames.bin"
        self.timestamps_file = self.recording_dir / "timestamps.bin"
        
        self.metadata = read_metadata(self.recording_dir)
        self.fps = self.metadata["fps"]
        self.width = self.metadata["width"]
        self.height = self.metadata["height"]
        self.frame_size = self.width * self.height

        # --------------------------------------------------------
        # Check files
        # --------------------------------------------------------

        if not self.frames_file.exists():
            raise FileNotFoundError(
                f"Frames file not found: {self.frames_file}"
            )

        if not self.timestamps_file.exists():
            raise FileNotFoundError(
                f"Timestamps file not found: {self.timestamps_file}"
            )

        # --------------------------------------------------------
        # Check file sizes
        # --------------------------------------------------------

        frame_file_size = self.frames_file.stat().st_size
        timestamp_file_size = self.timestamps_file.stat().st_size

        if frame_file_size % self.frame_size != 0:
            raise RuntimeError(
                f"frames.bin size ({frame_file_size}) is not "
                f"a multiple of frame size ({self.frame_size})."
            )

        if timestamp_file_size % 8 != 0:
            raise RuntimeError(
                f"timestamps.bin size ({timestamp_file_size}) is not "
                f"a multiple of 8 bytes."
            )

        self.num_frames = frame_file_size // self.frame_size
        self.num_timestamps = timestamp_file_size // 8

        if self.num_frames != self.num_timestamps:
            print(
                f"Frame/timestamp count mismatch: "
                f"{self.num_frames} frames vs "
                f"{self.num_timestamps} timestamps."
            )
            """
            raise RuntimeError(
                f"Frame/timestamp count mismatch: "
                f"{self.num_frames} frames vs "
                f"{self.num_timestamps} timestamps."
            )
            """

        # --------------------------------------------------------
        # Load timestamps once
        # --------------------------------------------------------

        self._timestamps = np.fromfile(
            self.timestamps_file,
            dtype=np.uint64
        )
        self._timestamps = self._timestamps #/ 1000 # From nano to micro seconds

        if len(self._timestamps) != self.num_timestamps:
            raise RuntimeError(
                "Could not read all timestamps."
            )

        # --------------------------------------------------------
        # Open frame file
        # --------------------------------------------------------

        self._frames = open(self.frames_file, "rb")

    def close(self):
        """Close the frame file."""

        if not self._frames.closed:
            self._frames.close()

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        self.close()

    # ------------------------------------------------------------
    # Basic information
    # ------------------------------------------------------------

    @property
    def frame_count(self):
        """Number of frames in the recording."""
        return self.num_frames

    def timestamps(self):
        """
        Return all frame timestamps.

        Returns:
            np.ndarray:
                uint64 timestamps in microseconds.
        """
        return self._timestamps

    def get_timestamp(self, index):
        """
        Get timestamp of a single frame.

        Args:
            index: Frame index.

        Returns:
            int:
                Timestamp in microseconds.
        """

        self._check_index(index)

        return int(self._timestamps[index])

    def first_timestamp(self):
        """Return timestamp of the first frame."""
        return int(self._timestamps[0])

    def last_timestamp(self):
        """Return timestamp of the last frame."""
        return int(self._timestamps[-1])

    # ------------------------------------------------------------
    # Image reading
    # ------------------------------------------------------------

    def read_image(self, index):
        """
        Read and decode a Basler image.

        Args:
            index: Frame index.

        Returns:
            np.ndarray:
                BGR OpenCV image.
        """

        self._check_index(index)

        # Jump directly to the requested frame.
        self._frames.seek(index * self.frame_size)

        raw = self._frames.read(self.frame_size)

        if len(raw) != self.frame_size:
            raise RuntimeError(
                f"Could not read complete frame {index}."
            )

        # BayerRG8
        bayer = np.frombuffer(
            raw,
            dtype=np.uint8
        ).reshape(
            (self.height, self.width)
        )

        # BayerRG8 -> BGR
        image = cv2.cvtColor(
            bayer,
            cv2.COLOR_BAYER_RG2RGB
        )

        return image

    def read_image_at_timestamp(self, timestamp):
        """
        Read the image whose timestamp is closest to the
        requested timestamp.

        Args:
            timestamp:
                Target timestamp in microseconds.

        Returns:
            image:
                BGR OpenCV image.

            actual_timestamp:
                Timestamp of the selected frame.

            index:
                Index of the selected frame.

            difference:
                Absolute timestamp difference in microseconds.
        """

        # Find insertion position in the sorted timestamp array.
        index = np.searchsorted(
            self._timestamps,
            timestamp
        )

        # Target is before the first frame.
        if index == 0:
            closest_index = 0

        # Target is after the last frame.
        elif index >= self.num_frames:
            closest_index = self.num_frames - 1

        # Target lies between two frames.
        else:
            previous_index = index - 1
            next_index = index

            previous_difference = abs(
                int(self._timestamps[previous_index]) - timestamp
            )

            next_difference = abs(
                int(self._timestamps[next_index]) - timestamp
            )
            print(previous_difference)
            print(next_difference)
            if previous_difference < next_difference:
                closest_index = previous_index
            else:
                closest_index = next_index

        actual_timestamp = int(
            self._timestamps[closest_index]
        )

        difference = abs(
            actual_timestamp - timestamp
        )

        image = self.read_image(closest_index)

        return (
            image,
            actual_timestamp,
            closest_index,
            difference,
        )

    # ------------------------------------------------------------
    # Timestamp analysis
    # ------------------------------------------------------------

    def timestamp_differences(self):
        """
        Calculate differences between consecutive timestamps.

        Returns:
            np.ndarray:
                Differences in microseconds.
        """

        return np.diff(self._timestamps)

    # ------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------

    def _check_index(self, index):
        if not isinstance(index, (int, np.integer)):
            raise TypeError(
                f"Frame index must be an integer, got {type(index)}."
            )

        if index < 0 or index >= self.num_frames:
            raise IndexError(
                f"Frame index {index} outside range "
                f"[0, {self.num_frames - 1}]."
            )