#include "json_out.h"

static void write_string(FILE *out, const char *text)
{
    fputc('"', out);
    for (const unsigned char *c = (const unsigned char *)text; *c != '\0'; c++) {
        if (*c == '"' || *c == '\\') {
            fprintf(out, "\\%c", *c);
        } else if (*c < 0x20) {
            fprintf(out, "\\u%04x", *c);
        } else {
            fputc(*c, out);
        }
    }
    fputc('"', out);
}

static void write_node(FILE *out, const Graph *graph, int id)
{
    const Node *node = &graph->nodes[id];
    fprintf(out, "{\"id\":%d,\"name\":", id);
    write_string(out, node->name);
    fprintf(out, ",\"x_m\":%.15g,\"y_m\":%.15g,\"elevation_m\":%.15g}", node->x_m,
            node->y_m, node->elevation_m);
}

static void write_arc(FILE *out, int from, const Arc *arc)
{
    fprintf(out,
            "{\"from\":%d,\"to\":%d,\"length_m\":%.15g,\"speed_limit_kmh\":%.15g,"
            "\"closed\":%s}",
            from, arc->to, arc->length_m, arc->speed_limit_kmh,
            arc->closed ? "true" : "false");
}

static const char *status_name(RouteStatus status)
{
    switch (status) {
    case ROUTE_FOUND:
        return "found";
    case ROUTE_UNREACHABLE:
        return "unreachable";
    case ROUTE_BAD_NODE:
        return "bad_node";
    case ROUTE_OUT_OF_MEMORY:
        return "out_of_memory";
    }
    return "unknown";
}

static void write_report(FILE *out, const RouteReport *report)
{
    const Route *route = report->route;
    fputs("{\"label\":", out);
    write_string(out, report->label);
    fputs(",\"algorithm\":", out);
    write_string(out, report->algorithm);
    fprintf(out, ",\"load\":\"%s\",\"from\":%d,\"to\":%d,\"status\":\"%s\",",
            report->load == LOAD_FULL ? "full" : "empty", report->from, report->to,
            status_name(report->status));
    /* JSON has no infinity, so a missing result is null. */
    if (report->status == ROUTE_FOUND) {
        fprintf(out, "\"cost_s\":%.15g,\"distance_m\":%.15g,", route->cost_s,
                route->distance_m);
    } else {
        fputs("\"cost_s\":null,\"distance_m\":null,", out);
    }
    fprintf(out, "\"expanded\":%d,\"nodes\":[", route->expanded);
    for (int i = 0; i < route->node_count; i++) {
        fprintf(out, "%s%d", i == 0 ? "" : ",", route->nodes[i]);
    }
    fputs("]}", out);
}

void json_out_write(FILE *out, const Graph *graph, const RouteReport *reports,
                    int report_count)
{
    fputs("{\n\"nodes\":[", out);
    for (int id = 0; id < graph->node_count; id++) {
        fputs(id == 0 ? "\n" : ",\n", out);
        write_node(out, graph, id);
    }

    fputs("\n],\n\"arcs\":[", out);
    bool first = true;
    for (int from = 0; from < graph->node_count; from++) {
        const Node *node = &graph->nodes[from];
        for (int i = 0; i < node->arc_count; i++) {
            fputs(first ? "\n" : ",\n", out);
            first = false;
            write_arc(out, from, &node->arcs[i]);
        }
    }

    fputs("\n],\n\"routes\":[", out);
    for (int i = 0; i < report_count; i++) {
        fputs(i == 0 ? "\n" : ",\n", out);
        write_report(out, &reports[i]);
    }
    fputs("\n]\n}\n", out);
}
