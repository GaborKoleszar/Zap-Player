#pragma once
#include <windows.h>

namespace win {

class AppWindow;

enum class MenuKind { Full, Subtitles, Audio, Speed };

inline constexpr double kSpeeds[] = {0.5, 0.75, 1.0, 1.25, 1.5, 2.0};

// Shows a popup menu and carries out the chosen item. `above` opens the menu upwards from the
// point (used for menus anchored to a control bar button).
void show_menu(AppWindow& app, MenuKind kind, POINT screen_pt, bool above = false);

}  // namespace win
