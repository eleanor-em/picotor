cmake_minimum_required(VERSION 3.24)
project(picotor)

set(CMAKE_CXX_STANDARD 17)

add_executable(picotor src/main.cpp include/sha1.hpp include/torrent.hpp src/torrent.cpp src/common.cpp include/httprequest.hpp include/message.hpp src/message.cpp)

include_directories(include)

find_library(PTHREAD pthread)
find_library(BOOST_THREAD boost_thread)
target_link_libraries(picotor PUBLIC ${PTHREAD} ${BOOST_THREAD})

add_compile_options(-Wall -Wextra -Wpedantic)