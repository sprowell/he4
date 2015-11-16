# he4
Fast fixed-memory hash table.

[![Build Status](https://travis-ci.org/sprowell/he4.svg?branch=master)](https://travis-ci.org/sprowell/he4)

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

This library uses [semantic versioning][semver], and is at version 1.0.0.

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

## Basic Usage

To use the library `#include <he4.h>` and then make use of the functions
found therein.  If you are happy with your platform's `malloc` and are not
constrained by library restrictions, then there is little else you need to
think about (but do read the following two sections on handling full tables).

A sample program that counts the entries in a file is in the `examples` folder.

Create a table with `he4_new` and dispose of it when done with `he4_delete`.
The table methods manage the deallocation of the keys and values.

```c
// Make a new table with the given size and the defaults.  The table
// will map widgets to things.
#define HE4_KEY_TYPE widget *
#define HE4_ENTRY_TYPE thing *
#include <he4.h>
// ...
HE4 * table = he4_new(size, NULL, NULL, NULL, NULL);
// Use the table...
he4_delete(table);
```

Insert items with `he4_insert` and remove them with `he4_remove` (to get the
value back) or `he4_discard` (to let the library free it).

Find an entry in the table with `he4_get`.

## Rehashing

Of course you can *force* a rehash.  Code like the following is recommended,
if you have memory to spare.

```c
if (he4_load(table) > 0.7) table = he4_rehash(table, 0);
```

The second argument (`0`) uses the default new capacity, which is double the
old capacity.

## Least-Recently-Used

By default the library adds a field to each entry called the *touch index*.
The table has a stored maximum value.  Whenever an entry is found or inserted,
its touch index is set to the table maximum and the maximum is then incremented.
The item with the lowest touch index at any time is the least-recently-used
item.

You can use this when rehashing.  Set a threshold and rehash to the same size
table.  The items whose touch index is below the threshold will be removed.
See `he4_trim_and_rehash`.  The new table's touch index will be reset.

This means you can use a "double buffer" kind of scheme, where you have enough
space for two tables, and you flip back and forth between them to maintain a
fixed amount of memory in use.

This comes with a slight cost in time and space.  If you do not want this, then
uncomment the appropriate line in `CMakeLists.txt`.

```c
// Delete old entries until the table is less than half full.
while (he4_load(table) > 0.5)
    table = he4_trim_and_rehash(table, table->capacity, table->max_touch / 4);
```

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

## nostdlib

Working on this.  For now you're stuck.  Right now you can uncomment
`set(NO_STD_LIB 1)` in the `CMakeLists.txt` and compile, but you will have
a few undefined symbols.  On my Mac I get a few undefined symbols.

  * `___error`
  * `___memcpy_chk`
  * `_abort`
  * `_memcmp`
  * `_memcpy`
  * `_memset`
  * `_mmap`
  * `_munmap`
  * `_sysconf`

## License

This is licensed under the two-clause "simplified" BSD license.  See the
[LICENSE](LICENSE) file in the distribution.


[xxhash]: https://github.com/Cyan4973/xxHash
[dlmalloc]: http://g.oswego.edu
[savezelda]: http://git.infradead.org/users/segher/savezelda.git/tree/HEAD:/loader
[pagetable]: http://www.pagetable.com/?p=298
[semver]: http://semver.org
