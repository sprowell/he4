# Configuration to build Superfluid He-4.

cmake_minimum_required(VERSION 3.3)
project(he4)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")
set(CMAKE_MACOSX_RPATH 1)

include_directories(AFTER SYSTEM include)
set(LIBRARY_FILES src/he4.c src/xxhash.c src/xxhsum.c)
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
