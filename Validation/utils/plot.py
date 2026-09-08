import numpy as np
import plotly.graph_objects as go


def plot_timestamp_timeline(timestamps, output="timestamp_timeline.html"):
    """
    Plot timestamps from multiple sensors on an interactive timeline.

    Parameters
    ----------
    timestamps : dict[str, np.ndarray]
        Dictionary mapping sensor names to 1D timestamp arrays in nanoseconds.
        Example:
            {
                "CAM_01": cam01_timestamps,
                "CAM_02": cam02_timestamps,
                "CAM_03": cam03_timestamps,
                "CAM_04": cam04_timestamps,
                "Ouster": ouster_timestamps,
            }

    output : str
        Output HTML filename.
    """

    # Convert everything to numpy arrays
    timestamps = {
        name: np.asarray(ts)
        for name, ts in timestamps.items()
    }

    # Remove empty arrays
    timestamps = {
        name: ts
        for name, ts in timestamps.items()
        if len(ts) > 0
    }

    if not timestamps:
        raise ValueError("No timestamps to plot.")

    # Use the earliest timestamp as t = 0
    t0 = min(np.min(ts) for ts in timestamps.values())

    fig = go.Figure()

    for sensor, ts in timestamps.items():

        # Convert ns -> seconds relative to start
        time_s = (ts - t0) / 1e9

        fig.add_trace(
            go.Scatter(
                x=time_s,
                y=[sensor] * len(ts),
                mode="markers",
                name=sensor,
                marker=dict(size=5),
                customdata=np.column_stack([
                    np.arange(len(ts)),
                    ts
                ]),
                hovertemplate=(
                    "<b>%{y}</b><br>"
                    "Time: %{x:.9f} s<br>"
                    "Frame: %{customdata[0]}<br>"
                    "Timestamp: %{customdata[1]} ns"
                    "<extra></extra>"
                ),
            )
        )

    fig.update_layout(
        title="Sensor Timestamp Timeline",
        xaxis_title="Time from first timestamp [s]",
        yaxis_title="Sensor",
        hovermode="closest",
        template="plotly_white",
        height=500,
    )

    fig.write_html(
        output,
        auto_open=True,
        include_plotlyjs=True,
    )

    return fig