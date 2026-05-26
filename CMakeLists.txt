CMAKE_MINIMUM_REQUIRED(VERSION 3.1...3.26)

project(atracdenc)
include_directories(src)

enable_testing()

find_package(GTest)
if (NOT GTest_FOUND)
    message("gtest library has not been found, skipping ut...")
endif()

set(ATRACDENC_PYTHON_EXECUTABLE "")
if (NOT CMAKE_VERSION VERSION_LESS "3.12")
    find_package(Python3 QUIET COMPONENTS Interpreter)
    if (Python3_Interpreter_FOUND)
        set(ATRACDENC_PYTHON_EXECUTABLE "${Python3_EXECUTABLE}")
    endif()
endif()

if (NOT ATRACDENC_PYTHON_EXECUTABLE)
    if (POLICY CMP0148)
        cmake_policy(PUSH)
        cmake_policy(SET CMP0148 OLD)
    endif()
    find_package(PythonInterp 3 QUIET)
    if (POLICY CMP0148)
        cmake_policy(POP)
    endif()
    if (PYTHONINTERP_FOUND)
        set(ATRACDENC_PYTHON_EXECUTABLE "${PYTHON_EXECUTABLE}")
    endif()
endif()

if (NOT ATRACDENC_PYTHON_EXECUTABLE)
    message("python3 has not been found, skipping integration tests...")
endif()

# build universal binary on macos
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
endif()

if (UNIX)
    if (NOT DEFINED CMAKE_INSTALL_MANDIR)
        set(CMAKE_INSTALL_MANDIR ${CMAKE_INSTALL_PREFIX}/share/man)
    endif()
    add_custom_target(man-atracdenc
        ALL
        DEPENDS atracdenc.1.gz
    )
    add_custom_command(
        OUTPUT atracdenc.1.gz
        COMMAND mkdir -p man && gzip --no-name --to-stdout ${CMAKE_CURRENT_SOURCE_DIR}/man/atracdenc.1 >"${CMAKE_CURRENT_BINARY_DIR}/man/atracdenc.1.gz"
        MAIN_DEPENDENCY man/atracdenc.1
    )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/man/atracdenc.1.gz DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
endif()

add_subdirectory(src)

if (ATRACDENC_PYTHON_EXECUTABLE)
    function(add_input_file_test test_name test_case)
        add_test(NAME ${test_name}
            COMMAND ${ATRACDENC_PYTHON_EXECUTABLE}
                ${CMAKE_SOURCE_DIR}/test/integration/input_file_tests.py
                --exe $<TARGET_FILE:atracdenc>
                --work-dir ${CMAKE_BINARY_DIR}/integration/${test_name}
                --case ${test_case}
        )
    endfunction()

    add_input_file_test(input_missing_file missing-input)
    add_input_file_test(input_utf8_filename utf8-input)
    add_input_file_test(input_utf8_atrac1_filename utf8-input-atrac1)
    add_input_file_test(output_utf8_oma_filename utf8-output-oma)
    add_input_file_test(output_utf8_at3_filename utf8-output-at3)
    add_input_file_test(output_utf8_rm_filename utf8-output-rm)
    add_input_file_test(output_utf8_aea_filename utf8-output-aea)
    add_input_file_test(decode_utf8_input_filename utf8-decode-input)
    add_input_file_test(decode_utf8_output_filename utf8-decode-output)
    add_input_file_test(explicit_container explicit-container)
endif()

if (GTest_FOUND)
    add_subdirectory(test)
endif()
