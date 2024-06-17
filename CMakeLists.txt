CMAKE_MINIMUM_REQUIRED(VERSION 3.1)

project(atracdenc)
include_directories(src)
if (UNIX)
    find_package(GTest)
    if (GTest_FOUND)
        add_subdirectory(test)
        enable_testing()
    else()
        message("gtest library has not been found, skipping ut...")
    endif()

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
