"""Draws the mine map and its routes from the JSON written by mine-route.

Usage:
    python viz/plot_route.py --title TITLE --output FIGURE.png INPUT.json [INPUT.json ...]

The map comes from the last input, whose closed roads are the latest state. Routes
from every input are drawn on it, so two runs can be compared on one map.
"""
import argparse
import json
import sys

# Categorical slots in a fixed order, so a colour never depends on the data.
ROUTE_COLOURS = ["#2a78d6", "#eb6834"]
# The first route is wider, so it still shows where a later route shares its road.
ROUTE_WIDTHS = [7.0, 2.5]
SURFACE = "#fcfcfb"
TEXT_PRIMARY = "#0b0b0b"
TEXT_SECONDARY = "#52514e"
ROAD = "#c9c8c1"
CLOSED_ROAD = "#0b0b0b"


def load_json(path):
    with open(path, encoding="utf-8") as file:
        return json.load(file)


def collect_routes(documents):
    """Returns every found route, each with the name used for it in the legend."""
    routes = []
    for document in documents:
        for route in document["routes"]:
            if route["status"] == "found":
                routes.append({**route, "name": route_name(route, documents)})
    return routes


def route_name(route, documents):
    """Names a route by whatever tells it apart in this figure."""
    if len(documents) > 1:
        return f"{route['load'].capitalize()} truck"
    return "Replanned after closure" if route["label"] == "replanned" else "Initial route"


def legend_text(route):
    minutes = route["cost_s"] / 60
    return f"{route['name']}: {minutes:.1f} min, {route['distance_m'] / 1000:.2f} km"


def unique_roads(arcs):
    """Merges the two arcs of a two-way road. Returns (from, to, one_way, closed)."""
    pairs = {(arc["from"], arc["to"]) for arc in arcs}
    roads = []
    seen = set()
    for arc in arcs:
        start, end = arc["from"], arc["to"]
        key = (min(start, end), max(start, end))
        if key not in seen:
            seen.add(key)
            roads.append((start, end, (end, start) not in pairs, arc["closed"]))
    return roads


def route_segments(route, nodes):
    """Returns the route as a list of ((x, y), (x, y)) segments."""
    points = [(nodes[i]["x_m"], nodes[i]["y_m"]) for i in route["nodes"]]
    return list(zip(points, points[1:]))


def labelled_node_ids(routes):
    """Labels only the nodes that routes use, so that the map stays readable."""
    return sorted({node for route in routes for node in route["nodes"]})


def draw(documents, title, output):
    """Draws and saves the figure. Imported here so the rest works without matplotlib."""
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    nodes = documents[-1]["nodes"]
    routes = collect_routes(documents)
    figure, axes = plt.subplots(figsize=(10, 9), facecolor=SURFACE)
    axes.set_facecolor(SURFACE)

    closed_label = "Closed road"  # Cleared after first use, for one legend entry.
    for start, end, one_way, closed in unique_roads(documents[-1]["arcs"]):
        xs = [nodes[start]["x_m"], nodes[end]["x_m"]]
        ys = [nodes[start]["y_m"], nodes[end]["y_m"]]
        if closed:
            axes.plot(xs, ys, color=CLOSED_ROAD, linewidth=2, linestyle=(0, (2, 2)), zorder=4,
                      label=closed_label)
            closed_label = None
            continue
        axes.plot(xs, ys, color=ROAD, linewidth=1.5, zorder=1, solid_capstyle="round")
        if one_way:
            axes.annotate("", xy=(xs[1], ys[1]), xytext=(xs[0], ys[0]), zorder=1,
                          arrowprops={"arrowstyle": "-|>", "color": ROAD, "linewidth": 1.5})

    for index, route in enumerate(routes):
        segments = route_segments(route, nodes)
        for segment_index, (start, end) in enumerate(segments):
            axes.plot([start[0], end[0]], [start[1], end[1]], color=ROUTE_COLOURS[index],
                      linewidth=ROUTE_WIDTHS[index], solid_capstyle="round", zorder=2 + index,
                      label=legend_text(route) if segment_index == 0 else None)

    elevations = [node["elevation_m"] for node in nodes]
    scatter = axes.scatter([node["x_m"] for node in nodes], [node["y_m"] for node in nodes],
                           c=elevations, cmap="Greys_r", s=70, edgecolors=SURFACE,
                           linewidths=2, zorder=5)
    colour_bar = figure.colorbar(scatter, ax=axes, shrink=0.6, pad=0.02)
    colour_bar.set_label("Elevation (m)", color=TEXT_SECONDARY)
    colour_bar.outline.set_visible(False)
    colour_bar.ax.tick_params(colors=TEXT_SECONDARY, length=0)

    for node_id in labelled_node_ids(routes):
        node = nodes[node_id]
        axes.annotate(node["name"].replace("_", " "), (node["x_m"], node["y_m"]),
                      xytext=(8, 6), textcoords="offset points", fontsize=8,
                      color=TEXT_PRIMARY, zorder=6,
                      bbox={"facecolor": SURFACE, "edgecolor": "none", "alpha": 0.8, "pad": 1})

    axes.set_title(title, loc="left", fontsize=14, color=TEXT_PRIMARY, pad=14)
    axes.set_xlabel("x (m)", color=TEXT_SECONDARY)
    axes.set_ylabel("y (m)", color=TEXT_SECONDARY)
    axes.set_aspect("equal")
    axes.tick_params(colors=TEXT_SECONDARY, length=0)
    for spine in axes.spines.values():
        spine.set_visible(False)
    axes.legend(loc="upper center", bbox_to_anchor=(0.5, -0.07), frameon=False, ncol=1,
                labelcolor=TEXT_PRIMARY)

    figure.savefig(output, dpi=150, bbox_inches="tight", facecolor=SURFACE)
    plt.close(figure)


def main():
    parser = argparse.ArgumentParser(description="Draw mine routes from mine-route JSON.")
    parser.add_argument("inputs", nargs="+", help="JSON files written by mine-route")
    parser.add_argument("--title", required=True)
    parser.add_argument("--output", required=True, help="image file to write")
    args = parser.parse_args()

    documents = [load_json(path) for path in args.inputs]
    if not collect_routes(documents):
        sys.exit("error: the inputs contain no route to draw")
    if len(collect_routes(documents)) > len(ROUTE_COLOURS):
        sys.exit(f"error: a figure can show at most {len(ROUTE_COLOURS)} routes")
    try:
        draw(documents, args.title, args.output)
    except ImportError:
        sys.exit("error: matplotlib is not installed; run: pip install -r viz/requirements.txt")


if __name__ == "__main__":
    main()
