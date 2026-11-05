#include "graph.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum { INITIAL_CAPACITY = 4 };

/*
 * Makes room for one more item by doubling the capacity, if the array is full.
 * Returns the array, which may have moved, or NULL if out of memory. On failure
 * the original array and capacity are untouched and still owned by the caller.
 */
static void *reserve_one(void *items, int count, int *capacity, size_t item_size)
{
    if (count < *capacity) {
        return items;
    }
    if (*capacity > INT_MAX / 2) {
        return NULL;
    }
    int new_capacity = *capacity == 0 ? INITIAL_CAPACITY : *capacity * 2;
    if ((size_t)new_capacity > SIZE_MAX / item_size) {
        return NULL;
    }
    void *grown = realloc(items, (size_t)new_capacity * item_size);
    if (grown != NULL) {
        *capacity = new_capacity;
    }
    return grown;
}

static bool reserve_arc(Node *node)
{
    Arc *arcs = reserve_one(node->arcs, node->arc_count, &node->arc_capacity,
                            sizeof(Arc));
    if (arcs == NULL) {
        return false;
    }
    node->arcs = arcs;
    return true;
}

/* The caller must already have reserved room in node->arcs. */
static void push_arc(Graph *graph, Node *node, int to, double length_m,
                     double speed_limit_kmh, bool closed)
{
    node->arcs[node->arc_count++] = (Arc){
        .to = to,
        .length_m = length_m,
        .speed_limit_kmh = speed_limit_kmh,
        .closed = closed,
    };
    graph->arc_count++;
}

static bool is_valid_node(const Graph *graph, int id)
{
    return id >= 0 && id < graph->node_count;
}

Graph *graph_create(void)
{
    return calloc(1, sizeof(Graph));
}

void graph_free(Graph *graph)
{
    if (graph == NULL) {
        return;
    }
    for (int i = 0; i < graph->node_count; i++) {
        free(graph->nodes[i].name);
        free(graph->nodes[i].arcs);
    }
    free(graph->nodes);
    free(graph);
}

int graph_add_node(Graph *graph, const char *name, double x_m, double y_m,
                   double elevation_m)
{
    /* strdup is POSIX rather than C11, so copy the name by hand. */
    size_t name_size = strlen(name) + 1;
    char *name_copy = malloc(name_size);
    if (name_copy == NULL) {
        return -1;
    }
    memcpy(name_copy, name, name_size);

    Node *nodes = reserve_one(graph->nodes, graph->node_count,
                              &graph->node_capacity, sizeof(Node));
    if (nodes == NULL) {
        free(name_copy);
        return -1;
    }
    graph->nodes = nodes;

    int id = graph->node_count++;
    graph->nodes[id] = (Node){
        .name = name_copy,
        .x_m = x_m,
        .y_m = y_m,
        .elevation_m = elevation_m,
    };
    return id;
}

int graph_find_node(const Graph *graph, const char *name)
{
    int found = GRAPH_NODE_NOT_FOUND;
    for (int i = 0; i < graph->node_count; i++) {
        if (strcmp(graph->nodes[i].name, name) == 0) {
            if (found != GRAPH_NODE_NOT_FOUND) {
                return GRAPH_NODE_AMBIGUOUS;
            }
            found = i;
        }
    }
    return found;
}

static int set_arcs_closed(Node *node, int to, bool closed)
{
    int count = 0;
    for (int i = 0; i < node->arc_count; i++) {
        if (node->arcs[i].to == to) {
            node->arcs[i].closed = closed;
            count++;
        }
    }
    return count;
}

int graph_set_road_closed(Graph *graph, int a, int b, bool closed)
{
    if (!is_valid_node(graph, a) || !is_valid_node(graph, b)) {
        return 0;
    }
    return set_arcs_closed(&graph->nodes[a], b, closed) +
           set_arcs_closed(&graph->nodes[b], a, closed);
}

bool graph_add_road(Graph *graph, int from, int to, double length_m,
                    double speed_limit_kmh, bool one_way, bool closed)
{
    /*
     * A self-loop would also break the reservation below: both directions would
     * share one node, which is checked for one free slot but then gets two arcs.
     */
    if (!is_valid_node(graph, from) || !is_valid_node(graph, to) || from == to) {
        return false;
    }

    /* Reserve both directions first, so a failure cannot leave half a road. */
    Node *start = &graph->nodes[from];
    Node *end = &graph->nodes[to];
    if (!reserve_arc(start) || (!one_way && !reserve_arc(end))) {
        return false;
    }

    push_arc(graph, start, to, length_m, speed_limit_kmh, closed);
    if (!one_way) {
        push_arc(graph, end, from, length_m, speed_limit_kmh, closed);
    }
    return true;
}
