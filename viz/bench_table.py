"""Prints bench/results.json as a Markdown table. Needs only the standard library.

Usage: python viz/bench_table.py [bench/results.json]
"""
import json
import sys


def percent_saved(before, after):
    return (before - after) / before * 100


def table_rows(results):
    """Yields one row per map and load, comparing Dijkstra with A*."""
    for bench_map in results["maps"]:
        for load in bench_map["loads"]:
            by_name = {r["algorithm"]: r for r in load["results"]}
            dijkstra = by_name["dijkstra"]
            astar = by_name["astar"]
            yield [
                f"{bench_map['nodes']:,}",
                load["load"],
                f"{dijkstra['mean_ms']:.2f}",
                f"{astar['mean_ms']:.2f}",
                f"{percent_saved(dijkstra['mean_ms'], astar['mean_ms']):.0f}%",
                f"{dijkstra['mean_expanded']:,.0f}",
                f"{astar['mean_expanded']:,.0f}",
                f"{percent_saved(dijkstra['mean_expanded'], astar['mean_expanded']):.0f}%",
            ]


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "bench/results.json"
    with open(path, encoding="utf-8") as file:
        results = json.load(file)
    header = ["Nodes", "Load", "Dijkstra (ms)", "A* (ms)", "Time saved",
              "Dijkstra expanded", "A* expanded", "Expansions saved"]
    print("| " + " | ".join(header) + " |")
    print("|---|---|" + "--:|" * (len(header) - 2))
    for row in table_rows(results):
        print("| " + " | ".join(row) + " |")


if __name__ == "__main__":
    main()
