# Set the minimum version of CMake
cmake_minimum_required(VERSION 3.15)

# Add the tests directory
project(tests)

include_directories(${CMAKE_SOURCE_DIR}/include)

# Initialize a variable to store library paths
set(LIBZZ_PATHS)

# Add the fixed directory
list(APPEND LIBZZ_PATHS /usr/lpp/hzc/lib/)

# Retrieve the environment variable ZOPEN_OLD_LIBPATH and split it by ':'
if(DEFINED ENV{ZOPEN_OLD_LIBPATH})
    string(REPLACE ":" ";" ZOPEN_OLD_LIBPATH_LIST $ENV{ZOPEN_OLD_LIBPATH})
    list(APPEND LIBZZ_PATHS ${ZOPEN_OLD_LIBPATH_LIST})
endif()

# Find the library 'libzz.a' in the specified paths
set(LIBZZ_FOUND_PATH "")
foreach(PATH ${LIBZZ_PATHS})
    if(EXISTS "${PATH}/libzz.a")
        set(LIBZZ_FOUND_PATH ${PATH})
        break()
    endif()
endforeach()

# Handle the case where the library is not found
if(LIBZZ_FOUND_PATH)
    message(STATUS "Found libzz.a in: ${LIBZZ_FOUND_PATH}")
    link_directories(${LIBZZ_FOUND_PATH})
else()
    message(FATAL_ERROR "libzz.a not found in any of the specified paths")
endif()

# Collect all test files in the folder
file(GLOB TEST_SOURCES "*.c")

# Add executables for each test file
foreach(TEST_SOURCE ${TEST_SOURCES})
    # Extract the filename without an extension to name the test
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
    
    # Create a test executable
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    
    # Link the zedc_ascii static library to the test
    target_link_libraries(${TEST_NAME} zedc_ascii -lzz)
endforeach()


set(TEST_SCRIPT "${CMAKE_SOURCE_DIR}/tests/tests.sh")

add_custom_target(run-tests
    COMMAND ${CMAKE_COMMAND} -E echo "Running tests with ${TEST_SCRIPT}"
    COMMAND ${TEST_SCRIPT}  
    DEPENDS ${TEST_SOURCES} 
    COMMENT "Running all tests with script"
)
