# he4

Fast fixed-memory hash table.

```text
|_| _ |_|
| |(/_  |
He4
```

[![He4 release](https://img.shields.io/badge/release-latest-lightgrey.svg)](https://github.com/sprowell/he4/releases)

[![CII Best Practices](https://bestpractices.coreinfrastructure.org/projects/453/badge)](https://bestpractices.coreinfrastructure.org/projects/453)

| Branch | Status |
| ------ | ------ |
| master | [![Build Status](https://travis-ci.org/sprowell/he4.svg?branch=master)](https://travis-ci.org/sprowell/he4?branch=master) |
| dev    | [![Build Status](https://travis-ci.org/sprowell/he4.svg?branch=dev)](https://travis-ci.org/sprowell/he4) [![Coverage Status](https://coveralls.io/repos/sprowell/he4/badge.svg?branch=dev&service=github)](https://coveralls.io/github/sprowell/he4?branch=dev) |

**Branch Policy:**

- The `master` branch is intended to be stable at all times.
- The `dev` branch is the one where all contributions must be merged before
  being promoted to master.
  - If you plan to propose a patch, please commit into the `dev` branch or a
    separate feature branch. Direct commit to `master` will not be accepted.

## Overview

The idea of this library is to create a fast in-memory hash
table implementation in C99. The following are the design criteria.

- This is a fixed-size hash table; new memory is never allocated by this
  library once the initial table is created, but you _can_ force a rehash.
- Because the table is fixed-size, it can fill up. If it fills up, then
  there are two possible outcomes: new entries are dropped, or older
  entries are dropped.
- A default hash algorithm is supplied; you can use your own. The default
  is the [xxHash algorithm][xxhash].

Why? Well, embedded systems with fixed memory may need hash tables, too.

This library uses [semantic versioning][semver]. The version number is set
in the `CMakeLists.txt` file and can be obtained at runtime with the
`he4_version` function.

You may **contribute** to this project by submitting issues or feature requests,
forking the project and submitting pull requests, and by contributing to the
meagre documentation. All help is welcome!

Please report vulnerabilities on the
[issue tracker](https://github.com/sprowell/he4/issues) with highest priority.

No specific style guide is adopted for this project. The closest is
[this guide][c-style].

## Building

This project is built with [CMake][cmake] and the API documentation is built
with [Doxygen][doxygen].

"Out of source" builds are supported by CMake, and to build
the static and dynamic libraries for your platform, along with the API
documentation, you can do the following from the project root folder.

```bash
mkdir build
cd build
cmake ..
make
```

This populates `doc/api/html` with the API documentation, with all other
build products in the `build` folder.

To build the examples that come with this library, do the following from the
`build` folder.

```bash
make examples
```

The `count` example counts the occurrences of lines in a file and write the
result. It provides an example of a table that maps `char *` keys to concrete
values (that is, values that are not pointers). An example input file
`wordlist.txt` is provided.

## CMake Generators

To instead create an Eclipse project, a Ninja file, or something else, use
a generator. The command `cmake -h` will list the generators supported by your
version of CMake.

## Windows

These instructions should work out of the box for Linux and OS X.

You can get a Windows standalone SDK (for Windows 10 you can
find it [here](https://dev.windows.com/en-US/downloads/windows-10-sdk)).

Be sure to open Visual Studio and create at least one minimal C project so
that any "on demand" components are installed. Then do the following from
the command prompt.

```bash
mkdir build
cd build
cmake ..
```

At this point you should have a `.sln` file you can open in Visual Studio.

This builds just fine with Windows 10 and the Community Edition of Visual
Studio.

## Implementation

This table implementation uses open addressing with linear probing (because
linear probing is simply faster to compute, even though it can lead to
clustering). As such, you should allocate enough space that your fixed-size
table will never have a load factor above 0.7.

This implementation uses lazy deletion, which makes deletion fast, but can
increase search time. To improve on this, any successful search that has
passed a lazily deleted entry will relocate the found entry to the first open
slot. Thus found items "move to the front of the line" to improve search time.

## Performance

**Be aware!** If the table becomes full your performance is going to be
_horrible_. Again, a full table will have _horrible_ performance, since
it cannot automagically rehash. Paradoxically this is worse as the table
becomes larger, because every search may have to go through the entire table to
find an entry.

## Basic Usage

To use the library `#include <he4.h>` and then make use of the functions
found therein. If you are happy with your platform's `malloc` and are not
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

Find an entry in the table with `he4_get`. Alternately use `he4_find`, which
returns a pointer to the item found in the table, as opposed to the item
itself. This allows modification of the item in place in the table, and can
improve performance significantly by avoiding multiple searches. See the
example `basics.c`.

## Rehashing

Of course you can _force_ a rehash. Code like the following is recommended,
if you have memory to spare. Be aware that the rehash function returns `NULL`
to signal that it cannot get memory for the new table.

```c
if (he4_load(table) > 0.7) {
    HE4 * newtable = he4_rehash(table, 0);
    table = newtable == NULL ? table : newtable;
}
```

The second argument (`0`) uses the default new capacity, which is double the
old capacity.

The original table is freed, but _only_ if the new table is successfully
constructed. See the next section for how you might use this strategy.

## Least-Recently-Used

By default the library adds a field to each entry called the _touch index_.
The table has a stored maximum value. Whenever an entry is found or inserted,
its touch index is set to the table maximum and the maximum is then incremented.
The item with the lowest touch index at any time is the least-recently-used
item.

You can use this to clean up the table. Set a threshold and trim the table by
discarding everything with touch index below the threshold. See
`he4_trim`. The new table's touch indices are debited by the threshold (so
least-recently-used information is preserved).

You can also use this when rehashing. Set a threshold and rehash to the same
size table. The items with touch index below the threshold will be removed.
See `he4_trim_and_rehash`.

Maintaining the touch index comes with a slight cost in time and space. If you
do not want this, then uncomment the appropriate line in `CMakeLists.txt`.

```c
// Delete old entries until the table is less than half full.
while (he4_load(table) > 0.5) {
    HE4 * newtable = he4_trim(table, table->max_touch / 4);
    table = newtable == NULL ? table : newtable;
}
```

## Include Files and Dependencies

The library includes [Doug Lea's][dlmalloc] `malloc` implementation. If you
want to use it, then uncomment the correct line in `CMakeLists.txt`.
Alternately you can just `#define` the macros `HE4MALLOC` and `HE4FREE` as
indicated in `he4.h`. In all these cases, `stdlib.h` is no longer included and
no longer required.

The debugging facility uses `stdio.h`. You can `#define NODEBUG` to eliminate
this dependency.

Both `stdint.h` and `stdbool.h` are required, but are also easily replaced.
Building requires `string.h`, and the string library is needed to run. It
should not be hard to replace it if you need to.

If you want debugging but can't tolerate `stdio.h`, have a look at
[this library][savezelda], or at [this implementation][pagetable].

## nostdlib

Working on this. For now you're stuck. Right now you can uncomment
`set(NO_STD_LIB 1)` in the `CMakeLists.txt` and compile, but you will have
a few undefined symbols. On my Mac I get a few undefined symbols.

- `___error`
- `___memcpy_chk`
- `_abort`
- `_memcmp`
- `_memcpy`
- `_memset`
- `_mmap`
- `_munmap`
- `_sysconf`

## Memory Testing

This library is tested for memory leaks using `valgrind`.  For example, you can try the following.

```bash
valgrind --leak-check=full -s ./table-test
valgrind --leak-check=full -s ./insert-test
valgrind --leak-check=full -s ./insert-2-test
```

These should all complete with **no errors** reported.

## License

This is licensed under the two-clause "simplified" BSD license. See the
[LICENSE](LICENSE) file in the distribution.

[xxHash][xxhash] is BSD licensed. See the [LICENSE-xxHash](LICENSE-xxHash)
file in the distribution. The **branch policy** was taken from xxHash.

[Doug Lea's malloc][dlmalloc] is in the public domain.

The Helium creep image used in the generated documentation is from
[Wikipedia][superfluid]. They determine our reality, and the image is
licensed under the [Creative Commons Attribution Share Alike 3.0 License][cc-by-sa-3].

[xxhash]: https://github.com/Cyan4973/xxHash
[dlmalloc]: https://github.com/sailfish009/malloc
[savezelda]: http://git.infradead.org/users/segher/savezelda.git/tree/HEAD:/loader
[pagetable]: http://www.pagetable.com/?p=298
[semver]: http://semver.org
[cmake]: http://cmake.org
[doxygen]: http://doxygen.org
[superfluid]: https://en.wikipedia.org/wiki/Superfluidity#/media/File:Helium-II-creep.svg
[cc-by-sa-3]: http://creativecommons.org/licenses/by-sa/3.0/
[c-style]: https://users.ece.cmu.edu/~eno/coding/CCodingStandard.html
