cmake_minimum_required(VERSION 2.8.12)
project(Geo-Data-Processing-With-S2)
set ( SOURCES s2-only.cpp)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost 1.50 REQUIRED COMPONENTS filesystem)
message(STATUS "Boost version: ${Boost_VERSION}")

find_library(S2 libs2.so)

include_directories( ${CMAKE_BINARY_DIR}  )

add_executable( ${PROJECT_NAME} ${SOURCES} )
target_link_libraries( ${PROJECT_NAME} ${S2} )