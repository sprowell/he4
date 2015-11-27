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

#include "test-frame.h"
#include <he4.h>

START_TEST
START_ITEM(table)

// Try to create a table. This should fail and return NULL.  Then try to
// delete the NULL table.
HE4 * table = he4_new(0, NULL, NULL, NULL, NULL);
ASSERT(table == NULL);
he4_delete(table);

// Create a one megabyte table.
size_t capacity = he4_best_capacity(1024 * 1024);
table = he4_new(capacity, NULL, NULL, NULL, NULL);
ASSERT(table != NULL); IF_FAIL_STOP;
ASSERT(table->capacity == capacity); IF_FAIL_STOP;
ASSERT(he4_size(table) == 0); IF_FAIL_STOP;
ASSERT(he4_capacity(table) == capacity); IF_FAIL_STOP;
ASSERT(he4_load(table) == 0.0); IF_FAIL_STOP;
ASSERT(he4_max_touch(table) == 0); IF_FAIL_STOP;
he4_delete(table);

// Test some edge cases.
ASSERT(he4_size(NULL) == 0);
ASSERT(he4_capacity(NULL) == 0);
ASSERT(he4_load(NULL) == 1.0);
ASSERT(he4_max_touch(NULL) == 0);

END_ITEM
END_TEST
