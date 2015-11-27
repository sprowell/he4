# Configuration to build Superfluid He-4.
#
# Build in release mode:
# cmake -D CMAKE_BUILD_TYPE=Release ..
#
# Build in debug mode:
# cmake -D CMAKE_BUILD_TYPE=Debug ..

cmake_minimum_required(VERSION 2.8.7)
set(CMAKE_MACOSX_RPATH 1)

# Projece name and version.
project(he4)
add_definitions(-DHE4_VERSION="1.0.4")

# Check and populate compiler flags.
include(CheckCCompilerFlag)
macro (SET_FLAGS_MAYBE)
    foreach (ARG ${ARGV})
        check_c_compiler_flag(${ARG} HAS_FLAG)
        if (HAS_FLAG)
            set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ARG}")
        endif (HAS_FLAG)
    endforeach (ARG)
endmacro (SET_FLAGS_MAYBE)
SET_FLAGS_MAYBE("-Wall" "-std=c99" "-march=native" "-mtune=native")

# Set some platform-specific definitions.
if (APPLE)
    add_definitions(-DDARWIN)
endif (APPLE)
if (MSVC)
	# Suppress warnings about fopen vs. fopen_s (C11).
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif (MSVC)

######################################################################
##
## User configuration section.
##
######################################################################

# If you want to use Doug Lea's malloc, uncomment this.
#
#set(HE4_DLMALLOC 1)

# If you want to suppress the standard library, uncomment this line and define
# your own malloc (or uncomment the line to use Doug Lea's).  This will also
# suppress debugging.
#
#set(NO_STD_LIB 1)

# By default the library includes an additional entry called the "touch index"
# for every cell.  Every time a cell is set or found, the maximum touch index
# for the table is incremented, and the value is stored with the cell.  This
# means that the least-recently-used cell will have the lowest touch index.
# This also takes time and space.  You can disable it by uncommenting the
# following line.
#
#add_definitions(-DHE4NOTOUCH)

######################################################################

if (NO_STD_LIB)
    message("Not including the standard library.")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -nostdlib")
    add_definitions(-DNODEBUG) # Prevents building debug lines.

    # These suppress some dependencies in dlmalloc.
    add_definitions(-DNO_MALLOC_STATS) # Suppresses some includes in dlmalloc.
    add_definitions(-DLACKS_STDLIB_H) # Suppress standard library.
    add_definitions(-DLACKS_STRING_H) # Suppress the string library.
    add_definitions(-DLACKS_TIME_H) # Suppress the time library.
    add_definitions(-DLACKS_UNISTD_H) # Suppress the unistd library.
endif (NO_STD_LIB)

include_directories(AFTER SYSTEM include)
set(LIBRARY_FILES src/he4.c src/xxhash.c)
if (HE4_DLMALLOC)
    message("Using Doug Lea's malloc.")
    set(LIBRARY_FILES ${LIBRARY_FILES} src/malloc.c)
    add_definitions(-DHE4_DLMALLOC)
endif (HE4_DLMALLOC)
add_library(he4_static STATIC ${LIBRARY_FILES})
add_library(he4_shared SHARED ${LIBRARY_FILES})
set_property(TARGET he4_static PROPERTY POSITION_INDEPENDENT_CODE 1)
if (UNIX)
    set_property(TARGET he4_static PROPERTY OUTPUT_NAME he4)
    set_property(TARGET he4_shared PROPERTY OUTPUT_NAME he4)
endif (UNIX)

# Examples.
add_custom_target(examples)
file(GLOB EXAMPLES "examples/*.c")
foreach (EXAMPLE ${EXAMPLES})
    get_filename_component(EXAMPLE_NAME ${EXAMPLE} NAME_WE)
    add_executable(${EXAMPLE_NAME} EXCLUDE_FROM_ALL ${EXAMPLE})
    target_link_libraries(${EXAMPLE_NAME} he4_static)
    add_dependencies(examples ${EXAMPLE_NAME})
endforeach(EXAMPLE)

# Testing.  Every .c file in the tests folder is assumed to be a test.
enable_testing()
file(GLOB TESTS "tests/*.c")
foreach (TEST ${TESTS})
    get_filename_component(TEST_NAME ${TEST} NAME_WE)
    add_executable(${TEST_NAME} ${TEST})
    target_link_libraries(${TEST_NAME} he4_static)
    add_test(${TEST_NAME} ${TEST_NAME})
endforeach(TEST)

# Generate API documentation using Doxygen.
find_package(Doxygen)
if(DOXYGEN_FOUND)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
    add_custom_target(doc ALL
            ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Generating API documentation with Doxygen" VERBATIM
            )
endif(DOXYGEN_FOUND)
