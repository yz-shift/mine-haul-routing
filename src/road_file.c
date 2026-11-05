#include "road_file.h"

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

enum {
    LINE_BUFFER_SIZE = 1024,
    NODE_FIELD_COUNT = 6,
    EDGE_FIELD_COUNT = 7,
    MAX_STORED_FIELDS = 7,
};

static const char FIELD_SEPARATORS[] = " \t\r\n";

/* Where the parser is, and where to report what went wrong. */
typedef struct {
    Graph *graph;
    const char *source;
    int line_number;
    char *error;
    size_t error_size;
} Parser;

static void set_error(const Parser *parser, const char *format, ...)
{
    if (parser->error == NULL || parser->error_size == 0) {
        return;
    }
    int prefix = snprintf(parser->error, parser->error_size, "%s:%d: ",
                          parser->source, parser->line_number);
    if (prefix < 0 || (size_t)prefix >= parser->error_size) {
        return;
    }
    va_list args;
    va_start(args, format);
    vsnprintf(parser->error + prefix, parser->error_size - (size_t)prefix,
              format, args);
    va_end(args);
}

/* strtol and strtod are used over sscanf, where int overflow is undefined. */
static bool read_int(const Parser *parser, const char *text, const char *field,
                     int *out)
{
    char *end;
    errno = 0;
    long value = strtol(text, &end, 10);
    if (end == text || *end != '\0' || errno == ERANGE || value < INT_MIN ||
        value > INT_MAX) {
        set_error(parser, "%s must be an integer, got '%s'", field, text);
        return false;
    }
    *out = (int)value;
    return true;
}

static bool read_double(const Parser *parser, const char *text, const char *field,
                        double *out)
{
    char *end;
    errno = 0;
    double value = strtod(text, &end);
    /* strtod accepts "inf" and "nan", which would poison every route cost. */
    if (end == text || *end != '\0' || errno == ERANGE || !isfinite(value)) {
        set_error(parser, "%s must be a finite number, got '%s'", field, text);
        return false;
    }
    *out = value;
    return true;
}

static bool read_positive(const Parser *parser, const char *text,
                          const char *field, double *out)
{
    if (!read_double(parser, text, field, out)) {
        return false;
    }
    if (*out <= 0) {
        set_error(parser, "%s must be greater than 0, got '%s'", field, text);
        return false;
    }
    return true;
}

static bool read_flag(const Parser *parser, const char *text, const char *field,
                      bool *out)
{
    if (strcmp(text, "0") != 0 && strcmp(text, "1") != 0) {
        set_error(parser, "%s must be 0 or 1, got '%s'", field, text);
        return false;
    }
    *out = text[0] == '1';
    return true;
}

static bool read_node_id(const Parser *parser, const char *text, const char *field,
                         int *out)
{
    if (!read_int(parser, text, field, out)) {
        return false;
    }
    if (*out < 0 || *out >= parser->graph->node_count) {
        set_error(parser, "%s refers to unknown node %d", field, *out);
        return false;
    }
    return true;
}

/* fields: N id name x_m y_m elevation_m */
static bool parse_node(const Parser *parser, char **fields)
{
    int id;
    double x_m, y_m, elevation_m;
    if (!read_int(parser, fields[1], "node id", &id) ||
        !read_double(parser, fields[3], "x_m", &x_m) ||
        !read_double(parser, fields[4], "y_m", &y_m) ||
        !read_double(parser, fields[5], "elevation_m", &elevation_m)) {
        return false;
    }
    if (id != parser->graph->node_count) {
        set_error(parser, "node id must be %d (ids count up from 0), got %d",
                  parser->graph->node_count, id);
        return false;
    }
    if (graph_add_node(parser->graph, fields[2], x_m, y_m, elevation_m) < 0) {
        set_error(parser, "out of memory");
        return false;
    }
    return true;
}

/* fields: E from to length_m speed_limit_kmh one_way closed */
static bool parse_edge(const Parser *parser, char **fields)
{
    int from, to;
    double length_m, speed_limit_kmh;
    bool one_way, closed;
    if (!read_node_id(parser, fields[1], "from", &from) ||
        !read_node_id(parser, fields[2], "to", &to) ||
        !read_positive(parser, fields[3], "length_m", &length_m) ||
        !read_positive(parser, fields[4], "speed_limit_kmh", &speed_limit_kmh) ||
        !read_flag(parser, fields[5], "one_way", &one_way) ||
        !read_flag(parser, fields[6], "closed", &closed)) {
        return false;
    }
    if (from == to) {
        set_error(parser, "edge cannot start and end at node %d", from);
        return false;
    }

    /* See road_file.h: the A* heuristic is only valid if this holds. */
    const Node *start = &parser->graph->nodes[from];
    const Node *end = &parser->graph->nodes[to];
    double straight_m = hypot(end->x_m - start->x_m, end->y_m - start->y_m);
    if (length_m < straight_m) {
        set_error(parser,
                  "length_m %g is shorter than the straight-line distance %g",
                  length_m, straight_m);
        return false;
    }

    if (!graph_add_road(parser->graph, from, to, length_m, speed_limit_kmh,
                        one_way, closed)) {
        set_error(parser, "out of memory");
        return false;
    }
    return true;
}

/*
 * Splits line in place. Stores up to MAX_STORED_FIELDS fields but counts them
 * all, so that a line with too many fields can be reported accurately.
 */
static int split_fields(char *line, char **fields)
{
    int count = 0;
    char *cursor = line;
    for (;;) {
        cursor += strspn(cursor, FIELD_SEPARATORS);
        if (*cursor == '\0') {
            return count;
        }
        if (count < MAX_STORED_FIELDS) {
            fields[count] = cursor;
        }
        count++;
        cursor += strcspn(cursor, FIELD_SEPARATORS);
        if (*cursor != '\0') {
            *cursor++ = '\0';
        }
    }
}

static bool parse_line(const Parser *parser, char *line)
{
    line[strcspn(line, "#")] = '\0';

    char *fields[MAX_STORED_FIELDS];
    int count = split_fields(line, fields);
    if (count == 0) {
        return true;
    }

    if (strcmp(fields[0], "N") == 0) {
        if (count != NODE_FIELD_COUNT) {
            set_error(parser, "N record needs %d fields, got %d",
                      NODE_FIELD_COUNT, count);
            return false;
        }
        return parse_node(parser, fields);
    }
    if (strcmp(fields[0], "E") == 0) {
        if (count != EDGE_FIELD_COUNT) {
            set_error(parser, "E record needs %d fields, got %d",
                      EDGE_FIELD_COUNT, count);
            return false;
        }
        return parse_edge(parser, fields);
    }
    set_error(parser, "unknown record type '%s'", fields[0]);
    return false;
}

/* True if fgets stopped because the buffer filled, not at the end of the line. */
static bool line_was_cut_off(const char *line, FILE *stream)
{
    return strchr(line, '\n') == NULL && !feof(stream);
}

Graph *road_file_read(FILE *stream, const char *source, char *error,
                      size_t error_size)
{
    Parser parser = {
        .graph = graph_create(),
        .source = source,
        .error = error,
        .error_size = error_size,
    };
    if (parser.graph == NULL) {
        set_error(&parser, "out of memory");
        return NULL;
    }

    char line[LINE_BUFFER_SIZE];
    while (fgets(line, sizeof line, stream) != NULL) {
        parser.line_number++;
        if (line_was_cut_off(line, stream)) {
            set_error(&parser, "line is longer than %d characters",
                      LINE_BUFFER_SIZE - 2);
            graph_free(parser.graph);
            return NULL;
        }
        if (!parse_line(&parser, line)) {
            graph_free(parser.graph);
            return NULL;
        }
    }

    if (ferror(stream)) {
        set_error(&parser, "read failed");
        graph_free(parser.graph);
        return NULL;
    }
    if (parser.graph->node_count == 0) {
        set_error(&parser, "file contains no nodes");
        graph_free(parser.graph);
        return NULL;
    }
    return parser.graph;
}

Graph *road_file_load(const char *path, char *error, size_t error_size)
{
    FILE *stream = fopen(path, "r");
    if (stream == NULL) {
        const Parser parser = {
            .source = path,
            .error = error,
            .error_size = error_size,
        };
        set_error(&parser, "cannot open file: %s", strerror(errno));
        return NULL;
    }
    Graph *graph = road_file_read(stream, path, error, error_size);
    fclose(stream);
    return graph;
}
