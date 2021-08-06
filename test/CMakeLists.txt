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
