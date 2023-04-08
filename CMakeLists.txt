cmake_minimum_required(VERSION 3.24)
project(picotor)

set(CMAKE_CXX_STANDARD 17)

add_executable(picotor src/main.cpp include/sha1.hpp include/torrent.hpp src/torrent.cpp src/common.cpp include/httprequest.hpp include/message.hpp src/message.cpp include/peer.hpp src/peer.cpp include/result.hpp)

include_directories(include /usr/local/include)

find_library(PTHREAD pthread)
target_link_libraries(picotor PUBLIC ${PTHREAD})

add_compile_options(-Wall -Wextra -Wpedantic)