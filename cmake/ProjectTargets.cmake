include_guard(GLOBAL)

include(CompilerWarnings)
include(GoogleTest)
include(InstallRules)
include(PrecompiledHeaders)
include(Sanitizers)

function(ponder_add_project_library library_name)
    set(options)
    set(one_value_args PCH_HEADER)
    set(multi_value_args
        PUBLIC_HEADERS
        PRIVATE_HEADERS
        SOURCES
        PUBLIC_DEPS
        PRIVATE_DEPS
        INSTALL_PRIVATE_DEPS)

    cmake_parse_arguments(
        PONDER_LIBRARY
        "${options}"
        "${one_value_args}"
        "${multi_value_args}"
        ${ARGN})

    set(target_name "ponder_${library_name}")
    set(alias_name "ponder::${library_name}")

    if(TARGET "${target_name}")
        message(FATAL_ERROR "Project library target already exists: ${target_name}")
    endif()

    add_library("${target_name}" STATIC)
    add_library("${alias_name}" ALIAS "${target_name}")

    target_sources("${target_name}"
        PRIVATE
            ${PONDER_LIBRARY_SOURCES}
            ${PONDER_LIBRARY_PRIVATE_HEADERS}
        PUBLIC
            FILE_SET HEADERS
            BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/include"
            FILES ${PONDER_LIBRARY_PUBLIC_HEADERS})

    target_compile_features("${target_name}" PUBLIC cxx_std_23)

    target_include_directories("${target_name}" PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:include>")

    if(PONDER_LIBRARY_PUBLIC_DEPS)
        target_link_libraries("${target_name}" PUBLIC ${PONDER_LIBRARY_PUBLIC_DEPS})
    endif()

    if(PONDER_LIBRARY_PRIVATE_DEPS)
        foreach(private_dep IN LISTS PONDER_LIBRARY_PRIVATE_DEPS)
            target_link_libraries("${target_name}" PRIVATE "$<BUILD_INTERFACE:${private_dep}>")
        endforeach()
    endif()

    if(PONDER_LIBRARY_INSTALL_PRIVATE_DEPS)
        foreach(private_dep IN LISTS PONDER_LIBRARY_INSTALL_PRIVATE_DEPS)
            target_link_libraries("${target_name}" PRIVATE "$<INSTALL_INTERFACE:${private_dep}>")
        endforeach()
    endif()

    set_target_properties("${target_name}" PROPERTIES
        EXPORT_NAME "${library_name}"
        FOLDER "libs"
        POSITION_INDEPENDENT_CODE ON)

    if(PONDER_LIBRARY_PCH_HEADER)
        if(IS_ABSOLUTE "${PONDER_LIBRARY_PCH_HEADER}")
            set(pch_header "${PONDER_LIBRARY_PCH_HEADER}")
        else()
            set(pch_header
                "${CMAKE_CURRENT_SOURCE_DIR}/${PONDER_LIBRARY_PCH_HEADER}")
        endif()
        ponder_enable_precompiled_headers("${target_name}" "${pch_header}")
    endif()

    ponder_enable_warnings("${target_name}")
    ponder_enable_sanitizers("${target_name}")
    ponder_install_target("${target_name}")
    ponder_install_public_headers("${CMAKE_CURRENT_SOURCE_DIR}/include")
endfunction()
function(ponder_add_project_executable target_name)
    set(options NO_INSTALL)
    set(one_value_args OUTPUT_NAME FOLDER PCH_HEADER)
    set(multi_value_args SOURCES PRIVATE_HEADERS PRIVATE_DEPS)

    cmake_parse_arguments(
        PONDER_EXECUTABLE
        "${options}"
        "${one_value_args}"
        "${multi_value_args}"
        ${ARGN})

    if(TARGET "${target_name}")
        message(FATAL_ERROR "Project executable target already exists: ${target_name}")
    endif()

    add_executable("${target_name}"
        ${PONDER_EXECUTABLE_SOURCES}
        ${PONDER_EXECUTABLE_PRIVATE_HEADERS})

    target_compile_features("${target_name}" PRIVATE cxx_std_23)

    if(PONDER_EXECUTABLE_PRIVATE_DEPS)
        target_link_libraries("${target_name}" PRIVATE ${PONDER_EXECUTABLE_PRIVATE_DEPS})
    endif()

    if(PONDER_EXECUTABLE_OUTPUT_NAME)
        set_target_properties("${target_name}" PROPERTIES
            OUTPUT_NAME "${PONDER_EXECUTABLE_OUTPUT_NAME}")
    endif()

    if(PONDER_EXECUTABLE_FOLDER)
        set_target_properties("${target_name}" PROPERTIES
            FOLDER "${PONDER_EXECUTABLE_FOLDER}")
    endif()

    if(PONDER_EXECUTABLE_PCH_HEADER)
        if(IS_ABSOLUTE "${PONDER_EXECUTABLE_PCH_HEADER}")
            set(pch_header "${PONDER_EXECUTABLE_PCH_HEADER}")
        else()
            set(pch_header
                "${CMAKE_CURRENT_SOURCE_DIR}/${PONDER_EXECUTABLE_PCH_HEADER}")
        endif()
        ponder_enable_precompiled_headers("${target_name}" "${pch_header}")
    endif()

    ponder_enable_warnings("${target_name}")
    ponder_enable_sanitizers("${target_name}")
    if(NOT PONDER_EXECUTABLE_NO_INSTALL)
        ponder_install_target("${target_name}")
    endif()
endfunction()
function(ponder_add_project_test target_name)
    set(options)
    set(one_value_args RESOURCE_LOCK TIMEOUT)
    set(multi_value_args SOURCES PRIVATE_DEPS LABELS)

    cmake_parse_arguments(
        PONDER_TEST
        "${options}"
        "${one_value_args}"
        "${multi_value_args}"
        ${ARGN})

    if(TARGET "${target_name}")
        message(FATAL_ERROR "Project test target already exists: ${target_name}")
    endif()

    add_executable("${target_name}" ${PONDER_TEST_SOURCES})
    target_compile_features("${target_name}" PRIVATE cxx_std_23)
    target_link_libraries("${target_name}" PRIVATE
        ${PONDER_TEST_PRIVATE_DEPS}
        ponder::gtest_main)

    set_target_properties("${target_name}" PROPERTIES
        FOLDER "tests")

    ponder_enable_warnings("${target_name}")
    ponder_enable_sanitizers("${target_name}")

    set(discovery_arguments DISCOVERY_TIMEOUT 30)
    set(test_properties)
    if(PONDER_TEST_RESOURCE_LOCK)
        list(APPEND test_properties RESOURCE_LOCK "${PONDER_TEST_RESOURCE_LOCK}")
    endif()
    if(PONDER_TEST_TIMEOUT)
        list(APPEND test_properties TIMEOUT "${PONDER_TEST_TIMEOUT}")
    endif()
    if(test_properties)
        list(APPEND discovery_arguments PROPERTIES ${test_properties})
    endif()

    gtest_discover_tests("${target_name}" ${discovery_arguments})

    if(PONDER_TEST_LABELS)
        list(JOIN PONDER_TEST_LABELS ";" test_labels)
        set(label_script "${CMAKE_CURRENT_BINARY_DIR}/${target_name}_labels.cmake")
        file(WRITE "${label_script}"
            "if(DEFINED ${target_name}_TESTS)\n"
            "    set_tests_properties(\${${target_name}_TESTS} PROPERTIES LABELS \"${test_labels}\")\n"
            "endif()\n")
        set_property(DIRECTORY APPEND PROPERTY TEST_INCLUDE_FILES "${label_script}")
    endif()
endfunction()
