cmake_minimum_required(VERSION 3.25)
project(gba_loader_test LANGUAGES C CXX)
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../GBAFeather/src/core/cpu.cpp")
  message(FATAL_ERROR "The emulator core is intentionally excluded from this public repository. See docs/building.md and THIRD_PARTY_NOTICES.md; matching permitted core sources are required.")
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_executable(gba_debug_log_test debug_log_test.cpp)
target_include_directories(gba_debug_log_test PRIVATE ../GBAFeather)
add_executable(gba_render_profile_test render_profile_test.cpp)
target_include_directories(gba_render_profile_test PRIVATE ../GBAFeather ../GBAFeather/src)
add_executable(gba_pc_profiler_test pc_profiler_test.cpp)
target_include_directories(gba_pc_profiler_test PRIVATE ../GBAFeather/src)
add_executable(gba_scan_buffer_test scan_buffer_test.cpp
  ../GBAFeather/src/BiosStorage.cpp
  ../GBAFeather/src/core/cartridge.cpp ../GBAFeather/src/core/rom_provider.cpp
  ../GBAFeather/src/psram_arena.cpp)
target_include_directories(gba_scan_buffer_test PRIVATE stubs ../GBAFeather/src)
target_compile_definitions(gba_scan_buffer_test PRIVATE GBA_HOST_TEST GBA_PLATFORM_RP2350 _CRT_SECURE_NO_WARNINGS)
if(MSVC)
  target_compile_options(gba_scan_buffer_test PRIVATE /O2 /wd4068)
endif()
file(GLOB CORE_SOURCES ../GBAFeather/src/core/*.cpp)
list(FILTER CORE_SOURCES EXCLUDE REGEX "/cpu.cpp$")
foreach(variant baseline candidate)
  add_executable(gba_v46_${variant} performance_probe.cpp portable_cpu.cpp ${CORE_SOURCES}
    ../GBAFeather/src/psram_arena.cpp ../GBAFeather/src/tinf/tinflate.c ../GBAFeather/src/tinf/crc32.c)
  target_include_directories(gba_v46_${variant} PRIVATE stubs ../GBAFeather ../GBAFeather/src)
  target_compile_definitions(gba_v46_${variant} PRIVATE GBA_HOST_TEST GBA_PLATFORM_RP2350
    GBA_ENABLE_HLE_BIOS GBA_NO_ALU_COUNTER GBA_DISABLE_BLOCKS GBA_DISABLE_ALU_BURST _CRT_SECURE_NO_WARNINGS)
  if(variant STREQUAL "baseline")
    target_compile_definitions(gba_v46_${variant} PRIVATE GBA_DISABLE_LEAF_FETCH)
  endif()
  if(MSVC)
    target_compile_options(gba_v46_${variant} PRIVATE /O2 /wd4068)
  endif()
endforeach()
foreach(variant baseline cpu render all)
  add_executable(gba_v45_${variant} performance_probe.cpp portable_cpu.cpp ${CORE_SOURCES}
    ../GBAFeather/src/psram_arena.cpp ../GBAFeather/src/tinf/tinflate.c ../GBAFeather/src/tinf/crc32.c)
  target_include_directories(gba_v45_${variant} PRIVATE stubs ../GBAFeather ../GBAFeather/src)
  target_compile_definitions(gba_v45_${variant} PRIVATE GBA_HOST_TEST GBA_PLATFORM_RP2350
    GBA_ENABLE_HLE_BIOS GBA_NO_ALU_COUNTER GBA_DISABLE_BLOCKS _CRT_SECURE_NO_WARNINGS)
  if(variant STREQUAL "baseline" OR variant STREQUAL "render")
    target_compile_definitions(gba_v45_${variant} PRIVATE GBA_DISABLE_ALU_BURST GBA_LEGACY_PREFETCH_STEP)
  endif()
  if(variant STREQUAL "baseline" OR variant STREQUAL "cpu")
    target_compile_definitions(gba_v45_${variant} PRIVATE GBA_LEGACY_COLOR_MATH GBA_LEGACY_BG_OUTPUT)
  endif()
  if(MSVC)
    target_compile_options(gba_v45_${variant} PRIVATE /O2 /wd4068)
  endif()
endforeach()
add_executable(gba_performance_probe performance_probe.cpp portable_cpu.cpp ${CORE_SOURCES}
  ../GBAFeather/src/psram_arena.cpp ../GBAFeather/src/tinf/tinflate.c ../GBAFeather/src/tinf/crc32.c)
target_include_directories(gba_performance_probe PRIVATE stubs ../GBAFeather ../GBAFeather/src)
target_compile_definitions(gba_performance_probe PRIVATE GBA_HOST_TEST GBA_PLATFORM_RP2350 GBA_ENABLE_HLE_BIOS GBA_NO_ALU_COUNTER _CRT_SECURE_NO_WARNINGS)
if(MSVC)
  target_compile_options(gba_performance_probe PRIVATE /O2 /wd4068)
endif()
add_executable(gba_block_baseline performance_probe.cpp portable_cpu.cpp ${CORE_SOURCES}
  ../GBAFeather/src/psram_arena.cpp ../GBAFeather/src/tinf/tinflate.c ../GBAFeather/src/tinf/crc32.c)
target_include_directories(gba_block_baseline PRIVATE stubs ../GBAFeather ../GBAFeather/src)
target_compile_definitions(gba_block_baseline PRIVATE GBA_HOST_TEST GBA_PLATFORM_RP2350
  GBA_ENABLE_HLE_BIOS GBA_NO_ALU_COUNTER GBA_DISABLE_BLOCKS _CRT_SECURE_NO_WARNINGS)
if(MSVC)
  target_compile_options(gba_block_baseline PRIVATE /O2 /wd4068)
endif()
add_executable(gba_fetch_baseline performance_probe.cpp portable_cpu.cpp ${CORE_SOURCES}
  ../GBAFeather/src/psram_arena.cpp ../GBAFeather/src/tinf/tinflate.c ../GBAFeather/src/tinf/crc32.c)
target_include_directories(gba_fetch_baseline PRIVATE stubs ../GBAFeather ../GBAFeather/src)
target_compile_definitions(gba_fetch_baseline PRIVATE GBA_HOST_TEST GBA_PLATFORM_RP2350
  GBA_ENABLE_HLE_BIOS GBA_NO_ALU_COUNTER GBA_LEGACY_FETCH GBA_DISABLE_BLOCKS _CRT_SECURE_NO_WARNINGS)
if(MSVC)
  target_compile_options(gba_fetch_baseline PRIVATE /O2 /wd4068)
endif()
if(GBA_COMPOSITOR_BASELINE_FILE)
  set(BASELINE_CORE_SOURCES ${CORE_SOURCES})
  list(FILTER BASELINE_CORE_SOURCES EXCLUDE REGEX "/ppu.cpp$")
  add_executable(gba_compositor_baseline performance_probe.cpp portable_cpu.cpp
    ${BASELINE_CORE_SOURCES} ${GBA_COMPOSITOR_BASELINE_FILE}
    ../GBAFeather/src/psram_arena.cpp ../GBAFeather/src/tinf/tinflate.c ../GBAFeather/src/tinf/crc32.c)
  target_include_directories(gba_compositor_baseline PRIVATE stubs ../GBAFeather ../GBAFeather/src)
  target_compile_definitions(gba_compositor_baseline PRIVATE GBA_HOST_TEST GBA_PLATFORM_RP2350
    GBA_ENABLE_HLE_BIOS GBA_NO_ALU_COUNTER GBA_DISABLE_BLOCKS _CRT_SECURE_NO_WARNINGS)
  if(MSVC)
    target_compile_options(gba_compositor_baseline PRIVATE /O2 /wd4068)
  endif()
endif()
add_executable(gba_loader_test main.cpp portable_cpu.cpp ${CORE_SOURCES}
  ../GBAFeather/src/psram_arena.cpp
  ../GBAFeather/src/tinf/tinflate.c ../GBAFeather/src/tinf/crc32.c)
target_include_directories(gba_loader_test PRIVATE stubs ../GBAFeather ../GBAFeather/src ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(gba_loader_test PRIVATE GBA_HOST_TEST GBA_PLATFORM_RP2350 GBA_ENABLE_HLE_BIOS _CRT_SECURE_NO_WARNINGS)
if(MSVC)
  target_compile_options(gba_loader_test PRIVATE /O2 /wd4068)
endif()

add_executable(gba_core_audit core_audit.cpp color_math_audit.cpp block_execution_audit.cpp thumb_alu_audit.cpp video_audit.cpp bios_audit.cpp instruction_window_audit.cpp portable_cpu.cpp ${CORE_SOURCES}
  ../GBAFeather/src/psram_arena.cpp)
target_include_directories(gba_core_audit PRIVATE stubs ../GBAFeather ../GBAFeather/src)
target_compile_definitions(gba_core_audit PRIVATE GBA_HOST_TEST GBA_PLATFORM_RP2350 GBA_ENABLE_HLE_BIOS)
if(GBA_TIMER_REFERENCE_DIR)
  target_sources(gba_core_audit PRIVATE timer_compare.cpp ${GBA_TIMER_REFERENCE_DIR}/timers_reference.cpp)
  target_include_directories(gba_core_audit PRIVATE ${GBA_TIMER_REFERENCE_DIR})
  target_compile_definitions(gba_core_audit PRIVATE GBA_TIMER_AUDIT)
endif()
if(MSVC)
  target_compile_options(gba_core_audit PRIVATE /O2 /wd4068)
endif()

if(GBA_PPU_REFERENCE_DIR)
  add_executable(gba_register_snapshot_test register_snapshot_test.cpp
    ${GBA_PPU_REFERENCE_DIR}/ppu_reference.cpp
    ../GBAFeather/src/core/ppu.cpp ../GBAFeather/src/core/irq.cpp
    ../GBAFeather/src/core/render_worker.cpp ../GBAFeather/src/psram_arena.cpp)
  target_include_directories(gba_register_snapshot_test PRIVATE stubs ../GBAFeather/src ${GBA_PPU_REFERENCE_DIR})
  target_compile_definitions(gba_register_snapshot_test PRIVATE GBA_HOST_TEST GBA_PLATFORM_RP2350)
  if(MSVC)
    target_compile_options(gba_register_snapshot_test PRIVATE /O2 /wd4068)
  endif()
  add_executable(gba_ppu_compare ppu_compare.cpp
    ${GBA_PPU_REFERENCE_DIR}/ppu_reference.cpp
    ../GBAFeather/src/core/ppu.cpp ../GBAFeather/src/core/irq.cpp
    ../GBAFeather/src/core/render_worker.cpp
    ../GBAFeather/src/psram_arena.cpp)
  target_include_directories(gba_ppu_compare PRIVATE stubs ../GBAFeather/src ${GBA_PPU_REFERENCE_DIR})
  target_compile_definitions(gba_ppu_compare PRIVATE GBA_HOST_TEST GBA_PLATFORM_RP2350)
  if(MSVC)
    target_compile_options(gba_ppu_compare PRIVATE /O2 /wd4068)
  endif()
endif()
