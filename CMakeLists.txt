cmake_minimum_required (VERSION 3.10.2)
set (CMAKE_CXX_STANDARD 17)

project ("blazevg")

set (BLAZEVG_SOURCES
    "include/blazevgc.h"
    "include/blazevg.hh"
    "include/backends/diligent.hh"
    "source/blazevgc.c"
    "source/blazevg.cc"
    "source/backends/diligent.cc")

set (GLM_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../glm/" )
set (DILIGENT_CORE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../DiligentCore/")
set (JSONCPP_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../jsoncpp/")

if (NOT TARGET glm)
    add_subdirectory (${GLM_PATH} "${CMAKE_CURRENT_BINARY_DIR}/glm")
endif()

if (NOT TARGET DiligentCore-ValidateFormatting)
        add_subdirectory (${DILIGENT_CORE_PATH} "${CMAKE_CURRENT_BINARY_DIR}/DiligentCore")
endif()

if (NOT TARGET jsoncpp)
    add_subdirectory(${JSONCPP_PATH} "${CMAKE_CURRENT_BINARY_DIR}/jsoncpp")
endif()

add_library (blazevg
             ${BLAZEVG_SOURCES})

target_include_directories (blazevg PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include/"
        "${JSONCPP_PATH}/include"
        ${DILIGENT_CORE_PATH}
        ${GLM_PATH})

target_link_libraries (blazevg glm Diligent-Common jsoncpp_static)
