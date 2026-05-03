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
    add_test(NAME input_missing_file
        COMMAND ${ATRACDENC_PYTHON_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/test/integration/input_file_tests.py
            --exe $<TARGET_FILE:atracdenc>
            --work-dir ${CMAKE_BINARY_DIR}/integration/input_missing_file
            --case missing-input
    )
    add_test(NAME input_utf8_filename
        COMMAND ${ATRACDENC_PYTHON_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/test/integration/input_file_tests.py
            --exe $<TARGET_FILE:atracdenc>
            --work-dir ${CMAKE_BINARY_DIR}/integration/input_utf8_filename
            --case utf8-input
    )
    add_test(NAME output_utf8_oma_filename
        COMMAND ${ATRACDENC_PYTHON_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/test/integration/input_file_tests.py
            --exe $<TARGET_FILE:atracdenc>
            --work-dir ${CMAKE_BINARY_DIR}/integration/output_utf8_oma_filename
            --case utf8-output-oma
    )
    add_test(NAME output_utf8_at3_filename
        COMMAND ${ATRACDENC_PYTHON_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/test/integration/input_file_tests.py
            --exe $<TARGET_FILE:atracdenc>
            --work-dir ${CMAKE_BINARY_DIR}/integration/output_utf8_at3_filename
            --case utf8-output-at3
    )
    add_test(NAME output_utf8_rm_filename
        COMMAND ${ATRACDENC_PYTHON_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/test/integration/input_file_tests.py
            --exe $<TARGET_FILE:atracdenc>
            --work-dir ${CMAKE_BINARY_DIR}/integration/output_utf8_rm_filename
            --case utf8-output-rm
    )
endif()

if (GTest_FOUND)
    add_subdirectory(test)
endif()
