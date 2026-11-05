/*
 * Indexed binary min-heap, used as the priority queue for Dijkstra and A*.
 *
 * Items are integers in [0, capacity), which for routing are node ids. Each item
 * is in the heap at most once. A position table maps an item to its slot in the
 * heap array, so decrease-key finds the item in O(1) instead of searching.
 *
 * Costs: insert, pop and decrease-key are O(log n). Contains is O(1).
 * Memory is O(capacity), allocated once up front, so these operations never
 * allocate and cannot run out of memory part-way through a search.
 *
 * Keys that compare equal come out in an unspecified order.
 */
#ifndef HEAP_H
#define HEAP_H

#include <stdbool.h>

typedef struct Heap Heap;

/* Returns an empty heap, or NULL if capacity is not positive or out of memory. */
Heap *heap_create(int capacity);

/* Frees the heap. Does nothing if heap is NULL. */
void heap_free(Heap *heap);

int heap_size(const Heap *heap);

/* False for an item that is out of range, as well as one that is not queued. */
bool heap_contains(const Heap *heap, int item);

/* Returns false if item is out of range or already queued, or if key is NaN. */
bool heap_insert(Heap *heap, int item, double key);

/*
 * Lowers item's key. An equal key is accepted and changes nothing.
 * Returns false, leaving the heap unchanged, if item is not queued, if key is
 * higher than the current key, or if key is NaN.
 */
bool heap_decrease_key(Heap *heap, int item, double key);

/* Removes the entry with the lowest key. Returns false if the heap is empty. */
bool heap_pop_min(Heap *heap, int *item, double *key);

#endif /* HEAP_H */
