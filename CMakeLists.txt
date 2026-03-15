cmake_minimum_required(VERSION 3.25)
project(bench VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(spdlog CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)

# Collect all headers so they appear in Visual Studio Solution Explorer
file(GLOB_RECURSE BENCH_HEADERS CONFIGURE_DEPENDS src/*.h)

add_executable(bench
    src/main.cpp
    src/core/signal.cpp
    src/core/component.cpp
    src/core/threaded_component.cpp
    src/core/fiber_component.cpp
    src/core/callback_component.cpp
    src/host_platform/fiber_minicoro.cpp
    src/board/socket.cpp
    src/board/isa_slot.cpp
    src/board/passive.cpp
    src/board/brd_parser.cpp
    src/board/motherboard.cpp
    src/tools/multimeter.cpp
    src/ic/ic_8284a.cpp
    src/ic/ic_8288.cpp
    src/ic/ic_8253.cpp
    src/ic/ic_74s175.cpp
    src/ic/ic_8259a.cpp
    src/ic/ic_8255a.cpp
    src/ic/ic_8237a.cpp
    src/ic/ic_8088.cpp
    src/ic/ic_rom_8k.cpp
    src/ic/ic_74s373.cpp
    src/ic/ic_74s245.cpp
    src/ic/ic_74s138.cpp
    src/ic/ic_74s20.cpp
    src/ic/ic_74s00.cpp
    src/ic/ic_74s04.cpp
    src/ic/ic_74s08.cpp
    src/ic/ic_74s158.cpp
    src/ic/ic_td1.cpp
    src/ic/ic_74s74.cpp
    src/ic/ic_74s00_u81.cpp
    src/ic/ic_dram_256k.cpp
    ${BENCH_HEADERS}
)

target_include_directories(bench PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(bench PRIVATE spdlog::spdlog fmt::fmt)

# Organize source files into VS filters matching directory structure
source_group(TREE ${CMAKE_SOURCE_DIR}/src PREFIX "Source Files" FILES ${BENCH_HEADERS})

# Pin validation: runtime checks that every read/write matches declared direction.
# Disable for release builds.
#add_compile_definitions(BENCH_PIN_VALIDATION)

# AVX2
if(MSVC)
    add_compile_options(/arch:AVX2)
else()
    add_compile_options(-mavx2)
endif()

# Set working directory to project root so relative paths (e.g. assets/) resolve correctly.
set_target_properties(bench PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# 8088 test bench -- isolated CPU instruction tests
add_executable(test_8088
    src/test/test_8088.cpp
    src/core/signal.cpp
    src/core/component.cpp
    src/core/threaded_component.cpp
    src/core/fiber_component.cpp
    src/core/callback_component.cpp
    src/host_platform/fiber_minicoro.cpp
    src/board/socket.cpp
    src/ic/ic_8088.cpp
    src/ic/ic_8288.cpp
    src/ic/ic_74s245.cpp
    src/ic/ic_8259a.cpp
    src/ic/ic_8284a.cpp
    src/ic/ic_74s373.cpp
    src/ic/ic_74s138.cpp
    src/ic/ic_74s20.cpp
    src/ic/ic_74s00.cpp
    src/ic/ic_74s04.cpp
    src/ic/ic_74s08.cpp
    src/ic/ic_74s158.cpp
    src/ic/ic_td1.cpp
    src/ic/ic_74s74.cpp
    src/ic/ic_74s00_u81.cpp
    src/ic/ic_rom_8k.cpp
    src/ic/ic_dram_256k.cpp
    ${BENCH_HEADERS}
)
target_include_directories(test_8088 PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_8088 PRIVATE spdlog::spdlog fmt::fmt)

# ---------- NASM assembler for test binaries ----------
find_program(NASM_EXECUTABLE nasm REQUIRED)
message(STATUS "Found NASM: ${NASM_EXECUTABLE}")

set(ASM_TEST_DIR ${CMAKE_SOURCE_DIR}/src/test/asm)
set(ASM_BIN_DIR  ${CMAKE_BINARY_DIR}/test_bins)
file(MAKE_DIRECTORY ${ASM_BIN_DIR})

file(GLOB ASM_SOURCES CONFIGURE_DEPENDS ${ASM_TEST_DIR}/*.asm)
set(ASM_BINS "")
foreach(asm_src ${ASM_SOURCES})
    get_filename_component(asm_name ${asm_src} NAME_WE)
    set(asm_bin ${ASM_BIN_DIR}/${asm_name}.bin)
    add_custom_command(
        OUTPUT  ${asm_bin}
        COMMAND ${NASM_EXECUTABLE} -f bin -o ${asm_bin} ${asm_src}
        DEPENDS ${asm_src}
        COMMENT "NASM: ${asm_name}.asm -> ${asm_name}.bin"
    )
    list(APPEND ASM_BINS ${asm_bin})
endforeach()

add_custom_target(asm_tests ALL DEPENDS ${ASM_BINS})
add_dependencies(test_8088 asm_tests)

# Pass bin directory to test_8088 so it can find the assembled tests.
target_compile_definitions(test_8088 PRIVATE
    ASM_TEST_DIR="${ASM_BIN_DIR}"
    ASM_SRC_DIR="${ASM_TEST_DIR}"
    ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets"
)
