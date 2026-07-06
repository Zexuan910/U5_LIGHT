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

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
message(STATUS "LCD selftest enabled: User/SelfTest/lcd_selftest.c")
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    User/Config/DEV_Config.c
    User/LCD/LCD_1in69.c
    User/LCD/lcd_port.c
    User/SelfTest/lcd_selftest.c
    User/GH3018/gh3018_comm.c
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
    User/Config
    User/LCD
    User/SelfTest
    User/GH3018
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx
    ${CMAKE_CURRENT_SOURCE_DIR}/User/GH3018/lib/hbd_ctrl_GCC9.2.1_M33_hard-fp.a

    # Add user defined libraries
)
