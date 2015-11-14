/**
 * @file
 * Basic example of using the He4 library.
 *
 * @private
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
#define HE4_ENTRY_TYPE int
#include <string.h>
#include <stdio.h>
#include <he4.h>

/**
 * The function to "deallocate" an entry.  Entries are simple integers, so we
 * never actually deallocate them.
 *
 * @param entry         The entry to deallocate.
 */
static void
free_entry(he4_entry_t entry) { /* Nothing to do. */ }

/**
 * Entry point from the command line.
 *
 * @param argc          Argument count.
 * @param argv          Arguments.
 * @return              Exit value.
 */
int
main(int argc, char * argv[]) {
    if (argc < 1) {
        fprintf(stdout, "Usage: %s [filename]\n", argv[0]);
        fprintf(stdout, "Read lines from the given file and then report the "
                "number of times each line occurs.");
        return 0;
    }

    // Open the input file.
    FILE * fin = fopen(argv[1], "rt");
    if (fin == NULL) {
        fprintf(stderr, "ERROR: Cannot open input file %s.\n", argv[1]);
        return 1;
    }

    // Create a table.  Use the defaults for everything except deleting an
    // entry.
    HE4 * table = he4_new(8192, NULL, NULL, NULL, free_entry);

    // Now process the input file.
    char buffer[4096];
    while (fgets(buffer, 4096, fin)) {
        // Successfully read a line.  Add it to the hash table.  Note that it
        // would actually be faster to get a pointer to an integer in the table
        // and directly increment the integer.  But this shows more of the API.
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len-1] == '\n') {
            --len;
            buffer[len] = '\0';
        }
        int value = he4_get(table, buffer, len);
        he4_insert(table, strdup(buffer), len, value+1);
        // This works because when an item is not found, NULL is returned, and
        // NULL is (essentially) zero.
        if (he4_load(table) > 0.7) {
            fprintf(stdout, "Rehashing... ");
            table = he4_rehash(table, 0);
            fprintf(stdout, "Table capacity is now %ld.\n", table->capacity);
        }
    } // Process all lines.

    // Now write the counts.
    for (size_t index = 0; index < table->capacity; ++index) {
        he4_map_t * map = he4_index(table, index);
        if (map != NULL && map->key != NULL) {
            fprintf(stdout, "%4zu: \"%s\"(%ld) -> %d\n", index, map->key,
                    map->klen, map->entry);
        }
        HE4FREE(map);
    } // Write all counts.

    // Done.  Free the map.
    he4_delete(table);

    // Close the file.
    fclose(fin);
    return 0;
}
