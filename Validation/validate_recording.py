import numpy as np

from pathlib import Path
from itertools import combinations

import argparse

from utils.basler_reader import BaslerReader
from utils.ouster_reader import OusterReader
from utils.stats import timestamp_statistics, validate_timestamps, compare_timestamps, ts2utc


# ============================================================
# Configuration
# ============================================================

# BASE_FOLDER = Path("I:/JammerTestTestData/")
# TAKE_NAME = "2026-09-05--17-56-44"

# RECORDING_DIR = BASE_FOLDER / TAKE_NAME

# OUSTER_FOLDER = Path("I:/JammerTestTestData/")
EXPECTED_OUSTER_HZ = 10.0

OUSTER_PERIOD_TOLERANCE_US = 1000.0
CAMERA_OUSTER_SYNC_TOLERANCE_US = 500.0


# Maximum allowed difference between synchronized cameras.
SYNC_TOLERANCE_NS = 500.0

# Expected frame period tolerance.
PERIOD_TOLERANCE_NS = 1000.0

# ============================================================
# Printing helpers
# ============================================================

def print_camera_statistics(name, reader):
    """
    Print numeric validation results for one camera.
    """
    
    EXPECTED_FPS = reader.fps

    timestamps = reader.timestamps()

    timestamp_check = validate_timestamps(timestamps)
    statistics = timestamp_statistics(timestamps)

    print()
    print("=" * 60)
    print(name)
    print("=" * 60)

    print(f"Frames:              {reader.frame_count:,}")

    print(
        f"First timestamp:     "
        f"{reader.first_timestamp():,} ns  /  "
        f"{ts2utc(reader.first_timestamp())}"
    )

    print(
        f"Last timestamp:      "
        f"{reader.last_timestamp():,} ns / "
        f"{ts2utc(reader.last_timestamp())}"
    )

    print(
        f"Duration:             "
        f"{statistics['duration_ns'] / 1_000_000:.3f} s"
    )

    print()
    print("Frame timing")

    print(
        f"Mean period:          "
        f"{statistics['mean_period_ns']:.3f} ns"
    )

    print(
        f"Period std:           "
        f"{statistics['std_period_ns']:.3f} ns"
    )

    print(
        f"Period min:           "
        f"{statistics['min_period_ns']} ns"
    )

    print(
        f"Period max:           "
        f"{statistics['max_period_ns']} ns"
    )

    print(
        f"Average FPS:          "
        f"{statistics['mean_fps']:.6f} Hz"
    )

    print()
    print("Timestamp integrity")

    print(
        f"Monotonic:             "
        f"{'PASS' if timestamp_check['monotonic'] else 'FAIL'}"
    )

    print(
        f"Duplicate timestamps: "
        f"{timestamp_check['duplicates']}"
    )

    print(
        f"Backwards timestamps:  "
        f"{timestamp_check['backwards']}"
    )

    print(
        f"Largest frame gap:     "
        f"{timestamp_check['largest_gap_ns']} ns"
    )

    # --------------------------------------------------------
    # Determine camera status
    # --------------------------------------------------------

    timing_ok = (
        abs(
            statistics["mean_period_ns"]
            - 1_000_000_000.0 / EXPECTED_FPS
        )
        <= PERIOD_TOLERANCE_NS
    )

    timestamp_ok = (
        timestamp_check["monotonic"]
        and timestamp_check["duplicates"] == 0
    )
    
    missing_ok = (
        abs(
            statistics["mean_period_ns"]
            - statistics["max_period_ns"]
        )
        <= PERIOD_TOLERANCE_NS
    )

    print()
    print(
        f"Timing:                "
        f"{'PASS' if timing_ok else 'FAIL'}"
    )

    print(
        f"Timestamp integrity:   "
        f"{'PASS' if timestamp_ok else 'FAIL'}"
    )
    
    print(
        f"Missing frames:        "
        f"{'PASS' if missing_ok else 'FAIL'}"
    )



def validate_ouster_timing(
    ouster,
    expected_hz,
    period_tolerance_us=1000.0,
):
    timestamps = np.asarray(
        ouster.timestamps(),
        dtype=np.int64
    )

    differences = np.diff(timestamps)

    if len(differences) == 0:
        print("Not enough Ouster scans for timing validation.")
        return

    duration_ns = (
        int(timestamps[-1])
        - int(timestamps[0])
    )

    mean_period_ns = np.mean(differences)
    std_period_ns = np.std(differences)

    min_period_ns = np.min(differences)
    max_period_ns = np.max(differences)

    mean_hz = 1_000_000_000.0 / mean_period_ns

    expected_period_ns = (
        1_000_000_000.0 / expected_hz
    )

    period_error_ns = (
        mean_period_ns
        - expected_period_ns
    )

    period_tolerance_ns = (
        period_tolerance_us * 1_000.0
    )

    monotonic = np.all(differences > 0)

    duplicate_timestamps = np.count_nonzero(
        differences == 0
    )

    backwards_timestamps = np.count_nonzero(
        differences < 0
    )

    largest_gap_ns = np.max(differences)

    period_ok = (
        abs(period_error_ns)
        <= period_tolerance_ns
    )

    timestamp_ok = (
        monotonic
        and duplicate_timestamps == 0
        and backwards_timestamps == 0
    )

    print()
    print("=" * 60)
    print("Ouster timing validation")
    print("=" * 60)

    print()
    print(f"Scans:                 {ouster.frame_count:,}")

    print(
        f"Duration:              "
        f"{duration_ns / 1_000_000_000:.3f} s"
    )

    print(
        f"First timestamp:       "
        f"{timestamps[0]} ns  /  "
        f"{ts2utc(timestamps[0])}"
    )

    print(
        f"Last timestamp:        "
        f"{timestamps[-1]} ns  /  "
        f"{ts2utc(timestamps[-1])}"
    )

    print()
    print(
        f"Expected frequency:    "
        f"{expected_hz:.3f} Hz"
    )

    print(
        f"Measured frequency:    "
        f"{mean_hz:.6f} Hz"
    )

    print(
        f"Mean period:           "
        f"{mean_period_ns / 1_000_000:.6f} ms"
    )

    print(
        f"Std period:            "
        f"{std_period_ns / 1_000:.3f} us"
    )

    print(
        f"Min period:            "
        f"{min_period_ns / 1_000:.3f} us"
    )

    print(
        f"Max period:            "
        f"{max_period_ns / 1_000:.3f} us"
    )

    print(
        f"Largest gap:           "
        f"{largest_gap_ns / 1_000_000:.3f} ms"
    )

    print()
    print(
        f"Duplicate timestamps:  "
        f"{duplicate_timestamps}"
    )

    print(
        f"Backwards timestamps:   "
        f"{backwards_timestamps}"
    )

    print()
    print(
        f"Timestamp integrity:    "
        f"{'PASS' if timestamp_ok else 'FAIL'}"
    )

    print(
        f"Frequency:              "
        f"{'PASS' if period_ok else 'FAIL'}"
    )
    
    
def compare_camera_to_ouster(
    camera,
    ouster,
    tolerance_us=500.0,
):
    camera_timestamps = np.asarray(
        camera.timestamps(),
        dtype=np.int64
    )

    ouster_timestamps = np.asarray(
        ouster.timestamps(),
        dtype=np.int64
    )

    differences = []

    for timestamp in camera_timestamps:

        index = np.searchsorted(
            ouster_timestamps,
            timestamp
        )

        if index == 0:
            closest_index = 0

        elif index >= len(ouster_timestamps):
            closest_index = len(ouster_timestamps) - 1

        else:
            previous_index = index - 1
            next_index = index

            previous_difference = abs(
                int(ouster_timestamps[previous_index])
                - int(timestamp)
            )

            next_difference = abs(
                int(ouster_timestamps[next_index])
                - int(timestamp)
            )

            if previous_difference <= next_difference:
                closest_index = previous_index
            else:
                closest_index = next_index

        difference = abs(
            int(ouster_timestamps[closest_index])
            - int(timestamp)
        )

        differences.append(difference)

    differences = np.asarray(
        differences,
        dtype=np.int64
    )

    tolerance_ns = int(
        tolerance_us * 1_000
    )

    return {
        "mean_ns": np.mean(differences),
        "std_ns": np.std(differences),
        "min_ns": np.min(differences),
        "max_ns": np.max(differences),
        "p95_ns": np.percentile(differences, 95),
        "p99_ns": np.percentile(differences, 99),
        "over_tolerance": np.count_nonzero(
            differences > tolerance_ns
        ),
        "total": len(differences),
    }


# ============================================================
# Main
# ============================================================

def main(BASLER_FOLDER, OUSTER_FOLDER, TAKE_NAME):
    
    BASLER_FOLDER = Path(BASLER_FOLDER) / TAKE_NAME
    
    print("=" * 60)
    print("Recording validation")
    print("=" * 60)

    print(f"Recording: {BASLER_FOLDER}")

    if not BASLER_FOLDER.exists():
        raise FileNotFoundError(
            f"Recording directory does not exist: {BASLER_FOLDER}"
        )

    # --------------------------------------------------------
    # Find cameras
    # --------------------------------------------------------

    camera_directories = sorted(
        path
        for path in BASLER_FOLDER.iterdir()
        if path.is_dir()
        and path.name.startswith("CAM_")
    )

    if not camera_directories:
        raise RuntimeError(
            f"No camera directories found in {BASLER_FOLDER}"
        )

    print()
    print(f"Found {len(camera_directories)} cameras:")

    for path in camera_directories:
        print(f"  {path.name}")

    # --------------------------------------------------------
    # Create readers
    # --------------------------------------------------------

    cameras = {}

    for camera_dir in camera_directories:

        try:
            cameras[camera_dir.name] = BaslerReader(
                camera_dir
            )

        except Exception as e:
            print()
            print(
                f"[FAIL] {camera_dir.name}: "
                f"Could not open recording"
            )
            print(f"       {e}")

    # --------------------------------------------------------
    # Individual camera checks
    # --------------------------------------------------------

    for name, reader in cameras.items():

        print_camera_statistics(
            name,
            reader
        )

    # --------------------------------------------------------
    # Camera synchronization
    # --------------------------------------------------------

    print()
    print()
    print("=" * 60)
    print("Basler camera synchronization")
    print("=" * 60)

    camera_names = sorted(cameras.keys())

    for name_a, name_b in combinations(camera_names, 2):

        timestamps_a = cameras[name_a].timestamps()
        timestamps_b = cameras[name_b].timestamps()

        result = compare_timestamps(
            timestamps_a,
            timestamps_b,
            SYNC_TOLERANCE_NS
        )

        print()
        print(f"{name_a} <-> {name_b}")

        print(
            f"  Mean difference:    "
            f"{result['mean_ns']:.3f} ns"
        )

        print(
            f"  Std difference:     "
            f"{result['std_ns']:.3f} ns"
        )

        print(
            f"  Min difference:     "
            f"{result['min_ns']} ns"
        )

        print(
            f"  Max difference:     "
            f"{result['max_ns']} ns"
        )

        print(
            f"  95th percentile:    "
            f"{result['p95_ns']:.3f} ns"
        )

        print(
            f"  99th percentile:    "
            f"{result['p99_ns']:.3f} ns"
        )

        print(
            f"  Over tolerance:     "
            f"{result['over_tolerance']:,} / "
            f"{result['total']:,}"
        )

        sync_ok = result["over_tolerance"] == 0

        print(
            f"  Result:              "
            f"{'PASS' if sync_ok else 'FAIL'}"
        )
        
    
    # --------------------------------------------------------
    # Frame count / duration consistency
    # --------------------------------------------------------
    
    print()
    print()
    print("=" * 60)
    print("Camera recording consistency")
    print("=" * 60)
    
    camera_names = sorted(cameras.keys())
    
    frame_counts = {
        name: cameras[name].frame_count
        for name in camera_names
    }
    
    durations = {
        name: (
            cameras[name].last_timestamp()
            - cameras[name].first_timestamp()
        )
        for name in camera_names
    }
    
    # Use the longest recording as the reference.
    reference_duration = max(durations.values())
    
    # Allow cameras to differ by this much in duration.
    DURATION_TOLERANCE_NS = 100_000_000  # 100 ms
    
    for name in camera_names:
    
        frame_count = frame_counts[name]
        duration_ns = durations[name]
    
        duration_difference_ns = (
            reference_duration - duration_ns
        )
    
        duration_ok = (
            abs(duration_difference_ns)
            <= DURATION_TOLERANCE_NS
        )
    
        print()
        print(f"{name}")
    
        print(
            f"  Frames:              "
            f"{frame_count:,}"
        )
    
        print(
            f"  Duration:             "
            f"{duration_ns / 1_000_000_000:.3f} s"
        )
    
        print(
            f"  Difference:           "
            f"{duration_difference_ns / 1_000_000:.3f} ms"
        )
    
        print(
            f"  Result:               "
            f"{'PASS' if duration_ok else 'FAIL'}"
        )


    # --------------------------------------------------------
    # Ouster statistics
    # --------------------------------------------------------

    ouster = OusterReader(OUSTER_FOLDER, TAKE_NAME)

    validate_ouster_timing(
        ouster,
        expected_hz=EXPECTED_OUSTER_HZ,
        period_tolerance_us=OUSTER_PERIOD_TOLERANCE_US,
    )

    # --------------------------------------------------------
    # Camera ↔ Ouster synchronization
    # --------------------------------------------------------
    
    print()
    print()
    print("=" * 60)
    print("Camera ↔ Ouster synchronization")
    print("=" * 60)
    
    for camera_name in sorted(cameras.keys()):
    
        camera = cameras[camera_name]
    
        stats = compare_camera_to_ouster(
            camera,
            ouster,
            tolerance_us=CAMERA_OUSTER_SYNC_TOLERANCE_US,
        )
    
        print()
        print(f"{camera_name} ↔ Ouster")
    
        print(
            f"  Mean:              "
            f"{stats['mean_ns'] / 1_000:.3f} us  /  "
            f"{stats['mean_ns'] / 1_000_000:.3f} ms "
        )
    
        print(
            f"  Std:               "
            f"{stats['std_ns'] / 1_000:.3f} us  /  "
            f"{stats['std_ns'] / 1_000_000:.3f} ms"
        )
    
        print(
            f"  Min:               "
            f"{stats['min_ns'] / 1_000:.3f} us  /  "
            f"{stats['min_ns'] / 1_000_000:.3f} ms"
        )
    
        print(
            f"  Max:               "
            f"{stats['max_ns'] / 1_000:.3f} us  /  "
            f"{stats['max_ns'] / 1_000_000:.3f} ms"
        )
    
        print(
            f"  P95:               "
            f"{stats['p95_ns'] / 1_000:.3f} us  /  "
            f"{stats['p95_ns'] / 1_000_000:.3f} ms"
        )
    
        print(
            f"  P99:               "
            f"{stats['p99_ns'] / 1_000:.3f} us  /  "
            f"{stats['p99_ns'] / 1_000_000:.3f} ms"
        )
    
        print(
            f"  Over tolerance:    "
            f"{stats['over_tolerance']} / "
            f"{stats['total']}"
        )
    
        sync_ok = (
            stats["over_tolerance"] == 0
        )
    
        print(
            f"  Result:            "
            f"{'PASS' if sync_ok else 'FAIL'}"
        )
    
    # --------------------------------------------------------
    # Close readers
    # --------------------------------------------------------

    for reader in cameras.values():
        reader.close()
    ouster.close()
    
    print()
    print("=" * 60)
    print("Validation complete")
    print("=" * 60)


if __name__ == "__main__":
    
    parser = argparse.ArgumentParser(
        description="Validate Basler and Ouster data"
    )

    parser.add_argument(
        "rec_dir",
        type=str,
        help="Recordings folder for Baslers. (eg. contains folder '2026-09-05--17-56-44' which contains 'CAM_01' etc )",
    )

    parser.add_argument(
        "ouster_dir",
        type=str,
        help="Base recordings folder for Ouster. Contains .pcap and .json files",
    )
    
    parser.add_argument(
        "take_name",
        type=str,
        help="Name of the recording (eg. 2026-09-05--17-56-44)",
    )
    
    args = parser.parse_args()
    
    main(args.rec_dir, args.ouster_dir, args.take_name)
    
    
    