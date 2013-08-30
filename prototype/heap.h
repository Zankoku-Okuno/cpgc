#ifndef HEAP_H
#define HEAP_H


/**
 * Opaque handle for objects managed by garbage collection.
 */
typedef void* gcobj; // == `gc_gate*`


// ============ Objects ============ //

/**
 * Initialize an object in the current thread's gc heap by copying `x`.
 */
gcobj new_gcobj( size_t bytes, const void* x
               , void (*trace)(void* obj)
               , void (*destroy)(void* obj)
               );
/**
 * Create an object in the current thread's gc heap without copying, invalidating `x`.
 *
 * Only use when creating large or long-lived objects, as gcobjs created this way are immediately tenured.
 */
gcobj to_gcobj( size_t bytes, void* x
              , void (*trace)(void* obj)
              , void (*destroy)(void* obj)
              );

/**
 * Read some interior value from `x` and place the result in `out`.
 */
void ask_gcobj(gcobj x, void (*question)(const void*, void*), void* out);

/**
 * Persistent mutation.
 *
 * Creates a copy of `x` on the heap and applies `f` to that copy. Returns the copy.
 */
gcobj mut_gcobj(gcobj x, void (*f)(void*));

/**
 * Extract a gc'd object into a native pointer by making a copy.
 */
void* from_gcobj(gcobj x);


// ============ Tracing ============ //

/**
 * Add a root to the current thread's garbage collector.
 */
void gc_root(void* x, void (*trace)(void*));

/**
 * Remove a root from the current thread's garbage collector.
 */
void gc_unroot(void* x);

/**
 * Perform a major garbage collection in the current thread.
 */
void gc_run();

/**
 * Mark an object as reachable during a collection cycle.
 * This is meant to be used from the trace function passed during `new_gcobj`.
 */
void gc_mark(gcobj x);


#endif
