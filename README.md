# he4
Fast fixed-memory hash table.

```
|_| _ |_|
| |(/_  |
He4
```

## Overview

The idea of this library is to create a fast in-memory hash
table implementation in C99. The following are the design criteria.

 * This is a fixed-size hash table; new memory is never allocated by this
   library once the initial table is created, but you *can* force a rehash.
 * Because the table is fixed-size, it can fill up.  If it fills up, then
   there are two possible outcomes: new entries are dropped, or older
   entries are dropped.
 * A default hash algorithm is supplied; you can use your own.  The default
   is the [xxHash algorithm][xxhash].
   
Why?  Well, embedded systems with fixed memory may need hash tables, too.

## Implementation

This table implementation uses open addressing with linear probing (because
linear probing is simply faster to compute, even though it can lead to
clustering).  As such, you should allocate enough space that your fixed-size
table will never have a load factor above 0.7.

This implementation uses lazy deletion, which makes deletion fast, but can
increase search time.  To improve on this, any successful search that has
passed a lazily deleted entry will relocate the found entry to the first open
slot.  Thus found items "move to the front of the line" to improve search time.

## Performance

**Be aware!**  If the table becomes full your performance is going to be
*horrible*.  Again, a full table will have *horrible* performance, since
it cannot automagically rehash.  Paradoxically this is worse as the table
becomes larger, because every search may have to go through the entire table to
find an entry.

## Rehashing

Of course you can *force* a rehash.  Code like the following is recommended,
if you have memory to spare.

```c
if (he4_load(table) > 0.7) table = he4_rehash(table, 0);
```

The second argument (`0`) uses the default new capacity, which is double the
old capacity.

## Include Files and Dependencies

The library includes [Doug Lea's][dlmalloc] `malloc` implementation.  If you
want to use it, then uncomment the correct line in `CMakeLists.txt`.
Alternately you can just `#define` the macros `HE4MALLOC` and `HE4FREE` as
indicated in `he4.h`.  In all these cases, `stdlib.h` is no longer included and
no longer required.

The debugging facility uses `stdio.h`.  You can `#define NODEBUG` to eliminate
this dependency.

Both `stdint.h` and `stdbool.h` are required, but are also easily replaced.
Building requires `string.h`, and the string library is needed to run.  It
should not be hard to replace it if you need to.

If you want debugging but can't tolerate `stdio.h`, have a look at
[this library][savezelda], or at [this implementation][pagetable].

## License

This is licensed under the two-clause "simplified" BSD license.  See the
[LICENSE](LICENSE) file in the distribution.


[xxhash]: https://github.com/Cyan4973/xxHash
[dlmalloc]: http://g.oswego.edu
[savezelda]: http://git.infradead.org/users/segher/savezelda.git/tree/HEAD:/loader
[pagetable]: http://www.pagetable.com/?p=298
