Architecture
============

Managed Objects
---------------
Managed objects are used through a handle `gc*`.
This handle is constant for the life of the object.
Every such handle is associated with precicely one engine.

The primary use of objects is to point to underlying data.
Secondarily, the handle also maintains metadata.

The handle also tracks metadata.
In particular, it specifies the layout of the underlying data more precisely, giving the number of subobjects and the number of raw bytes stored.
It also holds marking data during tracing and may hold a finalizer.

The finalizer takes a pointer to the underlying data.
It must not perform any allocation or otherwise trigger garbage collection.
It must not make any assumptions about whether any other objects are collected or cleaned.
Ideally, it is a short function that will always successfully terminate.
Further, there is no guarantee the cleanup will even be run should the program exit.

Each handle knows enough about the layout of its underlying data so that the data can be moved.

Layout
------

There are two layouts: fixed and custom.

A fixed layout specifies a non-empty array of a known length (possibly only length).
Each element in the array is layed out as a (possibly empty) array of subobjects, followed by a number of raw bytes.
The subobjects are `gc*` and subject to tracing, but the raw bytes are not examined.
The subobjects must belong to the same engine as the object itself.

A custom layout is simply a known number of bytes.
The metadata also maintains a tracer function, which is called when a custom-layout object is encountered during a trace.
The tracer function is supplied with the underlying data and a recursion function.
The recursion function should be called on any subobjects embedded in the underlying data.

As a final note, a null `gc*` is ignored by tracing, so it is safe to have null subobjects.

Engine
------
An engine knows how to find all of the objects allocated through it.
Thus, it is able to perform tracing and collection.

When allocating a managed object, not only must memory be allocated for the underlying data, but so must an object handle.
The engine maintains a circularly linked list of at least one handle allocation block.
Each block maintains a fixed number of handles, as well as which of those handles are currently in use.

FIXME: I need a root list, but I only have one root atm
my idea is to call create_root, which returns a pointer to a gc*, which is on the rootlist
allocate in a linked list of plain blocks, 
the idea is that there might only be a few roots in the whole system
we can also find a root by `gc*` if you lose track of what's root

Generations
-----------


Allocation and Collection
-------------------------

There are two generations of objects: baby and tenured.
Baby objects are allocated to the nursery, whereas tenured objects end up using the system's existing memory allocator.
Allocation can trigger collection, which may in turn move the underlying data of objects.

The search for an available handle begins where the last search left off.


TODO:
	objects with cleanup go straight to tenured space
	large objects go straight to tenured space
	objects which last through two minor collections get moved to tenure space

TODO:
	a minor half-collection is triggered when one nursery runs out of space
	major collection is triggered when?
		when X bytes are moved to tenured space from nuseries
		after N minor collections



FIXME nursery not impl'd yet, but it will be two-part, one for allocation and when it's filled the other is collected and the two swap roles.

FIXME mutation not implemented yet
okay, basically:
	without mutation, objects can only point to older objects
	whenever you modify an object's subobject, the subobject might be newer
	so the problem is: if a tenured object is modified to point to a baby object, but the baby is not reachable through only baby objects, we've got a problem
	when you cannot prove that the replacement subobject is older than the current, you add the replacement to the referenced list
	on minor collection, the reference list is also traced (as if they were roots), then emptied
	it's up to the user, since objects might have arbitrary layouts now, though for fixed layout we can layer some utilities


Profiling
---------
FIXME: there needs to be adaquate profiling support


Underlying Allocator
--------------------

FIXME link in a new malloc
We rely in malloc/free at the moment
However, we could rely on a different allocator with the signature `void* some_alloc(gcinfo*)` which might be able to speed things up or compress overhead. We'd then provide a default that just falls back to malloc.