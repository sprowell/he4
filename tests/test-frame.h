#ifndef TEST_FRAME_H_
#define TEST_FRAME_H_

/**
 * @file
 * Simple test framework.
 *
 * To use this, include this header file.  You can define a test using
 * START_TEST and END_TEST.  Within a test you can define individual
 * items with the START_ITEM(name_m) and END_ITEM macros, where
 * the item name is a symbol used in messages.
 *
 * Use FAIL to record failure, and FAIL_ITEM to fail and skip the rest of
 * the current item.  Also useful is IF_FAIL_STOP to halt a test entirely
 * if there has been a failure.
 *
 * @author sprowell@gmail.com
 *
 * @verbatim
 * Copyright (c) 2015, Stacy Prowell
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 * @endverbatim
 */


// By default every test is run in a separate process.  If you don't want
// that (because it can be a problem during debugging) then #define NOFORK
// before you #include this file.


#ifndef FPRINTF
#include <stdio.h> // For fprintf, fflush, stdout.
#endif
#ifndef NOFORK
#include <unistd.h> // For fork.
#include <sys/wait.h> // For waitpid.
#endif

#include <time.h> // For asctime, localtime.
#include <stdbool.h> // For bool.
#include <stdlib.h> // For srand.
#include <string.h> // For strlen.
#include <setjmp.h> // For setjmp, longjmp, jmp_buf.


//======================================================================
// Macros that are used to define how the test framework writes its output.
// You might need to modify these if you are on a platform that does not
// provide fprintf.
//======================================================================

/// Where should test messages go.  By default this points to the standard
/// output.  If you want to change that, #define this before you #include
/// this header.  If you re-define FPRINTF, you can probably ignore this.
#ifndef TEST_OUTPUT
#  define TEST_OUTPUT stdout
#endif

/// What should be used as the end of line.  By default this is newline.
/// If you want to change that, #define this before you #include this header.
#ifndef ENDL__
#  define ENDL__ "\n"
#endif

/// What function we call to write.  You can override this (if you must) by
/// #define-ing this before you #include the file.  Whatever you use here
/// is expected to have the structure of printf(...).
#ifndef FPRINTF
#  define FPRINTF(...) \
 	fprintf(TEST_OUTPUT , ##__VA_ARGS__); \
 	fflush(TEST_OUTPUT);
#endif


//======================================================================
// Private macros used to implement parts of the test framework.
// Do not use these; keep scrolling down for the public macros.
//======================================================================

// Create a label for a goto.
#define LABEL(x_m) LABEL ## x_m

// Convert the macro argument into a quoted string.  Use STRINGIFY instead.
#define STRINGIFY_(x_m) #x_m

// Convert the macro argument into a quoted string.
#define STRINGIFY(x_m) STRINGIFY_(x_m)

// If an end of line is needed, emit one.
#define IF_ENDL \
	if (tf_need_endl) { \
		FPRINTF(ENDL__); \
		tf_need_endl = false; \
		tf_need_space = false; \
		tf_need_indent = true; \
	}

// If a space is needed, emit one.
#define IF_SPACE \
	if (tf_need_space) { \
		FPRINTF(" "); \
		tf_need_space = false; \
	}

// If indentation is needed, emit it.
#define IF_INDENT \
	if (tf_need_indent) { \
		IF_ENDL; \
		FPRINTF("  "); \
		tf_need_endl = true; \
		tf_need_space = false; \
		tf_need_indent = false; \
	}

// Emit a timestamped message.
#define TS(...) \
	{ \
		IF_ENDL; \
    	time_t ltime = time(NULL); \
    	char * time = asctime(localtime(&ltime)); \
		time[strlen(time)-1] = '\0'; \
    	FPRINTF("%s: ", time); \
		FPRINTF(__VA_ARGS__); \
		FPRINTF(ENDL__); \
		tf_need_endl = false; \
		tf_need_space = false; \
		tf_need_indent = true; \
	}

/// A test for core dump.  This is not supported everywhere.
#ifndef NOFORK
#  ifdef WCOREDUMP
#    define CDC__(status_m) \
	if (WCOREDUMP(status_m)) { \
		TS("Child process generated a core dump."); \
	}
#  endif
#endif
#ifndef CDC__
#  define CDC__(status_m)
#endif

//======================================================================
// Public macros to write output during a test.
//======================================================================

/**
 * Write a message.  If this is used within a test item, it is indented.
 * subsequent writes are separated by a single space.  Arguments are the
 * same as printf(...).
 */
#define WRITE(...) \
	{ \
		IF_INDENT; \
		IF_SPACE; \
		FPRINTF(__VA_ARGS__); \
		tf_need_endl = true; \
		tf_need_space = true; \
		tf_need_indent = false; \
	}

/**
 * Write a message followed by a newline.  If this is used within a test,
 * it is indented.  Subsequent writes are separated by a single space.
 * Arguments are the same as printf(...).
 */
#define WRITELN(...) \
	{ \
		IF_INDENT; \
		IF_SPACE; \
		FPRINTF(__VA_ARGS__); \
		FPRINTF(ENDL__); \
		tf_need_endl = false; \
		tf_need_space = false; \
		tf_need_indent = true; \
	}

//======================================================================
// Private structural components.
//======================================================================

#ifndef NOFORK
#  define PREFLIGHT \
	int pid = fork(); \
	if (pid < 0) { \
		TS("Fork failed; aborting test."); \
		tf_fail_test = true; \
		tf_retval = -1; \
	} else if (pid != 0) { \
		int status = 0; \
		waitpid(pid, &status, 0); \
		if (WIFSIGNALED(status)) { \
			tf_fail_test = true; \
			tf_retval = -2; \
			fprintf(TEST_OUTPUT, \
				"Child process was terminated by the signal: %d%s", \
				WTERMSIG(status), ENDL__); \
		} else if (WIFEXITED(status)) { \
			tf_fail_test = true; \
			tf_retval = WEXITSTATUS(status); \
			if (tf_retval != 0) { \
				fprintf(TEST_OUTPUT, \
					"Child process exited with non-zero status: %d%s", \
					tf_retval, ENDL__); \
			} \
		} \
		CDC__(status); \
	} else {
#  define POSTFLIGHT \
	}
#else
#  define PREFLIGHT
#  define POSTFLIGHT
#endif

//======================================================================
// Structural macros that define a test, or the items in a test.
//======================================================================

/**
 * Macro to set up a test.  Use this once at the start of your test.
 */
#define START_TEST \
static jmp_buf buf; \
int main(int argc, char *argv[]) { \
	bool tf_need_space = false, tf_need_endl = false, tf_need_indent = false; \
	int tf_retval = 0; \
	int tf_item_enabled = true; \
	bool tf_fail_test = false; \
	srand(time(0)); \
	TS("Starting test"); \
	PREFLIGHT; \

/**
 * Write a success message and continue the test.  This is in addition to the
 * success message written if a test item or the entire test succeeds, so use
 * this if you want an extra success message.
 */
#define SUCCESS \
	WRITELN("SUCCESS");

/**
 * Abort and fail the entire test.  The test is immediately stopped.
 */
#define FAIL_TEST(fmt_m, ...) \
	tf_fail_test = true; \
    WRITE("FAILED at %s:%d", __FILE__, __LINE__); \
	WRITELN(fmt_m , ##__VA_ARGS__); \
	goto end_test;

/**
 * If failure was detected in a prior step, stop the test now.  That is, if a
 * prior step contained a FAIL_ITEM call, then stop the entire test.
 */
#define IF_FAIL_STOP \
	if (tf_fail_test) { \
		goto end_test; \
	}

/**
 * Execute a block of actions if failure has been detected.
 */
#define IF_FAIL \
	if (tf_fail_test)

/**
 * Execute a block of actions if failure has not been detected.
 */
#define IF_NO_FAIL \
	if (! tf_fail_test)

/**
 * End the test.  You must use this once at the end of your test, after ending
 * all test items.
 */
#define END_TEST \
		end_test: \
		TS("Ending test"); \
		if (tf_fail_test) { \
			WRITELN("FAILED"); \
			return EXIT_FAILURE; \
		} \
		WRITELN("SUCCESS"); \
		tf_retval = 0; \
		return EXIT_SUCCESS; \
	POSTFLIGHT; \
	return tf_retval; \
}

/**
 * Disable the next test item, if any.  Other test items remain enabled.
 * If you use this within a test item, then the next test item is disabled.
 * Typically you would use it just prior to a START_ITEM.
 */
#define DISABLE \
	tf_item_enabled = false;

/**
 * Start an item in the test.  Declarations in an item are local to the item.
 */
#define START_ITEM(item_name_m) \
	if (tf_item_enabled) { \
		bool tf_fail_item = false; \
		tf_need_space = false; \
		char * item_name = STRINGIFY(item_name_m); \
		TS("Starting item %s", item_name); \
		tf_need_space = false; \
		tf_need_indent = true; \
		tf_need_endl = false; \
		if (! setjmp(buf)) {

/**
 * Fail the current test item.  This allows testing to proceed to the next
 * test item, if any, provided it is not disabled.
 */
#define FAIL_ITEM(fmt_m, ...) \
			tf_fail_test = true; \
			tf_fail_item = true; \
			WRITE("FAILED at %s:%d", __FILE__, __LINE__); \
			WRITELN(fmt_m , ##__VA_ARGS__); \
			longjmp(buf, 1);

/**
 * Fail the current test item, but continue with the item.  This allows
 * reporting failure for long lists of checks without forcing the checks
 * to be stopped.
 */
#define FAIL(fmt_m, ...) \
	 		tf_fail_test = true; \
	 		tf_fail_item = true; \
			WRITE("FAILED at %s:%d", __FILE__, __LINE__); \
			WRITELN(fmt_m , ##__VA_ARGS__);

/**
 * End a test item.  Include this at the end of your test item code and
 * before starting any new item (or ending the test).
 */
#define END_ITEM \
 		} \
		IF_ENDL; \
		tf_need_indent = false; \
		TS("Ending item %s", item_name); \
	} \
	tf_item_enabled = true;

/**
 * Assert that a given predicate is "true."  If validation fails, write a
 * failure message.
 */
#define ASSERT(pred_m) \
		if (!(pred_m)) { \
            FAIL("assertion: " #pred_m); \
        }

/**
 * Require that a predicate be true for the test to proceed.  If the predicate
 * is false, fail the test.
 */
#define REQUIRE(pred_m) \
        if (!(pred_m)) { \
            FAIL_TEST("requirement not met: " #pred_m); \
        }

#endif /*TEST_FRAME_H_*/
