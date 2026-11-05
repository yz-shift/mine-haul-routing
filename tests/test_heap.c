#include "heap.h"

#include <math.h>
#include <stdbool.h>
#include <stdlib.h>

#include "test_framework.h"

/* Fixed seeds, so a failure reproduces on every run. */
enum { RANDOM_SEED = 2521 };

static int compare_doubles(const void *a, const void *b)
{
    double left = *(const double *)a;
    double right = *(const double *)b;
    return (left > right) - (left < right);
}

/* Evenly spread keys in [0, 1000), with ties being very unlikely. */
static double spread_key(void)
{
    return rand() / (RAND_MAX + 1.0) * 1000.0;
}

/* Only 20 distinct keys, so most entries tie with others. */
static double tied_key(void)
{
    return rand() % 20;
}

TEST(test_create_rejects_non_positive_capacity)
{
    ASSERT_TRUE(heap_create(0) == NULL);
    ASSERT_TRUE(heap_create(-1) == NULL);
}

TEST(test_free_null_is_safe)
{
    heap_free(NULL);
}

TEST(test_new_heap_is_empty)
{
    Heap *heap = heap_create(4);
    ASSERT_TRUE(heap != NULL);
    int item = -1;
    double key = -1;
    ASSERT_EQ_INT(0, heap_size(heap));
    ASSERT_TRUE(!heap_contains(heap, 0));
    ASSERT_TRUE(!heap_pop_min(heap, &item, &key));
    ASSERT_EQ_INT(-1, item);
    heap_free(heap);
}

TEST(test_pops_small_set_in_key_order)
{
    Heap *heap = heap_create(5);
    ASSERT_TRUE(heap != NULL);
    ASSERT_TRUE(heap_insert(heap, 0, 30.5));
    ASSERT_TRUE(heap_insert(heap, 1, -2));
    ASSERT_TRUE(heap_insert(heap, 2, 10));
    ASSERT_TRUE(heap_insert(heap, 3, 0));
    ASSERT_TRUE(heap_insert(heap, 4, 20));
    ASSERT_EQ_INT(5, heap_size(heap));

    const int expected_items[] = {1, 3, 2, 4, 0};
    const double expected_keys[] = {-2, 0, 10, 20, 30.5};
    for (int i = 0; i < 5; i++) {
        int item;
        double key;
        ASSERT_TRUE(heap_pop_min(heap, &item, &key));
        ASSERT_EQ_INT(expected_items[i], item);
        ASSERT_NEAR(expected_keys[i], key, 0);
    }
    ASSERT_EQ_INT(0, heap_size(heap));
    heap_free(heap);
}

TEST(test_insert_rejects_bad_items_and_nan)
{
    Heap *heap = heap_create(2);
    ASSERT_TRUE(heap != NULL);
    ASSERT_TRUE(!heap_insert(heap, -1, 1));
    ASSERT_TRUE(!heap_insert(heap, 2, 1));
    ASSERT_TRUE(!heap_insert(heap, 0, NAN));
    ASSERT_EQ_INT(0, heap_size(heap));

    ASSERT_TRUE(heap_insert(heap, 0, 5));
    ASSERT_TRUE(!heap_insert(heap, 0, 1));
    ASSERT_EQ_INT(1, heap_size(heap));
    heap_free(heap);
}

TEST(test_contains_follows_insert_and_pop)
{
    Heap *heap = heap_create(2);
    ASSERT_TRUE(heap != NULL);
    ASSERT_TRUE(!heap_contains(heap, -1));
    ASSERT_TRUE(!heap_contains(heap, 2));

    ASSERT_TRUE(heap_insert(heap, 1, 5));
    ASSERT_TRUE(heap_contains(heap, 1));
    ASSERT_TRUE(!heap_contains(heap, 0));

    int item;
    double key;
    ASSERT_TRUE(heap_pop_min(heap, &item, &key));
    ASSERT_TRUE(!heap_contains(heap, 1));
    heap_free(heap);
}

TEST(test_popped_item_can_be_inserted_again)
{
    Heap *heap = heap_create(1);
    ASSERT_TRUE(heap != NULL);
    ASSERT_TRUE(heap_insert(heap, 0, 5));
    int item;
    double key;
    ASSERT_TRUE(heap_pop_min(heap, &item, &key));
    ASSERT_TRUE(heap_insert(heap, 0, 7));
    ASSERT_TRUE(heap_pop_min(heap, &item, &key));
    ASSERT_NEAR(7, key, 0);
    heap_free(heap);
}

TEST(test_decrease_key_moves_item_to_the_front)
{
    Heap *heap = heap_create(4);
    ASSERT_TRUE(heap != NULL);
    ASSERT_TRUE(heap_insert(heap, 0, 10));
    ASSERT_TRUE(heap_insert(heap, 1, 20));
    ASSERT_TRUE(heap_insert(heap, 2, 30));
    ASSERT_TRUE(heap_insert(heap, 3, 40));
    ASSERT_TRUE(heap_decrease_key(heap, 3, 5));
    ASSERT_EQ_INT(4, heap_size(heap));

    int item;
    double key;
    ASSERT_TRUE(heap_pop_min(heap, &item, &key));
    ASSERT_EQ_INT(3, item);
    ASSERT_NEAR(5, key, 0);
    ASSERT_TRUE(heap_pop_min(heap, &item, &key));
    ASSERT_EQ_INT(0, item);
    heap_free(heap);
}

TEST(test_decrease_key_keeps_position_table_correct)
{
    /* Each decrease moves other items, whose positions must then still be right. */
    Heap *heap = heap_create(4);
    ASSERT_TRUE(heap != NULL);
    ASSERT_TRUE(heap_insert(heap, 0, 10));
    ASSERT_TRUE(heap_insert(heap, 1, 20));
    ASSERT_TRUE(heap_insert(heap, 2, 30));
    ASSERT_TRUE(heap_insert(heap, 3, 40));
    ASSERT_TRUE(heap_decrease_key(heap, 3, 5));
    ASSERT_TRUE(heap_decrease_key(heap, 1, 4));
    ASSERT_TRUE(heap_decrease_key(heap, 0, 3));

    const int expected[] = {0, 1, 3, 2};
    for (int i = 0; i < 4; i++) {
        int item;
        double key;
        ASSERT_TRUE(heap_pop_min(heap, &item, &key));
        ASSERT_EQ_INT(expected[i], item);
    }
    heap_free(heap);
}

TEST(test_decrease_key_to_equal_key_is_accepted)
{
    Heap *heap = heap_create(1);
    ASSERT_TRUE(heap != NULL);
    ASSERT_TRUE(heap_insert(heap, 0, 5));
    ASSERT_TRUE(heap_decrease_key(heap, 0, 5));

    int item;
    double key;
    ASSERT_TRUE(heap_pop_min(heap, &item, &key));
    ASSERT_NEAR(5, key, 0);
    heap_free(heap);
}

TEST(test_decrease_key_rejects_bad_requests_and_changes_nothing)
{
    Heap *heap = heap_create(3);
    ASSERT_TRUE(heap != NULL);
    ASSERT_TRUE(heap_insert(heap, 0, 5));

    ASSERT_TRUE(!heap_decrease_key(heap, 0, 6));
    ASSERT_TRUE(!heap_decrease_key(heap, 0, NAN));
    ASSERT_TRUE(!heap_decrease_key(heap, 1, 1));
    ASSERT_TRUE(!heap_decrease_key(heap, -1, 1));
    ASSERT_TRUE(!heap_decrease_key(heap, 3, 1));
    ASSERT_EQ_INT(1, heap_size(heap));

    int item;
    double key;
    ASSERT_TRUE(heap_pop_min(heap, &item, &key));
    ASSERT_NEAR(5, key, 0);
    ASSERT_TRUE(!heap_decrease_key(heap, 0, 1));
    heap_free(heap);
}

/* Inserts every item with a random key, then checks the pops against a sort. */
static bool pops_in_sorted_order(int count, double (*next_key)(void))
{
    Heap *heap = heap_create(count);
    double *sorted = malloc((size_t)count * sizeof(double));
    bool *seen = calloc((size_t)count, sizeof(bool));
    bool ok = heap != NULL && sorted != NULL && seen != NULL;

    for (int i = 0; ok && i < count; i++) {
        sorted[i] = next_key();
        ok = heap_insert(heap, i, sorted[i]);
    }
    if (ok) {
        qsort(sorted, (size_t)count, sizeof(double), compare_doubles);
    }
    for (int i = 0; ok && i < count; i++) {
        int item;
        double key;
        ok = heap_pop_min(heap, &item, &key) && key == sorted[i] && item >= 0 &&
             item < count && !seen[item];
        if (ok) {
            seen[item] = true;
        }
    }
    ok = ok && heap_size(heap) == 0;

    heap_free(heap);
    free(sorted);
    free(seen);
    return ok;
}

TEST(test_random_inserts_pop_in_sorted_order)
{
    const int sizes[] = {1, 2, 3, 7, 8, 100, 1000, 20000};
    srand(RANDOM_SEED);
    for (size_t i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        ASSERT_TRUE(pops_in_sorted_order(sizes[i], spread_key));
    }
}

TEST(test_random_inserts_with_many_ties_pop_in_sorted_order)
{
    const int sizes[] = {2, 7, 100, 1000, 20000};
    srand(RANDOM_SEED + 1);
    for (size_t i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        ASSERT_TRUE(pops_in_sorted_order(sizes[i], tied_key));
    }
}

/*
 * Runs random operations against both the heap and a plain array searched by
 * brute force. The two must agree after every step. This reaches interleavings
 * that hand-written cases miss, such as a decrease-key straight after a pop.
 */
TEST(test_random_operations_match_a_simple_model)
{
    enum { CAPACITY = 200, STEPS = 20000 };
    bool queued[CAPACITY] = {false};
    double keys[CAPACITY] = {0};
    int queued_count = 0;

    Heap *heap = heap_create(CAPACITY);
    ASSERT_TRUE(heap != NULL);
    srand(RANDOM_SEED + 2);

    for (int step = 0; step < STEPS; step++) {
        int item = rand() % CAPACITY;
        int operation = rand() % 3;

        if (operation == 0) {
            double key = spread_key();
            ASSERT_EQ_INT(!queued[item], heap_insert(heap, item, key));
            if (!queued[item]) {
                queued[item] = true;
                keys[item] = key;
                queued_count++;
            }
        } else if (operation == 1) {
            /* Half of these try to raise the key, which must be refused. */
            double key = keys[item] + (rand() % 2 ? 1 : -1) * spread_key();
            bool allowed = queued[item] && key <= keys[item];
            ASSERT_EQ_INT(allowed, heap_decrease_key(heap, item, key));
            if (allowed) {
                keys[item] = key;
            }
        } else {
            int popped;
            double popped_key;
            bool got = heap_pop_min(heap, &popped, &popped_key);
            ASSERT_EQ_INT(queued_count > 0, got);
            if (got) {
                for (int i = 0; i < CAPACITY; i++) {
                    ASSERT_TRUE(!queued[i] || popped_key <= keys[i]);
                }
                ASSERT_TRUE(queued[popped]);
                ASSERT_NEAR(keys[popped], popped_key, 0);
                queued[popped] = false;
                queued_count--;
            }
        }

        ASSERT_EQ_INT(queued_count, heap_size(heap));
        ASSERT_EQ_INT(queued[item], heap_contains(heap, item));
    }
    heap_free(heap);
}

int main(void)
{
    RUN_TEST(test_create_rejects_non_positive_capacity);
    RUN_TEST(test_free_null_is_safe);
    RUN_TEST(test_new_heap_is_empty);
    RUN_TEST(test_pops_small_set_in_key_order);
    RUN_TEST(test_insert_rejects_bad_items_and_nan);
    RUN_TEST(test_contains_follows_insert_and_pop);
    RUN_TEST(test_popped_item_can_be_inserted_again);
    RUN_TEST(test_decrease_key_moves_item_to_the_front);
    RUN_TEST(test_decrease_key_keeps_position_table_correct);
    RUN_TEST(test_decrease_key_to_equal_key_is_accepted);
    RUN_TEST(test_decrease_key_rejects_bad_requests_and_changes_nothing);
    RUN_TEST(test_random_inserts_pop_in_sorted_order);
    RUN_TEST(test_random_inserts_with_many_ties_pop_in_sorted_order);
    RUN_TEST(test_random_operations_match_a_simple_model);
    return TEST_SUMMARY();
}
