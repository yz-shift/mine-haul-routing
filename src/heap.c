#include "heap.h"

#include <math.h>
#include <stdlib.h>

enum { NOT_QUEUED = -1 };

typedef struct {
    int item;
    double key;
} Entry;

/*
 * entries[0..size) is the heap: the children of slot i are slots 2i+1 and 2i+2,
 * and no entry has a lower key than its parent.
 * positions[item] is the item's slot in entries, or NOT_QUEUED.
 */
struct Heap {
    Entry *entries;
    int *positions;
    int size;
    int capacity;
};

/* Every write to entries goes through here, so positions cannot fall out of sync. */
static void place(Heap *heap, int slot, Entry entry)
{
    heap->entries[slot] = entry;
    heap->positions[entry.item] = slot;
}

/*
 * The sift functions move an empty slot along the path and write entry once at
 * the end, which costs one write per level where swapping would cost two.
 */
static void sift_up(Heap *heap, int slot, Entry entry)
{
    while (slot > 0) {
        int parent = (slot - 1) / 2;
        if (heap->entries[parent].key <= entry.key) {
            break;
        }
        place(heap, slot, heap->entries[parent]);
        slot = parent;
    }
    place(heap, slot, entry);
}

static void sift_down(Heap *heap, int slot, Entry entry)
{
    /* A slot has a left child exactly when slot < size / 2. */
    while (slot < heap->size / 2) {
        int child = 2 * slot + 1;
        if (child + 1 < heap->size &&
            heap->entries[child + 1].key < heap->entries[child].key) {
            child++;
        }
        if (entry.key <= heap->entries[child].key) {
            break;
        }
        place(heap, slot, heap->entries[child]);
        slot = child;
    }
    place(heap, slot, entry);
}

Heap *heap_create(int capacity)
{
    if (capacity <= 0) {
        return NULL;
    }
    Heap *heap = calloc(1, sizeof(Heap));
    if (heap == NULL) {
        return NULL;
    }
    /* calloc checks the size multiplication for overflow; malloc would not. */
    heap->entries = calloc((size_t)capacity, sizeof(Entry));
    heap->positions = calloc((size_t)capacity, sizeof(int));
    if (heap->entries == NULL || heap->positions == NULL) {
        heap_free(heap);
        return NULL;
    }
    heap->capacity = capacity;
    for (int item = 0; item < capacity; item++) {
        heap->positions[item] = NOT_QUEUED;
    }
    return heap;
}

void heap_free(Heap *heap)
{
    if (heap == NULL) {
        return;
    }
    free(heap->entries);
    free(heap->positions);
    free(heap);
}

int heap_size(const Heap *heap)
{
    return heap->size;
}

bool heap_contains(const Heap *heap, int item)
{
    return item >= 0 && item < heap->capacity &&
           heap->positions[item] != NOT_QUEUED;
}

bool heap_insert(Heap *heap, int item, double key)
{
    if (item < 0 || item >= heap->capacity || heap_contains(heap, item) ||
        isnan(key)) {
        return false;
    }
    /* Each item is queued at most once, so size can never exceed capacity. */
    int slot = heap->size++;
    sift_up(heap, slot, (Entry){.item = item, .key = key});
    return true;
}

bool heap_decrease_key(Heap *heap, int item, double key)
{
    if (!heap_contains(heap, item) || isnan(key)) {
        return false;
    }
    int slot = heap->positions[item];
    if (key > heap->entries[slot].key) {
        return false;
    }
    /* A lower key can only break the ordering with the parent, never a child. */
    sift_up(heap, slot, (Entry){.item = item, .key = key});
    return true;
}

bool heap_pop_min(Heap *heap, int *item, double *key)
{
    if (heap->size == 0) {
        return false;
    }
    Entry min = heap->entries[0];
    heap->positions[min.item] = NOT_QUEUED;
    heap->size--;

    /* The last entry leaves a gap at the root and sinks to its proper level. */
    if (heap->size > 0) {
        sift_down(heap, 0, heap->entries[heap->size]);
    }
    *item = min.item;
    *key = min.key;
    return true;
}
