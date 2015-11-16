/**
 * @file
 * Public API implementation for the He4 library.
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
 */

#include <string.h>
#include <he4.h>
#include "xxhash.h"

// Make sure the version is defined.  If not, then given an error.
#ifndef HE4_VERSION
#error HE4_VERSION must be defined.
#endif

/*
 * How the data structure works.
 *
 * The table is stored in three arrays.  There is a key array, a key length
 * array, a hash array, and an entry array: (key, key length, hash) -> entry.
 *
 * Lazy deletion is supported by freeing both the key and entry and replacing
 * them with NULL, but setting the key length to one (or any non-zero value).
 *
 * klen == 0 --> an empty cell.
 * klen > 0 && key == NULL --> a deleted cell.
 * klen > 0 && key != NULL --> a non-empty cell.
 */

/**
 * The debugging level.  Right now there are two levels: 0 (suppress) and 1
 * (emit debugging information).
 */
int he4_debug = 0;

char *
he4_version() {
    return HE4_VERSION;
}

//======================================================================
// Local functions.
//======================================================================

/**
 * Determine if a cell is empty.  An empty cell is one that has never held
 * a value.  A deleted cell is not considered empty.
 *
 * @param table         The table.
 * @param index         The index of the cell.
 * @return              True if the cell is empty, and false otherwise.
 */
static inline bool
is_empty(HE4 * table, size_t index) {
    return table->maps[index].klen == 0;
}

/**
 * Determine if a cell is deleted ("lazy empty").  An deleted cell is one that
 * previously held a value.  A deleted cell is not considered empty.
 *
 * @param table         The table.
 * @param index         The index of the cell.
 * @return              True if the cell is deleted, and false otherwise.
 */
static inline bool
is_deleted(HE4 * table, size_t index) {
    return table->maps[index].klen > 0 && table->maps[index].key == NULL;
}

/**
 * Determine if a cell is open.  An open cell is one that can have a value
 * placed in it.  Both empty and deleted cells are considered open, so this
 * is equivalent to the check
 * `is_empty(table, index) || is_deleted(table, index)`.
 *
 * @param table         The table.
 * @param index         The index of the cell.
 * @return              True if the cell is open, and false otherwise.
 */
static inline bool
is_open(HE4 * table, size_t index) {
    return table->maps[index].key == NULL;
}

//======================================================================
// Default functions.
//======================================================================

/**
 * This is the default hash function.  You cannot use this function if you
 * have re-defined the hash type.
 *
 * @param key           Pointer to the key to hash.
 * @param length        Number of bytes in key.
 * @return              The hash value.
 */
static he4_hash_t
he4_hash(const he4_key_t key, const size_t length) {
    return XXH32(key, length, 0);
}

/**
 * This is the default key comparison.
 *
 * @param key1          First key.
 * @param klen1         First key length.
 * @param key2          Second key.
 * @param klen2         Second key length.
 * @return              Non-zero if unequal, zero if equal.
 */
static int
he4_compare(he4_key_t key1, size_t klen1, he4_key_t key2, size_t klen2) {
    return klen1 != klen2 ? 1 : memcmp(key1, key2, klen1);
}

/**
 * This is the default key deletion.
 *
 * @param key           The key to delete.
 */
static void
he4_delete_key(he4_key_t key) {
    HE4FREE(key);
}

/**
 * This is the default entry deletion.
 *
 * @param entry         The entry to delete.
 */
static void
he4_delete_entry(he4_entry_t entry) {
    HE4FREE(entry);
}

//======================================================================
// Table constructor.
//======================================================================

size_t
he4_best_capacity(size_t bytes) {
    // Subtract the size of the structure.
    bytes -= sizeof(HE4) * sizeof(char);
    // Every map must include (1) a key, (2) the key length, and (3) an
    // entry.
    size_t maximum = bytes / (sizeof(he4_map_t) * sizeof(char));
    return maximum;
}

HE4 *
he4_new(size_t entries,
        he4_hash_t (* hash)(he4_key_t key, size_t klen),
        int (* compare)(he4_key_t key1, size_t klen1, he4_key_t key2,
                        size_t klen2),
        void (* delete_key)(he4_key_t key),
        void (* delete_entry)(he4_entry_t thing)) {
    // Check arguments.
    if (entries < HE4_MINIMUM_SIZE) {
        DEBUG("Requested table size (%zu) is less than the minimum (%d).",
              entries, HE4_MINIMUM_SIZE);
        return NULL;
    }
#ifdef HE4_USER_HASH
    if (hash == NULL) {
        DEBUG("User must supply a hash function since a custom hash type is "
              "defined.");
        return NULL;
    }
#endif

    // Allocate the table.
    HE4 * table = HE4MALLOC(HE4, 1);
    if (table == NULL) {
        DEBUG("Unable to get memory for table structure.");
        return NULL;
    }

    // Set the fields.
    table->capacity = entries;
    table->compare = compare == NULL ? he4_compare : compare;
    table->delete_entry = delete_entry == NULL ? he4_delete_entry : delete_entry;
    table->delete_key = delete_key == NULL ? he4_delete_key : delete_key;
    table->free = entries;
    table->hash = hash == NULL ? he4_hash : hash;
#ifndef HE4NOTOUCH
    table->max_touch = 0;
#endif // HE4NOTOUCH

    // Allocate the key and entry arrays.
    table->maps = HE4MALLOC(he4_map_t, entries);
    if (table->maps == NULL) {
        DEBUG("Unable to get memory for the table.");
        HE4FREE(table);
        return NULL;
    }

    // Success.
    return table;
}

void
he4_delete(HE4 * table) {
    if (table == NULL) {
        DEBUG("Attempt to delete a NULL table.");
        return;
    }

    // Delete any remaining entries.
    table->free = 0;
    table->capacity = 0;
    for (size_t index = 0; index < table->capacity; ++index) {
        table->maps[index].klen = 0;
        if (table->maps[index].key != NULL) {
            table->delete_key(table->maps[index].key);
        }
        if (table->maps[index].entry != NULL) {
            table->delete_entry(table->maps[index].entry);
        }
    } // Delete any remaining entries.

    // Wipe the method table.
    table->compare = NULL;
    table->hash = NULL;
    table->delete_key = NULL;
    table->delete_entry = NULL;

    // Delete the internal arrays.
    HE4FREE(table->maps);
    table->maps = NULL;
    HE4FREE(table);
}

//======================================================================
// Table data.
//======================================================================

size_t
he4_size(HE4 * table) {
    if (table == NULL) return 0;
    return table->capacity - table->free;
}

size_t
he4_capacity(HE4 * table) {
    return table == NULL ? 0 : table->capacity;
}

double
he4_load(HE4 * table) {
    if (table == NULL) return 1.0;
    return (double)(table->capacity - table->free) / (double)(table->capacity);
}

#ifndef HE4NOTOUCH
size_t
he4_max_touch(HE4 * table) {
    return table->max_touch;
}
#endif // HE4NOTOUCH

//======================================================================
// Table insertion / deletion functions.
//======================================================================

bool
he4_insert(HE4 * table, const he4_key_t key, const size_t klen,
           const he4_entry_t entry) {
    // Check arguments.
    if (table == NULL) {
        DEBUG("Table is NULL.");
        return NULL;
    }
    if (key == NULL) {
        DEBUG("Key is NULL.");
        return NULL;
    }
    if (klen == 0) {
        DEBUG("Key length is NULL.");
        return NULL;
    }
    if (entry == NULL) {
        DEBUG("Entry is NULL.");
        return NULL;
    }

    // Hash the key, then wrap to table size.
    he4_hash_t hash = table->hash(key, klen);
    size_t start = hash % table->capacity;
    size_t index = start;

    // Insert at first open slot, or overwrite a match.
    do {
        if (is_open(table, index)) {
            // Found the place to insert.
            table->maps[index].key = key;
            table->maps[index].klen = klen;
            table->maps[index].hash = hash;
            table->maps[index].entry = entry;
#ifndef HE4NOTOUCH
            ++(table->max_touch);
            table->maps[index].touch = table->max_touch;
#endif // HE4NOTOUCH
            --(table->free);
            return false;
        }
        if (table->maps[index].hash == hash &&
                table->compare(table->maps[index].key, table->maps[index].klen,
                               key, klen) == 0) {
            // Found the key.  Replace the entry.
            table->delete_entry(table->maps[index].entry);
            table->maps[index].entry = entry;
#ifndef HE4NOTOUCH
            ++(table->max_touch);
            table->maps[index].touch = table->max_touch;
#endif // HE4NOTOUCH
            return false;
        }
        index = (index + 1) % table->capacity;
    } while (start != index);

    // There are no open slots.
    return true;
}

bool
he4_force_insert(HE4 * table, const he4_key_t key, const size_t klen,
           const he4_entry_t entry) {
    // Check arguments.
    if (table == NULL) {
        DEBUG("Table is NULL.");
        return NULL;
    }
    if (key == NULL) {
        DEBUG("Key is NULL.");
        return NULL;
    }
    if (klen == 0) {
        DEBUG("Key length is 0.");
        return NULL;
    }
    if (entry == NULL) {
        DEBUG("Entry is NULL.");
        return NULL;
    }

    // Hash the key, then wrap to table size.
    he4_hash_t hash = table->hash(key, klen);
    size_t start = hash % table->capacity;
    size_t index = start;

    // Insert at first open slot, or overwrite a match.
    do {
        if (is_open(table, index)) {
            // Found the place to insert.
            table->maps[index].key = key;
            table->maps[index].klen = klen;
            table->maps[index].hash = hash;
            table->maps[index].entry = entry;
#ifndef HE4NOTOUCH
            ++(table->max_touch);
            table->maps[index].touch = table->max_touch;
#endif // HE4NOTOUCH
            --(table->free);
            return false;
        }
        if (table->maps[index].hash == hash &&
                table->compare(table->maps[index].key, table->maps[index].klen,
                               key, klen) == 0) {
            // Found the key.  Replace the entry.
            table->delete_entry(table->maps[index].entry);
            table->maps[index].entry = entry;
#ifndef HE4NOTOUCH
            ++(table->max_touch);
            table->maps[index].touch = table->max_touch;
#endif // HE4NOTOUCH
            return false;
        }
        index = (index + 1) % table->capacity;
    } while (start != index);

    // We did not find an open slot, and we did not find a match.  Force
    // insertion at the current position (which is the first position for the
    // given key).
    table->delete_key(table->maps[index].key);
    table->maps[index].key = key;
    table->maps[index].klen = klen;
    table->maps[index].hash = hash;
    table->delete_entry(table->maps[index].entry);
    table->maps[index].entry = entry;
#ifndef HE4NOTOUCH
    ++(table->max_touch);
    table->maps[index].touch = table->max_touch;
#endif // HE4NOTOUCH
    return true;
}

he4_entry_t
he4_remove(HE4 * table, const he4_key_t key, const size_t klen) {
    // Check arguments.
    if (table == NULL) {
        DEBUG("Table is NULL.");
        return NULL;
    }
    if (key == NULL) {
        DEBUG("Key is NULL.");
        return NULL;
    }
    if (klen == 0) {
        DEBUG("Key length is 0.");
        return NULL;
    }

    // Hash the key, then wrap to table size.
    he4_hash_t hash = table->hash(key, klen);
    size_t start = hash % table->capacity;
    size_t index = start;

    // Find the corresponding entry.  Stop if we wrap around, which can happen
    // with a full table.
    do {
        // If we hit an empty cell, we can stop.
        if (is_empty(table, index)) return NULL;

        // Skip deleted cells.
        if (is_deleted(table, index)) continue;

        // Check the current slot.
        if (table->maps[index].hash == hash &&
                table->compare(key, klen, table->maps[index].key,
                               table->maps[index].klen) == 0) {
            // Found the entry.  Remove it.
            table->delete_key(table->maps[index].key);
            table->maps[index].key = NULL;
            he4_entry_t entry = table->maps[index].entry;
            table->maps[index].entry = NULL;
            table->maps[index].klen = 1;
            table->maps[index].hash = 0;
            ++(table->free);
            return entry;
        }

        // Move to the next slot.
        index = (index + 1) % table->capacity;
    } while (start != index); // Find and remove the entry.

    // Not found.
    return NULL;
}

bool
he4_discard(HE4 * table, const he4_key_t key, const size_t klen) {
    // Check arguments.
    if (table == NULL) {
        DEBUG("Table is NULL.");
        return NULL;
    }
    if (key == NULL) {
        DEBUG("Key is NULL.");
        return NULL;
    }
    if (klen == 0) {
        DEBUG("Key length is 0.");
        return NULL;
    }

    // Hash the key, then wrap to table size.
    he4_hash_t hash = table->hash(key, klen);
    size_t start = hash % table->capacity;
    size_t index = start;

    // Find the corresponding entry.  Stop if we wrap around, which can happen
    // with a full table.
    do {
        // If we find an empty cell, we can stop.
        if (is_empty(table, index)) return true;

        // Skip deleted cells.
        if (is_deleted(table, index)) continue;

        // Check the current slot.
        if (table->maps[index].hash == hash &&
                table->compare(key, klen, table->maps[index].key, table->maps[index].klen) == 0) {
            // Found the entry.  Remove it, and mark the cell as deleted.
            table->delete_key(table->maps[index].key);
            table->maps[index].key = NULL;
            table->delete_entry(table->maps[index].entry);
            table->maps[index].entry = NULL;
            table->maps[index].klen = 1;
            table->maps[index].hash = 0;
            ++(table->free);
            return false;
        }

        // Move to the next slot.
        index = (index + 1) % table->capacity;
    } while (start != index); // Find and remove the entry.

    // Not found.
    return true;
}

he4_entry_t
he4_get(HE4 * table, const he4_key_t key, const size_t klen) {
    // Check arguments.
    if (table == NULL) {
        DEBUG("Table is NULL.");
        return NULL;
    }
    if (key == NULL) {
        DEBUG("Key is NULL.");
        return NULL;
    }
    if (klen == 0) {
        DEBUG("Key length is 0.");
        return NULL;
    }

    // Hash the key, then wrap to table size.
    he4_hash_t hash = table->hash(key, klen);
    size_t start = hash % table->capacity;
    size_t index = start;

    // Find the corresponding entry.  Stop if we wrap around, which can happen
    // with a full table.
    bool lazy = false;
    size_t lazy_index = 0;
    do {
        // If we find an empty slot, stop.
        if (is_empty(table, index)) return NULL;

        // Keep track of the first deleted slot that is open.
        if (!lazy && is_deleted(table, index)) {
            // This is a lazy-deleted cell.
            lazy = true;
            lazy_index = index;
        }

        // Check the current slot.
        if (table->maps[index].hash == hash &&
                table->compare(key, klen, table->maps[index].key, table->maps[index].klen) == 0) {
            // Found the entry.  If we have a lazy-deleted index, move it there.
            he4_entry_t entry = table->maps[index].entry;
            if (lazy) {
                memcpy(&(table->maps[lazy_index]), &(table->maps[index]),
                       sizeof(he4_map_t));
                table->maps[index].key = NULL;
                table->maps[index].entry = NULL;
                table->maps[index].klen = 1;
#ifndef HE4NOTOUCH
                ++(table->max_touch);
                table->maps[lazy_index].touch = table->max_touch;
            } else {
                ++(table->max_touch);
                table->maps[index].touch = table->max_touch;
#endif // HE4NOTOUCH
            }
            return entry;
        }

        // Move to the next slot.
        index = (index + 1) % table->capacity;
    } while (start != index); // Find the entry.

    // Not found.
    return NULL;
}

//======================================================================
// Random access.
//======================================================================

he4_map_t *
he4_index(HE4 * table, const size_t index) {
    if (table == NULL || index >= table->capacity) return NULL;
    he4_map_t * map = HE4MALLOC(he4_map_t, 1);
    if (map == NULL) return NULL;
    memcpy(map, &(table->maps[index]), sizeof(he4_map_t));
    return map;
}

//======================================================================
// Rehash.
//======================================================================

HE4 *
he4_rehash(HE4 * table, const size_t newsize) {
    if (table == NULL) {
        // We could treat this as an empty table, but this is almost certainly
        // an error.
        DEBUG("Attempt to rehash a NULL table.");
        return NULL;
    }
    size_t capacity = newsize == 0 ? table->capacity * 2 : newsize;
    if (capacity <= table->capacity) {
        DEBUG("New table capacity is too small; nothing done.");
        return table;
    }

    // Make the new table.
    HE4 * newtable = he4_new(capacity, table->hash, table->compare,
                             table->delete_key, table->delete_entry);
    if (newtable == NULL) {
        DEBUG("Unable to get memory for rehashed table.");
        return NULL;
    }

    // Move everything to the rehashed table.
    for (size_t index = 0; index < table->capacity; ++index) {
        he4_insert(newtable, table->maps[index].key, table->maps[index].klen,
                   table->maps[index].entry);
        table->maps[index].key = NULL;
        table->maps[index].entry = NULL;
        table->maps[index].klen = 0;
    } // Rehash the table.

    // Free the original table.
    he4_delete(table);
    return newtable;
}

#ifndef HE4NOTOUCH
HE4 *
he4_trim_and_rehash(HE4 * table, const size_t newsize, const size_t trim_below) {
    if (table == NULL) {
        // We could treat this as an empty table, but this is almost certainly
        // an error.
        DEBUG("Attempt to rehash a NULL table.");
        return NULL;
    }
    size_t capacity = newsize == 0 ? table->capacity * 2 : newsize;
    if (capacity < table->capacity) {
        DEBUG("New table capacity is too small; keeping old size.");
        capacity = table->capacity;
    }

    // Make the new table.
    HE4 * newtable = he4_new(capacity, table->hash, table->compare,
                             table->delete_key, table->delete_entry);
    if (newtable == NULL) {
        DEBUG("Unable to get memory for rehashed table.");
        return NULL;
    }

    // Move everything to the rehashed table.
    for (size_t index = 0; index < table->capacity; ++index) {
        if (table->maps[index].touch < trim_below) continue;
        he4_insert(newtable, table->maps[index].key, table->maps[index].klen,
                   table->maps[index].entry);
        table->maps[index].key = NULL;
        table->maps[index].entry = NULL;
        table->maps[index].klen = 0;
    } // Rehash the table.

    // Free the original table.
    he4_delete(table);
    return newtable;
}
#endif // HE4NOTOUCH
