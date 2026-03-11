cmake_minimum_required(VERSION 3.14)
project(serve LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

# Boost 1.90+ removed boost_system (header-only since 1.69); no component needed
find_package(Boost 1.70 REQUIRED)
find_package(CURL REQUIRED)

add_executable(serve serve.cpp)
target_include_directories(serve PRIVATE ${Boost_INCLUDE_DIRS})
target_link_libraries(serve PRIVATE CURL::libcurl pthread)
