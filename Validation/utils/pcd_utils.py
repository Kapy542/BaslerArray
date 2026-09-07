from pathlib import Path

import numpy as np


def write_ply(
    points,
    filename,
    colors=None,
):
    """
    Write a point cloud to a PLY file.

    Parameters
    ----------
    points : np.ndarray
        Point coordinates with shape (N, 3).

    filename : str or Path
        Output PLY filename.

    colors : np.ndarray or None
        Optional RGB colors with shape (N, 3).
        Values can be uint8 [0, 255] or float [0, 1].
    """

    points = np.asarray(points)

    if points.ndim != 2 or points.shape[1] != 3:
        raise ValueError(
            f"Expected points with shape (N, 3), "
            f"got {points.shape}"
        )

    # Remove invalid points.
    valid = np.isfinite(points).all(axis=1)
    points = points[valid]

    if colors is not None:
        colors = np.asarray(colors)

        if colors.ndim != 2 or colors.shape[1] != 3:
            raise ValueError(
                f"Expected colors with shape (N, 3), "
                f"got {colors.shape}"
            )

        if len(colors) != len(valid):
            raise ValueError(
                "Number of colors must match number of points."
            )

        colors = colors[valid]

        # Convert float colors [0, 1] to uint8.
        if np.issubdtype(colors.dtype, np.floating):
            if colors.max() <= 1.0:
                colors = colors * 255.0

        colors = np.clip(
            colors,
            0,
            255,
        ).astype(np.uint8)

    filename = Path(filename)
    filename.parent.mkdir(
        parents=True,
        exist_ok=True,
    )

    with open(filename, "w") as f:

        f.write("ply\n")
        f.write("format ascii 1.0\n")
        f.write(f"element vertex {len(points)}\n")

        f.write("property float x\n")
        f.write("property float y\n")
        f.write("property float z\n")

        if colors is not None:
            f.write("property uchar red\n")
            f.write("property uchar green\n")
            f.write("property uchar blue\n")

        f.write("end_header\n")

        if colors is None:

            for x, y, z in points:
                f.write(
                    f"{x:.6f} {y:.6f} {z:.6f}\n"
                )

        else:

            for (x, y, z), (r, g, b) in zip(
                points,
                colors,
            ):
                f.write(
                    f"{x:.6f} {y:.6f} {z:.6f} "
                    f"{r} {g} {b}\n"
                )