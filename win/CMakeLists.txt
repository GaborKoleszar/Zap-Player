file(GLOB VP_WIN_SOURCES CONFIGURE_DEPENDS *.cpp *.h)
add_executable(Zap WIN32 ${VP_WIN_SOURCES} Zap.manifest zap.rc)
target_link_libraries(Zap PRIVATE vp_core dwmapi d2d1 dwrite comdlg32 shell32 delayimp)
# Libraries that are not needed to show the first frame are loaded on first use.
target_link_options(Zap PRIVATE
  /DELAYLOAD:d2d1.dll /DELAYLOAD:dwrite.dll /DELAYLOAD:comdlg32.dll /DELAYLOAD:shell32.dll)
set_target_properties(Zap PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/dist
  RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/dist)
# Version details shown by Explorer and read by the installer script.
set_source_files_properties(zap.rc PROPERTIES COMPILE_DEFINITIONS
  "ZAP_VERSION_COMMA=${PROJECT_VERSION_MAJOR},${PROJECT_VERSION_MINOR},${PROJECT_VERSION_PATCH},0;ZAP_VERSION_STR=\"${PROJECT_VERSION}\"")
vp_copy_mpv(Zap)
