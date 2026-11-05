CC       := gcc
CFLAGS   := -std=c11 -Wall -Wextra -Werror -pedantic -g -Iinclude
# Generate .d files so that editing a header rebuilds whatever includes it.
DEPFLAGS := -MMD -MP
LDLIBS   := -lm

BUILD    := build

# Everything in src/ except the CLI entry point is linked into each test.
LIB_SRCS := $(filter-out src/main.c,$(wildcard src/*.c))
LIB_OBJS := $(LIB_SRCS:%.c=$(BUILD)/%.o)

APP      := $(BUILD)/mine-route

# Each tests/test_*.c is a standalone program with its own main().
TEST_SRCS := $(wildcard tests/test_*.c)
TEST_OBJS := $(TEST_SRCS:%.c=$(BUILD)/%.o)
TEST_BINS := $(TEST_SRCS:%.c=$(BUILD)/%)

# --errors-for-leak-kinds=all makes any leak, even "still reachable", fail the
# run through --error-exitcode, so the target can gate a task on 0 leaks.
# The exit code is 99 so the CLI test can tell it from the tool's own exit code 1.
VALGRIND := valgrind --leak-check=full --show-leak-kinds=all \
            --errors-for-leak-kinds=all --track-origins=yes --error-exitcode=99

# Speed tests use an optimised build, kept apart from the debug objects.
RELEASE       := $(BUILD)/release
RELEASE_FLAGS := -O2 -DNDEBUG
RELEASE_OBJS  := $(LIB_SRCS:%.c=$(RELEASE)/%.o)
GEN_ROAD      := $(BUILD)/gen_road
BENCH_APP     := $(BUILD)/bench_routing
BENCH_SIDES   := 100 200 300
BENCH_MAPS    := $(BENCH_SIDES:%=$(BUILD)/bench/grid_%.road)
BENCH_PAIRS   := 200
BENCH_SEED    := 2521

# The JSON that the plots are drawn from.
DEMO          := $(BUILD)/demo
DEMO_ROUTE    := --map data/sample_mine.road --from PIT_FACE_A --to CRUSHER

.PHONY: all test valgrind bench demo clean

all: $(APP) $(TEST_BINS)

# set -e stops at the first failing binary and passes its exit code to make.
test: $(APP) $(TEST_BINS)
	@set -e; for t in $(TEST_BINS); do echo "== $$t"; "./$$t"; done
	@echo "== tests/cli_test.sh"; sh tests/cli_test.sh $(APP)

valgrind: $(APP) $(TEST_BINS)
	@set -e; for t in $(TEST_BINS); do echo "== $$t"; $(VALGRIND) "./$$t"; done
	@echo "== tests/cli_test.sh"; RUNNER="$(VALGRIND)" sh tests/cli_test.sh $(APP)

bench: $(BENCH_APP) $(BENCH_MAPS)
	./$(BENCH_APP) $(BENCH_PAIRS) $(BENCH_SEED) $(BENCH_MAPS) > bench/results.json

demo: $(APP)
	@mkdir -p $(DEMO)
	./$(APP) $(DEMO_ROUTE) --load full --output $(DEMO)/full.json
	./$(APP) $(DEMO_ROUTE) --load empty --output $(DEMO)/empty.json
	./$(APP) $(DEMO_ROUTE) --load full --close RAMP_MID_W,RAMP_TOP_W --at RAMP_MID_W \
	    --output $(DEMO)/closure.json

$(BUILD)/bench/grid_%.road: $(GEN_ROAD)
	@mkdir -p $(dir $@)
	./$(GEN_ROAD) $* $(BENCH_SEED) > $@

$(GEN_ROAD): bench/gen_road.c
	$(CC) $(CFLAGS) $(RELEASE_FLAGS) $< -o $@ $(LDLIBS)

$(BENCH_APP): $(RELEASE)/bench/bench_routing.o $(RELEASE_OBJS)
	$(CC) $^ -o $@ $(LDLIBS)

$(RELEASE)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(RELEASE_FLAGS) $(DEPFLAGS) -c $< -o $@

$(APP): $(BUILD)/src/main.o $(LIB_OBJS)
	$(CC) $^ -o $@ $(LDLIBS)

$(BUILD)/tests/%: $(BUILD)/tests/%.o $(LIB_OBJS)
	$(CC) $^ -o $@ $(LDLIBS)

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD)

# Keep test objects, which make would otherwise delete as intermediate files.
.SECONDARY: $(TEST_OBJS)

-include $(TEST_OBJS:.o=.d) $(LIB_OBJS:.o=.d) $(BUILD)/src/main.d \
         $(RELEASE_OBJS:.o=.d) $(RELEASE)/bench/bench_routing.d
