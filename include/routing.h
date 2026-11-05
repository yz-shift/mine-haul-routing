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

/* Frees route->nodes and resets the route. */
void route_free(Route *route);

#endif /* ROUTING_H */
