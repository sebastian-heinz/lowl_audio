cmake_minimum_required(VERSION 3.6)
project(lowl_audio)

set(LOWL_LIB_MAIN lowl_audio_lib)


message([LOWL] CMAKE_CXX_COMPILER_ID: ${CMAKE_CXX_COMPILER_ID})
message([LOWL] Project: ${PROJECT_NAME})
message([LOWL] BuildType: ${CMAKE_BUILD_TYPE})
message([LOWL] CMAKE_CXX_COMPILER_ID: ${CMAKE_CXX_COMPILER_ID})
message([LOWL] CMAKE_SYSTEM_NAME: ${CMAKE_SYSTEM_NAME})
if(WIN32)
    message([LOWL] WIN32)
endif()
if(APPLE)
    message([LOWL] APPLE)
endif()
if(UNIX)
    message([LOWL] UNIX)
endif()
if(MSVC)
    message([LOWL] MSVC)
endif()
if(MSYS)
    message([LOWL] MSYS)
endif()
if(MINGW)
    message([LOWL] MINGW)
endif()


set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

IF(CMAKE_BUILD_TYPE MATCHES Debug)
message([LOWL] Setting optimization level for Debug build to -O0)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0")
ENDIF()

set(LOWL_PA_DEFAULT_DRIVER_PRIORITY "ALSA,OSS,JACK,COREAUDIO,WASAPI,WDMKS,DSOUND,WMME,ASIO,ASIHPI")

set(LOWL_SOURCES
        src/lowl.cpp
        src/lowl_audio_source.cpp
        src/lowl_audio_data.cpp
        src/lowl_audio_mixer.cpp
        src/lowl_audio_reader.cpp
        src/lowl_audio_reader_flac.cpp
        src/lowl_audio_reader_mp3.cpp
        src/lowl_audio_reader_wav.cpp
        src/lowl_audio_stream.cpp
        src/lowl_audio_util.cpp
        src/lowl_buffer.cpp
        src/lowl_device.cpp
        src/lowl_channel_converter.cpp
        src/lowl_driver.cpp
        src/lowl_driver_dummy.cpp
        src/lowl_device_pa.cpp
        src/lowl_driver_pa.cpp
        src/lowl_error.cpp
        src/lowl_file.cpp
        src/lowl_logger.cpp
        src/lowl_re_sampler.cpp
        src/lowl_sample_converter.cpp
        src/lowl_space.cpp
        src/node/lowl_node.cpp
        src/node/lowl_node_in_stream.cpp
        src/node/lowl_node_out_stream.cpp
        src/node/lowl_node_process.cpp
        src/node/lowl_node_re_sampler.cpp
        )

add_library(${LOWL_LIB_MAIN} ${LOWL_SOURCES})
set_target_properties(${LOWL_LIB_MAIN} PROPERTIES PREFIX "")
target_include_directories(${LOWL_LIB_MAIN} PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>" "$<INSTALL_INTERFACE:include>")
target_compile_definitions(${LOWL_LIB_MAIN} PRIVATE LOWL_DRIVER_DUMMY)
target_compile_definitions(${LOWL_LIB_MAIN} PRIVATE LOWL_DRIVER_PORTAUDIO)
target_compile_definitions(${LOWL_LIB_MAIN} PRIVATE "LOWL_PA_DEFAULT_DRIVER_PRIORITY=\"${LOWL_PA_DEFAULT_DRIVER_PRIORITY}\"")

IF (WIN32)
    target_compile_definitions(${LOWL_LIB_MAIN} PRIVATE LOWL_WIN)
ENDIF ()
IF (UNIX)
    target_compile_definitions(${LOWL_LIB_MAIN} PRIVATE LOWL_UNIX)
ENDIF ()

# readerwriterqueue
add_subdirectory(third_party/readerwriterqueue)
target_link_libraries(${LOWL_LIB_MAIN} PUBLIC readerwriterqueue)

# concurrentqueue
add_subdirectory(third_party/concurrentqueue)
target_link_libraries(${LOWL_LIB_MAIN} PUBLIC concurrentqueue)

# portaudio
add_subdirectory(third_party/portaudio)
target_link_libraries(${LOWL_LIB_MAIN} PRIVATE portaudio_static)

# dr_lib
target_include_directories(${LOWL_LIB_MAIN} PRIVATE third_party/dr_libs)

# r8brain-free-src
add_compile_definitions(_USE_MATH_DEFINES)
target_include_directories(${LOWL_LIB_MAIN} PUBLIC third_party/r8brain-free-src)
target_sources(${LOWL_LIB_MAIN}
        PRIVATE
        third_party/r8brain-free-src/pffft.cpp
        third_party/r8brain-free-src/r8bbase.cpp
        )
# https://github.com/avaneev/r8brain-free-src#usage-information
#add_definitions(-DR8B_PFFFT=1)

add_subdirectory(demo)
add_subdirectory(test)