Architecture
============

Managed Objects
---------------
Managed objects are used through a handle `gc*`.
This handle is constant for the life of the object.
Every such handle is associated with precicely one engine.

The primary use of objects is to point to underlying data.
This data is layed out as a (possibly empty) array of subobjects, followed by a number of raw bytes.
The subobjects are `gc*` and subject to tracing, but the raw bytes are not examined.
The subobject must belong to the same engine as the object itself.

The handle also tracks metadata.
In particular, it specifies the layout of the underlying data more precisely, giving the number of subobjects and the number of raw bytes stored.
It also holds marking data during tracing and may hold a cleanup function.

The cleanup function takes a pointer to the underlying data.
It must not perform any allocation or otherwise trigger garbage collection.
It must not make any assumptions about whether any other objects are collected or cleaned.
Ideally, it is a short function that will always successfully terminate.
Further, there is no guarantee the cleanup will even be run should the program exit.

Since each handle is knows the precise layout of its underlying data, that data can be moved.

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
these will undoubtedly involve a special function that replaces a subobject of an object while also registering it to a recently-mutated list (since last nursery collection)
however, it may also require a second function which replaces the entire underlying data (or marks it as updated)
The second is more general, so let's stick with that and leave the former to silly little helper functions.


Profiling
---------
FIXME: there needs to be adaquate profiling support


Underlying Allocator
--------------------

FIXME link in a new malloc
We rely in malloc/free at the moment
However, we could rely on a different allocator with the signature `void* some_alloc(gcinfo*)` which might be able to speed things up or compress overhead. We'd then provide a default that just falls back to malloc.