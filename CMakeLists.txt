cmake_minimum_required(VERSION 3.9)
project(lowl_audio)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(LOWL_LIB_MAIN lowl_audio_lib)

message([LOWL] Project: ${PROJECT_NAME})
message([LOWL] BuildType: ${CMAKE_BUILD_TYPE})
message([LOWL] CMAKE_CXX_COMPILER_ID: ${CMAKE_CXX_COMPILER_ID})
message([LOWL] CMAKE_SYSTEM_NAME: ${CMAKE_SYSTEM_NAME})

set(LOWL_PA_DEFAULT_DRIVER_PRIORITY "ALSA,OSS,JACK,COREAUDIO,WASAPI,WDMKS,DSOUND,WMME,ASIO,ASIHPI")

set(LOWL_SOURCES
        src/lowl.cpp
        src/lowl_buffer.cpp
        src/lowl_error.cpp
        src/lowl_file.cpp
        src/lowl_logger.cpp

        src/audio/lowl_audio_data.cpp
        src/audio/lowl_audio_device.cpp
        src/audio/lowl_audio_driver.cpp
        src/audio/lowl_audio_mixer.cpp
        src/audio/lowl_audio_source.cpp
        src/audio/lowl_audio_space.cpp
        src/audio/lowl_audio_stream.cpp
        src/audio/lowl_audio_utilities.cpp

        src/audio/convert/lowl_audio_channel_converter.cpp
        src/audio/convert/lowl_audio_re_sampler.cpp
        src/audio/convert/lowl_audio_sample_converter.cpp

        src/audio/coreaudio/lowl_audio_core_audio_driver.cpp
        src/audio/coreaudio/lowl_audio_core_audio_device.cpp
        src/audio/coreaudio/lowl_audio_core_audio_utilities.cpp

        src/audio/dummy/lowl_audio_dummy_driver.cpp

        src/audio/portaudio/lowl_audio_pa_driver.cpp
        src/audio/portaudio/lowl_audio_pa_device.cpp

        src/audio/reader/lowl_audio_reader.cpp
        src/audio/reader/lowl_audio_reader_flac.cpp
        src/audio/reader/lowl_audio_reader_mp3.cpp
        src/audio/reader/lowl_audio_reader_wav.cpp
        )

add_library(${LOWL_LIB_MAIN} ${LOWL_SOURCES})
set_target_properties(${LOWL_LIB_MAIN} PROPERTIES PREFIX "")
target_include_directories(${LOWL_LIB_MAIN} PUBLIC src/)

target_compile_definitions(${LOWL_LIB_MAIN} PRIVATE LOWL_DRIVER_DUMMY)
target_compile_definitions(${LOWL_LIB_MAIN} PRIVATE LOWL_DRIVER_PORTAUDIO)
target_compile_definitions(${LOWL_LIB_MAIN} PRIVATE "LOWL_PA_DEFAULT_DRIVER_PRIORITY=\"${LOWL_PA_DEFAULT_DRIVER_PRIORITY}\"")

if (WIN32)
    message([LOWL] WIN32)
endif ()
if (APPLE)
    message([LOWL] APPLE)
    target_compile_definitions(${LOWL_LIB_MAIN} PRIVATE LOWL_DRIVER_CORE_AUDIO)
endif ()
if (UNIX)
    message([LOWL] UNIX)
endif ()
if (MSVC)
    message([LOWL] MSVC)
endif ()
if (MSYS)
    message([LOWL] MSYS)
endif ()
if (MINGW)
    message([LOWL] MINGW)
endif ()
IF (CMAKE_BUILD_TYPE MATCHES Debug)
    message([LOWL] Debug)
    set(LOWL_DEBUG TRUE)
    target_compile_definitions(${LOWL_LIB_MAIN} PUBLIC LOWL_DEBUG)
ENDIF()


if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message([LOWL] Clang)
    IF (LOWL_DEBUG)
        message([LOWL] Setting Debug Flags)
        target_compile_options(${LOWL_LIB_MAIN} PRIVATE
                -O0
                -Werror
                )
    ELSE()
        message([LOWL] Setting Release Flags)
        target_compile_options(${LOWL_LIB_MAIN} PRIVATE
                -O2
                -Werror
                -D_FORTIFY_SOURCE=2
                )
    ENDIF ()
    target_compile_options(${LOWL_LIB_MAIN} PRIVATE
            -Walloca
            -Wcast-qual
            -Wconversion
            -Wformat=2
            -Wformat-security
            -Wnull-dereference
            -Wstack-protector
            -Wvla
            -Warray-bounds
            -Warray-bounds-pointer-arithmetic
            -Wassign-enum
            -Wbad-function-cast
            -Wconditional-uninitialized
            -Wconversion
            -Wfloat-equal
            -Wformat-type-confusion
            -Widiomatic-parentheses
            -Wimplicit-fallthrough
            -Wloop-analysis
            -Wpointer-arith
            -Wshift-sign-overflow
            -Wshorten-64-to-32
            -Wswitch-enum
            -Wtautological-constant-in-range-compare
            -Wunreachable-code-aggressive
            -Wthread-safety
            -Wthread-safety-beta
            -Wcomma
            )
    target_link_options(${LOWL_LIB_MAIN} PRIVATE
            LINKER:-z,relro
            LINKER:-z,now
            LINKER:-z,noexecstack
            LINKER:-z,separate-code
            )
endif ()

# Third Party
add_subdirectory(third_party)
target_link_libraries(${LOWL_LIB_MAIN} PUBLIC lowl_third_party)

# Unit Test
add_subdirectory(test)

# Example App
add_subdirectory(demo)
