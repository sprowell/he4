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

#define HE4_KEY_TYPE size_t
#define HE4_ENTRY_TYPE size_t

#include "test-frame.h"
#include <he4.h>

he4_hash_t hash(he4_key_t key, size_t klen) {
    return (he4_hash_t)key;
}
int compare(he4_key_t key1, size_t klen1, he4_key_t key2, size_t klen2) {
    return (key1 == key2 ? 0 : 1);
}
void delete_key(he4_key_t key) {}
void delete_entry(he4_entry_t entry) {}

START_TEST

    he4_debug = 1;
    // Create a table.
    HE4 * table = he4_new(1024, hash, compare, delete_key, delete_entry);

START_ITEM(basics)

    // Check some basics.
    ASSERT(table != NULL); IF_FAIL_STOP;
    ASSERT(he4_capacity(table) == 1024); IF_FAIL_STOP;
    ASSERT(he4_size(table) == 0); IF_FAIL_STOP;
    ASSERT(he4_load(table) == 0.0); IF_FAIL_STOP;

END_ITEM
START_ITEM(fill)

    // Insert number pairs to fill the table.  We cannot use zero as key
    // because it is equivalent to NULL.
    for (size_t index = 1; index <= 1024; ++index) {
        if (he4_insert(table, index, sizeof(he4_key_t), index+7)) {
            FAIL_TEST("insertion at index: %zu", index);
        }
        ASSERT(he4_capacity(table) == 1024); IF_FAIL_STOP;
        ASSERT(he4_size(table) == index); IF_FAIL_STOP;
    } // Fill the table.
    ASSERT(he4_load(table) == 1.0); IF_FAIL_STOP;

END_ITEM
START_ITEM(verify_full)

    // Verify that every entry we created is in the table.
    for (size_t index = 1; index <= 1024; ++index) {
        he4_entry_t * pentry = he4_find(table, index, sizeof(size_t));
        if (pentry == NULL) {
            FAIL_TEST("missing key at index: %zu", index);
        }
        ASSERT(*pentry == index + 7);
        he4_entry_t entry = he4_get(table, index, sizeof(size_t));
        ASSERT(entry == index + 7);
    } // Check for every entry.

END_ITEM
START_ITEM(insert)

    // Insert some items.  The table is full, so these should fail.
    for (size_t index = 2048; index < 4096; ++index) {
        if (!he4_insert(table, index, sizeof(he4_key_t), index+7)) {
            FAIL_ITEM("inserted at index: %zu", index);
        }
    } // Test over-filling.
    // Verify that none of the bad items made it in.
    for (size_t index = 0; index < he4_size(table); ++index) {
        if (he4_index(table, index)->entry > 2047) {
            FAIL_ITEM("found bad item with index: %zu", index);
        }
    } // Check for bad items.
    ASSERT(he4_capacity(table) == 1024); IF_FAIL_STOP;
    ASSERT(he4_size(table) == 1024); IF_FAIL_STOP;
    ASSERT(he4_load(table) == 1.0); IF_FAIL_STOP;

END_ITEM
START_ITEM(force)

    // Force insert some items.  The table is full, so these will overwrite.
    for (size_t index = 2048; index < 3072; ++index) {
        if (!he4_force_insert(table, index, sizeof(he4_key_t), index+7)) {
            FAIL_ITEM("insertion at index: %zu", index);
        }
        he4_entry_t entry = he4_get(table, index, sizeof(size_t));
        if (entry != index+7) {
            FAIL_ITEM("missing forced entry at index: %zu", index);
        }
    } // Test over-filling.
    ASSERT(he4_capacity(table) == 1024); IF_FAIL_STOP;
    ASSERT(he4_size(table) == 1024); IF_FAIL_STOP;
    ASSERT(he4_load(table) == 1.0); IF_FAIL_STOP;

END_ITEM
START_ITEM(cleanup)

    // Empty the table.
    size_t size = he4_size(table);
    for (size_t index = 0; index < he4_capacity(table); ++index) {
        he4_map_t * pentry = he4_index(table, index);
        ASSERT(pentry != NULL); IF_FAIL_STOP;
        size_t entry = pentry->entry;
        he4_entry_t rval = he4_remove(table, pentry->key, pentry->klen);
        if (rval != entry) {
            FAIL_ITEM("removed entry is incorrect at index: %zu (%zu != %zu)",
                      index, rval, entry);
        }
        --size;
        ASSERT(he4_size(table) == size);
    } // Check for bad items.
    ASSERT(he4_capacity(table) == 1024); IF_FAIL_STOP;
    ASSERT(he4_size(table) == 0); IF_FAIL_STOP;
    ASSERT(he4_load(table) == 0.0); IF_FAIL_STOP;

END_ITEM

    // Done.
    he4_delete(table);

END_TEST
