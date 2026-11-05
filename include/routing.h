/*
 * Fastest-route search over the road graph.
 *
 * Complexity: O((V + E) log V) time and O(V) memory per search. The search stops
 * as soon as the destination leaves the queue, since its cost is final by then.
 */
#ifndef ROUTING_H
#define ROUTING_H

#include "cost.h"
#include "graph.h"

typedef enum {
    ROUTE_FOUND,
    ROUTE_UNREACHABLE, /* A valid request, but every way there is blocked. */
    ROUTE_BAD_NODE,    /* from or to is not a node in the graph. */
    ROUTE_OUT_OF_MEMORY,
} RouteStatus;

typedef struct {
    double cost_s;    /* INFINITY unless the status is ROUTE_FOUND. */
    double distance_m;
    int *nodes;       /* from ... to, or NULL unless the status is ROUTE_FOUND. */
    int node_count;
    int expanded;     /* Nodes taken off the queue, to compare search effort. */
} Route;

/*
 * Finds the fastest route for this load. Fills route whatever the status, so
 * route_free is always safe to call afterwards.
 */
RouteStatus route_dijkstra(const Graph *graph, Load load, int from, int to,
                           Route *route);

/*
 * Same result as route_dijkstra, usually with fewer nodes expanded. It adds a
 * heuristic to the queue priority: straight-line distance to the destination
 * divided by top_speed_mps.
 *
 * The heuristic never overestimates, which is what keeps the answer optimal,
 * provided that:
 *   - top_speed_mps comes from cost_top_speed_mps for this graph and load. It is
 *     a parameter so that the O(E) scan is not repeated for every search.
 *   - no road is shorter than the straight line between its ends. road_file
 *     enforces this, but a graph built by hand with graph_add_road must too.
 * A top_speed_mps of 0 or less turns the heuristic off.
 */
RouteStatus route_astar(const Graph *graph, Load load, double top_speed_mps,
                        int from, int to, Route *route);

/* Frees route->nodes and resets the route. */
void route_free(Route *route);

#endif /* ROUTING_H */
