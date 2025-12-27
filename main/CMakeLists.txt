file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")
idf_component_register(
    SRCS "main.c" ${SOURCES}
    INCLUDE_DIRS "include"
    REQUIRES driver
)
