/*
 * Writes the map and the routes as JSON, for the Python plotting script.
 *
 * Output only, so there is no need for a JSON library. Each node, arc and route
 * sits on its own line, which keeps diffs and grep useful. A number is written
 * with 15 significant digits, enough for plotting and every cost shown.
 *
 * Top-level shape:
 *   {"nodes": [{id, name, x_m, y_m, elevation_m}],
 *    "arcs": [{from, to, length_m, speed_limit_kmh, closed}],
 *    "routes": [{label, algorithm, load, from, to, status, cost_s, distance_m,
 *                expanded, nodes}]}
 *
 * cost_s and distance_m are null when status is not "found". closed shows the
 * state after any closures, so it can differ from the road file.
 */
#ifndef JSON_OUT_H
#define JSON_OUT_H

#include <stdio.h>

#include "cost.h"
#include "graph.h"
#include "routing.h"

typedef struct {
    const char *label;
    const char *algorithm;
    Load load;
    int from;
    int to;
    RouteStatus status;
    const Route *route;
} RouteReport;

/* Writes the whole document. The caller checks the stream for write errors. */
void json_out_write(FILE *out, const Graph *graph, const RouteReport *reports,
                    int report_count);

#endif /* JSON_OUT_H */
