#ifndef HE4_H
#define HE4_H

/**
 * @file
 * Public API definitions for the He4 library.
 *
 * @code{text}
 * |_| _ |_|
 * | |(/_  |
 * @endcode
 *
 * Copyright (c) 2015 by Stacy Prowell, all rights reserved.  Licensed under
 * the BSD 2-Clause license.  See the file license that is part of this
 * distribution.  This file may not be copied, modified, or distributed except
 * according to those terms.
 *
 * # Overview
 *
 * Note: Superfluid helium 4 has a viscosity of zero, the lowest of any
 * substance.  It can flow up and over barriers.
 *
 * The idea of this library is to create a very, very fast in-memory hash
 * table implementation. The following are the design criteria.
 *
 *   * This is a fixed-size hash table; new memory is never allocated by this
 *     library once the initial table is created.
 *   * Because the table is fixed-size, it can fill up.  If it fills up, then
 *     there are two possible outcomes: new entries are dropped, or older
 *     entries are dropped.
 *   * A default hash algorithm is supplied; you can use your own.  The default
 *     is the xxHash algorithm (https://github.com/Cyan4973/xxHash).
 *
 * This table implementation uses open addressing with linear probing (because
 * linear probing is simply faster to compute).  As such, you should allocate
 * enough space that your fixed-size table will never have a load factor above
 * 0.7.
 *
 * This implementation uses lazy deletion, which makes deletion fast, but can
 * increase search time.  To improve on this, any successful search that has
 * passed a lazily deleted entry will relocate the found entry to the open
 * slot.  Thus found items potentially "move to the front of the line" to
 * improve search time.
 *
 * **Be aware!**  If the table becomes full your performance is going to be
 * *horrible*.  Again, a full table will have *horrible* performance, since
 * it cannot automagically rehash.
 *
 */

/*
 * By default stdio.h is included.  To avoid this, you just need to define
 * NODEBUG, as the functions are only used for the DEBUG macro.
 */

#ifndef NODEBUG
#include <stdio.h>
#endif

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

//======================================================================
// Type handling.
//======================================================================

// Define memory handling.  Feel free to override this with jeMalloc,
// nedmalloc, or something else.  Note that to do this you simply
// need to include the appropriate header and then \#define HE4MALLOC
// and HE4FREE before you include this file.
//
// Whatever method you use should also zero out memory, a la calloc.
// This is important!

// This library comes with Doug Lea's malloc.  Set the correct value in the
// CMakeLists.txt file and compile.  If you just set the HE4_DLMALLOC here you
// won't magically get the necessary compiled code.

#ifdef HE4_DLMALLOC
#  include "../src/malloc.h"
#  define HE4MALLOC(m_thing, m_number) \
        (m_thing *)dlcalloc(m_number, sizeof(m_thing))
#  define HE4FREE(m_ptr) \
        ((m_ptr == NULL) ? NULL : dlfree(m_ptr), NULL)
#else
#ifndef HE4MALLOC
#include <stdlib.h>
/**
 * Allocate memory.
 * @param m_thing     The thing to allocate.  A pointer to this type is
 *                    returned.
 * @param m_number    How many things to allocate.
 * @return            A pointer to the allocated things.
 */
#define HE4MALLOC(m_thing, m_number) \
        (m_thing *)calloc(m_number, sizeof(m_thing))
#endif // HE4MALLOC
#ifndef HE4FREE
#include <stdlib.h>
/**
 * Free a pointer.  NULL pointers are ignored.
 *
 * @param m_ptr       Pointer to the thing to deallocate.
 */
#define HE4FREE(m_ptr) \
		{ if (m_ptr != NULL) free(m_ptr); }
#endif // HE4FREE
#endif // HE4_DLMALLOC

#ifndef HE4_ENTRY_TYPE
#  define HE4_ENTRY_TYPE void *
#endif
/**
 * Specify the type of an entry.  By default this is `void *`.  To override
 * this (so the compiler can type check your code) define the macro
 * `HE4_ENTRY_TYPE`.
 */
typedef HE4_ENTRY_TYPE he4_entry_t;

#ifndef HE4_KEY_TYPE
#  define HE4_KEY_TYPE void *
#endif
/**
 * Specify the type of an key.  By default this is `void *`.  To override
 * this (so the compiler can type check your code) define the macro
 * `HE4_KEY_TYPE`.
 */
typedef HE4_KEY_TYPE he4_key_t;

#ifndef HE4_HASH_TYPE
#  define HE4_HASH_TYPE uint32_t
#else
#  define HE4_USER_HASH
#endif
/**
 * Specify the type of a hash value.  By default this is `uint32_t`.  To
 * override this define the macro `HE4_HASH_TYPE` and be sure you provide
 * your own properly typed hash function when you create a table, or the
 * library won't create the table.
 */
typedef HE4_HASH_TYPE he4_hash_t;

//======================================================================
// Debugging.
//======================================================================

/**
 * This is the global debugging flag.  Right now there are two settings:
 * 0 suppresses, and 1 enables.  The default is zero.  See the `DEBUG` macro.
 */
extern int he4_debug;

#ifndef NODEBUG
/**
 * Macro to output a debug message.  This works like `printf`.  To omit this
 * from the code (perhaps for performance) you should define `NODEBUG`.
 *
 * @param m_fmt         The format string which must be a string literal.
 */
#define DEBUG(m_fmt, ...) \
    if (he4_debug) fprintf(stderr, "DEBUG (%s:%d): " m_fmt "\n", \
        __FILE__, __LINE__, ## __VA_ARGS__);
#else
/**
 * Disabled debug macro.
 */
#define DEBUG(m_fmt, ...)
#endif

/**
 * Obtain a string specifying the version number for the library.  He4 uses
 * semantic versioning.
 *
 * @return              The version number, as a string.  Do not deallocate!
 */
char * he4_version();

//======================================================================
// Structure information.
//======================================================================

/**
 * Structure used for a mapping in a hash table.
 */
typedef struct {
    he4_key_t key;          ///< Key.
    size_t klen;            ///< Length of key in bytes.
    he4_entry_t entry;      ///< Data.
    he4_hash_t hash;        ///< Hash of the key.
#ifndef HE4NOTOUCH
    size_t touch;           ///< The touch index.
#endif // HE4NOTOUCH
} he4_map_t;

/**
 * Structure defining the hash table.
 */
typedef struct {
    /// Hash function.
    he4_hash_t (* hash)(he4_key_t key, size_t klen);

    /// Key comparison function.
    int (* compare)(he4_key_t key1, size_t klen1, he4_key_t key2,
                    size_t klen2);

    /// Key deallocation function.
    void (* delete_key)(he4_key_t key);

    /// Entry deallocation function.
    void (* delete_entry)(he4_entry_t thing);

    size_t capacity;        ///< Capacity of the table.
    size_t free;            ///< Number of free cells.
    size_t max_touch;       ///< Maximum touch index.
    he4_map_t * maps;       ///< The hash table.
} HE4;

//======================================================================
// Table constructor.
//======================================================================

#ifndef HE4_MINIMUM_SIZE
/**
 * Minimum allowed size of the table.
 */
#define HE4_MINIMUM_SIZE 64
#endif

/**
 * Determine the maximum size of hash table that can fit in the provided number
 * of bytes.
 *
 * @param bytes         A number of bytes.
 * @return              The maximum number of entries that can fit in that
 *                      space.  This includes all hash table overhead.
 */
size_t he4_best_capacity(size_t bytes);

/**
 * Allocate and return a new hash table that has enough space for the provided
 * number of entries.  Remember that (1) the hash table is of fixed size, and
 * (2) the open addressing scheme will perform badly when the load factor is
 * above 0.7.
 *
 * To use this you (optionally) provide functions.
 *
 *   * The `hash` function is given a key, and it must return the hash of the
 *     key.  If this is `NULL`, then the default hash function is used.
 *   * The `compare` function is given pointers to two keys and must compare
 *     them.  All that is really required is that unequal keys return non-zero
 *     and equal keys return zero.  If it is `NULL`, then `memcmp` is used.
 *   * The `delete_key` function is given a pointer to a key and will
 *     deallocate it.  This is used when a key is discarded, overwritten, or
 *     when the entire table is deallocated.  If it is `NULL` then the default
 *     deallocator is used.
 *   * The `delete_entry` function is given a pointer to an entry and will
 *     deallocate it.  This is used when an entry is discarded, overwritten, or
 *     when the entire table is deallocated.  If it is `NULL` then the default
 *     deallocator is used.
 *
 * If the number of entries is less than `HE4_MINIMUM_SIZE`, creation will fail.
 * If memory cannot be allocated, creation will fail.
 *
 * @param entries       The maximum number of entries allowed in the table.
 * @param hash          A function to hash a key.
 * @param compare       The function to compare two keys.
 * @param delete_key    Function to deallocate a discarded key.
 * @param delete_entry  Function to deallocate a discarded entry.
 * @return              The newly allocated table, or NULL if creation fails.
 */
HE4 * he4_new(size_t entries,
              he4_hash_t (* hash)(he4_key_t key, size_t klen),
              int (* compare)(he4_key_t key1, size_t klen1, he4_key_t key2,
                              size_t klen2),
              void (* delete_key)(he4_key_t key),
              void (* delete_entry)(he4_entry_t thing));

/**
 * Delete the HE4 table, deallocating all entries.  Do not simply free the
 * table pointer, or you will have a serious memory leak!
 *
 * @param table         The table to deallocate.
 */
void he4_delete(HE4 * table);

//======================================================================
// Table data.
//======================================================================

/**
 * Determine the number of entries in the table.
 *
 * If the table is `NULL`, then zero is returned.
 *
 * @param table         The table.
 * @return              The number of entries.
 */
size_t he4_size(HE4 * table);

/**
 * Determine the capacity of a table.
 *
 * If the table is `NULL`, then zero is returned.
 *
 * @param table         The table.
 * @return              The capacity.
 */
size_t he4_capacity(HE4 * table);

/**
 * Determine the load factor of the table.  The load factor is determined
 * as follows.
 *
 * @code{c}
 * (double)he4_size(table) / (double)he4_capacity(table)
 * @endcode
 *
 * If the table is `NULL`, then 1.0 is returned.
 *
 * @param table         The table.
 * @return              The load factor.
 */
double he4_load(HE4 * table);

#ifndef HE4NOTOUCH
/**
 * Get the highest touch index of an item in the table.
 *
 * Every time an item is successfully found - or when an item is inserted -
 * the maximum touch index is incremented and then stored with the item.
 * This can be used to clear out the least-recently-used items (see
 * `he4_trim_and_rehash`).
 *
 * @param table         The table.
 * @return              The maximum touch index.
 */
size_t he4_max_touch(HE4 * table);
#endif // HE4NOTOUCH

//======================================================================
// Table insertion / deletion functions.
//======================================================================

/**
 * Insert the given entry into the hash table.  If the table is full then the
 * item is /not/ inserted, and is in fact discarded.
 *
 * If either the table, key, or entry is equal to `NULL`, or if the key length
 * is 0, then nothing is done and `true` is returned.
 *
 * @param table         The hash table to get the new entry.
 * @param key           The key.
 * @param klen          Length in bytes of key.
 * @param entry         The entry to insert.
 * @return              False if the entry was successfully inserted, and true
 *                      if not.  This mirrors the usual C error return value.
 */
bool he4_insert(HE4 * table, const he4_key_t key, const size_t klen,
                const he4_entry_t entry);

/**
 * Insert the given entry into the hash table.  If the table is full then the
 * first entry with the matching hash code is simply overwritten.
 *
 * If either the table, key, or entry is equal to `NULL`, or if the key length
 * is 0, then nothing is done and `true` is returned.
 *
 * @param table         The hash table to get the new entry.
 * @param key           The key.
 * @param klen          Length in bytes of key.
 * @param entry         The entry to insert.
 * @return              False if the entry was successfully inserted without
 *                      overwriting another entry, and true if another entry was
 *                      overwritten.
 */
bool he4_force_insert(HE4 * table, const he4_key_t key, const size_t klen,
                      const he4_entry_t entry);

/**
 * Delete an entry if it can be found.  If the entry is found in the table
 * then it is removed and the entry itself is returned.
 *
 * If either the table or the key is equal to `NULL`, then nothing is done and
 * `NULL` is returned.
 *
 * @param table         The hash table to search for the entry.
 * @param key           The key to delete.
 * @param klen          Length in bytes of key.
 * @return              The entry for the given key, if found.  `NULL` if not.
 *                      In this case the caller is responsible for freeing
 *                      the entry.
 */
he4_entry_t he4_remove(HE4 * table, const he4_key_t key, const size_t klen);

/**
 * Delete an entry if it can be found.  If the entry is found in the table
 * then it is removed and the entry itself is freed if a proper function was
 * provided at table construction.
 *
 * If either the table or the key is equal to `NULL`, then nothing is done and
 * `true` is returned.
 *
 * @param table         The hash table to search for the entry.
 * @param key           The key to delete.
 * @param klen          Length in bytes of key.
 * @return              False if the entry was successfully removed, and true
 *                      if not.  This mirrors the usual C error return value.
 */
bool he4_discard(HE4 * table, const he4_key_t key, const size_t klen);

/**
 * Find and return an entry with the specified key.
 *
 * If either the table or the key is equal to `NULL`, then nothing is done and
 * `NULL` is returned.
 *
 * @param table         The hash table to search for the entry.
 * @param key           The key to locate.
 * @param klen          Length in bytes of key.
 * @return              The entry, or `NULL` if it was not found.
 */
he4_entry_t he4_get(HE4 * table, const he4_key_t key, const size_t klen);

//======================================================================
// Random access.
//======================================================================

/**
 * Find and return a pointer to the entry with the specified key.  This is
 * distinct from `he4_get` in that it allows the entry to be manipulated in
 * the table directly.
 *
 * If either the table or the key is equal to `NULL`, then nothing is done and
 * `NULL` is returned.
 *
 * @param table         The hash table to search for the entry.
 * @param key           The key to locate.
 * @param klen          Length in bytes of key.
 * @return              The entry, or `NULL` if it was not found.
 */
he4_entry_t * he4_find(HE4 * table, const he4_key_t key, const size_t klen);

//======================================================================
// Direct access.
//======================================================================

/**
 * Return a mapping from the table by index.  This is primarily used to inspect
 * the content of a hash table for debugging.
 *
 * @code{c}
 * for (size_t index = 0; index < table->capacity; ++index) {
 *     he4_map_t * map = he4_index(table, index);
 *     // Do something with the map...
 *     HE4FREE(map);
 * }
 * @endcode
 *
 * Caution: Do not deallocate the key or entry.  When you are done with the
 * returned mapping, just free it using HE4FREE.
 *
 * @param table         The hash table.
 * @param index         The zero-based index.
 * @return              The mapping, or `NULL` if outside the table range.
 */
he4_map_t * he4_index(HE4 * table, const size_t index);

//======================================================================
// Rehash.
//======================================================================

/**
 * Rehash the table to one with the provided size.  This frees the original
 * table, so do not use it!
 *
 * If the provided size is not larger than the original size then the original
 * table is returned.  If the provided size is zero, then the new size will be
 * double the original size.
 *
 * @param table         The original table.
 * @param newsize       The new table size.
 * @return              The new table, or `NULL` if it cannot be created.
 */
HE4 * he4_rehash(HE4 * table, const size_t newsize);

#ifndef HE4NOTOUCH
/**
 * Rehash the table to one with the provided size.  This frees the original
 * table, so do not use it!  This also trims old entries that have a touch
 * index lower than the provided value.
 *
 * If the new size is smaller than the original size, then the original table
 * is returned with no changes.
 *
 * @param table         The original table.
 * @param newsize       The new table size.
 * @param trim_below    Discard and free any entries with an touch index
 *                      lower than this value.
 * @return              The new table, or `NULL` if it cannot be created.
 */
HE4 * he4_trim_and_rehash(HE4 * table, const size_t newsize,
                          const size_t trim_below);
#endif

// TODO Write an iterator.

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif //HE4_H
