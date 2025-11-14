file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")
idf_component_register(
    SRCS "main.c" ${SOURCES}
    INCLUDE_DIRS "include"
    REQUIRES driver
)



# idf_component_register(
#     SRCS
#         "main.c"
#         "phase_shifter.c"
#         "phase_detect.c"
#         "i2c.c"
#         "wave.c"
#         "timer.c"
#         "adc.c"
#         "max_value.c"
#     INCLUDE_DIRS
#         "."
#     REQUIRES
#         esp_adc
#         driver
# )
