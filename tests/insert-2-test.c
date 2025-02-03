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

#define HE4_KEY_TYPE char *
#define HE4_ENTRY_TYPE char *

#include "test-frame.h"
#include <string.h>
#include <he4.h>

#ifndef strdup
char* strdup(const char*);
#endif

// Maximum word length is 20 characters.
#define MAX_WORD_LENGTH 20

inline size_t strlen_n(const char * pstr) {
    size_t len = strlen(pstr);
    if (len > MAX_WORD_LENGTH) len = MAX_WORD_LENGTH;
    return len;
}

char * words[] = {
        // Each row is ten words.
        "the", "of", "and", "a", "to", "in", "is", "you", "that", "it",
        "he", "was", "for", "on", "are", "as", "with", "his", "they", "I",
        "at", "be", "this", "have", "from", "or", "one", "had", "by", "word",
        "but", "not", "what", "all", "were", "we", "when", "your", "can", "said",
        "there", "use", "an", "each", "which", "she", "do", "how", "their", "if",
        "will", "up", "about", "out", "many", "then", "them", "these", "so", "some",
        "her", "would", "make", "like", "him", "into", "time", "has", "look", "two",
        "more", "write", "go", "see", "number", "no", "way", "could", "people", "my",
        "than", "first", "water", "been", "call", "who", "oil", "its", "now", "find",
        "long", "down", "day", "did", "get", "come", "made", "may", "part", "dog"
};
size_t num_words = sizeof words / sizeof *words;

char * num_to_word(size_t num) {
    char * retval = (char *) calloc(1,1);
    while (num > 0) {
        size_t next = num % num_words;
        num = num / num_words;
        size_t newlen = strlen_n(retval) + strlen_n(words[next]) + 2;
        char * newretval = (char *)malloc(sizeof(char) * newlen);
        strcpy(newretval, retval);
        strcat(newretval, " ");
        strcat(newretval, words[next]);
        free(retval);
        retval = newretval;
    } // Build the string.
    return retval;
}

START_TEST

    he4_debug = 1;
    // Create a table.
    HE4 * table = he4_new(1024, NULL, NULL, NULL, NULL);

START_ITEM(basics)

    // Check some basics.
    ASSERT(table != NULL); IF_FAIL_STOP;
    ASSERT(he4_capacity(table) == 1024); IF_FAIL_STOP;
    ASSERT(he4_size(table) == 0); IF_FAIL_STOP;
    ASSERT(he4_load(table) == 0.0); IF_FAIL_STOP;

END_ITEM
START_ITEM(fill)

    // Insert data to fill the table.  The data is generated by combining
    // words from the list.
    for (size_t index = 1; index <= 1024; ++index) {
        // These go in the table, so we do not deallocate them.
        he4_key_t key = (he4_key_t) num_to_word(index);
        he4_entry_t entry = (he4_entry_t) num_to_word(index + 7);
        if (he4_insert(table, key, strlen_n(key), entry)) {
            FAIL_TEST("insertion at key: %s", key);
        }
        ASSERT(he4_capacity(table) == 1024); IF_FAIL_STOP;
        ASSERT(he4_size(table) == index); IF_FAIL_STOP;
    } // Fill the table.
    ASSERT(he4_load(table) == 1.0); IF_FAIL_STOP;

END_ITEM
START_ITEM(verify_full)

    // Verify that every entry we created is in the table.
    for (size_t index = 1; index <= 1024; ++index) {
        // These do not go in the table, so they must be deallocated.
        he4_key_t key = num_to_word(index);
        he4_entry_t entry = num_to_word(index +7);
        // This is a pointer to the entry in the table, so it must not be
        // deallocated.
        he4_entry_t * pentry = he4_find(table, key, strlen_n(key));
        if (pentry == NULL) {
            FAIL("missing entry for key: %s", key);
            free(key);
            free(entry);
	        IF_FAIL_END_ITEM;
        }
        ASSERT(strcmp(*pentry, entry) == 0);
        he4_entry_t entry2 = he4_get(table, key, strlen_n(key));
        ASSERT(strcmp(entry, entry2) == 0);
        free(key);
        free(entry);
    } // Check for every entry.

END_ITEM
START_ITEM(insert)

    // Insert some items.  The table is full, so these should fail.
    for (size_t index = 2048; index < 4096; ++index) {
        // These do not go in the table, so they must be deallocated.
        he4_key_t key = num_to_word(index);
        he4_entry_t entry = num_to_word(index +7);
        if (!he4_insert(table, key, strlen_n(key), entry)) {
            // Do not deallocate, since this made it into the table.
            FAIL("inserted at index: %zu", index);
            free(key);
            free(entry);
	        IF_FAIL_END_ITEM;
        }
        free(key);
        free(entry);
    } // Test over-filling.
    ASSERT(he4_capacity(table) == 1024); IF_FAIL_STOP;
    ASSERT(he4_size(table) == 1024); IF_FAIL_STOP;
    ASSERT(he4_load(table) == 1.0); IF_FAIL_STOP;

END_ITEM
START_ITEM(force)

    // Force insert some items.  The table is full, so these will overwrite.
    for (size_t index = 2048; index < 3072; ++index) {
        // These go in the table, so we do not deallocate them.
        he4_key_t key = (he4_key_t) num_to_word(index);
        he4_entry_t entry = (he4_entry_t) num_to_word(index + 7);
        if (!he4_force_insert(table, key, strlen_n(key), entry)) {
            // Since these did not go in the table, deallocate them.
            FAIL("insertion at index: %zu", index);
            free(key);
            free(entry);
	        IF_FAIL_END_ITEM;
        }
        he4_entry_t entry2 = he4_get(table, key, strlen_n(key));
        if (strcmp(entry2, entry) != 0) {
            FAIL_ITEM("missing forced entry for key: %s", key);
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
        // We have to duplicate the entry, in case remove fails and deallocates
        // it.  We must deallocate both later.
        he4_entry_t entry = strdup(pentry->entry);
        if (entry == NULL) {
            FAIL_ITEM("unable to get memory for a string");
        }
        he4_entry_t rval = he4_remove(table, pentry->key, pentry->klen);
        if (strcmp(entry, rval) != 0) {
            FAIL("removed entry is incorrect: %s != %s", rval, entry);
            free(entry);
            free(rval);
            IF_FAIL_END_ITEM;
        }
        free(entry);
        free(rval);
        free(pentry);
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
