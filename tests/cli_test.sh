#!/bin/sh
# End-to-end tests for the command-line tool, using only POSIX tools.
# Usage: tests/cli_test.sh path/to/mine-route
# Set RUNNER to run the tool under another program, such as valgrind. Valgrind's
# exit code 99 is not 0 or 1, so every status check below would catch it.

APP=$1
OUT=build/cli_test
MAP=data/sample_mine.road
failures=0
checks=0

rm -rf "$OUT"
mkdir -p "$OUT"

fail() {
    echo "  FAIL [$name]: $1"
    failures=$((failures + 1))
}

# run NAME ARGS...: runs the tool and records its status and output.
run() {
    name=$1
    shift
    # RUNNER is deliberately unquoted, because it holds a command and its options.
    $RUNNER "$APP" "$@" >"$OUT/stdout" 2>"$OUT/stderr"
    status=$?
    checks=$((checks + 1))
}

expect_status() {
    [ "$status" -eq "$1" ] || fail "exit code $status, wanted $1: $(cat "$OUT/stderr")"
}

expect_stdout() {
    grep -qF -- "$1" "$OUT/stdout" || fail "stdout lacks: $1"
}

expect_stderr() {
    grep -qF -- "$1" "$OUT/stderr" || fail "stderr lacks: $1"
}

# The line of the route with this label, without its search-effort fields.
route_line() {
    grep "\"label\":\"$1\"" "$OUT/stdout"
}

cost_of() {
    route_line "$1" | sed 's/.*"cost_s":\([^,]*\),.*/\1/'
}

# ---- Planning ----

run "full truck takes the gentle ramp" --map "$MAP" --from PIT_FACE_A --to CRUSHER --load full
expect_status 0
expect_stdout '"nodes":[0,2,3,4,5,11,12,14]'
expect_stdout '"status":"found"'
expect_stdout '"load":"full"'
expect_stdout '"algorithm":"astar"'
expect_stdout '{"id":14,"name":"CRUSHER"'

run "empty truck takes the steep ramp" --map "$MAP" --from PIT_FACE_A --to CRUSHER --load empty
expect_status 0
expect_stdout '"nodes":[0,2,6,7,12,14]'
expect_stdout '"load":"empty"'

run "load defaults to full" --map "$MAP" --from PIT_FACE_A --to CRUSHER
expect_status 0
expect_stdout '"load":"full"'

run "dijkstra" --map "$MAP" --from PIT_FACE_A --to CRUSHER --algorithm dijkstra
expect_status 0
expect_stdout '"algorithm":"dijkstra"'
dijkstra_cost=$(cost_of initial)

run "astar costs the same as dijkstra" --map "$MAP" --from PIT_FACE_A --to CRUSHER --algorithm astar
expect_status 0
[ "$(cost_of initial)" = "$dijkstra_cost" ] || fail "$(cost_of initial) != $dijkstra_cost"

run "only one route without a closure" --map "$MAP" --from PIT_FACE_A --to CRUSHER
[ "$(grep -c '"label"' "$OUT/stdout")" -eq 1 ] || fail "expected one route"

# ---- Closures and replanning ----

run "closure on the route forces a detour" --map "$MAP" --from PIT_FACE_A --to CRUSHER \
    --close RAMP_MID_W,RAMP_TOP_W --at RAMP_MID_W
expect_status 0
route_line initial | grep -qF '"nodes":[0,2,3,4,5,11,12,14]' || fail "initial route changed"
route_line replanned | grep -qF '"from":4,"to":14' || fail "does not start at --at"
route_line replanned | grep -qF '"nodes":[4,3,2,8,9,10,13,12,14]' || fail "wrong detour"
expect_stdout '{"from":4,"to":5,"length_m":412,"speed_limit_kmh":30,"closed":true}'
expect_stdout '{"from":5,"to":4,"length_m":412,"speed_limit_kmh":30,"closed":true}'

run "replanning starts at --from by default" --map "$MAP" --from PIT_FACE_A --to CRUSHER \
    --close RAMP_MID_W,RAMP_TOP_W
expect_status 0
route_line replanned | grep -qF '"from":0,"to":14' || fail "does not start at --from"

run "closure names may be given in either order" --map "$MAP" --from PIT_FACE_A --to CRUSHER \
    --close RAMP_TOP_W,RAMP_MID_W
expect_status 0
route_line replanned | grep -qF '"nodes":[0,2,8,9,10,13,12,14]' || fail "wrong detour"

run "several closures all apply" --map "$MAP" --from PIT_FACE_A --to CRUSHER --load empty \
    --close STEEP_LOW,STEEP_TOP --close RAMP_MID_W,RAMP_TOP_W
expect_status 0
route_line replanned | grep -qF '"nodes":[0,2,8,9,10,13,12,14]' || fail "wrong detour"

run "closing every exit leaves the destination unreachable" --map "$MAP" --from PIT_FACE_A --to CRUSHER \
    --close PIT_JUNCTION,RAMP_LOW_W --close PIT_JUNCTION,RAMP_LOW_E --close PIT_JUNCTION,STEEP_LOW
expect_status 0
route_line replanned | grep -qF '"status":"unreachable","cost_s":null' || fail "not unreachable"

# ---- Files ----

run "road closed in the file stays closed" --map tests/data/small_mine.road \
    --from WASTE_DUMP --to PIT_A
expect_status 0
expect_stdout '"status":"unreachable"'

run "writes to a file" --map "$MAP" --from PIT_FACE_A --to CRUSHER --output "$OUT/route.json"
expect_status 0
[ ! -s "$OUT/stdout" ] || fail "stdout should be empty"
grep -qF '"status":"found"' "$OUT/route.json" || fail "file lacks the route"

run "unwritable output is an error" --map "$MAP" --from PIT_FACE_A --to CRUSHER \
    --output "$OUT/no/such/dir/route.json"
expect_status 1
expect_stderr "cannot open"

run "missing map is an error" --map "$OUT/none.road" --from A --to B
expect_status 1
expect_stderr "none.road"

printf 'N 0 A 0 0 0\nN 1 B oops 0 0\n' >"$OUT/bad.road"
run "bad map reports the line" --map "$OUT/bad.road" --from A --to B
expect_status 1
expect_stderr "bad.road:2:"
expect_stderr "x_m"

printf 'N 0 A 0 0 0\nN 1 A 1 0 0\nN 2 B 2 0 0\n' >"$OUT/twins.road"
run "ambiguous node name is an error" --map "$OUT/twins.road" --from A --to B
expect_status 1
expect_stderr "more than one node is named 'A'"

# ---- Bad requests ----

run "help" --help
expect_status 0
expect_stdout "Usage: mine-route"

run "no arguments"
expect_status 1
expect_stderr "are required"

run "unknown option" --map "$MAP" --from A --to B --colour red
expect_status 1
expect_stderr "unknown option '--colour'"

run "stray argument" stray
expect_status 1
expect_stderr "unexpected argument 'stray'"

run "option without a value" --map "$MAP" --from
expect_status 1
expect_stderr "--from needs a value"

run "bad load" --map "$MAP" --from PIT_FACE_A --to CRUSHER --load heavy
expect_status 1
expect_stderr "--load must be"

run "bad algorithm" --map "$MAP" --from PIT_FACE_A --to CRUSHER --algorithm bfs
expect_status 1
expect_stderr "--algorithm must be"

run "unknown node" --map "$MAP" --from NOWHERE --to CRUSHER
expect_status 1
expect_stderr "no node named 'NOWHERE'"

run "--at without --close" --map "$MAP" --from PIT_FACE_A --to CRUSHER --at CRUSHER
expect_status 1
expect_stderr "--at only applies"

run "unknown --at node" --map "$MAP" --from PIT_FACE_A --to CRUSHER --close STEEP_LOW,STEEP_TOP --at NOWHERE
expect_status 1
expect_stderr "no node named 'NOWHERE'"

run "closure without a comma" --map "$MAP" --from PIT_FACE_A --to CRUSHER --close STEEP_LOW
expect_status 1
expect_stderr "--close needs A,B"

run "closure of an unknown node" --map "$MAP" --from PIT_FACE_A --to CRUSHER --close STEEP_LOW,NOWHERE
expect_status 1
expect_stderr "no node named 'NOWHERE'"

run "closure of a road that does not exist" --map "$MAP" --from PIT_FACE_A --to CRUSHER --close PIT_FACE_A,CRUSHER
expect_status 1
expect_stderr "no road between 'PIT_FACE_A' and 'CRUSHER'"

echo "$checks run, $failures failed"
[ "$failures" -eq 0 ]
