#ifndef GATEWAY_H
#define GATEWAY_H

/**
 * Abstract type allowing access to gc-managed objects.
 * Gateways never move while they are valid, so a pointer to it will remain valid as long as the object is alive.
 */
typedef struct gc_gate gc_gate;

/**
 * Less verbose handle on gc-managed objects.
 */
typedef gc_gate* gcobj;


/**
 * Type of functions that perform tracing.
 * These functions should do nothing more than call `gc_mark` on any gc-managed objects referenced by the passed object.
 */
typedef void (*tracer_t)(const void*);

/**
 * Type of functions that perform finalization.
 */
typedef void (*finalizer_t)(void*);


/**
 * Create a new gc-managed object from `source`, leaving the input data valid.
 */
gcobj new_gcobj( const void* source, size_t bytes
               , tracer_t
               , finalizer_t);
/**
 * Create a new gc-managed object from `source`, causing the input to become invalid.
 */
gcobj to_gcobj( void* source, size_t bytes
               , tracer_t
               , finalizer_t);

/**
 * Perform a hardware-accelerated query on a gc-managed object.
 */
void ask_gcobj(gcobj, void (*f)(const void* obj, void* res));

/**
 * Update a gc-managed object and return the updated version.
 * This is a persistant update, meaning the input object is unmodified.
 */
gcobj mut_gcobj(gcobj, void (*f)(void* obj));

/**
 * Copy data out of gc-managed memory.
 */
void from_gcobj(gcobj, void* destination);

 /**
  * Prevent a gc-managed object from being collected, even if dead.
  * Note that any sub-objects might still be collected if the are dead and not pinned themselves.
  */
void pin_gcobj(gcobj);

/**
 * Unpin a gc-managed object. See `pin_gcobj`.
 */
void unpin_gcobj(gcobj);


/**
 * Allocate a gateway and return a gcobj handle to it.
 * Remember: `gcobj` remains valid as long as the object is tracable from the roots of the gc system.
 */
static gcobj fresh_gateway();

//TODO inline functinos to update gcobjs



#endif