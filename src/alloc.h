#ifndef ALLOC_H
#define ALLOC_H


/**
 * Allocate space for an object.
 * If the object is small enough, it goes in the nursery, otherwise it goes into tenure.
 * If a minor collection is needed to free up space in the nursery, then this is done.
 * If the system is out of memory, then return `NULL`.
 */
static void* gc_alloc(void* source, size_t bytes);

/**
 * Move an object from the nursery to tenured space, updating its gateway.
 * If the object is already tenured, then do nothing.
 */
static void gc_age(gcobj);

/**
 * Perform finalization and free memory for a gc-managed object.
 */
static void gc_free(gcobj);


#endif