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
    src/tools/multimeter.cpp
    src/ic/ic_8284a.cpp
    src/ic/ic_8288.cpp
)

target_include_directories(bench PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(bench PRIVATE spdlog::spdlog fmt::fmt)

# Set working directory to project root so relative paths (e.g. assets/) resolve correctly.
set_target_properties(bench PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
