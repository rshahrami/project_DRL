# main/CMakeLists.txt
# این CMake داخل کامپوننت main اجرا می‌شود
# فایل‌های .c از فولدر src/ را پیدا کرده و به component اضافه می‌کنیم
# main/CMakeLists.txt
# همه فایل‌های .c در src/ این کامپوننت اضافه می‌شوند
# main/CMakeLists.txt

# جمع‌آوری تمام فایل‌های .c و .cpp داخل src
file(GLOB_RECURSE MAIN_SRC_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

# اضافه کردن main.c به لیست فایل‌ها
set(MAIN_SRC ${CMAKE_CURRENT_SOURCE_DIR}/main.c ${MAIN_SRC_FILES})

idf_component_register(
    SRCS ${MAIN_SRC}      # همه فایل‌های source
    INCLUDE_DIRS "include"  # مسیر headerها
    REQUIRES driver         # اگر به driver نیاز دارد
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
