cmake_minimum_required(VERSION 3.16)

# Глобальный фикc для LVGL: сделать LV_ATTRIBUTE_LARGE_CONST пустым токеном
add_compile_definitions(LV_ATTRIBUTE_LARGE_CONST=)
add_compile_definitions(LV_ATTRIBUTE_MEM_ALIGN=)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(hydroponics)