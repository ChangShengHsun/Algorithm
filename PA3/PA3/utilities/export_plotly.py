#!/usr/bin/env python3
"""
Convert routing solutions into an interactive Plotly (WebGL) HTML viewer.

Usage:
    python3 utilities/export_plotly.py \
        --cap inputs/toy1.cap \
        --route outputs/toy1.route \
        --net inputs/toy1.net \
        --out outputs/toy1_plot.html
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path
from typing import Dict, List, Tuple

try:
    import plotly.graph_objects as go
except ImportError:  # pragma: no cover - dependency hint
    print(
        "Error: plotly is required. Install it with "
        "'python3 -m pip install plotly'.",
        file=sys.stderr,
    )
    sys.exit(1)


Coord = Tuple[int, int, int]


def parse_cap(path: Path):
    tokens: List[str] = []
    with path.open() as fin:
        for raw in fin:
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            tokens.extend(line.split())

    it = iter(tokens)
    try:
        num_layers = int(next(it))
        if num_layers != 2:
            raise ValueError("Only 2-layer designs are supported.")
        x_size = int(next(it))
        y_size = int(next(it))
        via_cost = int(next(it))

        horizontal = [float(next(it)) for _ in range(max(0, x_size - 1))]
        vertical = [float(next(it)) for _ in range(max(0, y_size - 1))]

        layers = []
        for _ in range(num_layers):
            name = next(it)
            direction = next(it)
            layers.append((name, direction))
            for _ in range(x_size * y_size):
                next(it)  # skip capacities

    except StopIteration as exc:
        raise ValueError(f"Malformed .cap file: {path}") from exc

    return {
        "x_size": x_size,
        "y_size": y_size,
        "horizontal": horizontal,
        "vertical": vertical,
        "layers": layers,
        "via_cost": via_cost,
    }


def parse_route(path: Path):
    nets = []
    with path.open() as fin:
        lines = fin.readlines()

    idx = 0
    while idx < len(lines):
        name = lines[idx].strip()
        idx += 1
        if not name:
            continue

        while idx < len(lines) and lines[idx].strip() == "":
            idx += 1
        if idx >= len(lines) or not lines[idx].strip().startswith("("):
            raise ValueError(f"Expected '(' after net '{name}'")
        idx += 1

        segments = []
        while idx < len(lines):
            line = lines[idx].strip()
            idx += 1
            if not line:
                continue
            if line.startswith(")"):
                break
            parts = line.replace(",", " ").split()
            if len(parts) != 6:
                raise ValueError(f"Bad segment line '{line}' in net '{name}'")
            vals = list(map(int, parts))
            segments.append(((vals[0], vals[1], vals[2]), (vals[3], vals[4], vals[5])))
        nets.append({"name": name, "segments": segments})

    return nets


def parse_net(path: Path | None) -> Dict[str, List[Coord]]:
    pins: Dict[str, List[Coord]] = {}
    if path is None:
        return pins

    with path.open() as fin:
        lines = fin.readlines()

    idx = 0
    while idx < len(lines):
        name = lines[idx].strip()
        idx += 1
        if not name:
            continue
        while idx < len(lines) and lines[idx].strip() == "":
            idx += 1
        if idx >= len(lines) or not lines[idx].strip().startswith("("):
            raise ValueError(f"Expected '(' after net '{name}' in .net file")
        idx += 1

        coords = []
        while idx < len(lines):
            line = lines[idx].strip()
            idx += 1
            if not line:
                continue
            if line.startswith(")"):
                break
            parts = (
                line.replace("(", "")
                .replace(")", "")
                .replace(",", " ")
                .split()
            )
            if len(parts) != 3:
                raise ValueError(f"Malformed coordinate '{line}' in net '{name}'")
            coord = tuple(map(int, parts))
            coords.append(coord)
        if len(coords) >= 2:
            pins[name] = [coords[0], coords[-1]]

    return pins


def cumulative_positions(distances: List[float], count: int) -> List[float]:
    coords = [0.0]
    cur = 0.0
    for idx in range(1, count):
        step = distances[idx - 1] if idx - 1 < len(distances) else 1.0
        cur += step
        coords.append(cur)
    return coords


def build_segment_lines(nets, xs, ys, zs):
    traces = []
    colors = [
        "#267bb8",
        "#ff7f0e",
        "#2ca02c",
        "#d62728",
        "#9467bd",
        "#8c564b",
        "#e377c2",
        "#7f7f7f",
        "#bcbd22",
        "#17becf",
    ]
    for idx, net in enumerate(nets):
        x_vals: List[float] = []
        y_vals: List[float] = []
        z_vals: List[float] = []
        for (l1, j1, i1), (l2, j2, i2) in net["segments"]:
            x_vals.extend([xs[j1], xs[j2], None])
            y_vals.extend([ys[i1], ys[i2], None])
            z_vals.extend([zs[l1], zs[l2], None])

        traces.append(
            go.Scatter3d(
                x=x_vals,
                y=y_vals,
                z=z_vals,
                mode="lines",
                line=dict(width=5, color=colors[idx % len(colors)]),
                name=net["name"],
                hoverinfo="name",
            )
        )
    return traces


def build_pin_trace(pin_map, xs, ys, zs):
    if not pin_map:
        return None
    x_vals = []
    y_vals = []
    z_vals = []
    text = []
    for name, (pin1, pin2) in pin_map.items():
        for pin in (pin1, pin2):
            x_vals.append(xs[pin[1]])
            y_vals.append(ys[pin[2]])
            z_vals.append(zs[pin[0]])
            text.append(f"{name} pin")

    return go.Scatter3d(
        x=x_vals,
        y=y_vals,
        z=z_vals,
        mode="markers",
        marker=dict(size=6, color="black", symbol="circle"),
        name="Pins",
        text=text,
        hoverinfo="text",
    )


def generate_plot(cap_info, nets, pin_map):
    xs = cumulative_positions(cap_info["horizontal"], cap_info["x_size"])
    ys = cumulative_positions(cap_info["vertical"], cap_info["y_size"])
    spacing = max(
        1.0,
        0.2 * max(
            xs[-1] if len(xs) > 1 else 1.0,
            ys[-1] if len(ys) > 1 else 1.0,
        ),
    )
    zs = [0.0, spacing]

    traces = build_segment_lines(nets, xs, ys, zs)
    pin_trace = build_pin_trace(pin_map, xs, ys, zs)
    if pin_trace:
        traces.append(pin_trace)

    fig = go.Figure(data=traces)
    fig.update_layout(
        title="Routing Visualization (Plotly WebGL)",
        scene=dict(
            xaxis_title="Column",
            yaxis_title="Row",
            zaxis_title="Layer",
            aspectmode="data",
        ),
        legend=dict(itemsizing="constant"),
    )
    return fig


def main():
    parser = argparse.ArgumentParser(description="Export routing to Plotly HTML.")
    parser.add_argument("--cap", required=True, type=Path, help="Path to .cap file.")
    parser.add_argument("--route", required=True, type=Path, help="Path to .route file.")
    parser.add_argument(
        "--net",
        type=Path,
        default=None,
        help="Optional .net file for pin coordinates.",
    )
    parser.add_argument(
        "--out",
        required=True,
        type=Path,
        help="Destination HTML file (interactive WebGL viewer).",
    )
    args = parser.parse_args()

    cap_info = parse_cap(args.cap)
    nets = parse_route(args.route)
    pin_map = parse_net(args.net)

    fig = generate_plot(cap_info, nets, pin_map)
    fig.write_html(str(args.out), include_plotlyjs="cdn")
    print(f"Wrote Plotly viewer to {args.out}")


if __name__ == "__main__":
    main()
