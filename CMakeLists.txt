cmake_minimum_required (VERSION 3.10.2)
set (CMAKE_CXX_STANDARD 17)

project ("blazevg")

set (BLAZEVG_SOURCES
    "include/blazevg.hh"
    "source/blazevg.cc")

set (GLM_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../glm/" )

if (NOT TARGET glm)
    add_subdirectory(${GLM_PATH} "${CMAKE_CURRENT_BINARY_DIR}/glm")
endif()

add_library (
             blazevg
             ${BLAZEVG_SOURCES} )

target_include_directories (blazevg PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include/"
        ${GLM_PATH})

target_link_libraries (blazevg glm)
