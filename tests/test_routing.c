#include "routing.h"

#include <math.h>
#include <stdbool.h>

#include "test_framework.h"

/*
 * Unless stated otherwise, these graphs are flat and use the empty load with a
 * 36 km/h limit. That is 10 m/s, so an arc's hand-worked cost is length / 10.
 */
#define LIMIT 36.0
#define TOLERANCE 1e-9

static Graph *create_flat_graph(int node_count)
{
    Graph *graph = graph_create();
    for (int i = 0; graph != NULL && i < node_count; i++) {
        graph_add_node(graph, "N", i * 1000.0, 0, 0);
    }
    return graph;
}

/* Prints the actual path itself, because ASSERT_TRUE only shows the call. */
static bool path_is(const Route *route, const int *expected, int count)
{
    bool same = route->node_count == count;
    for (int i = 0; same && i < count; i++) {
        same = route->nodes[i] == expected[i];
    }
    if (!same) {
        printf("  actual path:");
        for (int i = 0; i < route->node_count; i++) {
            printf(" %d", route->nodes[i]);
        }
        printf("\n");
    }
    return same;
}

TEST(test_follows_a_chain_of_roads)
{
    Graph *graph = create_flat_graph(4);
    ASSERT_TRUE(graph != NULL);
    graph_add_road(graph, 0, 1, 1000, LIMIT, false, false);
    graph_add_road(graph, 1, 2, 2000, LIMIT, false, false);
    graph_add_road(graph, 2, 3, 3000, LIMIT, false, false);

    Route route;
    ASSERT_EQ_INT(ROUTE_FOUND, route_dijkstra(graph, LOAD_EMPTY, 0, 3, &route));
    ASSERT_NEAR(600, route.cost_s, TOLERANCE);
    ASSERT_NEAR(6000, route.distance_m, TOLERANCE);
    ASSERT_TRUE(path_is(&route, (const int[]){0, 1, 2, 3}, 4));
    route_free(&route);
    graph_free(graph);
}

/*
 *        B
 *   1000/ \1000       A-B-D = 200 s
 *      A   D          A-C-D = 150 s, the answer
 *    500\ /1000       A-D direct = 400 s
 *        C
 */
TEST(test_matches_a_hand_worked_graph)
{
    Graph *graph = create_flat_graph(4);
    ASSERT_TRUE(graph != NULL);
    graph_add_road(graph, 0, 1, 1000, LIMIT, false, false);
    graph_add_road(graph, 1, 3, 1000, LIMIT, false, false);
    graph_add_road(graph, 0, 2, 500, LIMIT, false, false);
    graph_add_road(graph, 2, 3, 1000, LIMIT, false, false);
    graph_add_road(graph, 0, 3, 4000, LIMIT, false, false);

    Route route;
    ASSERT_EQ_INT(ROUTE_FOUND, route_dijkstra(graph, LOAD_EMPTY, 0, 3, &route));
    ASSERT_NEAR(150, route.cost_s, TOLERANCE);
    ASSERT_NEAR(1500, route.distance_m, TOLERANCE);
    ASSERT_TRUE(path_is(&route, (const int[]){0, 2, 3}, 3));
    route_free(&route);
    graph_free(graph);
}

TEST(test_later_discovery_replaces_a_worse_queued_cost)
{
    /* Node 2 is first queued at 300 s, then improved to 200 s through node 1. */
    Graph *graph = create_flat_graph(4);
    ASSERT_TRUE(graph != NULL);
    graph_add_road(graph, 0, 2, 3000, LIMIT, true, false);
    graph_add_road(graph, 0, 1, 1000, LIMIT, true, false);
    graph_add_road(graph, 1, 2, 1000, LIMIT, true, false);
    graph_add_road(graph, 2, 3, 1000, LIMIT, true, false);

    Route route;
    ASSERT_EQ_INT(ROUTE_FOUND, route_dijkstra(graph, LOAD_EMPTY, 0, 3, &route));
    ASSERT_NEAR(300, route.cost_s, TOLERANCE);
    ASSERT_TRUE(path_is(&route, (const int[]){0, 1, 2, 3}, 4));
    route_free(&route);
    graph_free(graph);
}

TEST(test_improved_node_is_expanded_at_its_new_priority)
{
    /*
     * Node 2 is queued at 300 s, then improved to 200 s through node 1. If its
     * queue priority stayed at 300 s, the direct road to node 3 would come off
     * the queue first at 250 s and wrongly beat the true answer of 210 s.
     */
    Graph *graph = create_flat_graph(4);
    ASSERT_TRUE(graph != NULL);
    graph_add_road(graph, 0, 2, 3000, LIMIT, true, false);
    graph_add_road(graph, 0, 1, 1000, LIMIT, true, false);
    graph_add_road(graph, 1, 2, 1000, LIMIT, true, false);
    graph_add_road(graph, 2, 3, 100, LIMIT, true, false);
    graph_add_road(graph, 0, 3, 2500, LIMIT, true, false);

    Route route;
    ASSERT_EQ_INT(ROUTE_FOUND, route_dijkstra(graph, LOAD_EMPTY, 0, 3, &route));
    ASSERT_NEAR(210, route.cost_s, TOLERANCE);
    ASSERT_TRUE(path_is(&route, (const int[]){0, 1, 2, 3}, 4));
    route_free(&route);
    graph_free(graph);
}

TEST(test_minimises_time_rather_than_distance)
{
    /* Direct: 1000 m at 18 km/h = 200 s. Around: 1500 m at 36 km/h = 150 s. */
    Graph *graph = create_flat_graph(3);
    ASSERT_TRUE(graph != NULL);
    graph_add_road(graph, 0, 2, 1000, 18, false, false);
    graph_add_road(graph, 0, 1, 500, LIMIT, false, false);
    graph_add_road(graph, 1, 2, 1000, LIMIT, false, false);

    Route route;
    ASSERT_EQ_INT(ROUTE_FOUND, route_dijkstra(graph, LOAD_EMPTY, 0, 2, &route));
    ASSERT_NEAR(150, route.cost_s, TOLERANCE);
    ASSERT_NEAR(1500, route.distance_m, TOLERANCE);
    ASSERT_TRUE(path_is(&route, (const int[]){0, 1, 2}, 3));
    route_free(&route);
    graph_free(graph);
}

TEST(test_distance_comes_from_the_parallel_road_actually_used)
{
    /* 1000 m at 18 km/h = 200 s, against 1500 m at 36 km/h = 150 s. */
    Graph *graph = create_flat_graph(2);
    ASSERT_TRUE(graph != NULL);
    graph_add_road(graph, 0, 1, 1000, 18, false, false);
    graph_add_road(graph, 0, 1, 1500, LIMIT, false, false);

    Route route;
    ASSERT_EQ_INT(ROUTE_FOUND, route_dijkstra(graph, LOAD_EMPTY, 0, 1, &route));
    ASSERT_NEAR(150, route.cost_s, TOLERANCE);
    ASSERT_NEAR(1500, route.distance_m, TOLERANCE);
    route_free(&route);
    graph_free(graph);
}

TEST(test_start_equal_to_destination_costs_nothing)
{
    Graph *graph = create_flat_graph(2);
    ASSERT_TRUE(graph != NULL);

    Route route;
    ASSERT_EQ_INT(ROUTE_FOUND, route_dijkstra(graph, LOAD_EMPTY, 1, 1, &route));
    ASSERT_NEAR(0, route.cost_s, 0);
    ASSERT_NEAR(0, route.distance_m, 0);
    ASSERT_EQ_INT(1, route.expanded);
    ASSERT_TRUE(path_is(&route, (const int[]){1}, 1));
    route_free(&route);
    graph_free(graph);
}

TEST(test_disconnected_destination_is_reported_as_unreachable)
{
    Graph *graph = create_flat_graph(3);
    ASSERT_TRUE(graph != NULL);
    graph_add_road(graph, 0, 1, 1000, LIMIT, false, false);

    Route route;
    ASSERT_EQ_INT(ROUTE_UNREACHABLE, route_dijkstra(graph, LOAD_EMPTY, 0, 2, &route));
    ASSERT_TRUE(isinf(route.cost_s));
    ASSERT_TRUE(route.nodes == NULL);
    ASSERT_EQ_INT(0, route.node_count);
    ASSERT_EQ_INT(2, route.expanded);
    route_free(&route);
    graph_free(graph);
}

TEST(test_one_way_road_cannot_be_driven_backwards)
{
    Graph *graph = create_flat_graph(2);
    ASSERT_TRUE(graph != NULL);
    graph_add_road(graph, 0, 1, 1000, LIMIT, true, false);

    Route route;
    ASSERT_EQ_INT(ROUTE_FOUND, route_dijkstra(graph, LOAD_EMPTY, 0, 1, &route));
    route_free(&route);
    ASSERT_EQ_INT(ROUTE_UNREACHABLE, route_dijkstra(graph, LOAD_EMPTY, 1, 0, &route));
    route_free(&route);
    graph_free(graph);
}

TEST(test_closed_road_forces_a_detour)
{
    /* The closed direct road would take 100 s. The detour takes 300 s. */
    Graph *graph = create_flat_graph(3);
    ASSERT_TRUE(graph != NULL);
    graph_add_road(graph, 0, 2, 1000, LIMIT, false, true);
    graph_add_road(graph, 0, 1, 1500, LIMIT, false, false);
    graph_add_road(graph, 1, 2, 1500, LIMIT, false, false);

    Route route;
    ASSERT_EQ_INT(ROUTE_FOUND, route_dijkstra(graph, LOAD_EMPTY, 0, 2, &route));
    ASSERT_NEAR(300, route.cost_s, TOLERANCE);
    ASSERT_TRUE(path_is(&route, (const int[]){0, 1, 2}, 3));
    route_free(&route);
    graph_free(graph);
}

TEST(test_closing_the_only_road_makes_the_destination_unreachable)
{
    Graph *graph = create_flat_graph(2);
    ASSERT_TRUE(graph != NULL);
    graph_add_road(graph, 0, 1, 1000, LIMIT, false, true);

    Route route;
    ASSERT_EQ_INT(ROUTE_UNREACHABLE, route_dijkstra(graph, LOAD_EMPTY, 0, 1, &route));
    route_free(&route);
    graph_free(graph);
}

/*
 * Two ways up the same 100 m hill.
 *   Steep:  2 x 250 m at 20%. Empty 141 s. Full is past its traction limit: 450 s.
 *   Gentle: 2 x 500 m at 10%. Empty 159 s. Full 398 s.
 */
TEST(test_load_changes_the_route)
{
    Graph *graph = graph_create();
    ASSERT_TRUE(graph != NULL);
    graph_add_node(graph, "BOTTOM", 0, 0, 0);
    graph_add_node(graph, "STEEP_MID", 150, 50, 50);
    graph_add_node(graph, "GENTLE_MID", 150, -300, 50);
    graph_add_node(graph, "TOP", 300, 0, 100);
    graph_add_road(graph, 0, 1, 250, 60, false, false);
    graph_add_road(graph, 1, 3, 250, 60, false, false);
    graph_add_road(graph, 0, 2, 500, 60, false, false);
    graph_add_road(graph, 2, 3, 500, 60, false, false);

    Route route;
    ASSERT_EQ_INT(ROUTE_FOUND, route_dijkstra(graph, LOAD_EMPTY, 0, 3, &route));
    ASSERT_NEAR(141.0, route.cost_s, 0.1);
    ASSERT_TRUE(path_is(&route, (const int[]){0, 1, 3}, 3));
    route_free(&route);

    ASSERT_EQ_INT(ROUTE_FOUND, route_dijkstra(graph, LOAD_FULL, 0, 3, &route));
    ASSERT_NEAR(398.6, route.cost_s, 0.1);
    ASSERT_TRUE(path_is(&route, (const int[]){0, 2, 3}, 3));
    route_free(&route);
    graph_free(graph);
}

TEST(test_stops_as_soon_as_the_destination_is_reached)
{
    /* Node 2 is far away, so it must not be expanded on the way to node 1. */
    Graph *graph = create_flat_graph(3);
    ASSERT_TRUE(graph != NULL);
    graph_add_road(graph, 0, 1, 1000, LIMIT, false, false);
    graph_add_road(graph, 0, 2, 9000, LIMIT, false, false);

    Route route;
    ASSERT_EQ_INT(ROUTE_FOUND, route_dijkstra(graph, LOAD_EMPTY, 0, 1, &route));
    ASSERT_EQ_INT(2, route.expanded);
    route_free(&route);
    graph_free(graph);
}

TEST(test_bad_node_ids_are_rejected)
{
    Graph *graph = create_flat_graph(2);
    ASSERT_TRUE(graph != NULL);

    Route route;
    ASSERT_EQ_INT(ROUTE_BAD_NODE, route_dijkstra(graph, LOAD_EMPTY, -1, 0, &route));
    ASSERT_TRUE(route.nodes == NULL);
    ASSERT_EQ_INT(ROUTE_BAD_NODE, route_dijkstra(graph, LOAD_EMPTY, 0, 2, &route));
    ASSERT_EQ_INT(ROUTE_BAD_NODE, route_dijkstra(graph, LOAD_EMPTY, 2, 0, &route));
    route_free(&route);
    graph_free(graph);
}

TEST(test_route_free_leaves_the_route_safe_to_free_again)
{
    Graph *graph = create_flat_graph(2);
    ASSERT_TRUE(graph != NULL);
    graph_add_road(graph, 0, 1, 1000, LIMIT, false, false);

    Route route;
    ASSERT_EQ_INT(ROUTE_FOUND, route_dijkstra(graph, LOAD_EMPTY, 0, 1, &route));
    route_free(&route);
    ASSERT_TRUE(route.nodes == NULL);
    route_free(&route);
    graph_free(graph);
}

int main(void)
{
    RUN_TEST(test_follows_a_chain_of_roads);
    RUN_TEST(test_matches_a_hand_worked_graph);
    RUN_TEST(test_later_discovery_replaces_a_worse_queued_cost);
    RUN_TEST(test_improved_node_is_expanded_at_its_new_priority);
    RUN_TEST(test_minimises_time_rather_than_distance);
    RUN_TEST(test_distance_comes_from_the_parallel_road_actually_used);
    RUN_TEST(test_start_equal_to_destination_costs_nothing);
    RUN_TEST(test_disconnected_destination_is_reported_as_unreachable);
    RUN_TEST(test_one_way_road_cannot_be_driven_backwards);
    RUN_TEST(test_closed_road_forces_a_detour);
    RUN_TEST(test_closing_the_only_road_makes_the_destination_unreachable);
    RUN_TEST(test_load_changes_the_route);
    RUN_TEST(test_stops_as_soon_as_the_destination_is_reached);
    RUN_TEST(test_bad_node_ids_are_rejected);
    RUN_TEST(test_route_free_leaves_the_route_safe_to_free_again);
    return TEST_SUMMARY();
}
