cmake_minimum_required(VERSION 3.25)
project(bench VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(spdlog CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)

add_executable(bench
    src/main.cpp
    src/core/signal.cpp
    src/core/component.cpp
    src/board/socket.cpp
    src/board/isa_slot.cpp
    src/board/passive.cpp
    src/board/brd_parser.cpp
    src/board/motherboard.cpp
)

target_include_directories(bench PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(bench PRIVATE spdlog::spdlog fmt::fmt)
