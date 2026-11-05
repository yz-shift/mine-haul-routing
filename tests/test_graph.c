#include "graph.h"

#include "test_framework.h"

/* Three unconnected nodes. Roads are added by each test. */
static Graph *create_three_nodes(void)
{
    Graph *graph = graph_create();
    if (graph != NULL) {
        graph_add_node(graph, "A", 0, 0, 0);
        graph_add_node(graph, "B", 3, 4, 10);
        graph_add_node(graph, "C", 6, 8, -20);
    }
    return graph;
}

TEST(test_new_graph_is_empty)
{
    Graph *graph = graph_create();
    ASSERT_TRUE(graph != NULL);
    ASSERT_EQ_INT(0, graph->node_count);
    ASSERT_EQ_INT(0, graph->arc_count);
    graph_free(graph);
}

TEST(test_free_null_is_safe)
{
    graph_free(NULL);
}

TEST(test_add_node_stores_fields_and_returns_sequential_ids)
{
    Graph *graph = graph_create();
    ASSERT_TRUE(graph != NULL);
    ASSERT_EQ_INT(0, graph_add_node(graph, "PIT", 1.5, 2.5, -85));
    ASSERT_EQ_INT(1, graph_add_node(graph, "CRUSHER", 3, 4, 12));

    ASSERT_EQ_STR("CRUSHER", graph->nodes[1].name);
    ASSERT_NEAR(3, graph->nodes[1].x_m, 0);
    ASSERT_NEAR(4, graph->nodes[1].y_m, 0);
    ASSERT_NEAR(12, graph->nodes[1].elevation_m, 0);
    ASSERT_EQ_INT(0, graph->nodes[1].arc_count);
    graph_free(graph);
}

TEST(test_add_node_copies_name)
{
    Graph *graph = graph_create();
    ASSERT_TRUE(graph != NULL);
    char name[] = "PIT";
    graph_add_node(graph, name, 0, 0, 0);
    name[0] = 'X';
    ASSERT_EQ_STR("PIT", graph->nodes[0].name);
    graph_free(graph);
}

TEST(test_two_way_road_adds_an_arc_each_way)
{
    Graph *graph = create_three_nodes();
    ASSERT_TRUE(graph != NULL);
    ASSERT_TRUE(graph_add_road(graph, 0, 1, 5, 40, false, false));

    ASSERT_EQ_INT(2, graph->arc_count);
    ASSERT_EQ_INT(1, graph->nodes[0].arc_count);
    ASSERT_EQ_INT(1, graph->nodes[0].arcs[0].to);
    ASSERT_EQ_INT(1, graph->nodes[1].arc_count);
    ASSERT_EQ_INT(0, graph->nodes[1].arcs[0].to);
    ASSERT_NEAR(5, graph->nodes[1].arcs[0].length_m, 0);
    ASSERT_NEAR(40, graph->nodes[1].arcs[0].speed_limit_kmh, 0);
    graph_free(graph);
}

TEST(test_one_way_road_adds_a_single_arc)
{
    Graph *graph = create_three_nodes();
    ASSERT_TRUE(graph != NULL);
    ASSERT_TRUE(graph_add_road(graph, 0, 1, 5, 40, true, false));

    ASSERT_EQ_INT(1, graph->arc_count);
    ASSERT_EQ_INT(1, graph->nodes[0].arc_count);
    ASSERT_EQ_INT(0, graph->nodes[1].arc_count);
    graph_free(graph);
}

TEST(test_closed_road_is_stored_closed_in_both_directions)
{
    Graph *graph = create_three_nodes();
    ASSERT_TRUE(graph != NULL);
    ASSERT_TRUE(graph_add_road(graph, 0, 1, 5, 40, false, true));

    ASSERT_TRUE(graph->nodes[0].arcs[0].closed);
    ASSERT_TRUE(graph->nodes[1].arcs[0].closed);
    graph_free(graph);
}

TEST(test_road_with_invalid_endpoint_is_rejected_and_changes_nothing)
{
    Graph *graph = create_three_nodes();
    ASSERT_TRUE(graph != NULL);
    ASSERT_TRUE(!graph_add_road(graph, -1, 0, 5, 40, false, false));
    ASSERT_TRUE(!graph_add_road(graph, 0, 3, 5, 40, false, false));

    ASSERT_EQ_INT(0, graph->arc_count);
    ASSERT_EQ_INT(0, graph->nodes[0].arc_count);
    graph_free(graph);
}

TEST(test_self_loop_is_rejected_and_changes_nothing)
{
    Graph *graph = create_three_nodes();
    ASSERT_TRUE(graph != NULL);
    ASSERT_TRUE(!graph_add_road(graph, 1, 1, 5, 40, false, false));
    ASSERT_TRUE(!graph_add_road(graph, 1, 1, 5, 40, true, false));

    ASSERT_EQ_INT(0, graph->arc_count);
    ASSERT_EQ_INT(0, graph->nodes[1].arc_count);
    graph_free(graph);
}

/* Goes well past the initial capacity, so the arrays are moved several times. */
TEST(test_arrays_keep_their_contents_when_they_grow)
{
    enum { COUNT = 1000 };
    Graph *graph = graph_create();
    ASSERT_TRUE(graph != NULL);
    for (int i = 0; i < COUNT; i++) {
        ASSERT_EQ_INT(i, graph_add_node(graph, "N", i, 0, 0));
    }
    for (int i = 1; i < COUNT; i++) {
        ASSERT_TRUE(graph_add_road(graph, 0, i, i, 40, true, false));
    }

    ASSERT_EQ_INT(COUNT, graph->node_count);
    ASSERT_EQ_INT(COUNT - 1, graph->arc_count);
    ASSERT_EQ_INT(COUNT - 1, graph->nodes[0].arc_count);
    for (int i = 1; i < COUNT; i++) {
        ASSERT_NEAR(i, graph->nodes[i].x_m, 0);
        ASSERT_EQ_INT(i, graph->nodes[0].arcs[i - 1].to);
    }
    graph_free(graph);
}

TEST(test_find_node_by_name)
{
    Graph *graph = create_three_nodes();
    ASSERT_TRUE(graph != NULL);
    ASSERT_EQ_INT(0, graph_find_node(graph, "A"));
    ASSERT_EQ_INT(2, graph_find_node(graph, "C"));
    ASSERT_EQ_INT(GRAPH_NODE_NOT_FOUND, graph_find_node(graph, "Z"));
    ASSERT_EQ_INT(GRAPH_NODE_NOT_FOUND, graph_find_node(graph, "a"));

    graph_add_node(graph, "A", 9, 9, 9);
    ASSERT_EQ_INT(GRAPH_NODE_AMBIGUOUS, graph_find_node(graph, "A"));
    ASSERT_EQ_INT(1, graph_find_node(graph, "B"));
    graph_free(graph);
}

TEST(test_closing_a_two_way_road_closes_both_directions)
{
    Graph *graph = create_three_nodes();
    ASSERT_TRUE(graph != NULL);
    graph_add_road(graph, 0, 1, 5, 40, false, false);
    graph_add_road(graph, 1, 2, 5, 40, false, false);

    ASSERT_EQ_INT(2, graph_set_road_closed(graph, 1, 0, true));
    ASSERT_TRUE(graph->nodes[0].arcs[0].closed);
    ASSERT_TRUE(graph->nodes[1].arcs[0].closed);
    /* The other road at node 1 stays open. */
    ASSERT_TRUE(!graph->nodes[1].arcs[1].closed);

    ASSERT_EQ_INT(2, graph_set_road_closed(graph, 0, 1, false));
    ASSERT_TRUE(!graph->nodes[0].arcs[0].closed);
    ASSERT_TRUE(!graph->nodes[1].arcs[0].closed);
    graph_free(graph);
}

TEST(test_closing_a_one_way_road_works_from_either_end)
{
    Graph *graph = create_three_nodes();
    ASSERT_TRUE(graph != NULL);
    graph_add_road(graph, 0, 1, 5, 40, true, false);
    ASSERT_EQ_INT(1, graph_set_road_closed(graph, 1, 0, true));
    ASSERT_TRUE(graph->nodes[0].arcs[0].closed);
    graph_free(graph);
}

TEST(test_closing_affects_every_parallel_road)
{
    Graph *graph = create_three_nodes();
    ASSERT_TRUE(graph != NULL);
    graph_add_road(graph, 0, 1, 5, 40, false, false);
    graph_add_road(graph, 0, 1, 9, 20, false, false);
    ASSERT_EQ_INT(4, graph_set_road_closed(graph, 0, 1, true));
    ASSERT_TRUE(graph->nodes[0].arcs[1].closed);
    ASSERT_TRUE(graph->nodes[1].arcs[1].closed);
    graph_free(graph);
}

TEST(test_closing_a_missing_road_reports_nothing_found)
{
    Graph *graph = create_three_nodes();
    ASSERT_TRUE(graph != NULL);
    graph_add_road(graph, 0, 1, 5, 40, false, false);
    ASSERT_EQ_INT(0, graph_set_road_closed(graph, 0, 2, true));
    ASSERT_EQ_INT(0, graph_set_road_closed(graph, -1, 0, true));
    ASSERT_EQ_INT(0, graph_set_road_closed(graph, 0, 3, true));
    ASSERT_TRUE(!graph->nodes[0].arcs[0].closed);
    graph_free(graph);
}

int main(void)
{
    RUN_TEST(test_find_node_by_name);
    RUN_TEST(test_closing_a_two_way_road_closes_both_directions);
    RUN_TEST(test_closing_a_one_way_road_works_from_either_end);
    RUN_TEST(test_closing_affects_every_parallel_road);
    RUN_TEST(test_closing_a_missing_road_reports_nothing_found);
    RUN_TEST(test_new_graph_is_empty);
    RUN_TEST(test_free_null_is_safe);
    RUN_TEST(test_add_node_stores_fields_and_returns_sequential_ids);
    RUN_TEST(test_add_node_copies_name);
    RUN_TEST(test_two_way_road_adds_an_arc_each_way);
    RUN_TEST(test_one_way_road_adds_a_single_arc);
    RUN_TEST(test_closed_road_is_stored_closed_in_both_directions);
    RUN_TEST(test_road_with_invalid_endpoint_is_rejected_and_changes_nothing);
    RUN_TEST(test_self_loop_is_rejected_and_changes_nothing);
    RUN_TEST(test_arrays_keep_their_contents_when_they_grow);
    return TEST_SUMMARY();
}
