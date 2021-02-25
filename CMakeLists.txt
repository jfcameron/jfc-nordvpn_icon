# © 2021 Joseph Cameron - All Rights Reserved
cmake_minimum_required(VERSION 3.9 FATAL_ERROR)

project(null)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/jfc-cmake/jfclib.cmake")

option(JFC_BUILD_DOCS "Build documentation" OFF)
option(JFC_BUILD_TESTS "Build unit tests" OFF)

add_subdirectory(thirdparty)

# GUI
FIND_PACKAGE(PkgConfig REQUIRED)
PKG_CHECK_MODULES(GTK3 REQUIRED gtk+-3.0)

jfc_project(executable
    NAME "jfcnordvpnicon"
    VERSION 1.0
    DESCRIPTION "gtk icon for nordvpn"
    C++_STANDARD 17
    C_STANDARD 90

    PUBLIC_INCLUDE_DIRECTORIES
        ${CMAKE_CURRENT_SOURCE_DIR}/include

    PRIVATE_INCLUDE_DIRECTORIES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/include
        ${GTK3_INCLUDE_DIRS}

    LIBRARIES
        ${GTK3_LIBRARIES}

    SOURCE_LIST
        ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/nordvpn-32-connected.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/nordvpn-32-default.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/nordvpn-32-disconnected.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/nordvpn-32-error.cpp
)

if (NOT ${CMAKE_CXX_COMPILER_ID} STREQUAL MSVC)
    target_link_libraries(${PROJECT_NAME} stdc++fs)
endif()

if (JFC_BUILD_TESTS)
    add_subdirectory(test)
endif()

if (JFC_BUILD_DOCS)
    add_subdirectory(docs)
endif()

