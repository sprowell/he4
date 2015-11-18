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
#include <time.h>
#include <he4.h>

#define INITIAL_TABLE_SIZE 16384

// Leave this in to allow the table to grow.  Comment it out to trim the least
// recently used items from the table when it becomes full.  This will only
// work if the library is compiled with touch enabled.
#define EMBIGGEN

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
    HE4 * table = he4_new(INITIAL_TABLE_SIZE, NULL, NULL, NULL, free_entry);

    // Time the operation.
    clock_t start, end;
    double cpu_time_used;
    start = clock();

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
        // Locate the entry in the table, if it exists.  Note that if we used
        // he4_get we would have to then do an he4_insert to update the value,
        // which could require two searches of the table.  If the entry does
        // not exist, then two searches will be required, still.  A "find or
        // set" function would avoid this!
        int * value = he4_find(table, buffer, len);
        if (value == NULL) {
            // The entry is not already present, so insert it.
            char * clone = HE4MALLOC(char, len);
            memcpy(clone, buffer, len);
            he4_insert(table, clone, len, 1);
        } else {
            // The entry is in the table.  Increment it.
            ++*value;
        }

        // Handle the case of the table becoming too full.  There are two ways
        // to deal with this.  We can let the table get larger, or we can trim
        // the last-recently-used items.  Both are shown here.  Choose the one
        // you want at compile time.
#ifdef EMBIGGEN
        // Grow the table.
        if (he4_load(table) > 0.7) {
            table = he4_rehash(table, 0);
        }
#else
        // Trim the table.
        if (he4_load(table) > 0.7)
            while (he4_load(table) > 0.3) {
                table = he4_trim_and_rehash(table, table->capacity, table->max_touch / 2);
            } // Trim least recently used.
#endif // EMBIGGEN
    } // Process all lines.

    // Get the elapsed time.
    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;

    // Now write the counts.
    for (size_t index = 0; index < table->capacity; ++index) {
        he4_map_t * map = he4_index(table, index);
        if (map != NULL && map->key != NULL) {
            fprintf(stdout, "%4zu: \"%s\"(%ld) -> %d\n", index, map->key,
                    map->klen, map->entry);
        }
        HE4FREE(map);
    } // Write all counts.

    // Tell the user how much time was taken.
    fprintf(stdout, "Initial table capacity: %ld\n", (long)INITIAL_TABLE_SIZE);
    fprintf(stdout, "Final table capacity: %ld\n", table->capacity);
    fprintf(stdout, "CPU Time Used: %f seconds\n", cpu_time_used);

    // Done.  Free the map.
    he4_delete(table);

    // Close the file.
    fclose(fin);
    return 0;
}
