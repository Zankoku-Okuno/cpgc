#ifndef TRACE_H
#define TRACE_H


/**
 * Mark a gcobj as alive during a collection cycle.
 */
void gc_mark(gcobj);

/**
 * Add an object (with tracer function) to the tracer as a root object.
 */
void gc_root(void*, tracer_t);

/**
 * Remove an object from the list of root objects.
 */
void gc_unroot(void*);

/**
 * Perform a trace.
 * Pass `0` in stage for minor, `1` for major.
 */
static void trace(int stage);


#endif