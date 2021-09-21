cmake_minimum_required(VERSION 3.6)
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
        src/lowl_audio_device.cpp
        src/lowl_audio_channel_converter.cpp
        src/lowl_audio_driver.cpp
        src/lowl_audio_driver_dummy.cpp
        src/lowl_audio_device_pa.cpp
        src/lowl_audio_driver_pa.cpp
        src/lowl_error.cpp
        src/lowl_file.cpp
        src/lowl_logger.cpp
        src/lowl_re_sampler.cpp
        src/lowl_sample_converter.cpp
        src/lowl_audio_space.cpp
        )

add_library(${LOWL_LIB_MAIN} ${LOWL_SOURCES})

if (WIN32)
    message([LOWL] WIN32)
endif ()
if (APPLE)
    message([LOWL] APPLE)
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
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message([LOWL] Clang)
    #set_source_files_properties(${LOWL_SOURCES} PROPERTIES COMPILE_OPTIONS -Werror)
    target_compile_options(${LOWL_LIB_MAIN} PRIVATE
            -O2
            #-Werror
            -D_FORTIFY_SOURCE=2
            )
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

IF (CMAKE_BUILD_TYPE MATCHES Debug)
    message([LOWL] Debug)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0")
ENDIF ()

set_target_properties(${LOWL_LIB_MAIN} PROPERTIES PREFIX "")
target_include_directories(${LOWL_LIB_MAIN} PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>" "$<INSTALL_INTERFACE:include>")
target_compile_definitions(${LOWL_LIB_MAIN} PRIVATE LOWL_DRIVER_DUMMY)
target_compile_definitions(${LOWL_LIB_MAIN} PRIVATE LOWL_DRIVER_PORTAUDIO)
target_compile_definitions(${LOWL_LIB_MAIN} PRIVATE "LOWL_PA_DEFAULT_DRIVER_PRIORITY=\"${LOWL_PA_DEFAULT_DRIVER_PRIORITY}\"")

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