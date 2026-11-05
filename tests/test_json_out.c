#include "json_out.h"

#include <math.h>
#include <stdio.h>

#include "test_framework.h"

enum { BUFFER_SIZE = 4096 };

/* Writes the document to a real stream and reads it back as one string. */
static bool render(const Graph *graph, const RouteReport *reports, int count,
                   char *buffer)
{
    FILE *stream = tmpfile();
    if (stream == NULL) {
        return false;
    }
    json_out_write(stream, graph, reports, count);
    rewind(stream);
    size_t used = fread(buffer, 1, BUFFER_SIZE - 1, stream);
    buffer[used] = '\0';
    fclose(stream);
    return true;
}

static Graph *create_two_nodes(const char *first_name)
{
    Graph *graph = graph_create();
    if (graph != NULL) {
        graph_add_node(graph, first_name, 1.5, -2, 30);
        graph_add_node(graph, "B", 100, 0, 0);
        graph_add_road(graph, 0, 1, 250, 40, true, false);
    }
    return graph;
}

TEST(test_writes_found_route_exactly)
{
    Graph *graph = create_two_nodes("A");
    ASSERT_TRUE(graph != NULL);
    int nodes[] = {0, 1};
    Route route = {.cost_s = 12.5, .distance_m = 250, .nodes = nodes, .node_count = 2,
                   .expanded = 3};
    RouteReport report = {.label = "initial", .algorithm = "astar", .load = LOAD_FULL,
                          .from = 0, .to = 1, .status = ROUTE_FOUND, .route = &route};

    char buffer[BUFFER_SIZE];
    ASSERT_TRUE(render(graph, &report, 1, buffer));
    ASSERT_EQ_STR(
        "{\n"
        "\"nodes\":[\n"
        "{\"id\":0,\"name\":\"A\",\"x_m\":1.5,\"y_m\":-2,\"elevation_m\":30},\n"
        "{\"id\":1,\"name\":\"B\",\"x_m\":100,\"y_m\":0,\"elevation_m\":0}\n"
        "],\n"
        "\"arcs\":[\n"
        "{\"from\":0,\"to\":1,\"length_m\":250,\"speed_limit_kmh\":40,\"closed\":false}\n"
        "],\n"
        "\"routes\":[\n"
        "{\"label\":\"initial\",\"algorithm\":\"astar\",\"load\":\"full\",\"from\":0,"
        "\"to\":1,\"status\":\"found\",\"cost_s\":12.5,\"distance_m\":250,\"expanded\":3,"
        "\"nodes\":[0,1]}\n"
        "]\n"
        "}\n",
        buffer);
    graph_free(graph);
}

TEST(test_unreachable_route_has_null_cost_instead_of_infinity)
{
    Graph *graph = create_two_nodes("A");
    ASSERT_TRUE(graph != NULL);
    Route route = {.cost_s = INFINITY, .expanded = 1};
    RouteReport report = {.label = "x", .algorithm = "dijkstra", .load = LOAD_EMPTY,
                          .from = 1, .to = 0, .status = ROUTE_UNREACHABLE, .route = &route};

    char buffer[BUFFER_SIZE];
    ASSERT_TRUE(render(graph, &report, 1, buffer));
    ASSERT_TRUE(strstr(buffer,
                       "\"load\":\"empty\",\"from\":1,\"to\":0,"
                       "\"status\":\"unreachable\",\"cost_s\":null,"
                       "\"distance_m\":null,\"expanded\":1,\"nodes\":[]") != NULL);
    ASSERT_TRUE(strstr(buffer, "inf") == NULL);
    graph_free(graph);
}

TEST(test_closed_arc_is_written_as_closed)
{
    Graph *graph = create_two_nodes("A");
    ASSERT_TRUE(graph != NULL);
    graph_set_road_closed(graph, 0, 1, true);

    char buffer[BUFFER_SIZE];
    ASSERT_TRUE(render(graph, NULL, 0, buffer));
    ASSERT_TRUE(strstr(buffer, "\"closed\":true") != NULL);
    graph_free(graph);
}

TEST(test_special_characters_in_names_are_escaped)
{
    Graph *graph = create_two_nodes("a\"b\\c\td");
    ASSERT_TRUE(graph != NULL);

    char buffer[BUFFER_SIZE];
    ASSERT_TRUE(render(graph, NULL, 0, buffer));
    ASSERT_TRUE(strstr(buffer, "\"name\":\"a\\\"b\\\\c\\u0009d\"") != NULL);
    graph_free(graph);
}

TEST(test_empty_lists_are_valid)
{
    Graph *graph = graph_create();
    ASSERT_TRUE(graph != NULL);

    char buffer[BUFFER_SIZE];
    ASSERT_TRUE(render(graph, NULL, 0, buffer));
    ASSERT_EQ_STR("{\n\"nodes\":[\n],\n\"arcs\":[\n],\n\"routes\":[\n]\n}\n", buffer);
    graph_free(graph);
}

TEST(test_several_routes_are_comma_separated)
{
    Graph *graph = graph_create();
    ASSERT_TRUE(graph != NULL);
    Route route = {.cost_s = 0, .expanded = 0};
    RouteReport reports[] = {
        {.label = "one", .algorithm = "a", .status = ROUTE_FOUND, .route = &route},
        {.label = "two", .algorithm = "a", .status = ROUTE_FOUND, .route = &route},
    };

    char buffer[BUFFER_SIZE];
    ASSERT_TRUE(render(graph, reports, 2, buffer));
    ASSERT_TRUE(strstr(buffer, "\"nodes\":[]},\n{\"label\":\"two\"") != NULL);
    graph_free(graph);
}

int main(void)
{
    RUN_TEST(test_writes_found_route_exactly);
    RUN_TEST(test_unreachable_route_has_null_cost_instead_of_infinity);
    RUN_TEST(test_closed_arc_is_written_as_closed);
    RUN_TEST(test_special_characters_in_names_are_escaped);
    RUN_TEST(test_empty_lists_are_valid);
    RUN_TEST(test_several_routes_are_comma_separated);
    return TEST_SUMMARY();
}
