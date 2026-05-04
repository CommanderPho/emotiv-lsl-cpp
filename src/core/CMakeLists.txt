# Core library - Qt-independent, shared between CLI and GUI
add_library(lsltemplate_core STATIC
    src/Device.cpp
    src/LSLOutlet.cpp
    src/Config.cpp
    src/StreamThread.cpp
    src/recording.cpp
)

target_include_directories(lsltemplate_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(lsltemplate_core
    PUBLIC
        LSL::lsl
        Threads::Threads
        xdfwriter
)

# Alias for consistent naming
add_library(LSLTemplate::core ALIAS lsltemplate_core)
