
# /* (c) Conduce, Inc. */

cmake_minimum_required(VERSION 2.8.7)


project(live-data-generator CXX C)

if (NOT PACKAGE_VERSION)
   set (PACKAGE_VERSION unknown)
endif()

include ("cmake/RapidJSONConfig.cmake")

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Werror")
set (CMAKE_CXX_FLAGS_DEBUG "-O0 -g -DDEBUG")

add_subdirectory(src/entity-generator)
