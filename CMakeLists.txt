cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME U5_LIGHT)
option(UI_ASSET_PROGRAMMER "Build temporary firmware that writes UI assets to external Flash" OFF)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Refresh the firmware's initial calendar time from the build computer before
# compiling the application. The generated header stays inside the build tree.
set(PC_TIME_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(PC_TIME_GENERATED_HEADER "${PC_TIME_GENERATED_DIR}/ui_pc_time.h")
add_custom_target(generate_pc_time_header ALL
    COMMAND "${CMAKE_COMMAND}"
            "-DOUTPUT_FILE=${PC_TIME_GENERATED_HEADER}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_pc_time_header.cmake"
    BYPRODUCTS "${PC_TIME_GENERATED_HEADER}"
    COMMENT "Generating firmware initial time from the build computer"
    VERBATIM
)
add_dependencies(${CMAKE_PROJECT_NAME} generate_pc_time_header)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
message(STATUS "LCD selftest enabled: User/SelfTest/lcd_selftest.c")
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    User/Config/DEV_Config.c
    User/Flash/ext_flash.c
    User/LCD/LCD_1in69.c
    User/LCD/lcd_port.c
    User/Motion/imu_sensor.c
    User/Motion/walk_metrics.c
    User/Power/battery_monitor.c
    User/SelfTest/lcd_selftest.c
    User/GH3018/gh3018_comm.c
    User/GH3018/gh3018_green_ppg_bpm.c
    User/GH3018/gh3018_goodix_hrspo2.c
    User/GH3018/gh3018_soft_i2c.c
    User/Touch/cst816t.c
    User/Touch/touch_controller.c
    User/UI/ui_asset_package.c
    User/UI/ui_asset_programmer.c
    User/UI/ui_assets.c
)

get_filename_component(ARM_TOOLCHAIN_BIN_DIR "${CMAKE_C_COMPILER}" DIRECTORY)
find_program(ARM_OBJCOPY arm-none-eabi-objcopy
    HINTS "${ARM_TOOLCHAIN_BIN_DIR}"
    NO_DEFAULT_PATH
)
if(NOT ARM_OBJCOPY)
    find_program(ARM_OBJCOPY arm-none-eabi-objcopy)
endif()
if(NOT ARM_OBJCOPY AND CMAKE_OBJCOPY)
    set(ARM_OBJCOPY "${CMAKE_OBJCOPY}")
endif()

if(ARM_OBJCOPY)
    add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
        COMMAND "${ARM_OBJCOPY}" -O ihex
                "$<TARGET_FILE:${CMAKE_PROJECT_NAME}>"
                "$<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>/${CMAKE_PROJECT_NAME}.hex"
        COMMAND "${ARM_OBJCOPY}" -O binary
                "$<TARGET_FILE:${CMAKE_PROJECT_NAME}>"
                "$<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>/${CMAKE_PROJECT_NAME}.bin"
        COMMENT "Generating ${CMAKE_PROJECT_NAME}.hex and ${CMAKE_PROJECT_NAME}.bin"
    )
else()
    message(WARNING "arm-none-eabi-objcopy was not found; flash tasks need ${CMAKE_PROJECT_NAME}.hex")
endif()

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${PC_TIME_GENERATED_DIR}
    ${CMAKE_SOURCE_DIR}/Middlewares/GH30x/Inc
    User/Config
    User/Flash
    User/LCD
    User/Motion
    User/Power
    User/SelfTest
    User/GH3018
    User/Touch
    User/UI
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
    UI_ASSET_PROGRAMMER=$<BOOL:${UI_ASSET_PROGRAMMER}>
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx
    ${CMAKE_SOURCE_DIR}/Middlewares/GH30x/Lib/Cortex-M33/gcc/9.2.1-hard-fp/hbd_ctrl_GCC9.2.1_M33_hard-fp.a
    m

    # Add user defined libraries
)
