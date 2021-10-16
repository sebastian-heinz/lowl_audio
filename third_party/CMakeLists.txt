cmake_minimum_required(VERSION 3.9)

set(LOWL_THIRD_PARTY lowl_third_party)
project(${LOWL_THIRD_PARTY} VERSION 1.0.0)

set(LOWL_THIRD_PARTY_SOURCES
        src/lowl_dr_imp.cpp
        )
add_library(${LOWL_THIRD_PARTY} STATIC ${LOWL_THIRD_PARTY_SOURCES})
target_include_directories(${LOWL_THIRD_PARTY} PUBLIC include/)

# readerwriterqueue
add_subdirectory(readerwriterqueue)
target_link_libraries(${LOWL_THIRD_PARTY} PUBLIC readerwriterqueue)

# concurrentqueue
add_subdirectory(concurrentqueue)
target_link_libraries(${LOWL_THIRD_PARTY} PUBLIC concurrentqueue)

# portaudio
add_subdirectory(portaudio)
target_link_libraries(${LOWL_THIRD_PARTY} PUBLIC portaudio_static)

#dr_libs
set(LOWL_LIB_DR_LIBS lowl_dr_libs_lib)
add_library(${LOWL_LIB_DR_LIBS} INTERFACE)
target_include_directories(${LOWL_LIB_DR_LIBS} INTERFACE dr_libs)
target_link_libraries(${LOWL_THIRD_PARTY} PUBLIC ${LOWL_LIB_DR_LIBS})

# r8brain-free-src
set(LOWL_LIB_R8BRAIN lowl_r8brain_lib)
set(LOWL_LIB_R8BRAIN_SOURCES
        r8brain-free-src/pffft.cpp
        r8brain-free-src/r8bbase.cpp
        )
add_library(${LOWL_LIB_R8BRAIN} STATIC ${LOWL_LIB_R8BRAIN_SOURCES})
target_include_directories(${LOWL_LIB_R8BRAIN} SYSTEM INTERFACE r8brain-free-src) # SYSTEM to suppress warnings
target_compile_definitions(${LOWL_LIB_R8BRAIN} PRIVATE _USE_MATH_DEFINES)
target_link_libraries(${LOWL_THIRD_PARTY} PUBLIC ${LOWL_LIB_R8BRAIN})