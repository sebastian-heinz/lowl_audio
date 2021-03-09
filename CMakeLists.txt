cmake_minimum_required(VERSION 3.6)
project(lowl_audio)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(LOWL_SOURCES
        src/lowl.cpp
        src/lowl_audio_reader.cpp
        src/lowl_audio_reader_wav.cpp
        src/lowl_audio_stream.cpp
        src/lowl_buffer.cpp
        src/lowl_device.cpp
        src/lowl_driver.cpp
        src/lowl_dummy_driver.cpp
        src/lowl_error.cpp
        src/lowl_file_unix.cpp
        src/lowl_file_win.cpp
        src/lowl_dummy_driver.cpp
        src/lowl_dummy_driver.cpp
        )

set(LOWL_DRIVER_PA
        src/lowl_pa_device.cpp
        src/lowl_pa_driver.cpp
        )

add_library(${PROJECT_NAME} ${LOWL_SOURCES} ${LOWL_DRIVER_PA})

add_subdirectory(third_party/readerwriterqueue)
TARGET_LINK_LIBRARIES(${PROJECT_NAME} readerwriterqueue)

add_subdirectory(third_party/portaudio)
TARGET_LINK_LIBRARIES(${PROJECT_NAME} portaudio)

target_include_directories(${PROJECT_NAME} PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>" "$<INSTALL_INTERFACE:include>")

add_definitions(-DLOWL_DRIVER_DUMMY)
add_definitions(-DLOWL_DRIVER_PORTAUDIO)
add_definitions(-DLOWL_UNIX)

add_subdirectory(demo)