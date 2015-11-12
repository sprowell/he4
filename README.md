# he4
Fast fixed-memory hash table.

```
|_| _ |_|
| |(/_  |
He4
```

## Overview

The idea of this library is to create a very, very fast in-memory hash
table implementation. The following are the design criteria.

 * This is a fixed-size hash table; new memory is never allocated by this
   library once the initial table is created.
 * Because the table is fixed-size, it can fill up.  If it fills up, then
   there are two possible outcomes: new entries are dropped, or older
   entries are dropped.
 * A default hash algorithm is supplied; you can use your own.  The default
   is the xxHash algorithm (https://github.com/Cyan4973/xxHash).

This table implementation uses open addressing with linear probing (because
linear probing is simply faster to compute).  As such, you should allocate
enough space that your fixed-size table will never have a load factor above
0.7.

This implementation uses lazy deletion, which makes deletion fast, but can
increase search time.  To improve on this, any successful search that has
passed a lazily deleted entry will relocate the found entry to the open
slot.  Thus found items potentially "move to the front of the line" to
improve search time.

**Be aware!**  If the table becomes full your performance is going to be
*horrible*.  Again, a full table will have *horrible* performance, since
it cannot automagically rehash.
