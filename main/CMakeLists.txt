
set(SRC_CLASSES)

file(GLOB SRC_CLASSES *.c *.cpp)
file(GLOB_RECURSE IMAGE_SOURCES images/*.c)


idf_component_register(SRCS ${SRC_CLASSES}
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES driver  esp_lcd esp_timer
                       REQUIRES  TouchLib )

 target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu++2b "-Wmissing-field-initializers")



