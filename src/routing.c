#include "routing.h"

#include <math.h>
#include <stdbool.h>
#include <stdlib.h>

#include "heap.h"

enum { NO_NODE = -1 };

/* Working memory for one search, indexed by node id. */
typedef struct {
    double *best_cost_s;
    int *previous_node;
    int *previous_arc; /* Index into the previous node's arcs. */
    Heap *queue;
    int destination;
    double top_speed_mps; /* 0 or less turns the heuristic off. */
} Search;

static void search_free(Search *search)
{
    free(search->best_cost_s);
    free(search->previous_node);
    free(search->previous_arc);
    heap_free(search->queue);
}

static bool search_init(Search *search, int node_count)
{
    size_t count = (size_t)node_count;
    *search = (Search){
        .best_cost_s = malloc(count * sizeof(double)),
        .previous_node = malloc(count * sizeof(int)),
        .previous_arc = malloc(count * sizeof(int)),
        .queue = heap_create(node_count),
    };
    if (search->best_cost_s == NULL || search->previous_node == NULL ||
        search->previous_arc == NULL || search->queue == NULL) {
        search_free(search);
        return false;
    }
    for (int i = 0; i < node_count; i++) {
        search->best_cost_s[i] = INFINITY;
        search->previous_node[i] = NO_NODE;
    }
    return true;
}

/* A lower bound on the seconds left from node to the destination. */
static double remaining_seconds(const Graph *graph, const Search *search, int node)
{
    if (search->top_speed_mps <= 0) {
        return 0;
    }
    const Node *here = &graph->nodes[node];
    const Node *there = &graph->nodes[search->destination];
    return hypot(there->x_m - here->x_m, there->y_m - here->y_m) /
           search->top_speed_mps;
}

/*
 * Relaxes every open arc out of node: if going through node beats the best known
 * way to a neighbour, record it and queue the neighbour at its new priority.
 * Priority is cost so far plus the heuristic, so with the heuristic off this is
 * exactly Dijkstra. A settled node can be queued again if a cheaper way to it
 * turns up, so no separate closed set is needed.
 * A closed arc costs INFINITY, which can never beat a best known cost.
 * The heap calls cannot fail: ids are valid, and a lower cost gives a lower key.
 */
static void expand_node(const Graph *graph, Load load, Search *search, int node_id)
{
    const Node *node = &graph->nodes[node_id];
    for (int i = 0; i < node->arc_count; i++) {
        const Arc *arc = &node->arcs[i];
        double new_cost_s = search->best_cost_s[node_id] +
                            cost_arc_seconds(graph, node_id, arc, load);
        if (new_cost_s >= search->best_cost_s[arc->to]) {
            continue;
        }
        search->best_cost_s[arc->to] = new_cost_s;
        search->previous_node[arc->to] = node_id;
        search->previous_arc[arc->to] = i;
        double priority = new_cost_s + remaining_seconds(graph, search, arc->to);
        if (heap_contains(search->queue, arc->to)) {
            heap_decrease_key(search->queue, arc->to, priority);
        } else {
            heap_insert(search->queue, arc->to, priority);
        }
    }
}

/* Walks the previous_node links back from the destination, then fills the route. */
static RouteStatus build_route(const Graph *graph, const Search *search, int to,
                               Route *route)
{
    int count = 0;
    for (int at = to; at != NO_NODE; at = search->previous_node[at]) {
        count++;
    }
    int *nodes = malloc((size_t)count * sizeof(int));
    if (nodes == NULL) {
        return ROUTE_OUT_OF_MEMORY;
    }

    double distance_m = 0;
    int slot = count;
    for (int at = to; at != NO_NODE; at = search->previous_node[at]) {
        nodes[--slot] = at;
        int before = search->previous_node[at];
        if (before != NO_NODE) {
            /* Uses the stored arc, because parallel roads can differ in length. */
            distance_m += graph->nodes[before].arcs[search->previous_arc[at]].length_m;
        }
    }

    route->cost_s = search->best_cost_s[to];
    route->distance_m = distance_m;
    route->nodes = nodes;
    route->node_count = count;
    return ROUTE_FOUND;
}

static void route_reset(Route *route)
{
    *route = (Route){.cost_s = INFINITY};
}

static RouteStatus run_search(const Graph *graph, Load load, double top_speed_mps,
                              int from, int to, Route *route)
{
    route_reset(route);
    if (from < 0 || from >= graph->node_count || to < 0 ||
        to >= graph->node_count) {
        return ROUTE_BAD_NODE;
    }

    Search search;
    if (!search_init(&search, graph->node_count)) {
        return ROUTE_OUT_OF_MEMORY;
    }

    search.destination = to;
    search.top_speed_mps = top_speed_mps;
    search.best_cost_s[from] = 0;
    heap_insert(search.queue, from, remaining_seconds(graph, &search, from));

    RouteStatus status = ROUTE_UNREACHABLE;
    int node_id;
    double priority;
    while (heap_pop_min(search.queue, &node_id, &priority)) {
        route->expanded++;
        if (node_id == to) {
            status = build_route(graph, &search, to, route);
            break;
        }
        expand_node(graph, load, &search, node_id);
    }

    search_free(&search);
    return status;
}

RouteStatus route_dijkstra(const Graph *graph, Load load, int from, int to,
                           Route *route)
{
    return run_search(graph, load, 0, from, to, route);
}

RouteStatus route_astar(const Graph *graph, Load load, double top_speed_mps,
                        int from, int to, Route *route)
{
    return run_search(graph, load, top_speed_mps, from, to, route);
}

void route_free(Route *route)
{
    free(route->nodes);
    route_reset(route);
}
