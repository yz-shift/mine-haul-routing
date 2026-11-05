/*
 * Reads a road network from a text file into a Graph.
 *
 * Format, one record per line. '#' starts a comment and blank lines are ignored:
 *
 *     # N id name x_m y_m elevation_m
 *     N 0 PIT_A 120.0 40.0 -85
 *     N 1 CRUSHER 300.0 210.0 12
 *     # E from to length_m speed_limit_kmh one_way closed
 *     E 0 1 450 40 0 0
 *
 * Rules:
 *   - Node ids count up from 0 in file order, so an id is its array index.
 *   - An edge can only refer to nodes defined above it, and not to its own start.
 *   - length_m and speed_limit_kmh are greater than 0. one_way and closed are 0 or 1.
 *   - length_m is at least the straight-line distance between the two nodes.
 *     A* relies on this: its heuristic divides that distance by the top speed,
 *     and a shorter road could make the heuristic overestimate.
 *
 * Elevation is kept in the node and not repeated on the edge, so slope is always
 * derived from one source and the file cannot contradict itself.
 */
#ifndef ROAD_FILE_H
#define ROAD_FILE_H

#include <stddef.h>
#include <stdio.h>

#include "graph.h"

/*
 * Parses the file at path. Returns a graph owned by the caller, or NULL on
 * failure, with "source:line: reason" written to error. The library never
 * prints, so the caller decides how to report the error.
 */
Graph *road_file_load(const char *path, char *error, size_t error_size);

/* Same as road_file_load, for an open stream. source only labels error messages. */
Graph *road_file_read(FILE *stream, const char *source, char *error,
                      size_t error_size);

#endif /* ROAD_FILE_H */
