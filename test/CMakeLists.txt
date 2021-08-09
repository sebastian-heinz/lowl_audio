set(LOWL_TEST_MAIN lowl_audio_test)
set(LOWL_TEST_FILES
        main.cpp
        test_driver.cpp
        )

add_executable(${LOWL_TEST_MAIN} ${LOWL_TEST_FILES})

#set_target_properties(${LOWL_TEST_MAIN} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})

# lowl
target_link_libraries(${LOWL_TEST_MAIN} PRIVATE ${LOWL_LIB_MAIN})

# doctest
add_subdirectory(../third_party/doctest ../third_party/doctest)
target_link_libraries(${LOWL_TEST_MAIN} PRIVATE doctest)
if(MINGW)
    # MinGw - requires following dlls
    # - libstdc++-6.dll
    # - libgcc_s_seh-1.dll
    # - libwinpthread-1.dll
    # This links them statically
    set(CMAKE_CXX_STANDARD_LIBRARIES "-static-libgcc -static-libstdc++ -lwsock32 -lws2_32 ${CMAKE_CSS_STANDARD_LIBRARIES}")
    set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive")
endif()