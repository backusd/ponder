include_guard(GLOBAL)

function(ponder_enable_precompiled_headers target_name)
    if(NOT PONDER_ENABLE_PCH)
        return()
    endif()

    if(NOT TARGET "${target_name}")
        message(FATAL_ERROR
            "Cannot enable precompiled headers for missing target: ${target_name}")
    endif()

    if(NOT ARGN)
        message(FATAL_ERROR
            "ponder_enable_precompiled_headers requires at least one header.")
    endif()

    target_precompile_headers("${target_name}" PRIVATE ${ARGN})
endfunction()
