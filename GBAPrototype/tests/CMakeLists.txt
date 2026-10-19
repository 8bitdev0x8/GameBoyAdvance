cmake_minimum_required(VERSION 3.25)
project(gba_loader_test LANGUAGES C CXX)
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../GBAFeather/src/core/cpu.cpp")
  message(FATAL_ERROR "The emulator core is intentionally excluded from this public repository. See docs/building.md and THIRD_PARTY_NOTICES.md; the native emulator tests require separately permitted matching core sources.")
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
file(GLOB CORE_SOURCES ../GBAFeather/src/core/*.cpp)
list(FILTER CORE_SOURCES EXCLUDE REGEX "/cpu.cpp$")
add_executable(gba_loader_test main.cpp portable_cpu.cpp ${CORE_SOURCES}
  ../GBAFeather/src/psram_arena.cpp
  ../GBAFeather/src/tinf/tinflate.c ../GBAFeather/src/tinf/crc32.c)
target_include_directories(gba_loader_test PRIVATE stubs ../GBAFeather ../GBAFeather/src ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(gba_loader_test PRIVATE GBA_HOST_TEST GBA_PLATFORM_RP2350 GBA_ENABLE_HLE_BIOS _CRT_SECURE_NO_WARNINGS)
if(MSVC)
  target_compile_options(gba_loader_test PRIVATE /O2 /wd4068)
endif()

if(GBA_PPU_REFERENCE_DIR)
  add_executable(gba_ppu_compare ppu_compare.cpp
    ${GBA_PPU_REFERENCE_DIR}/ppu_reference.cpp
    ../GBAFeather/src/core/ppu.cpp ../GBAFeather/src/core/irq.cpp
    ../GBAFeather/src/psram_arena.cpp)
  target_include_directories(gba_ppu_compare PRIVATE stubs ../GBAFeather/src ${GBA_PPU_REFERENCE_DIR})
  target_compile_definitions(gba_ppu_compare PRIVATE GBA_HOST_TEST GBA_PLATFORM_RP2350)
  if(MSVC)
    target_compile_options(gba_ppu_compare PRIVATE /O2 /wd4068)
  endif()
endif()
