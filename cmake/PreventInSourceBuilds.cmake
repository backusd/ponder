include_guard(GLOBAL)

function(ponder_prevent_in_source_builds)
    if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
        message(FATAL_ERROR
            "In-source builds are not supported. "
            "Configure ponder from a separate build directory.")
    endif()
endfunction()
