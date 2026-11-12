file(GLOB VP_CORE_SOURCES CONFIGURE_DEPENDS src/*.cpp)
add_library(vp_core STATIC ${VP_CORE_SOURCES})
target_include_directories(vp_core PUBLIC include)
target_link_libraries(vp_core PUBLIC mpv::mpv)
