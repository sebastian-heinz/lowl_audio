# Install script for directory: /Users/shiba/dev/godot/modules/gd_lowl_audio/lowl_audio/third_party/readerwriterqueue

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Debug")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set path to fallback-tool for dependency-resolution.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/usr/bin/objdump")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/readerwriterqueue" TYPE FILE FILES
    "/Users/shiba/dev/godot/modules/gd_lowl_audio/lowl_audio/third_party/readerwriterqueue/atomicops.h"
    "/Users/shiba/dev/godot/modules/gd_lowl_audio/lowl_audio/third_party/readerwriterqueue/readerwriterqueue.h"
    "/Users/shiba/dev/godot/modules/gd_lowl_audio/lowl_audio/third_party/readerwriterqueue/readerwritercircularbuffer.h"
    "/Users/shiba/dev/godot/modules/gd_lowl_audio/lowl_audio/third_party/readerwriterqueue/LICENSE.md"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Devel" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/readerwriterqueue/readerwriterqueueTargets.cmake")
    file(DIFFERENT _cmake_export_file_changed FILES
         "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/readerwriterqueue/readerwriterqueueTargets.cmake"
         "/Users/shiba/dev/godot/modules/gd_lowl_audio/lowl_audio/build-debug/third_party/readerwriterqueue/CMakeFiles/Export/8994e3f4f61a5badde4ac84576fbfa78/readerwriterqueueTargets.cmake")
    if(_cmake_export_file_changed)
      file(GLOB _cmake_old_config_files "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/readerwriterqueue/readerwriterqueueTargets-*.cmake")
      if(_cmake_old_config_files)
        string(REPLACE ";" ", " _cmake_old_config_files_text "${_cmake_old_config_files}")
        message(STATUS "Old export file \"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/readerwriterqueue/readerwriterqueueTargets.cmake\" will be replaced.  Removing files [${_cmake_old_config_files_text}].")
        unset(_cmake_old_config_files_text)
        file(REMOVE ${_cmake_old_config_files})
      endif()
      unset(_cmake_old_config_files)
    endif()
    unset(_cmake_export_file_changed)
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/readerwriterqueue" TYPE FILE FILES "/Users/shiba/dev/godot/modules/gd_lowl_audio/lowl_audio/build-debug/third_party/readerwriterqueue/CMakeFiles/Export/8994e3f4f61a5badde4ac84576fbfa78/readerwriterqueueTargets.cmake")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Devel" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/readerwriterqueue" TYPE FILE FILES
    "/Users/shiba/dev/godot/modules/gd_lowl_audio/lowl_audio/build-debug/third_party/readerwriterqueue/readerwriterqueueConfig.cmake"
    "/Users/shiba/dev/godot/modules/gd_lowl_audio/lowl_audio/build-debug/third_party/readerwriterqueue/readerwriterqueueConfigVersion.cmake"
    )
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/Users/shiba/dev/godot/modules/gd_lowl_audio/lowl_audio/build-debug/third_party/readerwriterqueue/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
