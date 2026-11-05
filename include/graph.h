/*
 * Directed road graph stored as an adjacency list.
 *
 * Each node owns a dynamic array of outgoing arcs rather than a linked list.
 * Routing spends most of its time scanning arcs, and contiguous memory is
 * friendlier to the cache. Adding an arc is still amortised O(1).
 *
 * Node ids are array indices, so lookup by id is O(1) and no hash map is needed.
 *
 * The structs are public so that the routing loops can read them directly.
 * Callers treat them as read-only and change the graph through these functions.
 */
#ifndef GRAPH_H
#define GRAPH_H

#include <stdbool.h>

typedef struct {
    int to;
    double length_m;
    double speed_limit_kmh;
    bool closed; /* Stored, not dropped, so that a road can be reopened. */
} Arc;

typedef struct {
    char *name;
    double x_m;
    double y_m;
    double elevation_m;
    Arc *arcs;
    int arc_count;
    int arc_capacity;
} Node;

typedef struct {
    Node *nodes;
    int node_count;
    int node_capacity;
    int arc_count; /* Total over all nodes. */
} Graph;

/* Returns an empty graph, or NULL if out of memory. */
Graph *graph_create(void);

/* Frees the graph and everything it owns. Does nothing if graph is NULL. */
void graph_free(Graph *graph);

/* Copies name. Returns the new node's id, or -1 if out of memory. */
int graph_add_node(Graph *graph, const char *name, double x_m, double y_m,
                   double elevation_m);

/*
 * Adds a road: one arc if one_way, otherwise an arc in each direction.
 * Returns false, leaving the graph unchanged, if an endpoint is not a valid
 * node id, if from equals to, or if out of memory.
 */
bool graph_add_road(Graph *graph, int from, int to, double length_m,
                    double speed_limit_kmh, bool one_way, bool closed);

#endif /* GRAPH_H */
