#include "road_file.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "test_framework.h"

enum { ERROR_SIZE = 256 };

/* Two nodes 100 m apart, for the tests that only care about the edge line. */
#define TWO_NODES "N 0 A 0 0 0\nN 1 B 100 0 0\n"

/* Feeds text to the parser through a real stream. tmpfile is standard C. */
static Graph *parse_text(const char *text, char *error)
{
    FILE *stream = tmpfile();
    if (stream == NULL) {
        snprintf(error, ERROR_SIZE, "tmpfile failed");
        return NULL;
    }
    fputs(text, stream);
    rewind(stream);
    Graph *graph = road_file_read(stream, "test", error, ERROR_SIZE);
    fclose(stream);
    return graph;
}

/* Prints the details itself, because ASSERT_TRUE only shows the call. */
static bool rejects(const char *text, const char *expected_error)
{
    char error[ERROR_SIZE] = "";
    Graph *graph = parse_text(text, error);
    bool rejected = graph == NULL;
    graph_free(graph);
    if (!rejected) {
        printf("  accepted bad input: %s\n", text);
        return false;
    }
    if (strstr(error, expected_error) == NULL) {
        printf("  wanted error containing \"%s\", got \"%s\"\n", expected_error,
               error);
        return false;
    }
    return true;
}

TEST(test_parses_nodes_and_edges)
{
    char error[ERROR_SIZE] = "";
    Graph *graph = parse_text("N 0 PIT 120.5 40 -85\n"
                              "N 1 CRUSHER 300 210 12\n"
                              "E 0 1 450 40 0 0\n",
                              error);
    ASSERT_TRUE(graph != NULL);
    ASSERT_EQ_INT(2, graph->node_count);
    ASSERT_EQ_INT(2, graph->arc_count);

    ASSERT_EQ_STR("PIT", graph->nodes[0].name);
    ASSERT_NEAR(120.5, graph->nodes[0].x_m, 1e-12);
    ASSERT_NEAR(40, graph->nodes[0].y_m, 1e-12);
    ASSERT_NEAR(-85, graph->nodes[0].elevation_m, 1e-12);

    ASSERT_EQ_INT(1, graph->nodes[0].arcs[0].to);
    ASSERT_NEAR(450, graph->nodes[0].arcs[0].length_m, 1e-12);
    ASSERT_NEAR(40, graph->nodes[0].arcs[0].speed_limit_kmh, 1e-12);
    ASSERT_TRUE(!graph->nodes[0].arcs[0].closed);
    graph_free(graph);
}

TEST(test_one_way_and_closed_flags_reach_the_graph)
{
    char error[ERROR_SIZE] = "";
    Graph *graph = parse_text(TWO_NODES "E 0 1 100 40 1 1\n", error);
    ASSERT_TRUE(graph != NULL);
    ASSERT_EQ_INT(1, graph->arc_count);
    ASSERT_EQ_INT(0, graph->nodes[1].arc_count);
    ASSERT_TRUE(graph->nodes[0].arcs[0].closed);
    graph_free(graph);
}

TEST(test_ignores_comments_and_blank_lines)
{
    char error[ERROR_SIZE] = "";
    Graph *graph = parse_text("# leading comment\n"
                              "\n"
                              "   \t  \n"
                              "   # indented comment\n"
                              "N 0 A 0 0 0 # after a node\n"
                              "N 1 B 100 0 0\n"
                              "E 0 1 100 40 0 0 # after an edge\n"
                              "#N 2 HIDDEN 0 0 0\n",
                              error);
    ASSERT_TRUE(graph != NULL);
    ASSERT_EQ_INT(2, graph->node_count);
    ASSERT_EQ_INT(2, graph->arc_count);
    graph_free(graph);
}

TEST(test_accepts_tabs_crlf_and_a_missing_final_newline)
{
    char error[ERROR_SIZE] = "";
    Graph *graph = parse_text("N\t0\tA\t0\t0\t0\r\n"
                              "  N   1 B 100 0 0  \r\n"
                              "E 0 1 100 40 0 0",
                              error);
    ASSERT_TRUE(graph != NULL);
    ASSERT_EQ_INT(2, graph->node_count);
    ASSERT_EQ_STR("A", graph->nodes[0].name);
    ASSERT_EQ_INT(2, graph->arc_count);
    graph_free(graph);
}

TEST(test_length_equal_to_straight_line_distance_is_allowed)
{
    char error[ERROR_SIZE] = "";
    /* A 3-4-5 triangle scaled by 100, so the distance is exactly 500. */
    Graph *graph =
        parse_text("N 0 A 0 0 0\nN 1 B 300 400 0\nE 0 1 500 40 0 0\n", error);
    ASSERT_TRUE(graph != NULL);
    graph_free(graph);
}

TEST(test_rejects_records_with_the_wrong_shape)
{
    ASSERT_TRUE(rejects("X 0 A 0 0 0\n", "test:1: unknown record type 'X'"));
    ASSERT_TRUE(rejects("n 0 A 0 0 0\n", "unknown record type 'n'"));
    ASSERT_TRUE(rejects("N 0 A 0 0\n", "N record needs 6 fields, got 5"));
    ASSERT_TRUE(rejects("N 0 A 0 0 0 9\n", "N record needs 6 fields, got 7"));
    ASSERT_TRUE(rejects(TWO_NODES "E 0 1 100 40 0\n",
                        "test:3: E record needs 7 fields, got 6"));
    ASSERT_TRUE(rejects(TWO_NODES "E 0 1 100 40 0 0 1 1 1\n",
                        "E record needs 7 fields, got 10"));
}

TEST(test_rejects_bad_nodes)
{
    ASSERT_TRUE(rejects("N abc A 0 0 0\n", "node id must be an integer"));
    ASSERT_TRUE(rejects("N 99999999999 A 0 0 0\n", "node id must be an integer"));
    ASSERT_TRUE(rejects("N 1 A 0 0 0\n", "node id must be 0"));
    ASSERT_TRUE(rejects("N 0 A 0 0 0\nN 0 B 0 0 0\n",
                        "test:2: node id must be 1"));
    ASSERT_TRUE(rejects("N 0 A 1x 0 0\n", "x_m must be a finite number"));
    ASSERT_TRUE(rejects("N 0 A 0 nan 0\n", "y_m must be a finite number"));
    ASSERT_TRUE(rejects("N 0 A 0 0 inf\n", "elevation_m must be a finite number"));
}

TEST(test_rejects_bad_edges)
{
    ASSERT_TRUE(rejects(TWO_NODES "E 0 2 100 40 0 0\n",
                        "test:3: to refers to unknown node 2"));
    ASSERT_TRUE(rejects(TWO_NODES "E -1 1 100 40 0 0\n",
                        "from refers to unknown node -1"));
    ASSERT_TRUE(rejects("E 0 1 100 40 0 0\nN 0 A 0 0 0\nN 1 B 100 0 0\n",
                        "test:1: from refers to unknown node 0"));
    ASSERT_TRUE(rejects(TWO_NODES "E 1 1 100 40 0 0\n",
                        "edge cannot start and end at node 1"));
    ASSERT_TRUE(rejects(TWO_NODES "E 0 1 0 40 0 0\n",
                        "length_m must be greater than 0"));
    ASSERT_TRUE(rejects(TWO_NODES "E 0 1 100 -5 0 0\n",
                        "speed_limit_kmh must be greater than 0"));
    ASSERT_TRUE(rejects(TWO_NODES "E 0 1 100 40 2 0\n", "one_way must be 0 or 1"));
    ASSERT_TRUE(rejects(TWO_NODES "E 0 1 100 40 0 yes\n", "closed must be 0 or 1"));
}

TEST(test_rejects_road_shorter_than_straight_line)
{
    ASSERT_TRUE(rejects(TWO_NODES "E 0 1 99.5 40 0 0\n",
                        "shorter than the straight-line distance"));
}

TEST(test_rejects_file_without_nodes)
{
    ASSERT_TRUE(rejects("", "file contains no nodes"));
    ASSERT_TRUE(rejects("# only a comment\n\n", "file contains no nodes"));
}

TEST(test_rejects_line_that_does_not_fit_the_buffer)
{
    char text[3000];
    memset(text, '#', sizeof text - 1);
    text[sizeof text - 1] = '\0';
    ASSERT_TRUE(rejects(text, "test:1: line is longer than"));
}

TEST(test_line_that_exactly_fills_the_buffer_is_accepted)
{
    /* 1022 characters + '\n' + the terminator fill the 1024-byte buffer. */
    char text[1024 + sizeof "N 0 A 0 0 0\n"];
    memset(text, '#', 1022);
    strcpy(text + 1022, "\nN 0 A 0 0 0\n");

    char error[ERROR_SIZE] = "";
    Graph *graph = parse_text(text, error);
    ASSERT_TRUE(graph != NULL);
    ASSERT_EQ_INT(1, graph->node_count);
    graph_free(graph);
}

TEST(test_loads_a_real_file)
{
    char error[ERROR_SIZE] = "";
    Graph *graph = road_file_load("tests/data/small_mine.road", error, sizeof error);
    ASSERT_TRUE(graph != NULL);
    ASSERT_EQ_INT(3, graph->node_count);
    /* Two-way 0-1, one-way 1->2, closed two-way 0-2. */
    ASSERT_EQ_INT(5, graph->arc_count);
    ASSERT_EQ_STR("WASTE_DUMP", graph->nodes[2].name);
    ASSERT_NEAR(30, graph->nodes[2].elevation_m, 1e-12);
    ASSERT_EQ_INT(2, graph->nodes[0].arc_count);
    ASSERT_TRUE(graph->nodes[0].arcs[1].closed);
    graph_free(graph);
}

TEST(test_missing_file_reports_its_path)
{
    char error[ERROR_SIZE] = "";
    Graph *graph = road_file_load("tests/data/no_such_file.road", error, sizeof error);
    ASSERT_TRUE(graph == NULL);
    ASSERT_TRUE(strstr(error, "tests/data/no_such_file.road") != NULL);
    ASSERT_TRUE(strstr(error, "cannot open file") != NULL);
}

TEST(test_null_or_tiny_error_buffer_is_safe)
{
    FILE *stream = tmpfile();
    ASSERT_TRUE(stream != NULL);
    fputs("X\n", stream);
    rewind(stream);
    ASSERT_TRUE(road_file_read(stream, "test", NULL, 0) == NULL);

    rewind(stream);
    char tiny[4] = "";
    ASSERT_TRUE(road_file_read(stream, "test", tiny, sizeof tiny) == NULL);
    ASSERT_EQ_STR("tes", tiny);
    fclose(stream);
}

int main(void)
{
    RUN_TEST(test_parses_nodes_and_edges);
    RUN_TEST(test_one_way_and_closed_flags_reach_the_graph);
    RUN_TEST(test_ignores_comments_and_blank_lines);
    RUN_TEST(test_accepts_tabs_crlf_and_a_missing_final_newline);
    RUN_TEST(test_length_equal_to_straight_line_distance_is_allowed);
    RUN_TEST(test_rejects_records_with_the_wrong_shape);
    RUN_TEST(test_rejects_bad_nodes);
    RUN_TEST(test_rejects_bad_edges);
    RUN_TEST(test_rejects_road_shorter_than_straight_line);
    RUN_TEST(test_rejects_file_without_nodes);
    RUN_TEST(test_rejects_line_that_does_not_fit_the_buffer);
    RUN_TEST(test_line_that_exactly_fills_the_buffer_is_accepted);
    RUN_TEST(test_loads_a_real_file);
    RUN_TEST(test_missing_file_reports_its_path);
    RUN_TEST(test_null_or_tiny_error_buffer_is_safe);
    return TEST_SUMMARY();
}
