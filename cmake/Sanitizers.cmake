include_guard(GLOBAL)

function(ponder_enable_sanitizers target_name)
    if(NOT TARGET "${target_name}")
        message(FATAL_ERROR "Cannot enable sanitizers for missing target: ${target_name}")
    endif()

    get_target_property(ponder_target_imported "${target_name}" IMPORTED)
    if(ponder_target_imported)
        return()
    endif()

    set(ponder_sanitizers)

    if(PONDER_ENABLE_ASAN)
        list(APPEND ponder_sanitizers address)
    endif()

    if(PONDER_ENABLE_UBSAN)
        list(APPEND ponder_sanitizers undefined)
    endif()

    if(NOT ponder_sanitizers)
        return()
    endif()

    if(MSVC)
        message(WARNING "Sanitizers are not configured for MSVC builds yet.")
        return()
    endif()

    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        message(WARNING
            "Sanitizers requested for unsupported compiler: ${CMAKE_CXX_COMPILER_ID}")
        return()
    endif()

    list(JOIN ponder_sanitizers "," ponder_sanitizer_list)
    target_compile_options("${target_name}" PRIVATE "-fsanitize=${ponder_sanitizer_list}")
    target_link_options("${target_name}" PRIVATE "-fsanitize=${ponder_sanitizer_list}")
endfunction()
