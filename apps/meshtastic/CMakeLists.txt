set(MESHTASTIC_APP_NAME m1-meshtastic)
set(MESHTASTIC_APP_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${MESHTASTIC_APP_NAME}.m1app)

file(GLOB MESHTASTIC_APP_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/nanopb/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/meshtastic_api/meshtastic/*.c
)

add_library(meshtastic_app_objects OBJECT ${MESHTASTIC_APP_SOURCES})
target_include_directories(meshtastic_app_objects PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/nanopb
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/meshtastic_api
)
target_compile_options(meshtastic_app_objects PRIVATE
    -mcpu=cortex-m33
    -mthumb
    -mfloat-abi=soft
    -mword-relocations
    -mlong-calls
    -fno-common
    -fdata-sections
    -ffunction-sections
    -Os
    -g0
)

execute_process(
    COMMAND ${CMAKE_C_COMPILER} -mcpu=cortex-m33 -mthumb -mfloat-abi=soft -print-libgcc-file-name
    OUTPUT_VARIABLE MESHTASTIC_LIBGCC
    OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY
)

add_custom_command(
    OUTPUT ${MESHTASTIC_APP_OUTPUT}
    COMMAND ${CMAKE_C_COMPILER}
        -mcpu=cortex-m33 -mthumb -mfloat-abi=soft
        -nostdlib -nostartfiles
        -Wl,-Ur -Wl,--gc-sections -Wl,-e,app_main
        -T${CMAKE_CURRENT_SOURCE_DIR}/linker/m1app.ld
        -o ${MESHTASTIC_APP_OUTPUT}
        $<TARGET_OBJECTS:meshtastic_app_objects>
        ${MESHTASTIC_LIBGCC}
    COMMAND ${CMAKE_SIZE} ${MESHTASTIC_APP_OUTPUT}
    DEPENDS meshtastic_app_objects
        $<TARGET_OBJECTS:meshtastic_app_objects>
        ${CMAKE_CURRENT_SOURCE_DIR}/linker/m1app.ld
    COMMENT "Building ${MESHTASTIC_APP_NAME}.m1app"
    COMMAND_EXPAND_LISTS
    VERBATIM
)
add_custom_target(meshtastic_app ALL DEPENDS ${MESHTASTIC_APP_OUTPUT})
