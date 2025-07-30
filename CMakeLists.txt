# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.0)
set(toolchainVersion 13_3_Rel1)
set(picotoolVersion 2.1.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(embedded_sensor_logger C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

include_directories( ${CMAKE_SOURCE_DIR}/lib ) # Inclui os files .h na pasta lib
add_subdirectory(lib/FatFs_SPI)    

# Add executable. Default name is the project name, version 0.1
add_executable(embedded_sensor_logger 
        embedded_sensor_logger.c
        lib/ssd1306.c
        hw_config.c
        )

pico_set_program_name(embedded_sensor_logger "embedded_sensor_logger")
pico_set_program_version(embedded_sensor_logger "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(embedded_sensor_logger 0)
pico_enable_stdio_usb(embedded_sensor_logger 1)

# Add the standard library to the build
target_link_libraries(embedded_sensor_logger
        pico_stdlib)

# Add the standard include files to the build
target_include_directories(embedded_sensor_logger PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

# Add any user requested libraries
target_link_libraries(embedded_sensor_logger 
        hardware_i2c
        hardware_pwm
        hardware_timer
        FatFs_SPI
        hardware_clocks
        )

pico_add_extra_outputs(embedded_sensor_logger)