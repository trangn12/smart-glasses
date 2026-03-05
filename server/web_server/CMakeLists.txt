cmake_minimum_required(VERSION 3.14)
project(subtitle_web_server CXX)

set(CMAKE_CXX_STANDARD 17)

find_package(Boost 1.70 REQUIRED)
find_package(CURL REQUIRED)

add_executable(web_server main.cpp)
target_include_directories(web_server PRIVATE
  ${Boost_INCLUDE_DIRS}
  ${CURL_INCLUDE_DIRS}
)
target_link_libraries(web_server
  CURL::libcurl
)
