cmake_minimum_required (VERSION 3.10.2)
set (CMAKE_CXX_STANDARD 17)

project ("blazevg")

set (BLAZEVG_SOURCES
    "include/blazevg.hh"
    "include/backends/diligent.hh"
    "source/blazevg.cc"
    "source/backends/diligent.cc")

set (GLM_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../glm/" )
set (DILIGENT_CORE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../DiligentCore/")

if (NOT TARGET glm)
    add_subdirectory (${GLM_PATH} "${CMAKE_CURRENT_BINARY_DIR}/glm")
endif()

if (NOT TARGET DiligentCore-ValidateFormatting)
        add_subdirectory (${DILIGENT_CORE_PATH} "${CMAKE_CURRENT_BINARY_DIR}/DiligentCore")
endif()

add_library (blazevg
             ${BLAZEVG_SOURCES})

target_include_directories (blazevg PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include/"
        ${DILIGENT_CORE_PATH}
        ${GLM_PATH})

target_link_libraries (blazevg glm)
