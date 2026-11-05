/*
 * Command-line tool: plans a truck route, optionally replans after road closures,
 * and writes the result as JSON.
 *
 * An unreachable destination is a valid answer, reported in the JSON with exit
 * code 0. Exit code 1 means the request itself failed, with the reason on stderr.
 */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cost.h"
#include "graph.h"
#include "json_out.h"
#include "road_file.h"
#include "routing.h"

enum { ERROR_SIZE = 512, MAX_REPORTS = 2 };

static const char USAGE[] =
    "Usage: mine-route --map FILE --from NODE --to NODE [options]\n"
    "\n"
    "Options:\n"
    "  --load empty|full            Truck load (default: full)\n"
    "  --algorithm dijkstra|astar   Search algorithm (default: astar)\n"
    "  --close A,B                  After planning, close the road between nodes\n"
    "                               A and B and replan. Can be repeated\n"
    "  --at NODE                    Where the truck is when replanning\n"
    "                               (default: the --from node)\n"
    "  --output FILE                Write JSON to FILE (default: standard output)\n"
    "  --help                       Show this message\n";

typedef struct {
    const char *map_path;
    const char *from;
    const char *to;
    const char *at;
    const char *output_path;
    const char *algorithm;
    Load load;
    char **closures; /* Points into argv, which C allows a program to modify. */
    int closure_count;
    bool show_help;
} Options;

static bool parse_load(const char *text, Load *load)
{
    if (strcmp(text, "full") == 0) {
        *load = LOAD_FULL;
        return true;
    }
    if (strcmp(text, "empty") == 0) {
        *load = LOAD_EMPTY;
        return true;
    }
    fprintf(stderr, "error: --load must be empty or full, got '%s'\n", text);
    return false;
}

/* Handles one option that takes a value. Returns false if it is not valid. */
static bool apply_option(Options *options, const char *name, char *value)
{
    if (strcmp(name, "--map") == 0) {
        options->map_path = value;
    } else if (strcmp(name, "--from") == 0) {
        options->from = value;
    } else if (strcmp(name, "--to") == 0) {
        options->to = value;
    } else if (strcmp(name, "--at") == 0) {
        options->at = value;
    } else if (strcmp(name, "--output") == 0) {
        options->output_path = value;
    } else if (strcmp(name, "--close") == 0) {
        options->closures[options->closure_count++] = value;
    } else if (strcmp(name, "--load") == 0) {
        return parse_load(value, &options->load);
    } else if (strcmp(name, "--algorithm") == 0) {
        if (strcmp(value, "dijkstra") != 0 && strcmp(value, "astar") != 0) {
            fprintf(stderr, "error: --algorithm must be dijkstra or astar, got '%s'\n",
                    value);
            return false;
        }
        options->algorithm = value;
    } else {
        fprintf(stderr, "error: unknown option '%s'\n", name);
        return false;
    }
    return true;
}

static bool parse_options(int argc, char **argv, Options *options)
{
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            options->show_help = true;
            return true;
        }
        if (strncmp(argv[i], "--", 2) != 0) {
            fprintf(stderr, "error: unexpected argument '%s'\n", argv[i]);
            return false;
        }
        if (i + 1 >= argc) {
            fprintf(stderr, "error: %s needs a value\n", argv[i]);
            return false;
        }
        if (!apply_option(options, argv[i], argv[i + 1])) {
            return false;
        }
        i++;
    }
    if (options->map_path == NULL || options->from == NULL || options->to == NULL) {
        fprintf(stderr, "error: --map, --from and --to are required\n");
        return false;
    }
    if (options->at != NULL && options->closure_count == 0) {
        fprintf(stderr, "error: --at only applies when replanning with --close\n");
        return false;
    }
    return true;
}

static bool resolve_node(const Graph *graph, const char *name, int *id)
{
    *id = graph_find_node(graph, name);
    if (*id == GRAPH_NODE_NOT_FOUND) {
        fprintf(stderr, "error: no node named '%s'\n", name);
        return false;
    }
    if (*id == GRAPH_NODE_AMBIGUOUS) {
        fprintf(stderr, "error: more than one node is named '%s'\n", name);
        return false;
    }
    return true;
}

/* closure has the form "A,B". */
static bool apply_closure(Graph *graph, char *closure)
{
    char *comma = strchr(closure, ',');
    if (comma == NULL) {
        fprintf(stderr, "error: --close needs A,B, got '%s'\n", closure);
        return false;
    }
    *comma = '\0';
    const char *second = comma + 1;

    int a, b;
    if (!resolve_node(graph, closure, &a) || !resolve_node(graph, second, &b)) {
        return false;
    }
    if (graph_set_road_closed(graph, a, b, true) == 0) {
        fprintf(stderr, "error: no road between '%s' and '%s'\n", closure, second);
        return false;
    }
    return true;
}

/* Runs one search and fills its report. Returns false on a failed search. */
static bool plan(const Graph *graph, const Options *options, double top_speed_mps,
                 const char *label, int from, int to, Route *route, RouteReport *report)
{
    RouteStatus status =
        strcmp(options->algorithm, "astar") == 0
            ? route_astar(graph, options->load, top_speed_mps, from, to, route)
            : route_dijkstra(graph, options->load, from, to, route);
    *report = (RouteReport){
        .label = label,
        .algorithm = options->algorithm,
        .load = options->load,
        .from = from,
        .to = to,
        .status = status,
        .route = route,
    };
    if (status == ROUTE_OUT_OF_MEMORY || status == ROUTE_BAD_NODE) {
        fprintf(stderr, "error: search for the %s route failed\n", label);
        return false;
    }
    return true;
}

static bool write_output(const Options *options, const Graph *graph,
                         const RouteReport *reports, int report_count)
{
    if (options->output_path == NULL) {
        json_out_write(stdout, graph, reports, report_count);
        if (fflush(stdout) != 0 || ferror(stdout)) {
            fprintf(stderr, "error: cannot write to standard output\n");
            return false;
        }
        return true;
    }
    FILE *out = fopen(options->output_path, "w");
    if (out == NULL) {
        fprintf(stderr, "error: cannot open '%s' for writing\n", options->output_path);
        return false;
    }
    json_out_write(out, graph, reports, report_count);
    bool ok = !ferror(out);
    /* Buffered data may only fail to reach the disk when the file is closed. */
    if (fclose(out) != 0 || !ok) {
        fprintf(stderr, "error: cannot write to '%s'\n", options->output_path);
        return false;
    }
    return true;
}

/*
 * The initial route, then, if roads were closed, the replanned one. top speed is
 * computed once and stays valid afterwards because it includes closed roads.
 */
static bool run(const Options *options, Graph *graph)
{
    int from, to, at;
    if (!resolve_node(graph, options->from, &from) ||
        !resolve_node(graph, options->to, &to) ||
        !resolve_node(graph, options->at != NULL ? options->at : options->from, &at)) {
        return false;
    }

    double top_speed_mps = cost_top_speed_mps(graph, options->load);
    Route routes[MAX_REPORTS] = {{.nodes = NULL}, {.nodes = NULL}};
    RouteReport reports[MAX_REPORTS];
    int report_count = 0;

    bool ok = plan(graph, options, top_speed_mps, "initial", from, to, &routes[0],
                   &reports[0]);
    if (ok) {
        report_count = 1;
    }
    for (int i = 0; ok && i < options->closure_count; i++) {
        ok = apply_closure(graph, options->closures[i]);
    }
    if (ok && options->closure_count > 0) {
        ok = plan(graph, options, top_speed_mps, "replanned", at, to, &routes[1],
                  &reports[1]);
        report_count = 2;
    }
    if (ok) {
        ok = write_output(options, graph, reports, report_count);
    }
    route_free(&routes[0]);
    route_free(&routes[1]);
    return ok;
}

int main(int argc, char **argv)
{
    Options options = {.algorithm = "astar", .load = LOAD_FULL};
    /* argc is an upper bound on the number of --close options. */
    options.closures = malloc((size_t)argc * sizeof(char *));
    if (options.closures == NULL) {
        fprintf(stderr, "error: out of memory\n");
        return 1;
    }

    int exit_code = 1;
    if (!parse_options(argc, argv, &options)) {
        fprintf(stderr, "Try --help.\n");
    } else if (options.show_help) {
        fputs(USAGE, stdout);
        exit_code = 0;
    } else {
        char error[ERROR_SIZE] = "";
        Graph *graph = road_file_load(options.map_path, error, sizeof error);
        if (graph == NULL) {
            fprintf(stderr, "error: %s\n", error);
        } else {
            exit_code = run(&options, graph) ? 0 : 1;
            graph_free(graph);
        }
    }
    free(options.closures);
    return exit_code;
}
