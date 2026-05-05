cmake_minimum_required(VERSION 3.13)

# ---- Pico SDK bootstrap --------------------------------------------------
# Set PICO_SDK_PATH before including the SDK init file, or pass it on the
# cmake command line:  cmake -DPICO_SDK_PATH=/path/to/pico-sdk ..
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(drummachine C CXX ASM)

set(CMAKE_C_STANDARD   11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

# ---- PIO source ----------------------------------------------------------
# Generate the i2s_output.pio.h header from the PIO assembly file.
pico_generate_pio_header(drummachine
    ${CMAKE_CURRENT_LIST_DIR}/src/i2s_output.pio
)

# ---- Firmware target -----------------------------------------------------
add_executable(drummachine
    src/main.cpp
    src/ads1115.cpp
    src/drum_machine.cpp
)

target_include_directories(drummachine PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src
)

target_link_libraries(drummachine
    pico_stdlib
    hardware_i2c
    hardware_pio
    hardware_gpio
    hardware_clocks
    m           # libm for sinf/expf in drum voices
)

# Enable USB serial (printf output) and disable UART
pico_enable_stdio_usb(drummachine  1)
pico_enable_stdio_uart(drummachine 0)

# Generate UF2 / bin / hex output files
pico_add_extra_outputs(drummachine)
