include_guard(GLOBAL)

function(ponder_enable_warnings target_name)
    if(NOT TARGET "${target_name}")
        message(FATAL_ERROR "Cannot enable warnings for missing target: ${target_name}")
    endif()

    get_target_property(ponder_target_imported "${target_name}" IMPORTED)
    if(ponder_target_imported)
        return()
    endif()

    if(MSVC)
        target_compile_options("${target_name}" PRIVATE
            /W4
            /WX
            /permissive-
            /Zc:__cplusplus
            /EHsc
            /GR)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options("${target_name}" PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Werror)
    endif()
endfunction()
