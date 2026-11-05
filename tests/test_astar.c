#include <math.h>
#include <stdbool.h>
#include <stdlib.h>

#include "routing.h"
#include "test_framework.h"

/* A fixed seed, so a failure reproduces on every run. */
enum {
    RANDOM_SEED = 2521,
    GRID_SIDE = 30,
    PAIR_COUNT = 1000,
};

#define LIMIT 36.0
#define TOLERANCE 1e-9

static double random_between(double low, double high)
{
    return low + rand() / (RAND_MAX + 1.0) * (high - low);
}

static void add_random_road(Graph *graph, int a, int b)
{
    static const double limits[] = {20, 30, 40, 60};
    const Node *start = &graph->nodes[a];
    const Node *end = &graph->nodes[b];
    double straight_m = hypot(end->x_m - start->x_m, end->y_m - start->y_m);
    /* Never shorter than the straight line, as the heuristic requires. */
    double length_m = straight_m * random_between(1.0, 1.3);
    graph_add_road(graph, a, b, length_m, limits[rand() % 4], rand() % 100 < 15,
                   rand() % 100 < 8);
}

/*
 * A jittered grid over rolling hills, with mixed speed limits and some one-way
 * and closed roads. Grades reach the range where a full truck has to crawl.
 */
static Graph *create_random_mine(void)
{
    Graph *graph = graph_create();
    if (graph == NULL) {
        return NULL;
    }
    for (int row = 0; row < GRID_SIDE; row++) {
        for (int col = 0; col < GRID_SIDE; col++) {
            double x = col * 200.0 + random_between(-60, 60);
            double y = row * 200.0 + random_between(-60, 60);
            double elevation = 60 * sin(x / 700) + 40 * cos(y / 500) +
                               random_between(-8, 8);
            graph_add_node(graph, "N", x, y, elevation);
        }
    }
    for (int row = 0; row < GRID_SIDE; row++) {
        for (int col = 0; col < GRID_SIDE; col++) {
            int id = row * GRID_SIDE + col;
            if (col + 1 < GRID_SIDE) {
                add_random_road(graph, id, id + 1);
            }
            if (row + 1 < GRID_SIDE) {
                add_random_road(graph, id, id + GRID_SIDE);
            }
            if (col + 1 < GRID_SIDE && row + 1 < GRID_SIDE && rand() % 4 == 0) {
                add_random_road(graph, id, id + GRID_SIDE + 1);
            }
        }
    }
    return graph;
}

/* The cheapest open arc between two consecutive route nodes, or INFINITY. */
static double hop_cost(const Graph *graph, int from, int to, Load load)
{
    double best = INFINITY;
    const Node *node = &graph->nodes[from];
    for (int i = 0; i < node->arc_count; i++) {
        if (node->arcs[i].to == to) {
            double cost = cost_arc_seconds(graph, from, &node->arcs[i], load);
            best = cost < best ? cost : best;
        }
    }
    return best;
}

/* The route runs from from to to over open arcs and costs what it claims. */
static bool route_is_valid(const Graph *graph, const Route *route, int from, int to,
                           Load load)
{
    if (route->nodes[0] != from || route->nodes[route->node_count - 1] != to) {
        return false;
    }
    double total = 0;
    for (int i = 0; i + 1 < route->node_count; i++) {
        total += hop_cost(graph, route->nodes[i], route->nodes[i + 1], load);
    }
    return fabs(total - route->cost_s) < 1e-6;
}

typedef struct {
    int found;
    int unreachable;
    long dijkstra_expanded;
    long astar_expanded;
} CrossCheck;

/* Prints details itself, because ASSERT_TRUE only shows the call. */
static bool agree_on_random_pairs(const Graph *graph, Load load, CrossCheck *check)
{
    double top_speed = cost_top_speed_mps(graph, load);
    *check = (CrossCheck){0};
    for (int pair = 0; pair < PAIR_COUNT; pair++) {
        int from = rand() % graph->node_count;
        int to = rand() % graph->node_count;
        Route plain;
        Route guided;
        RouteStatus plain_status = route_dijkstra(graph, load, from, to, &plain);
        RouteStatus guided_status = route_astar(graph, load, top_speed, from, to, &guided);

        bool ok = plain_status == guided_status && plain.cost_s == guided.cost_s;
        if (ok && plain_status == ROUTE_FOUND) {
            ok = route_is_valid(graph, &plain, from, to, load) &&
                 route_is_valid(graph, &guided, from, to, load);
            check->found++;
        } else if (ok) {
            ok = plain_status == ROUTE_UNREACHABLE;
            check->unreachable++;
        }
        check->dijkstra_expanded += plain.expanded;
        check->astar_expanded += guided.expanded;
        if (!ok) {
            printf("  pair %d (%d -> %d): dijkstra %d %.17g, astar %d %.17g\n", pair,
                   from, to, (int)plain_status, plain.cost_s, (int)guided_status,
                   guided.cost_s);
        }
        route_free(&plain);
        route_free(&guided);
        if (!ok) {
            return false;
        }
    }
    printf("  %d found, %d unreachable, nodes expanded: dijkstra %ld, astar %ld\n",
           check->found, check->unreachable, check->dijkstra_expanded,
           check->astar_expanded);
    return true;
}

TEST(test_matches_dijkstra_on_1000_random_pairs_for_both_loads)
{
    srand(RANDOM_SEED);
    Graph *graph = create_random_mine();
    ASSERT_TRUE(graph != NULL);

    const Load loads[] = {LOAD_EMPTY, LOAD_FULL};
    for (int i = 0; i < 2; i++) {
        CrossCheck check;
        ASSERT_TRUE(agree_on_random_pairs(graph, loads[i], &check));
        /* Guards against a test that passes only because nothing was connected. */
        ASSERT_TRUE(check.found > PAIR_COUNT / 2);
        ASSERT_TRUE(check.astar_expanded < check.dijkstra_expanded);
    }
    graph_free(graph);
}

static Graph *create_line_with_side_road(void)
{
    /* Side road 1 points away from the destination, 3 is at the far end. */
    Graph *graph = graph_create();
    if (graph != NULL) {
        graph_add_node(graph, "START", 0, 0, 0);
        graph_add_node(graph, "SIDE", -500, 0, 0);
        graph_add_node(graph, "MIDDLE", 1000, 0, 0);
        graph_add_node(graph, "END", 2000, 0, 0);
        graph_add_road(graph, 0, 1, 500, LIMIT, false, false);
        graph_add_road(graph, 0, 2, 1000, LIMIT, false, false);
        graph_add_road(graph, 2, 3, 1000, LIMIT, false, false);
    }
    return graph;
}

TEST(test_heuristic_avoids_expanding_a_road_leading_away)
{
    Graph *graph = create_line_with_side_road();
    ASSERT_TRUE(graph != NULL);
    double top_speed = cost_top_speed_mps(graph, LOAD_EMPTY);

    Route plain;
    Route guided;
    ASSERT_EQ_INT(ROUTE_FOUND, route_dijkstra(graph, LOAD_EMPTY, 0, 3, &plain));
    ASSERT_EQ_INT(ROUTE_FOUND, route_astar(graph, LOAD_EMPTY, top_speed, 0, 3, &guided));
    ASSERT_EQ_INT(4, plain.expanded);
    ASSERT_EQ_INT(3, guided.expanded);
    ASSERT_NEAR(200, guided.cost_s, TOLERANCE);
    ASSERT_NEAR(2000, guided.distance_m, TOLERANCE);
    ASSERT_EQ_INT(3, guided.node_count);
    route_free(&plain);
    route_free(&guided);
    graph_free(graph);
}

TEST(test_zero_top_speed_behaves_exactly_like_dijkstra)
{
    Graph *graph = create_line_with_side_road();
    ASSERT_TRUE(graph != NULL);

    Route guided;
    ASSERT_EQ_INT(ROUTE_FOUND, route_astar(graph, LOAD_EMPTY, 0, 0, 3, &guided));
    ASSERT_NEAR(200, guided.cost_s, TOLERANCE);
    ASSERT_EQ_INT(4, guided.expanded);
    route_free(&guided);
    graph_free(graph);
}

TEST(test_graph_without_roads_is_handled)
{
    /* cost_top_speed_mps returns 0 here, so a division must not produce NaN. */
    Graph *graph = graph_create();
    ASSERT_TRUE(graph != NULL);
    graph_add_node(graph, "A", 0, 0, 0);
    graph_add_node(graph, "B", 0, 0, 0);
    double top_speed = cost_top_speed_mps(graph, LOAD_FULL);

    Route route;
    ASSERT_EQ_INT(ROUTE_FOUND, route_astar(graph, LOAD_FULL, top_speed, 0, 0, &route));
    ASSERT_EQ_INT(1, route.node_count);
    route_free(&route);
    ASSERT_EQ_INT(ROUTE_UNREACHABLE,
                  route_astar(graph, LOAD_FULL, top_speed, 0, 1, &route));
    route_free(&route);
    graph_free(graph);
}

TEST(test_unreachable_and_bad_nodes_are_reported)
{
    Graph *graph = create_line_with_side_road();
    ASSERT_TRUE(graph != NULL);
    graph_add_node(graph, "ISLAND", 5000, 0, 0);
    double top_speed = cost_top_speed_mps(graph, LOAD_EMPTY);

    Route route;
    ASSERT_EQ_INT(ROUTE_UNREACHABLE,
                  route_astar(graph, LOAD_EMPTY, top_speed, 0, 4, &route));
    ASSERT_TRUE(isinf(route.cost_s));
    route_free(&route);
    ASSERT_EQ_INT(ROUTE_BAD_NODE, route_astar(graph, LOAD_EMPTY, top_speed, 0, 9, &route));
    route_free(&route);
    graph_free(graph);
}

int main(void)
{
    RUN_TEST(test_matches_dijkstra_on_1000_random_pairs_for_both_loads);
    RUN_TEST(test_heuristic_avoids_expanding_a_road_leading_away);
    RUN_TEST(test_zero_top_speed_behaves_exactly_like_dijkstra);
    RUN_TEST(test_graph_without_roads_is_handled);
    RUN_TEST(test_unreachable_and_bad_nodes_are_reported);
    return TEST_SUMMARY();
}
