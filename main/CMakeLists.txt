# idf_component_register(SRCS "phase_shifter.c" "phase_detect.c" "ADS1115.c" "i2c.c" "wave.c" "main.c" "timer.c" "adc.c" "max_value.c"
#                     INCLUDE_DIRS ".")

idf_component_register(
    SRCS
        "main.c"
        "phase_shifter.c"
        "phase_detect.c"
        "ADS1115.c"
        "i2c.c"
        "wave.c"
        "timer.c"
        "adc.c"
        "max_value.c"
        "ads1115_oneshot.c"
    INCLUDE_DIRS
        "."
    REQUIRES
        esp_adc
        ads111x
        i2cdev
        driver
)
