set(LOWL_DEMO_MAIN lowl_audio_app)
set(LOWL_DEMO_FILES
        main.cpp
        )


add_executable(${LOWL_DEMO_MAIN} ${LOWL_DEMO_FILES})

# lowl
target_link_libraries (${LOWL_DEMO_MAIN} PRIVATE ${LOWL_LIB_MAIN})