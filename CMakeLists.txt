cmake_minimum_required(VERSION 3.10)
project(zusage C) 

# Set include and source directories
set(INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)

include_directories(${INCLUDE_DIR})

add_subdirectory(src)
add_subdirectory(tests)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  SET(CMAKE_INSTALL_PREFIX "." CACHE PATH "install path" FORCE)
endif(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)

add_custom_target(check
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target run-tests
    COMMENT "Running all tests defined in the tests folder"
)
