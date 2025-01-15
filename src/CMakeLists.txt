set(libsrc
  zusage.c
)

add_library(libzusage OBJECT ${libsrc})
add_library(zusage STATIC $<TARGET_OBJECTS:libzusage>)

install(
    DIRECTORY ${PROJECT_BINARY_DIR}/lib/
    DESTINATION "lib"
    FILES_MATCHING PATTERN "*.?")

install(
    DIRECTORY ${CMAKE_SOURCE_DIR}/include/
    DESTINATION "include"
    FILES_MATCHING PATTERN "*")
