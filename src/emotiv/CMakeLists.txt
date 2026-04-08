# Emotiv LSL C++ Application
# Streams EEG, motion, and electrode quality data from Emotiv EPOC X headset via LSL.

add_executable(emotiv_lsl
    main.cpp
    lab_recorder_cfg.cpp
    emotiv_base.cpp
    emotiv_epoc_x.cpp
    emotiv_lsl_log_config.cpp
    "${tiny_aes_SOURCE_DIR}/aes.c"
)

target_include_directories(emotiv_lsl PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${tiny_aes_SOURCE_DIR}
)

# tiny-AES-c options: only ECB mode is required
target_compile_definitions(emotiv_lsl PRIVATE
    CBC=0
    CTR=0
    ECB=1
)

if(WIN32)
    target_link_libraries(emotiv_lsl PRIVATE hidapi_winapi LSL::lsl xdfwriter LSLTemplate::core shell32)

    add_custom_command(TARGET emotiv_lsl POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:hidapi_winapi>
            $<TARGET_FILE_DIR:emotiv_lsl>
        COMMAND_EXPAND_LISTS
        COMMENT "Copying hidapi DLL for emotiv_lsl"
    )
    add_custom_command(TARGET emotiv_lsl POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:LSL::lsl>
            $<TARGET_FILE_DIR:emotiv_lsl>
        COMMAND_EXPAND_LISTS
        COMMENT "Copying lsl DLL for emotiv_lsl"
    )
    add_custom_command(TARGET emotiv_lsl POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:xdfwriter>
            $<TARGET_FILE_DIR:emotiv_lsl>
        COMMAND_EXPAND_LISTS
        COMMENT "Copying xdfwriter DLL for emotiv_lsl"
    )
elseif(APPLE)
    target_link_libraries(emotiv_lsl PRIVATE hidapi_darwin LSL::lsl xdfwriter LSLTemplate::core)
else()
    target_link_libraries(emotiv_lsl PRIVATE hidapi_libusb LSL::lsl xdfwriter LSLTemplate::core)
endif()

add_custom_command(TARGET emotiv_lsl POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/lsl_api.cfg"
        $<TARGET_FILE_DIR:emotiv_lsl>
    COMMENT "Copying lsl_api.cfg (quiet liblsl logging) for emotiv_lsl"
)
