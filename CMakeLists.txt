cmake_minimum_required(VERSION 3.13)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(PICO_BOARD pico_w CACHE STRING "Board type")
include(pico_sdk_import.cmake)
project(ProjetoIntegrado C CXX ASM)

pico_sdk_init()

add_executable(ProjetoIntegrado ProjetoIntegrado.c lib/ssd1306.c)

pico_set_program_name(ProjetoIntegrado "ProjetoIntegrado")
pico_set_program_version(ProjetoIntegrado "0.1")

pico_generate_pio_header(ProjetoIntegrado ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio)

pico_enable_stdio_uart(ProjetoIntegrado 1)
pico_enable_stdio_usb(ProjetoIntegrado 1)

target_link_libraries(ProjetoIntegrado
        pico_stdlib
        hardware_i2c
        hardware_pwm
        hardware_adc
        hardware_pio)

target_include_directories(ProjetoIntegrado PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

pico_add_extra_outputs(ProjetoIntegrado)

