# Emotiv LSL C++ Application
# Streams EEG, motion, and electrode quality data from Emotiv EPOC X headset via LSL.

add_executable(emotiv_lsl
    main.cpp
    emotiv_base.cpp
    emotiv_epoc_x.cpp
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
    target_link_libraries(emotiv_lsl PRIVATE hidapi_winapi LSL::lsl)

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
elseif(APPLE)
    target_link_libraries(emotiv_lsl PRIVATE hidapi_darwin LSL::lsl)
else()
    target_link_libraries(emotiv_lsl PRIVATE hidapi_libusb LSL::lsl)
endif()
