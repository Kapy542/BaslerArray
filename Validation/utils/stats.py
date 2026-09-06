# -*- coding: utf-8 -*-

# ============================================================
# Timestamp helpers
# ============================================================

import numpy as np

from datetime import datetime, timezone


def ts2utc(timestamp_us: int) -> str:
    # dt = datetime.fromtimestamp(timestamp_us / 1_000_000, tz=timezone.utc) # from us
    dt = datetime.fromtimestamp(timestamp_us / 1_000_000_000, tz=timezone.utc) # from ns
    return dt.strftime("%Y-%m-%d %H:%M:%S.%f UTC")

def timestamp_statistics(timestamps):
    """
    Calculate statistics for a timestamp sequence.

    Timestamps are expected to be in nanoseconds.

    Returns a dictionary containing:
        duration_ns
        mean_period_ns
        std_period_ns
        min_period_ns
        max_period_ns
        mean_fps
    """

    timestamps = np.asarray(timestamps, dtype=np.int64)

    if len(timestamps) < 2:
        raise ValueError("At least two timestamps are required.")

    differences = np.diff(timestamps)

    mean_period = np.mean(differences)

    return {
        "duration_ns": int(timestamps[-1] - timestamps[0]),
        "mean_period_ns": float(mean_period),
        "std_period_ns": float(np.std(differences)),
        "min_period_ns": int(np.min(differences)),
        "max_period_ns": int(np.max(differences)),
        "mean_fps": float(1_000_000_000.0 / mean_period),
    }


def validate_timestamps(timestamps):
    """
    Check timestamp sequence for invalid values.

    Returns a dictionary with:
        monotonic
        duplicates
        backwards
        largest_gap_us
        negative_periods
    """

    timestamps = np.asarray(timestamps, dtype=np.int64)

    differences = np.diff(timestamps)

    negative_periods = np.sum(differences < 0)
    duplicates = np.sum(differences == 0)

    return {
        "monotonic": negative_periods == 0,
        "duplicates": int(duplicates),
        "backwards": int(negative_periods),
        "largest_gap_ns": int(np.max(differences)),
    }


def compare_timestamps(timestamps_a, timestamps_b, SYNC_TOLERANCE_US):
    """
    Compare two timestamp sequences.

    For every timestamp in A, find the closest timestamp in B.

    Returns statistics of the absolute differences.

    Timestamps are expected to be in nanoseconds.
    """

    timestamps_a = np.asarray(timestamps_a, dtype=np.int64)
    timestamps_b = np.asarray(timestamps_b, dtype=np.int64)

    differences = []

    for timestamp in timestamps_a:

        index = np.searchsorted(
            timestamps_b,
            timestamp
        )

        if index == 0:
            closest = timestamps_b[0]

        elif index >= len(timestamps_b):
            closest = timestamps_b[-1]

        else:
            previous = timestamps_b[index - 1]
            next_timestamp = timestamps_b[index]

            if abs(timestamp - previous) <= abs(next_timestamp - timestamp):
                closest = previous
            else:
                closest = next_timestamp

        differences.append(
            abs(timestamp - closest)
        )

    differences = np.asarray(
        differences,
        dtype=np.int64
    )

    return {
        "mean_ns": float(np.mean(differences)),
        "std_ns": float(np.std(differences)),
        "min_ns": int(np.min(differences)),
        "max_ns": int(np.max(differences)),
        "p95_ns": float(np.percentile(differences, 95)),
        "p99_ns": float(np.percentile(differences, 99)),
        "over_tolerance": int(
            np.sum(differences > SYNC_TOLERANCE_US)
        ),
        "total": len(differences),
    }

