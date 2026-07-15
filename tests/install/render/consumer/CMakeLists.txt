cmake_minimum_required(VERSION 3.28)

project(PonderRenderInstallConsumer LANGUAGES CXX)

include(CTest)

find_package(ponder CONFIG REQUIRED)

add_executable(ponder_render_install_consumer Main.cpp)
target_compile_features(ponder_render_install_consumer PRIVATE cxx_std_23)
target_link_libraries(ponder_render_install_consumer PRIVATE
    ponder::core
    ponder::platform
    ponder::render)

if(MSVC)
    target_compile_options(ponder_render_install_consumer PRIVATE /W4 /WX /permissive-)
else()
    target_compile_options(ponder_render_install_consumer PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_test(NAME ponder_render_install_consumer_runs
    COMMAND ponder_render_install_consumer)
