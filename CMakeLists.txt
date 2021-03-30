cmake_minimum_required(VERSION 3.6)
project(lowl_audio)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(LOWL_SOURCES
        src/lowl.cpp
        src/lowl_audio_data.cpp
        src/lowl_audio_mixer.cpp
        src/lowl_audio_reader.cpp
        src/lowl_audio_reader_flac.cpp
        src/lowl_audio_reader_mp3.cpp
        src/lowl_audio_reader_wav.cpp
        src/lowl_audio_stream.cpp
        src/lowl_buffer.cpp
        src/lowl_device.cpp
        src/lowl_driver.cpp
        src/lowl_driver_dummy.cpp
        src/lowl_device_pa.cpp
        src/lowl_driver_pa.cpp
        src/lowl_error.cpp
        src/lowl_file_unix.cpp
        src/lowl_file_win.cpp
        src/lowl_node.cpp
        src/lowl_node_re_sampler.cpp
        src/lowl_re_sampler.cpp
        src/lowl_sample_converter.cpp
        )

add_library(${PROJECT_NAME} ${LOWL_SOURCES})
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
target_include_directories(${PROJECT_NAME} PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>" "$<INSTALL_INTERFACE:include>")

add_definitions(-DLOWL_DRIVER_DUMMY)
add_definitions(-DLOWL_DRIVER_PORTAUDIO)
add_definitions(-DLOWL_PROFILING)

IF (WIN32)
    add_definitions(-DLOWL_WIN)
ENDIF ()

IF (UNIX)
    add_definitions(-DLOWL_UNIX)
ENDIF ()

# readerwriterqueue
add_subdirectory(third_party/readerwriterqueue)
target_link_libraries(${PROJECT_NAME} readerwriterqueue)

# concurrentqueue
add_subdirectory(third_party/concurrentqueue)
target_link_libraries(${PROJECT_NAME} concurrentqueue)

# portaudio
add_subdirectory(third_party/portaudio)
target_link_libraries(${PROJECT_NAME} portaudio_static)

# dr_lib
target_include_directories(${PROJECT_NAME} PRIVATE third_party/dr_libs)

# r8brain-free-src
target_include_directories(${PROJECT_NAME} PUBLIC third_party/r8brain-free-src)
target_sources(${PROJECT_NAME}
        PRIVATE
        third_party/r8brain-free-src/pffft.cpp
        third_party/r8brain-free-src/r8bbase.cpp
        )
# https://github.com/avaneev/r8brain-free-src#usage-information
#add_definitions(-DR8B_PFFFT=1)

add_subdirectory(demo)