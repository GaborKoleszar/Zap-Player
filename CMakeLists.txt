cmake_minimum_required(VERSION 3.24)
project(Zap VERSION 0.1.0 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
add_compile_options(/W4 /utf-8 /permissive- /MP)
add_compile_definitions(UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)

set(MPV_DIR ${CMAKE_SOURCE_DIR}/third_party/mpv)
if(NOT EXISTS ${MPV_DIR}/mpv.lib)
  message(FATAL_ERROR "libmpv not found. Run: pwsh scripts/fetch-mpv.ps1")
endif()
add_library(mpv::mpv SHARED IMPORTED GLOBAL)
set_target_properties(mpv::mpv PROPERTIES
  IMPORTED_IMPLIB ${MPV_DIR}/mpv.lib
  IMPORTED_LOCATION ${MPV_DIR}/libmpv-2.dll
  INTERFACE_INCLUDE_DIRECTORIES ${MPV_DIR}/include)

function(vp_copy_mpv target)
  add_custom_command(TARGET ${target} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${MPV_DIR}/libmpv-2.dll $<TARGET_FILE_DIR:${target}>)
endfunction()

enable_testing()
add_subdirectory(core)
add_subdirectory(tests)
add_subdirectory(win)
