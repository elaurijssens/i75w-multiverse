cmake_minimum_required(VERSION 3.12)

set(MULTIVERSE_BOARD i75w)


if (DEFINED ENV{PICO_BOARD})
    set(PICO_BOARD $ENV{PICO_BOARD})
endif ()

if (DEFINED ENV{PICO_PLATFORM})
    set(PICO_PLATFORM $ENV{PICO_PLATFORM})
endif ()

set(PICO_BOARD_HEADER_DIRS ${CMAKE_CURRENT_LIST_DIR}/src/boards)

set(NAME ${MULTIVERSE_BOARD}-ledmatrix)
include(pimoroni_pico_import.cmake)
include(pico_sdk_import.cmake)
include(zlib_import.cmake)

# Gooey boilerplate
project(${NAME} C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the SDK
pico_sdk_init()

# Tiny USB
set(FAMILY rp2040)
set(BOARD pico_sdk)

set(SRC_DIR ${CMAKE_CURRENT_LIST_DIR}/src)

# Add your source files
add_executable(${NAME}
        ${SRC_DIR}/main.cpp

)

target_include_directories(
        ${NAME} PRIVATE
        ${SRC_DIR}
)

target_compile_definitions(
        ${NAME} PRIVATE
        MULTIVERSE_BOARD="${MULTIVERSE_BOARD}"
)

add_subdirectory(src/server)
add_subdirectory(src/config_storage)
add_subdirectory(src/matrix)
add_subdirectory(src/usb_handler)

# Don't forget to link the libraries you need!
target_link_libraries(${NAME}

        pico_stdlib
        pico_unique_id
        hardware_pio
        hardware_watchdog

        zlib
        server
        config_storage
        matrix
        usb_handler
)

# create map/bin/hex file etc.
pico_add_extra_outputs(${NAME})

# Set up files for the release packages
install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/${NAME}.uf2
        ${CMAKE_CURRENT_LIST_DIR}/README.md
        ${CMAKE_CURRENT_LIST_DIR}/LICENSE
        DESTINATION .
)

set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
set(CPACK_GENERATOR "ZIP" "TGZ")
include(CPack)


# Get build number from environment or set a default
# Get the current date and time in YYYYMMDDHHMM format
execute_process(
        COMMAND date +%Y%m%d%H%M
        OUTPUT_VARIABLE BUILD_NUMBER
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Enable debug flag for Debug builds
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DDEBUG)  # Define DEBUG macro
endif()

# Configure a header file with these values
configure_file(
        ${CMAKE_SOURCE_DIR}/buildinfo.h.in
        ${CMAKE_BINARY_DIR}/buildinfo.h
        @ONLY
)

include_directories(${CMAKE_BINARY_DIR})