cmake_minimum_required(VERSION 3.20)

project(jarvis_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(JARVIS_ENABLE_QT_GUI "Build Qt Widgets GUI when Qt6 is available" ON)

add_executable(jarvis
    src/main.cpp
    src/app/application.cpp
    src/config/app_config.cpp
    src/core/http_client.cpp
    src/core/providers.cpp
    src/platform/platform_services.cpp
    src/runtime/runtime.cpp
    src/tools/tool_registry.cpp
    src/ui/asset_catalog.cpp
    src/util/common.cpp
    src/util/json.cpp
)

target_include_directories(jarvis PRIVATE src)

if (WIN32)
    target_compile_definitions(jarvis PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(jarvis PRIVATE Crypt32)
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_link_options(jarvis PRIVATE -static -static-libgcc -static-libstdc++)
    endif()
endif()

set_target_properties(jarvis PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

if (JARVIS_ENABLE_QT_GUI)
    find_package(Qt6 COMPONENTS Widgets Concurrent QUIET)
    if (Qt6_FOUND)
        target_sources(jarvis PRIVATE
            src/gui/main_window.cpp
            src/gui/orb_widget.cpp
        )
        set_target_properties(jarvis PROPERTIES AUTOMOC ON)
        if (WIN32)
            set_target_properties(jarvis PROPERTIES WIN32_EXECUTABLE ON)
        endif()
        target_link_libraries(jarvis PRIVATE Qt6::Widgets Qt6::Concurrent)
        target_compile_definitions(jarvis PRIVATE JARVIS_HAS_QT=1)
        message(STATUS "Qt6 bulundu. GUI hedefe eklendi.")
    else()
        message(STATUS "Qt6 bulunamadi. CLI modu ile devam ediliyor.")
    endif()
endif()
