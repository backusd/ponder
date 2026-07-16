cmake_minimum_required(VERSION 3.28)

project(PonderUiInstallConsumer LANGUAGES CXX)

include(CTest)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT DEFINED PONDER_EXPECT_UI_EXPERIMENTAL)
    message(FATAL_ERROR "PONDER_EXPECT_UI_EXPERIMENTAL is required.")
endif()

find_package(ponder CONFIG REQUIRED)

function(ponder_configure_ui_install_consumer target_name)
    target_compile_features("${target_name}" PRIVATE cxx_std_23)
    set_target_properties("${target_name}" PROPERTIES DISABLE_PRECOMPILE_HEADERS ON)

    if(MSVC)
        target_compile_options("${target_name}" PRIVATE /W4 /WX /permissive-)
    else()
        target_compile_options("${target_name}" PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endfunction()

add_executable(ponder_ui_core_install_consumer Main.cpp)
target_link_libraries(ponder_ui_core_install_consumer PRIVATE ponder::ui)
ponder_configure_ui_install_consumer(ponder_ui_core_install_consumer)

add_library(ponder_ui_core_install_header_tests OBJECT
    HeaderColor.cpp
    HeaderError.cpp
    HeaderGeometry.cpp
    HeaderLibrary.cpp
    HeaderLimits.cpp
    HeaderMetrics.cpp
    HeaderOutcome.cpp)
target_link_libraries(ponder_ui_core_install_header_tests PRIVATE ponder::ui)
ponder_configure_ui_install_consumer(ponder_ui_core_install_header_tests)

if(PONDER_EXPECT_UI_EXPERIMENTAL)
    if(NOT TARGET ponder::ui_experimental)
        message(FATAL_ERROR "The installed package is missing ponder::ui_experimental.")
    endif()

    add_executable(ponder_ui_experimental_install_consumer ExperimentalMain.cpp)
    target_link_libraries(ponder_ui_experimental_install_consumer PRIVATE
        ponder::ui_experimental)
    ponder_configure_ui_install_consumer(ponder_ui_experimental_install_consumer)

    add_library(ponder_ui_experimental_install_header_tests OBJECT
        HeaderExperimentalRectangleRenderer.cpp)
    target_link_libraries(ponder_ui_experimental_install_header_tests PRIVATE
        ponder::ui_experimental)
    ponder_configure_ui_install_consumer(ponder_ui_experimental_install_header_tests)
elseif(TARGET ponder::ui_experimental)
    message(FATAL_ERROR "The render-independent package unexpectedly exports ui_experimental.")
endif()

add_test(NAME ponder_ui_core_install_consumer_runs
    COMMAND ponder_ui_core_install_consumer)
if(PONDER_EXPECT_UI_EXPERIMENTAL)
    add_test(NAME ponder_ui_experimental_install_consumer_runs
        COMMAND ponder_ui_experimental_install_consumer)
endif()
