/*
 * Travel-time cost model. The cost of an arc is seconds, not metres, because
 * time is what a mine cares about and it puts distance, slope and load in a
 * single unit.
 *
 * A truck's speed on an arc is the lowest of these limits:
 *   - the road's speed limit and the truck's top speed;
 *   - engine power: P = F * v, where F = m * g * (grade + rolling resistance);
 *   - traction: a climb that needs more force than the wheels can deliver is
 *     done at a fixed crawl in the lowest gear;
 *   - braking: going downhill, the safe speed falls as the slope steepens, and
 *     falls faster when loaded.
 *
 * The traction limit is what makes load change the route. Under the power limit
 * alone, climb time depends mostly on the height gained, so the shortest and
 * steepest ramp would always win for every truck.
 *
 * Grade is signed: positive is uphill in the direction of travel, so the two
 * directions of a two-way road usually have different costs.
 */
#ifndef COST_H
#define COST_H

#include <stdbool.h>

#include "graph.h"

typedef enum { LOAD_EMPTY, LOAD_FULL } Load;

/* Speed in m/s on a road with this grade (rise / length) and speed limit. */
double cost_speed_mps(double grade, double speed_limit_kmh, Load load);

/* Seconds to drive the arc from node from, or INFINITY if the arc is closed. */
double cost_arc_seconds(const Graph *graph, int from, const Arc *arc, Load load);

/*
 * The highest speed this load reaches on any arc, closed ones included, so the
 * answer stays valid when a road is closed or reopened. No journey can beat
 * (distance / this speed), which gives A* its heuristic. Scans every arc, so
 * compute it once per graph and load rather than once per search.
 */
double cost_top_speed_mps(const Graph *graph, Load load);

#endif /* COST_H */
