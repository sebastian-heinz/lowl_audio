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
        src/lowl_error.cpp
        src/lowl_file_unix.cpp
        src/lowl_file_win.cpp
        src/lowl_sample_converter.cpp
        )

set(LOWL_DRIVER_DUMMY
        src/driver/dummy/lowl_dummy_driver.cpp
        )

set(LOWL_DRIVER_PA
        src/driver/portaudio/lowl_pa_device.cpp
        src/driver/portaudio/lowl_pa_driver.cpp
        )

#set(DR_LIB_HEADER
#        third_party/dr_libs/dr_flac.h
#        third_party/dr_libs/dr_mp3.h
#        third_party/dr_libs/dr_wav.h
#        )

add_library(${PROJECT_NAME} ${LOWL_SOURCES} ${LOWL_DRIVER_DUMMY} ${LOWL_DRIVER_PA})
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
target_include_directories(${PROJECT_NAME} PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>" "$<INSTALL_INTERFACE:include>")

add_definitions(-DLOWL_DRIVER_DUMMY)
add_definitions(-DLOWL_DRIVER_PORTAUDIO)
add_definitions(-DLOWL_UNIX)

# readerwriterqueue
add_subdirectory(third_party/readerwriterqueue)
target_link_libraries(${PROJECT_NAME} readerwriterqueue)

# portaudio
add_subdirectory(third_party/portaudio)
target_link_libraries(${PROJECT_NAME} portaudio)

# dr_lib
target_include_directories(${PROJECT_NAME} PRIVATE third_party/dr_libs)

add_subdirectory(demo)