cmake_minimum_required(VERSION 3.31)
project(lowl_audio)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(LOWL_LIB_MAIN lowl_audio_lib)

message("[LOWL] Project: ${PROJECT_NAME}")
message("[LOWL] BuildType: ${CMAKE_BUILD_TYPE}")
message("[LOWL] CMAKE_CXX_COMPILER_ID: ${CMAKE_CXX_COMPILER_ID}")
message("[LOWL] CMAKE_SYSTEM_NAME: ${CMAKE_SYSTEM_NAME}")

# config - start
set(LOWL_DEBUG TRUE)
set(LOWL_DRIVER_DUMMY TRUE)
set(LOWL_DRIVER_CORE_AUDIO TRUE)
set(LOWL_DRIVER_WASAPI TRUE)
set(LOWL_TYPE_SAMPLE_64 FALSE)
# config - end

set(LOWL_SOURCES
        src/lowl.cpp
        src/lowl_buffer.cpp
        src/lowl_error.cpp
        src/lowl_file.cpp
        src/lowl_logger.cpp

        src/audio/lowl_audio_setting.cpp
        src/audio/lowl_audio_buffer.cpp
        src/audio/lowl_audio_utilities.cpp

        src/audio/backend/lowl_audio_device.cpp
        src/audio/backend/lowl_audio_driver.cpp

        src/audio/backend/coreaudio/lowl_audio_core_audio_driver.cpp
        src/audio/backend/coreaudio/lowl_audio_core_audio_device.cpp
        src/audio/backend/coreaudio/lowl_audio_core_audio_utilities.cpp

        src/audio/backend/dummy/lowl_audio_dummy_driver.cpp

        src/audio/backend/wasapi/lowl_audio_wasapi_driver.cpp
        src/audio/backend/wasapi/lowl_audio_wasapi_device.cpp
        src/audio/backend/wasapi/lowl_audio_wasapi_com.cpp

        src/audio/convert/lowl_audio_channel_converter.cpp
        src/audio/convert/lowl_audio_re_sampler_r8b.cpp

        src/audio/reader/lowl_audio_reader.cpp
        src/audio/reader/lowl_audio_reader_flac.cpp
        src/audio/reader/lowl_audio_reader_mp3.cpp
        src/audio/reader/lowl_audio_reader_ogg.cpp
        src/audio/reader/lowl_audio_reader_opus.cpp
        src/audio/reader/lowl_audio_reader_wav.cpp

        src/audio/source/lowl_audio_data.cpp
        src/audio/source/lowl_audio_mixer.cpp
        src/audio/source/lowl_audio_source.cpp
        src/audio/source/lowl_audio_space.cpp
        src/audio/source/lowl_audio_stream.cpp
        src/audio/source/lowl_audio_voice.cpp
)

add_library(${LOWL_LIB_MAIN} ${LOWL_SOURCES})
set_target_properties(${LOWL_LIB_MAIN} PROPERTIES PREFIX "")
target_include_directories(${LOWL_LIB_MAIN} PUBLIC src/)

IF (LOWL_DRIVER_DUMMY)
    target_compile_definitions(${LOWL_LIB_MAIN} PRIVATE LOWL_DRIVER_DUMMY)
ENDIF ()

if (WIN32)
    message("[LOWL] WIN32")
    IF (LOWL_DRIVER_WASAPI)
        target_compile_definitions(${LOWL_LIB_MAIN} PRIVATE LOWL_DRIVER_WASAPI)
        target_link_libraries(${LOWL_LIB_MAIN} PUBLIC avrt)
    ENDIF ()
endif ()
if (APPLE)
    if (CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
        message("[LOWL] APPLE arm64")
        set(CMAKE_OSX_ARCHITECTURES "arm64")
    else ()
        message("[LOWL] APPLE x86_64")
        set(CMAKE_OSX_ARCHITECTURES "x86_64")
    endif ()

    IF (LOWL_DRIVER_CORE_AUDIO)
        message("[LOWL] Resolving CoreAudio Frameworks")
        find_library(CORE_AUDIO CoreAudio REQUIRED)
        find_library(CORE_FOUNDATION CoreFoundation REQUIRED)
        find_library(AUDIO_UNIT AudioUnit REQUIRED)
        target_link_libraries(${LOWL_LIB_MAIN} PRIVATE
                ${CORE_AUDIO}
                ${CORE_FOUNDATION}
                ${AUDIO_UNIT}
        )
        target_compile_definitions(${LOWL_LIB_MAIN} PRIVATE LOWL_DRIVER_CORE_AUDIO)
    ENDIF ()
endif ()
if (UNIX)
    message("[LOWL] UNIX")
endif ()
if (MSVC)
    message("[LOWL] MSVC")
endif ()
if (MSYS)
    message("[LOWL] MSYS")
endif ()
if (MINGW)
    message("[LOWL] MINGW")
endif ()

IF (CMAKE_BUILD_TYPE MATCHES Debug)
    set(LOWL_DEBUG TRUE)
ENDIF ()


IF (LOWL_DEBUG)
    message("[LOWL] Debug")
    target_compile_definitions(${LOWL_LIB_MAIN} PUBLIC LOWL_DEBUG)
ELSE ()
    message("[LOWL] Release")
ENDIF ()

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message("[LOWL] Clang")
    IF (LOWL_DEBUG)
        message("[LOWL] Setting Debug Flags")
        target_compile_options(${LOWL_LIB_MAIN} PRIVATE
                -O0
                -Werror
        )
    ELSE ()
        message("[LOWL] Setting Release Flags")
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


# Doxygen
# look for Doxygen package
find_package(Doxygen)
if (DOXYGEN_FOUND)
    # set input and output files
    set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
    set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile.out)

    # request to configure the file
    configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)
    message("Doxygen build started")

    # Note: do not put "ALL" - this builds docs together with application EVERY TIME!
    add_custom_target( docs
            COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Generating API documentation with Doxygen"
            VERBATIM )
else (DOXYGEN_FOUND)
    message("Doxygen need to be installed to generate the doxygen documentation")
endif (DOXYGEN_FOUND)
