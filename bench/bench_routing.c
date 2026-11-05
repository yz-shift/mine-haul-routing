/*
 * Times Dijkstra against A* and writes the results as JSON on standard output.
 * Usage: bench_routing PAIRS SEED MAP_FILE...
 *
 * Both algorithms answer the same random start and destination pairs, and the
 * run fails if they ever disagree on a cost. The time for a batch of pairs is
 * the median of an odd number of passes, which resists a one-off slow pass from
 * other work on the machine. The scan for the top speed is not part of the time,
 * because it happens once per map rather than once per search.
 */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "cost.h"
#include "graph.h"
#include "road_file.h"
#include "routing.h"

enum { PASSES = 5, ERROR_SIZE = 512 };

typedef struct {
    int from;
    int to;
} Pair;

typedef struct {
    double median_ms;
    long expanded;
    int found;
} BatchResult;

static double now_ms(void)
{
    struct timespec time;
    timespec_get(&time, TIME_UTC);
    return time.tv_sec * 1000.0 + time.tv_nsec / 1e6;
}

static int compare_doubles(const void *a, const void *b)
{
    double left = *(const double *)a;
    double right = *(const double *)b;
    return (left > right) - (left < right);
}

/*
 * Runs every pair once, using A* if top_speed_mps is positive, and stores each
 * pair's cost in costs. Returns false if a search fails outright.
 */
static bool run_pass(const Graph *graph, Load load, double top_speed_mps,
                     const Pair *pairs, int pair_count, double *costs, double *elapsed_ms,
                     BatchResult *result)
{
    result->expanded = 0;
    result->found = 0;
    double start = now_ms();
    for (int i = 0; i < pair_count; i++) {
        Route route;
        RouteStatus status =
            top_speed_mps > 0
                ? route_astar(graph, load, top_speed_mps, pairs[i].from, pairs[i].to, &route)
                : route_dijkstra(graph, load, pairs[i].from, pairs[i].to, &route);
        costs[i] = route.cost_s;
        result->expanded += route.expanded;
        result->found += status == ROUTE_FOUND;
        route_free(&route);
        if (status == ROUTE_OUT_OF_MEMORY || status == ROUTE_BAD_NODE) {
            return false;
        }
    }
    *elapsed_ms = now_ms() - start;
    return true;
}

static void write_batch(const char *name, const BatchResult *result, int pair_count)
{
    fprintf(stdout,
            "{\"algorithm\":\"%s\",\"total_ms\":%.3f,\"mean_ms\":%.4f,"
            "\"mean_expanded\":%.1f,\"found\":%d}",
            name, result->median_ms, result->median_ms / pair_count,
            (double)result->expanded / pair_count, result->found);
}

/*
 * Alternates the algorithms pass by pass, so that neither always runs first and
 * benefits from, or suffers, whatever the other left in the cache.
 */
static bool bench_load(const Graph *graph, Load load, const Pair *pairs, int pair_count,
                       bool first_load)
{
    double top_speed_mps = cost_top_speed_mps(graph, load);
    double *plain_costs = malloc((size_t)pair_count * sizeof(double));
    double *guided_costs = malloc((size_t)pair_count * sizeof(double));
    bool ok = plain_costs != NULL && guided_costs != NULL && top_speed_mps > 0;
    if (!ok) {
        fprintf(stderr, "error: out of memory, or the map has no roads\n");
    }
    double plain_ms[PASSES];
    double guided_ms[PASSES];
    BatchResult plain = {0};
    BatchResult guided = {0};

    for (int pass = 0; ok && pass < PASSES; pass++) {
        ok = run_pass(graph, load, 0, pairs, pair_count, plain_costs, &plain_ms[pass], &plain) &&
             run_pass(graph, load, top_speed_mps, pairs, pair_count, guided_costs,
                      &guided_ms[pass], &guided);
        for (int i = 0; ok && i < pair_count; i++) {
            if (plain_costs[i] != guided_costs[i]) {
                fprintf(stderr, "error: pair %d: dijkstra %.17g, astar %.17g\n", i,
                        plain_costs[i], guided_costs[i]);
                ok = false;
            }
        }
    }
    free(plain_costs);
    free(guided_costs);
    if (!ok) {
        return false;
    }

    qsort(plain_ms, PASSES, sizeof(double), compare_doubles);
    qsort(guided_ms, PASSES, sizeof(double), compare_doubles);
    plain.median_ms = plain_ms[PASSES / 2];
    guided.median_ms = guided_ms[PASSES / 2];

    printf("%s\n{\"load\":\"%s\",\"top_speed_kmh\":%.2f,\"results\":[", first_load ? "" : ",",
           load == LOAD_FULL ? "full" : "empty", top_speed_mps * 3.6);
    write_batch("dijkstra", &plain, pair_count);
    putchar(',');
    write_batch("astar", &guided, pair_count);
    fputs("]}", stdout);
    return true;
}

static bool bench_map(const char *path, int pair_count, bool first_map)
{
    char error[ERROR_SIZE] = "";
    Graph *graph = road_file_load(path, error, sizeof error);
    if (graph == NULL) {
        fprintf(stderr, "error: %s\n", error);
        return false;
    }
    Pair *pairs = malloc((size_t)pair_count * sizeof(Pair));
    bool ok = pairs != NULL;
    for (int i = 0; ok && i < pair_count; i++) {
        /* RAND_MAX is large on glibc, and the bias from % is too small to matter. */
        pairs[i] = (Pair){rand() % graph->node_count, rand() % graph->node_count};
    }
    if (ok) {
        printf("%s\n{\"nodes\":%d,\"arcs\":%d,\"pairs\":%d,\"loads\":[", first_map ? "" : ",",
               graph->node_count, graph->arc_count, pair_count);
        ok = bench_load(graph, LOAD_EMPTY, pairs, pair_count, true) &&
             bench_load(graph, LOAD_FULL, pairs, pair_count, false);
        fputs("\n]}", stdout);
    }
    free(pairs);
    graph_free(graph);
    return ok;
}

int main(int argc, char **argv)
{
    if (argc < 4) {
        fprintf(stderr, "Usage: %s PAIRS SEED MAP_FILE...\n", argv[0]);
        return 1;
    }
    int pair_count = atoi(argv[1]);
    if (pair_count <= 0) {
        fprintf(stderr, "error: PAIRS must be positive\n");
        return 1;
    }
    srand((unsigned)atoi(argv[2]));

    printf("{\"passes\":%d,\"maps\":[", PASSES);
    for (int i = 3; i < argc; i++) {
        if (!bench_map(argv[i], pair_count, i == 3)) {
            return 1;
        }
    }
    puts("\n]}");
    return 0;
}
