#include "cost.h"

#include <math.h>

#define KMH_PER_MPS 3.6

/*
 * Round numbers of the right order for a large haul truck, chosen so the model
 * is easy to explain. They are assumptions, not a manufacturer's specification.
 */
static const double GRAVITY_MPS2 = 9.81;
static const double ROLLING_RESISTANCE = 0.03;  /* As a fraction of weight. */
static const double WHEEL_POWER_W = 800e3;
static const double MAX_TRACTION_N = 450e3;
static const double TOP_SPEED_KMH = 60.0;
static const double CRAWL_SPEED_KMH = 4.0;

typedef struct {
    double mass_kg;
    double brake_kmh_per_percent; /* Speed lost for each 1% of downhill grade. */
    double min_downhill_kmh;
} TruckState;

static const TruckState EMPTY_TRUCK = {100e3, 2.0, 20.0};
static const TruckState FULL_TRUCK = {250e3, 4.0, 10.0};

static double min_double(double a, double b)
{
    return a < b ? a : b;
}

/* Speed the drivetrain can hold, in km/h. Only limits when force is needed. */
static double drivetrain_limit_kmh(double grade, const TruckState *truck)
{
    double resistance = grade + ROLLING_RESISTANCE;
    if (resistance <= 0) {
        return TOP_SPEED_KMH; /* Gravity alone keeps the truck moving. */
    }
    double force_n = truck->mass_kg * GRAVITY_MPS2 * resistance;
    if (force_n > MAX_TRACTION_N) {
        return CRAWL_SPEED_KMH;
    }
    return WHEEL_POWER_W / force_n * KMH_PER_MPS;
}

/* Safe speed under braking, in km/h. Only limits when going downhill. */
static double braking_limit_kmh(double grade, const TruckState *truck)
{
    if (grade >= 0) {
        return TOP_SPEED_KMH;
    }
    double percent_down = -grade * 100.0;
    double limit = TOP_SPEED_KMH - truck->brake_kmh_per_percent * percent_down;
    return limit > truck->min_downhill_kmh ? limit : truck->min_downhill_kmh;
}

double cost_speed_mps(double grade, double speed_limit_kmh, Load load)
{
    const TruckState *truck = load == LOAD_FULL ? &FULL_TRUCK : &EMPTY_TRUCK;
    double kmh = min_double(speed_limit_kmh, TOP_SPEED_KMH);
    kmh = min_double(kmh, drivetrain_limit_kmh(grade, truck));
    kmh = min_double(kmh, braking_limit_kmh(grade, truck));
    return kmh / KMH_PER_MPS;
}

static double arc_speed_mps(const Graph *graph, int from, const Arc *arc, Load load)
{
    double rise_m = graph->nodes[arc->to].elevation_m - graph->nodes[from].elevation_m;
    return cost_speed_mps(rise_m / arc->length_m, arc->speed_limit_kmh, load);
}

double cost_arc_seconds(const Graph *graph, int from, const Arc *arc, Load load)
{
    if (arc->closed) {
        return INFINITY;
    }
    return arc->length_m / arc_speed_mps(graph, from, arc, load);
}

double cost_top_speed_mps(const Graph *graph, Load load)
{
    double top = 0;
    for (int from = 0; from < graph->node_count; from++) {
        const Node *node = &graph->nodes[from];
        for (int i = 0; i < node->arc_count; i++) {
            double speed = arc_speed_mps(graph, from, &node->arcs[i], load);
            if (speed > top) {
                top = speed;
            }
        }
    }
    return top;
}
