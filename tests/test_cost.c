#include "cost.h"

#include <math.h>

#include "test_framework.h"

#define KMH(mps) ((mps) * 3.6)
#define NO_LIMIT 1000.0
#define TOLERANCE 1e-3

/* A at 0 m and B at 50 m, joined by a 1000 m road: a 5% grade from A to B. */
static Graph *create_hill(bool one_way, bool closed)
{
    Graph *graph = graph_create();
    if (graph != NULL) {
        graph_add_node(graph, "A", 0, 0, 0);
        graph_add_node(graph, "B", 900, 0, 50);
        graph_add_road(graph, 0, 1, 1000, 60, one_way, closed);
    }
    return graph;
}

/*
 * Expected values below are worked by hand from v = P / (m * g * (grade + 0.03)),
 * with P = 800 kW, m = 100 t empty or 250 t full, and g = 9.81.
 */
TEST(test_power_limits_speed_on_the_flat)
{
    /* 800e3 / (250e3 * 9.81 * 0.03) = 10.873 m/s = 39.14 km/h */
    ASSERT_NEAR(39.14, KMH(cost_speed_mps(0, NO_LIMIT, LOAD_FULL)), 0.01);
    /* The empty figure works out at 97.9 km/h, so top speed limits it instead. */
    ASSERT_NEAR(60, KMH(cost_speed_mps(0, NO_LIMIT, LOAD_EMPTY)), TOLERANCE);
}

TEST(test_power_limits_speed_on_a_climb)
{
    /* 800e3 / (250e3 * 9.81 * 0.13) = 2.509 m/s = 9.03 km/h */
    ASSERT_NEAR(9.03, KMH(cost_speed_mps(0.10, NO_LIMIT, LOAD_FULL)), 0.01);
    /* 800e3 / (100e3 * 9.81 * 0.13) = 6.273 m/s = 22.58 km/h */
    ASSERT_NEAR(22.58, KMH(cost_speed_mps(0.10, NO_LIMIT, LOAD_EMPTY)), 0.01);
}

TEST(test_full_truck_is_slower_uphill_than_empty_truck)
{
    const double grades[] = {0.01, 0.05, 0.10, 0.15, 0.20, 0.40};
    for (int i = 0; i < 6; i++) {
        ASSERT_TRUE(cost_speed_mps(grades[i], NO_LIMIT, LOAD_FULL) <
                    cost_speed_mps(grades[i], NO_LIMIT, LOAD_EMPTY));
    }
}

TEST(test_steeper_climbs_are_never_faster)
{
    for (int percent = 0; percent < 60; percent++) {
        ASSERT_TRUE(cost_speed_mps((percent + 1) / 100.0, NO_LIMIT, LOAD_FULL) <=
                    cost_speed_mps(percent / 100.0, NO_LIMIT, LOAD_FULL));
        ASSERT_TRUE(cost_speed_mps((percent + 1) / 100.0, NO_LIMIT, LOAD_EMPTY) <=
                    cost_speed_mps(percent / 100.0, NO_LIMIT, LOAD_EMPTY));
    }
}

TEST(test_full_truck_crawls_beyond_its_traction_limit)
{
    /* 450e3 / (250e3 * 9.81) = 0.1835, so the limit is a grade of about 15.3%. */
    /* 800e3 / (250e3 * 9.81 * 0.18) = 1.812 m/s = 6.52 km/h */
    ASSERT_NEAR(6.52, KMH(cost_speed_mps(0.15, NO_LIMIT, LOAD_FULL)), 0.01);
    ASSERT_NEAR(4, KMH(cost_speed_mps(0.16, NO_LIMIT, LOAD_FULL)), TOLERANCE);
    /* An empty truck has the traction for the same grade. */
    ASSERT_TRUE(KMH(cost_speed_mps(0.16, NO_LIMIT, LOAD_EMPTY)) > 15);
}

TEST(test_braking_limits_speed_downhill)
{
    /* Full: 60 - 4 * 10 = 20. Empty: 60 - 2 * 10 = 40. */
    ASSERT_NEAR(20, KMH(cost_speed_mps(-0.10, NO_LIMIT, LOAD_FULL)), TOLERANCE);
    ASSERT_NEAR(40, KMH(cost_speed_mps(-0.10, NO_LIMIT, LOAD_EMPTY)), TOLERANCE);
}

TEST(test_braking_speed_has_a_floor_on_steep_descents)
{
    ASSERT_NEAR(10, KMH(cost_speed_mps(-0.50, NO_LIMIT, LOAD_FULL)), TOLERANCE);
    ASSERT_NEAR(20, KMH(cost_speed_mps(-0.50, NO_LIMIT, LOAD_EMPTY)), TOLERANCE);
}

TEST(test_gentle_descent_is_limited_by_power_while_it_still_needs_force)
{
    /* Grade -1%: 800e3 / (250e3 * 9.81 * 0.02) = 58.7 km/h, braking allows 56. */
    ASSERT_NEAR(56, KMH(cost_speed_mps(-0.01, NO_LIMIT, LOAD_FULL)), TOLERANCE);
    /* Grade -0.5%: 800e3 / (250e3 * 9.81 * 0.025) = 46.97 km/h, braking allows 58. */
    ASSERT_NEAR(46.97, KMH(cost_speed_mps(-0.005, NO_LIMIT, LOAD_FULL)), 0.01);
}

TEST(test_road_speed_limit_wins_when_it_is_lowest)
{
    ASSERT_NEAR(15, KMH(cost_speed_mps(0, 15, LOAD_FULL)), TOLERANCE);
    ASSERT_NEAR(15, KMH(cost_speed_mps(-0.10, 15, LOAD_EMPTY)), TOLERANCE);
    ASSERT_NEAR(60, KMH(cost_speed_mps(0, 80, LOAD_EMPTY)), TOLERANCE);
}

TEST(test_arc_cost_is_length_over_speed)
{
    Graph *graph = create_hill(false, false);
    ASSERT_TRUE(graph != NULL);
    /* Uphill at 5%, full: 800e3 / (250e3 * 9.81 * 0.08) = 4.0775 m/s. */
    ASSERT_NEAR(1000 / 4.0775,
                cost_arc_seconds(graph, 0, &graph->nodes[0].arcs[0], LOAD_FULL), 0.01);
    /* Downhill at 5%, full: 60 - 4 * 5 = 40 km/h = 11.111 m/s. */
    ASSERT_NEAR(90, cost_arc_seconds(graph, 1, &graph->nodes[1].arcs[0], LOAD_FULL), 0.01);
    graph_free(graph);
}

TEST(test_full_truck_takes_longer_uphill_on_a_real_arc)
{
    Graph *graph = create_hill(false, false);
    ASSERT_TRUE(graph != NULL);
    const Arc *up = &graph->nodes[0].arcs[0];
    ASSERT_TRUE(cost_arc_seconds(graph, 0, up, LOAD_FULL) >
                cost_arc_seconds(graph, 0, up, LOAD_EMPTY));
    graph_free(graph);
}

TEST(test_closed_arc_cannot_be_driven)
{
    Graph *graph = create_hill(false, true);
    ASSERT_TRUE(graph != NULL);
    ASSERT_TRUE(isinf(cost_arc_seconds(graph, 0, &graph->nodes[0].arcs[0], LOAD_FULL)));
    ASSERT_TRUE(isinf(cost_arc_seconds(graph, 1, &graph->nodes[1].arcs[0], LOAD_EMPTY)));
    graph_free(graph);
}

TEST(test_one_way_road_has_no_arc_to_drive_back)
{
    Graph *graph = create_hill(true, false);
    ASSERT_TRUE(graph != NULL);
    ASSERT_EQ_INT(1, graph->nodes[0].arc_count);
    ASSERT_TRUE(isfinite(cost_arc_seconds(graph, 0, &graph->nodes[0].arcs[0], LOAD_FULL)));
    ASSERT_EQ_INT(0, graph->nodes[1].arc_count);
    graph_free(graph);
}

TEST(test_top_speed_is_the_fastest_arc_and_ignores_closure)
{
    Graph *graph = graph_create();
    ASSERT_TRUE(graph != NULL);
    graph_add_node(graph, "A", 0, 0, 0);
    graph_add_node(graph, "B", 1000, 0, 0);
    graph_add_node(graph, "C", 2000, 0, 0);
    graph_add_road(graph, 0, 1, 1000, 20, true, false);
    ASSERT_NEAR(20, KMH(cost_top_speed_mps(graph, LOAD_EMPTY)), TOLERANCE);

    /* A closed arc still counts, because it could be reopened. */
    graph_add_road(graph, 1, 2, 1000, 50, true, true);
    ASSERT_NEAR(50, KMH(cost_top_speed_mps(graph, LOAD_EMPTY)), TOLERANCE);
    /* A full truck only reaches 39.14 km/h on the flat, whatever the limit. */
    ASSERT_NEAR(39.14, KMH(cost_top_speed_mps(graph, LOAD_FULL)), 0.01);
    graph_free(graph);
}

TEST(test_no_arc_is_faster_than_the_reported_top_speed)
{
    Graph *graph = create_hill(false, false);
    ASSERT_TRUE(graph != NULL);
    const Load loads[] = {LOAD_EMPTY, LOAD_FULL};
    for (int i = 0; i < 2; i++) {
        double top = cost_top_speed_mps(graph, loads[i]);
        ASSERT_TRUE(cost_arc_seconds(graph, 0, &graph->nodes[0].arcs[0], loads[i]) >=
                    1000 / top);
        ASSERT_TRUE(cost_arc_seconds(graph, 1, &graph->nodes[1].arcs[0], loads[i]) >=
                    1000 / top);
    }
    graph_free(graph);
}

int main(void)
{
    RUN_TEST(test_power_limits_speed_on_the_flat);
    RUN_TEST(test_power_limits_speed_on_a_climb);
    RUN_TEST(test_full_truck_is_slower_uphill_than_empty_truck);
    RUN_TEST(test_steeper_climbs_are_never_faster);
    RUN_TEST(test_full_truck_crawls_beyond_its_traction_limit);
    RUN_TEST(test_braking_limits_speed_downhill);
    RUN_TEST(test_braking_speed_has_a_floor_on_steep_descents);
    RUN_TEST(test_gentle_descent_is_limited_by_power_while_it_still_needs_force);
    RUN_TEST(test_road_speed_limit_wins_when_it_is_lowest);
    RUN_TEST(test_arc_cost_is_length_over_speed);
    RUN_TEST(test_full_truck_takes_longer_uphill_on_a_real_arc);
    RUN_TEST(test_closed_arc_cannot_be_driven);
    RUN_TEST(test_one_way_road_has_no_arc_to_drive_back);
    RUN_TEST(test_top_speed_is_the_fastest_arc_and_ignores_closure);
    RUN_TEST(test_no_arc_is_faster_than_the_reported_top_speed);
    return TEST_SUMMARY();
}
