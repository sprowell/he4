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

#include <stdlib.h>
#include <string.h>
#include <he4.h>
#include "xxhash.h"

/*
 * How the data structure works.
 *
 * The table is stored in three arrays.  There is a key array, a key length
 * array, and an entry array: (key, key length) -> entry.
 *
 * Lazy deletion is supported by freeing both the key and entry and replacing
 * them with NULL, but setting the key length to one (or any non-zero value).
 *
 * klen == 0 --> an empty cell.
 * klen > 0 && key == NULL --> a lazy empty cell.
 * klen > 0 && key != NULL --> a non-empty cell.
 */

/**
 * The debugging level.  Right now there are two levels: 0 (suppress) and 1
 * (emit debugging information).
 */
int he4_debug = 0;

//======================================================================
// Default functions.
//======================================================================

/**
 * This is the default hash function.
 *
 * @param key           Pointer to the key to hash.
 * @param length        Number of bytes in key.
 * @return              The hash value.
 */
static uint32_t
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
    size_t map_size = sizeof(he4_key_t) + sizeof(he4_entry_t) + sizeof(size_t);
    size_t maximum = bytes / (map_size * sizeof(char));
    return maximum;
}

HE4 *
he4_new(size_t entries,
        uint32_t (* hash)(he4_key_t key, size_t klen),
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

    // Allocate the key and entry arrays.
    table->keys = HE4MALLOC(he4_key_t, entries);
    if (table->keys == NULL) {
        DEBUG("Unable to get memory for key table.");
        HE4FREE(table);
        return NULL;
    }
    table->klen = HE4MALLOC(size_t, entries);
    if (table->klen == NULL) {
        DEBUG("Unable to get memory for key length array.");
        HE4FREE(table->keys);
        table->keys = NULL;
        HE4FREE(table);
        return NULL;
    }
    table->entries = HE4MALLOC(he4_entry_t, entries);
    if (table->entries == NULL) {
        DEBUG("Unable to get memory for entry table.");
        HE4FREE(table->klen);
        table->klen = NULL;
        HE4FREE(table->keys);
        table->keys = NULL;
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
        table->klen[index] = 0;
        if (table->keys[index] != NULL) table->delete_key(table->keys[index]);
        if (table->entries[index] != NULL) table->delete_entry(table->entries[index]);
    } // Delete any remaining entries.

    // Wipe the method table.
    table->compare = NULL;
    table->hash = NULL;
    table->delete_key = NULL;
    table->delete_entry = NULL;

    // Delete the internal arrays.
    HE4FREE(table->keys);
    table->keys = NULL;
    HE4FREE(table->entries);
    table->entries = NULL;
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
    size_t start = table->hash(key, klen) % table->capacity;
    size_t index = start;

    // Insert at first open (lazy or not) slot, or overwrite a match.
    do {
        if (table->keys[index] == NULL) {
            // Found the place to insert.
            table->keys[index] = key;
            table->klen[index] = klen;
            table->entries[index] = entry;
            --(table->free);
            return false;
        }
        if (table->compare(table->keys[index], table->klen[index], key, klen) == 0) {
            // Found the key.  Replace the entry.
            table->delete_entry(table->entries[index]);
            table->entries[index] = entry;
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
    size_t start = table->hash(key, klen) % table->capacity;
    size_t index = start;

    // Insert at first open (lazy or not) slot, or overwrite a match.
    do {
        if (table->keys[index] == NULL) {
            // Found the place to insert.
            table->keys[index] = key;
            table->klen[index] = klen;
            table->entries[index] = entry;
            --(table->free);
            return false;
        }
        if (table->compare(table->keys[index], table->klen[index], key, klen) == 0) {
            // Found the key.  Replace the entry.
            table->delete_entry(table->entries[index]);
            table->entries[index] = entry;
            return false;
        }
        index = (index + 1) % table->capacity;
    } while (start != index);

    // We did not find an open slot, and we did not find a match.  Force
    // insertion at the current position (which is the first position for the
    // given key).
    table->delete_key(table->keys[index]);
    table->keys[index] = key;
    table->klen[index] = klen;
    table->delete_entry(table->entries[index]);
    table->entries[index] = entry;
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
    size_t start = table->hash(key, klen) % table->capacity;
    size_t index = start;

    // Find the corresponding entry.  Stop if we wrap around, which can happen
    // with a full table.
    do {
        // If we hit an (non-lazy) empty cell, we can stop.
        if (table->klen[index] == 0) return NULL;

        // Check the current slot.
        if (table->compare(key, klen, table->keys[index], table->klen[index]) == 0) {
            // Found the entry.  Remove it.
            table->delete_key(table->keys[index]);
            table->keys[index] = NULL;
            he4_entry_t entry = table->entries[index];
            table->entries[index] = NULL;
            table->klen[index] = 1;
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
    size_t start = table->hash(key, klen) % table->capacity;
    size_t index = start;

    // Find the corresponding entry.  Stop if we wrap around, which can happen
    // with a full table.
    do {
        // If we find a non-lazy empty cell, we can stop.
        if (table->klen[index] == 0) return true;

        // Check the current slot.
        if (table->compare(key, klen, table->keys[index], table->klen[index]) == 0) {
            // Found the entry.  Remove it, and mark the cell as lazy deleted.
            table->delete_key(table->keys[index]);
            table->keys[index] = NULL;
            table->delete_entry(table->entries[index]);
            table->entries[index] = NULL;
            table->klen[index] = 1;
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
    size_t start = table->hash(key, klen) % table->capacity;
    size_t index = start;

    // Find the corresponding entry.  Stop if we wrap around, which can happen
    // with a full table.
    bool lazy = false;
    size_t lazy_index = 0;
    do {
        // If we find a non-lazy empty slot, stop.
        if (table->klen[index] == 0) return NULL;

        // Keep track of the first lazy-deleted slot that is open.
        if (!lazy && table->keys[index] == NULL) {
            // This is a lazy-deleted cell.
            lazy = true;
            lazy_index = index;
        }

        // Check the current slot.
        if (table->compare(key, klen, table->keys[index], table->klen[index]) == 0) {
            // Found the entry.  If we have a lazy-deleted index, move it there.
            he4_entry_t entry = table->entries[index];
            if (lazy) {
                table->keys[lazy_index] = table->keys[index];
                table->entries[lazy_index] = table->entries[index];
                table->klen[lazy_index] = table->klen[index];
                table->keys[index] = NULL;
                table->entries[index] = NULL;
                table->klen[index] = 0;
            }
            return entry;
        }

        // Move to the next slot.
        index = (index + 1) % table->capacity;
    } while (start != index); // Find and remove the entry.

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
    map->key = table->keys[index];
    map->klen = table->klen[index];
    map->entry = table->entries[index];
    return map;
}
